# View / KDK: штатная синхронизация MU1316

Проверено 2026-09-22 по декомпилированному MU1316 и байткоду MU1316-final.jar.
Задача: при View подложка inTube появляется поверх карты до окончания трансформации VC.
Статическое исследование дополнено реализацией и host-проверками; см. последний раздел.

## Найденный механизм

Сток разделяет положение панели и разрешение её видимости. Для KDK на FPK
в рассмотренной цепочке нет таймера ожидания завершения View: видимость приходит
обратно через BAP. Это не универсальный callback завершения всей анимации.

### Видимость — NavSD FctID 44, MapViewAndOrientation_SetGet

В mapVisibility:

- bit0: lvdsMapIsVisible — основная карта;
- bit1: supplementaryMapViewIsVisible — дополнительная область KDK.

Цепочка обработки входящего SetGet:

```
BAPIndicationHandlerNavi.processMapViewAndOrientationSetGet()
  → getNaviService().setMapVisibility(lvds, supplementary)
  → CombiBAPListener.setMapVisibility(lvds, supplementary, false)
  → ClusterService.setSupplementaryMap(view, supplementary)
  → ClusterService.setKDKVisibility(supplementary)
  → ClusterKDKHandlerImpl.setKDKVisibility(supplementary)
  → refreshState / refreshHints
  → модель choice(1,168), hint16 = kdkVisible
  → CombiMapController.handleKdkDualTerminal()
  → opacity манёвра и выбранной подложки = hint16 ? 100 : 0
```

Indication handler вызывает setMapVisibility только при отличии входящей
структуры mapVisibility от lastStatus. CombiBAPListener тоже проверяет изменение
supplementaryMapViewVisible, прежде чем передать его ClusterService.
Поэтому отсутствие callback не доказывает отсутствие входящего пакета.
Для диагностики важна точка ДО подавления повторных значений.

Смысл hint16 в этой цепочке — полученная видимость KDK. Имя FADED_IN в коде
потребителя не доказывает, что это отдельное событие с прогрессом или точной
временной меткой окончания всех анимаций.

Hint4 отвечает за наличие/выбор KDK. На FPK при доступном KDK он может остаться
установленным, пока hint16 сброшен: панель существует, но прозрачна.
Именно это различие теряется при принудительном opacity100 по navActive.

ClusterKDKHandlerImpl.Q7FPK_FALLBACK_TIMEOUT инициализирован -1; его fallbackTimer
при этом не создаётся. Это не штатный фиксированный таймер 250 мс.

### Положение — NavSD FctID 54, Map_Presentation_SetGet

```
BAPIndicationHandlerNavi.processMapPresentationSetGet()
  → CombiBAPListener.setMapPresentation(largeMapView, leftMenu, rightMenu)
  → ViewSizeManagerCluster.setViewSize(large ? 2 : 1, false)
  → dispatcher → ClusterKDKHandlerImpl.viewSizeChanged()
  → refreshHints()
  → hint8 = isSmallStageActive()
```

Hint8 выбирает crop/позицию и подложку: inTube101 или popup102.
Hint16 независимо выбирает opacity0/100; вторая подложка получает0.
В стоке нет правила «получили новую позицию — автоматически opacity100».

Само setViewSize сразу меняет currentViewSize и асинхронно уведомляет слушателей.
Это уведомление не является доказанным событием окончания анимации.

## Где callback завершения НЕ найден

Конкретная FPK-реализация ViewSizeManagerCluster:

- getAnimationStatus(): NOT IN USE, return null;
- isAnimationRunning(): return false;
- screenChangeStateFinished(int): пустой метод.

Наличие соответствующих методов интерфейса IViewSizeManager не означает,
что они работают для этой конфигурации. CMD_ALT_ZONE_ACK текущего патча —
ответ телефона на updateViewArea, не окончание stage-morph VC.

## Дефект CarPlay-патча до этой правки

До правки AltScreenModule.setViewAreaMode сразу обновлял desiredViewAreaMode и вызывает
ClusterLayerController.reapply. Контроллер выбирает новую геометрию по этому
целевому режиму и в ветке inTube безусловно задаёт плоскостям98/101 opacity100
при активной навигации. Полученная от стока видимость при владении CarPlay
не управляет этой веткой.

