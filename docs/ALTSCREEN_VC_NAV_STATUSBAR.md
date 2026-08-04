# altScreen: штатная NavStatus-полоса приборки

Полоса поверх altScreen — не часть iOS-видео. Это `SV_LVDS_FPK_NavMap.NavStatus`
в `hmi_app.elf` приборки. Её данные приходят отдельными BAP Navigation
properties, поэтому существующий route-guidance gate их не блокировал.

## Подтверждённые поля и значения очистки

| Элемент | BAP | Отправляемое значение при CarPlay takeover | Проверка в VC |
|---|---:|---|---|
| Compass | FctID 16 | angle `361`, symbolic `16` | `VC_RecomputeStatusbarCompass` `0x176474–0x176554`: angle > 360 и symbolic > 15 дают `NavFPK_NavStatusbar_Compass_visible=0` (`DP 0x400004C`) |
| Altitude | FctID 47 | altitude `0x7FFF`, unit `2` | `VC_RecomputeAltitude` `0x177B1C–0x177C78`: altitude `0x7FFF` или unit > 1 очищают строку и выставляют unavailable-unit |
| Scale | FctID 45 | autoZoom `15`, scale `0xFFFF`, unit `0xFF` | `VC_RecomputeMapScale` `0x177D10`; `VC_IsMapScaleUnitRenderable` `0x610768–0x61078C`; `VC_RecomputeMapScaleVisibility` `0x6121B8–0x6121D4` выставляет `Nav_Map_Scale_visible=0` |
| Center text | FctID 19/20 | CurrentPosition `""`; TurnTo/SignPost `"",""` | `VC_UpdateNavStatusBarText` `0x178318–0x178688` очищает `Nav_Map_Streetname` и availability DP |

Для compass нельзя вызывать штатный `AppConnectorNavi` с symbolic `0xFF`:
он преобразует это значение обратно в symbolic по angle
(`AppConnectorNavi.java:144–149`). Пара `361/16` проходит через wrapper как
`angle=0xFFFF, symbolic=16`, что попадает именно в invalid-ветку VC.

## Реализация

`GatedCombiService.setTakeover(true)` теперь один раз отправляет все invalid
statuses, после чего блокирует стоковые обновления compass/altitude/scale/text,
чтобы навигация MMI не нарисовала полосу снова
(`java_patch_new_replace/com/luka/carplay/rgd/GatedCombiService.java:44–61,100–133`).
`BAPBridge` использует этот transition API на connect/start/disconnect
(`BAPBridge.java:541–565,617–620,736–740`).

`MapVisibility`/FctID 44 намеренно не меняется. В VC обработчик
MapViewAndOrientation использует LVDS-visible bit для открытия KDK video-hole;
его очистка спрятала бы вместе с полосой и сам altScreen.

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
