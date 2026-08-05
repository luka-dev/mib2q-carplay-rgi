# altScreen: штатная NavStatus-полоса приборки

Полоса поверх altScreen — не часть iOS-видео. Это `SV_LVDS_FPK_NavMap.NavStatus`
в `hmi_app.elf` приборки. Её данные приходят отдельными BAP Navigation
properties, поэтому существующий route-guidance gate их не блокировал.

## Подтверждённые поля и значения очистки

| Элемент | BAP | Отправляемое значение при CarPlay takeover | Проверка в VC |
|---|---:|---|---|
| Compass | FctID 16 | Java `angle=-1`, `symbolic=-1` → wire `0xFFFF/0xFF` (`not supported`) | `VC_RecomputeStatusbarCompass` `0x176474–0x176554`; `CompassInfo_Status.serialize` writes byte+short, and enum `0xFF` is the protocol-defined `not supported` value |
| Altitude | FctID 47 | altitude `0x7FFF`, unit `2` | `VC_RecomputeAltitude` `0x177B1C–0x177C78`: altitude `0x7FFF` или unit > 1 очищают строку и выставляют unavailable-unit |
| Scale | FctID 45 | autoZoom `15`, scale `0xFFFF`, unit `0xFF` | `VC_RecomputeMapScale` `0x177D10`; `VC_IsMapScaleUnitRenderable` `0x610768–0x61078C`; `VC_RecomputeMapScaleVisibility` `0x6121B8–0x6121D4` выставляет `Nav_Map_Scale_visible=0` |
| Center text | FctID 19/20 | CurrentPosition `""`; TurnTo/SignPost `"",""` | `VC_UpdateNavStatusBarText` `0x178318–0x178688` очищает `Nav_Map_Streetname` и availability DP |

Для compass нельзя передавать в штатный `AppConnectorNavi` Java-значение
`symbolic=255`: оно запускает ветку обратного расчёта heading (`AppConnectorNavi.java:144–149`).
Пара `-1/-1` не попадает в эту special-case ветку, а `CompassInfo_Status.serialize`
записывает Java `-1` как байт `0xFF` и short `0xFFFF`. Прежняя пара
`361/16` на unit оставляла видимый `N` и больше не используется.

## Реализация

`AltScreenModule` владеет этим gate на всём интервале
`connected || videoAvailable`. `AltScreenNavStatusGate` устанавливает/переиспользует
единственный `GatedCombiService`, после чего `setAltScreenTakeover(true)`
один раз отправляет invalid statuses и блокирует стоковые
compass/altitude/scale/text до освобождения altScreen. `BAPBridge` не владеет
виджетами и только переключает отдельный route-guidance gate.

## Порядок включения maneuver-контекста

Новая CarPlay-сессия всегда начинает с `ctx 81 = {99}` — обычная
altScreen-карта без maneuver-слоёв. `AltScreenModule.start()` атомарно очищает
старый `navActive`, поэтому cold boot/re-activate не может унаследовать `ctx 80`
от предыдущего запуска.

`ctx 80 = {98,101,102,99}` разрешается только после последовательности:

1. `RouteGuidance` получает RGI по прежнему проверенному activation gate;
2. `BAPBridge.onStart()` успешно завершает BAP start/sync;
3. текущий cached/sticky RGI delta успешно проходит `BAPBridge.update()`;
4. текущий TCP peer `maneuver_render` подтверждает `FRAME_READY` после
   `eglSwapBuffers`.

Только после этого `RouteGuidance` вызывает `AltScreenModule.setNavActive(true)`.
При старте Java с уже подключённым и ведущим маршрут телефоном
`RouteGuidance.start()` отправляет `CMD_SYNC_REQ`; C hook повторно отдаёт sticky
snapshot уже после регистрации Java listener. Это закрывает старую гонку, когда
connect-time snapshot терялся до старта RGI-модуля и `setNavActive(true)` больше
никогда не вызывался.

`MapVisibility`/FctID 44 намеренно не меняется. В VC обработчик
MapViewAndOrientation использует LVDS-visible bit для открытия KDK video-hole;
его очистка спрятала бы вместе с полосой и сам altScreen.

## MapScale SetGet от рулевой крутилки

Сокрытие FctID 45 не отменяет входящие запросы приборки. Поворот рулевой
крутилки приходит как `MapScale_SetGet`; штатный
`BAPIndicationHandlerNavi.processMapScaleSetGet()` вызывает
`setMapScaleSetting()` и `setMapScale()`. Последний обязан закончиться
`AppConnectorNavi.updateMapScale()` — это ответный `MapScale_Status`.

Старая версия gate проглатывала этот callback. В результате выставленный в
`BAPFunctionPropertyFSG.setGetPending` запрос оставался без ответа, что на unit
проявлялось переходом NAVSD/приборки в `INITIALIZING` после вращения крутилки.

Во время altScreen `GatedCombiService.updateMapScale()` теперь отвечает тем же
невидимым статусом `15/0xFFFF/0xFF`. Это не возвращает overlay: точный MU1316
`BAPFunctionPropertyFSG.statusREQ()` принудительно отправляет даже неизменившийся
status при `setGetPending=true`, а обычные фоновые повторы по-прежнему
отфильтровываются `sendStatusIfChanged()`.

Перехват рулевого события начинается по ownership intent
`AltScreenModule.isConnected()`, до завершения асинхронного DisplayManager
switch. Поэтому во время SETUP/bounce запрос больше не успевает попасть в
скрытую Audi-карту через `activeContext.increment(400476, steps)`.

## Проверка на unit

После загрузки JAR и штатного reboot/connect:

1. altScreen-карта продолжает отображаться;
2. исчезают elevation, центральный BAP text, scale и compass;
3. управление scale с руля не возвращает stock scale overlay;
4. после отключения CarPlay gate открывается, и штатные BAP updates снова
   восстанавливают элементы Audi navigation.

Если после очистки всех дочерних элементов останется пустая полупрозрачная
подложка, она принадлежит корневому `NavStatus` scene node. Тогда требуется
отдельный VC scene/display-state gate; выключать для этого FctID 44 нельзя.