Это согласуется с ранним появлением прямоугольника. Что наблюдаемый серый
прямоугольник действительно plane101, нужно подтвердить на устройстве.

## Практическая точка интеграции и остающаяся проверка

Кандидат для синхронизации — входящая supplementaryMapViewIsVisible FctID44,
а не задержка, рассчитанная из 200мс KDK fade. Для положения полезен также
largeMapView FctID54. Существующий ClusterService.setSupplementaryMap — удобная
поздняя точка наблюдения, но она не видит пакеты, отфильтрованные выше как
неизменившиеся.

Для проверки внедрённого visibility-gate осталось записать на реальной машине
оба направления View с активным манёвром, отдельно Classic и Sport:

1. Входящие FctID44: время, supplementary visibility, lvds visibility, mapView.
2. Входящие FctID54: время, largeMapView.
3. Публикации hint8/hint16 и фактические изменения opacity98/101/102.
4. Сопоставить с видеозаписью появления квадрата/окончания движения.

Проверить, приходит ли supplementary false→true во время каждого перехода и
доходит ли он до модели при CarPlay. До этого нельзя обещать, что данный флаг
закроет все случаи View. Поведение при отсутствии BAP-ответа описано ниже.
Утверждение «FADED_IN во время CarPlay не бывает» без такого наблюдения
считать установленным нельзя: рабочая входящая цепочка в стоке существует.

## Источники

Корень декомпиляции:
`/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-vf/`

- de/audi/app/combi/bap/app/navi/BAPIndicationHandlerNavi.java:651,683
- de/audi/tghu/navi/app/cluster/CombiBAPListener.java:1854,2034
- de/audi/tghu/navi/app/cluster/ClusterService.java:663,959
- de/audi/tghu/navi/app/cluster/ClusterKDKHandlerImpl.java:168,187,222
- de/audi/tghu/navi/app/cluster/ViewSizeManagerCluster.java:50,126
- de/esolutions/hmi/widgets/audi/evo/high/widgets/CombiMapController.java:175
- de/vw/mib/bap/generated/navsd/serializer/MapViewAndOrientation_SetGet.java:163
- de/vw/mib/bap/generated/navsd/serializer/MapViewAndOrientation_MapVisibility.java
- de/vw/mib/bap/generated/navsd/serializer/Map_Presentation_SetGet.java:57

Дополнительная проверка javap -c -p ClusterKDKHandlerImpl из MU1316-final.jar:
setKDKVisibilityHints действительно addHint/removeHint16 по kdkVisible,
отдельно addHint/removeHint4 по доступности/видимости. Вывод декомпилятора в
этой ключевой точке подтверждён байткодом.

## VC: последовательность подтверждена статически (2026-09-22)

Этот раздел уточняет прежнее ограничение: исследован не только HU-получатель,
но и отправитель в VC gtf2 AU491 0379/1310. Нормальный путь смены stage при
активном KDK действительно использует callbacks анимации и BAP-видимость.
Это подтверждение алгоритма; успешная доставка пакетов конкретной машины
и отсутствие ошибок/переопределений в текущем CarPlay-патче из него не следуют.

### Последовательность нормального перехода с активным KDK

1. `0x12acf0 requestUserStageChange` запоминает пользовательский запрос и
   вызывает `0x12a92c sendStageChangeMessage`.
2. Если KDK активен, механизм включён, fade-out ещё не идёт и KDK не скрыт,
   `0x12a92c` сначала вызывает `0x12a7d8`: App10029 KDKHide. Собственно stage
   пока не запускается. Навигационный обработчик `0x17bf90` вызывает
   `0x176780 closeKDKHole`, который запускает AT14 KDKFadeOut.
3. После пересечения **30% прогресса AT14** callback `0x12b500` посылает
   App10030 KDKHideProgress. Навигация вызывает `0x177800(..., false)`:
   сбрасывает MapVisibility bit1 и отправляет запрос **FctID44**.
   HU по уже восстановленной цепочке ставит opacity манёвра/подложки0.
4. По завершении AT14 callback `0x12b42c → 0x12b2f8` отмечает KDKFadedOut,
   сбрасывает FadeOutRunning и исполняет отложенный запрос stage.
   `0x12a92c` запускает stage-анимацию0/1, обновляет DP текущего stage и через
   `0x12a298` посылает App10002. Навигация ставит stageChangeRunning=1.
5. После пересечения **50% stage-прогресса** callback `0x12a23c` посылает
   App10037. Навигация вызывает `0x177230 sendStageInfo`, формирует bit0
   ASG_HMI_State (largeMapView), отправляет **FctID54**.
   HU переключает положение/crop по hint8; видимость остаётся независимой.
6. По завершении stage0/1 callback `0x12a844` сначала посылает App10040
   (навигация снимает stageChangeRunning), затем, если KDKFadedOut и fade-out
   не идёт, App10031 KDKShow.
7. App10031 вызывает `0x177800(..., RG_Status == 1)`. Bit1 может стать1
   только при разрешённой видимости, открытом flap и отсутствии stage-change.
   Формируется запрос **FctID44**; HU включает выбранную подложку/манёвр.
8. Ответный HU **MapViewAndOrientation Status** принимает `0x178ff4`.
   При bit1=1 он вызывает `0x1765bc openKDKHole`; дополнительные guards
   проверяют маршрут, питание, отсутствие перехода/уже идущего открытия.
   Таким образом нормальный путь восстановления включает обмен
   VC SetGet → HU видимость/Status → VC открытие KDK.

В App10031 есть также попытка openKDKHole, если отверстие закрыто и нет
неподтверждённого MO-запроса. Поэтому шаг8 описывает нормальный путь с
отправленным запросом; не следует объявлять, что абсолютно любое открытие
KDK во всех состояниях может происходить только из Status-callback.

### Точная регистрация callbacks

`0x12a490 StageManager_installKdkSyncCallbacks`, вызывается из Init0x12b1c0.
Флаг включения manager+56 по конструктору0x12a18c равен1.

| Анимация | Тип callback | Порог | Функция | Действие |
|---|---|---|---|---|
|14 KDKFadeOut|progress (4)|0.30|0x12b500|App10030 → Fct44 bit1=0|
|14 KDKFadeOut|finished (1)|не используется|0x12b42c|исполнить отложенный stage|
|0/1 stage|progress (4)|0.50|0x12a23c|App10037 → Fct54|
|0/1 stage|finished (1)|не используется|0x12a844|App10040, затем условный App10031|
|19 KDKFlapFadeIn|progress (4)|0.85|0x12b4a0|App10032 → условный запрос видимости|

`0x120b20` использует float-порог только для type4. Для остальных типов
сохраняет callback по состоянию. Поэтому **0.98 в регистрации finished
не означает вызов на98%**. Это подтверждено dispatch завершения в0x11d02c.
`0x11cf6c` вызывает progress-callback при `progress > threshold`, не при
точном равенстве. Реальное выполнение — на первом соответствующем update.

Проценты относятся к прогрессу соответствующей анимации менеджера, а не к
общему времени после View. Если общая длительность AT14 составляет200мс и
выбран локальный линейный ход, порог0.30 соответствует примерно60мс после
начала AT14, с округлением до update. Это не основание ставить HU-таймер60мс:
участники, внешний прогресс и задержки обработки могут влиять на время.

### Защита от преждевременного показа

`0x177800 Nav_requestKdkMapVisibility` выставляет bit1 только если:

```
requestedVisible && KDKFlapOpen && !stageChangeRunning
```

Иначе bit1 сбрасывается. App10032 на85% flap fade-in также проверяет активный
маршрут, OP_State и отсутствие stageChangeRunning. Поэтому это отдельный
разрешённый путь возврата видимости, а не универсальное «после всего View».

### Проверка, что это именно FctID44/54

- `0x177800 → 0x1775a0 → 0x792670`: сериализует MapView,
  SupplementaryMapView, MapVisibility и вызывает транспорт с LSG50/Fct44.
- `0x177230 → 0x17719c → 0x78e398`: отправляет Map_Presentation
  через LSG50/Fct54.
- `0x178ff4`: принимает Status, обновляет подтверждённый MapVisibility и
  вызывает openKDKHole при bit1=1.

Запросы сериализуются по каждому типу: MO outstanding=Nav+2473,
MP outstanding=Nav+2472. Новый MO при незавершённом предыдущем помечается
pending и отправляется при обработке Status. В OP_State!=0 отправка блокируется.
Поэтому порядок логических событий подтверждён, но точные моменты доставки
на HU нельзя заменить жёсткой временной шкалой.

### Вывод для миграции

Для нормального активного KDK статически подтверждена штатная последовательность:
**fade-out → выключение HU-слоя → stage → перестановка в середине → завершение
stage → разрешение HU-слоя → открытие KDK**.

Это обосновывает восстановление независимых штатных hint8/hint16 вместо
принудительного включения новой подложки по целевому View. Оно также уточняет
старую рекомендацию «держать подложку непрозрачной, пока VC всё анимирует»:
при stage-change VC сам просит HU скрыть её во время перехода.

Нельзя обобщать этот путь на неактивный KDK, preview/follow, заблокированный
stage, прерывание перехода и ошибки BAP без учёта их веток. Для реализации
важно сохранять поверхности до штатного снятия видимости и не перекрывать
штатное разрешение нашими navActive/desiredViewAreaMode.

IDA: добавлены комментарии и имена ключевых функций; сохранена
`/Users/luka/Desktop/AUDI_2/Firmwares/VC/gtf2.orig.i64`.


## Реализация и ревью (2026-09-22)

- `AltScreenCombiBAPListener.updateMapVisibility()` перед штатным Status передаёт
  принятое `supplementaryMapViewVisible` в `ClusterLayerController`. Покрывает
  входящий Fct44, внутренний трёхаргументный setter и повторную публикацию Status.
  Сам штатный listener, обновления ClusterService и BAP-подтверждения сохранены.
- `setMapPresentation()` передаёт Fct54 в контроллер; целевой View телефона больше
  не переставляет KDK. Viewport renderer получает тот же принятый этап VC.
- Видимость хранится в одном месте, независимо от сессии CarPlay. `navActive`
  определяет право включить плоскости в контекст; разрешение видимости даёт Fct44.
  Записи контроллера в DisplayManager сериализованы; popup/inTube используют
  общий код применения геометрии и opacity.
- Удалены 250-мс поток снятия, его generation и прежний принудительный opacity100.
  На конце маршрута ctx80 удерживается до штатного снятия видимости. Если KDK уже
  скрыт, ожидания нет. Перезапуск маршрута отменяет ожидающее снятие контекста.
- `BAPBridge.onRouteEnd()` сохраняет последний кадр и соединение renderer,
  прекращая подачу маршрута. Реальное отключение выполняет полный clear/dispose.
  Сохранённое соединение используется повторно на следующем маршруте.
- Возврат штатной навигации использует сохранённые stock hint8/hint16 и opacity.

Проверка: `bash scripts/test_route_info.sh` с полной пересборкой Java 1.4 JAR.
Добавлены сценарии hide → midpoint → show, повторных и внутренних BAP-вызовов,
проверка применения слоёв до Status через реальный MU1316 listener и RG gate,
завершение/перезапуск маршрута, поздний show, восстановление stock backing,
сохранение renderer на route end и освобождение даже после ошибки BAP teardown.
Старый KOMO-тест воспроизводимо падал и на HEAD: ожидал MOST-изменений на FPK.
Теперь он устанавливает framework, используемый BAPBridge, и проверяет действующий
FPK-контракт: не менять штатные gfxAvailable/dataRate и pacing.

Границы проверки: host-тесты подтверждают Java-порядок, а не физический кадр LVDS.
DisplayManager не предоставляет здесь fence готовности кадра; транспорт и encoder
сохраняют собственную задержку. При утрате BAP hide контекст может оставаться ctx80
до нового состояния или отключения: аварийного таймера, выдаваемого за синхронизацию,
нет. На машине требуется полный reboot и проверка View в обе стороны, route end,
быстрого нового маршрута и отключения CarPlay с возвратом штатной карты.
