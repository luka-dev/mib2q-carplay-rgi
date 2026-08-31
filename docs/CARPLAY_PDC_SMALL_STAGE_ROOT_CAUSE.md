# CarPlay сворачивается при правом PDC popup: причина и природа поведения

Дата исследования: 2026-08-31  
Платформа: Audi MHI2Q, MU1316  
Исходники для анализа: `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-combined-vf`

Статус: исправление реализовано в `java_patch/` и собрано в `build/carplay_hook.jar`.

## Краткий вывод

Правый PDC/OPS блок действительно является HMI partial popup, но исчезновение CarPlay вызывает не отрисовка popup сама по себе. Одновременно парковочная политика MMI/Kombi переводит центральный экран в `SMALL_STAGE`. Штатный экран TerminalMode/CarPlay не умеет продолжать проекцию в этом режиме: он настроен как `SMALL_STAGE_OMISSION`, а приложение TerminalMode при получении малого размера вызывает `hmiDeactivated()`.

Итоговая цепочка:

```text
PDC/OPS activation
  -> HMI partial popup 2100008 справа
  -> MMI/Kombi viewSize = SMALL (1)
  -> MMICombiStageListener.updateKombiStage(false)
  -> ExternalEventsListener.hmiDeactivated()
  -> CarPlay projection скрывается/сворачивается
```

Это архитектурная политика штатного TerminalMode, а не ограничение всех HMI partial popup. Громкость и массаж используют partial popup без parking-stage перехода, поэтому остаются поверх активного CarPlay.

## Что представляет собой правый PDC

Чистый OPS использует partial popup:

```java
return new ParkingPopupIdentifier(1, 2100008);
```

Здесь тип `1` означает partial popup. Его показ выполняется обычным HMI-вызовом:

```java
hmiService.showPartialPopup(0, 2100008);
```

Источники:

- `de/audi/app/earlyfunc/evo/parking/ops/ParkingSystemOPSComponentEvo.java:92`
- `de/audi/app/earlyfunc/core/parking/ParkingPartialPopupHandler.java:52`

Геометрия и параметры popup `2100008`:

```text
bounds       = (720, 0, 304, 480)
HMI layer    = 3
popup slot   = 4
survive      = true
type         = 7 (unbound partial popup)
ZPM priority = 135
ZPM slot     = 1 (PARKING)
```

Источник:

- `de/audi/tghu/earlyapps/hmi/evohigh/EarlyAppsScreenBag1.java:1742-1770`
- `de/audi/mib/config/PopupPrioMapper.java:5`

Таким образом, сам правый OPS действительно рассчитан на последние 304 пикселя экрана и технически может быть отрисован поверх другого displayable через HMI-слой.

## Почему именно CarPlay исчезает

Основной экран проекции — `TERMINALMODEIPODOUT`, screen ID `3200000`. Он настроен следующим образом:

```java
screenwidgetevo.setLargeViewSizePreferred(true);
screenwidgetevo.setSmallStageType(1);
```

Значение `1` — `SMALL_STAGE_OMISSION`. То есть при переходе в малую сцену этот экран не масштабируется и не обрезается для левой части дисплея, а исключается из отображения.

Источники:

- `de/audi/tghu/terminalmode/hmi/evohigh/TerminalModeScreenBag1.java:136-157`
- `de/esolutions/hmi/widgets/audi/base/AbstractScreenWidget.java:72-81`

TerminalMode отдельно следит за общим `IViewSizeManager`:

```java
public void viewSizeChanged(int viewSize) {
    if (viewSize == 1) {
        eventBus.updateKombiStage(false);
    } else if (viewSize == 2) {
        eventBus.updateKombiStage(true);
    }
}
```

Источник:

- `de/audi/app/terminalmode/evo/MMICombiStageListener.java:89-100`

После `updateKombiStage(false)` штатное приложение делает следующее:

```java
public void updateKombiStage(boolean bigStage) {
    if (bigStage && screenActive) {
        hmiActivated();
    } else {
        hmiDeactivated();
    }
    this.bigStage = bigStage;
}
```

Источник:

- `de/audi/app/terminalmode/ExternalEventsListener.java:247-255`

Следовательно, `SMALL_STAGE` для TerminalMode семантически равен потере видимого HMI. Команда `HMIDeactivated` останавливает видимую проекцию CarPlay, поэтому пользователь видит её сворачивание вместо сохранения CarPlay слева от PDC.

Downstream-действие `HMIDeactivated` подтверждает точную природу переключения: команда передаёт ресурс `Resource.SCREEN` владельцу `ResourceOwner.MAINUNIT`, после чего запускает переход состояния TerminalMode.

```java
tmstate.setOwnerForResource(Resource.SCREEN, ResourceOwner.MAINUNIT);
stateHandler.changeState(tmstate, IRequestor.MAINUNIT, -1L);
```

Источник:

- `de/audi/app/terminalmode/commands/HMIDeactivated.java:21-31`

То есть CarPlay не просто перекрывается непрозрачным PDC-виджетом: TerminalMode действительно отдаёт экранный ресурс головному устройству.

При возврате `BIG_STAGE` приложение снова вызывает `hmiActivated()`, если TerminalMode screen всё ещё считается активным.

## Где возникает команда SMALL_STAGE

В исследованном Java-коде чистый OPS не вызывает `requestViewSizeChange(1)` и не переключает display context. Его Java-ветка заканчивается на `showPartialPopup(0, 2100008)`.

Реализация сервиса `IMMICombiViewSizeSync`, который фактически обменивается размером сцены с MMI/Kombi, отсутствует в декомпилированном Java-дереве. Java видит уже готовое изменение через `IViewSizeManager.viewSizeChanged(1)`.

Поэтому подтверждённая граница выглядит так:

```text
parking policy / MMICombi native arbitration
  -> viewSize SMALL
  -> Java MMICombiStageListener
```

PDC при этом явно помечен как `ZPM_SLOT_PARKING=1`, в отличие от громкости и массажа. Однако прямой вызов «ZPM slot 1 -> SMALL_STAGE» в Java отсутствует, поэтому точный внутренний producer команды находится ниже Java-слоя. Не следует приписывать этот вызов `ParkingPartialPopupHandler` или `setSmallStageType(3)` без анализа native MMICombi реализации.

## Почему `setSmallStageType(3)` у PDC не является причиной

У PDC popup действительно присутствует:

```java
partialpopupcontroller.setSmallStageType(3);
```

Но такой же параметр имеют popup громкости и массажа. Более того, setter partial-popup контроллера в этой сборке пуст:

```java
public void setSmallStageType(int value) {
}
```

Источник:

- `de/esolutions/hmi/widgets/audi/evo/widgets/AbstractPartialPopupController.java:1416`

Это не команда на изменение размера текущего приложения. Реальное важное значение находится на основном CarPlay screen: `setSmallStageType(1)`.

## Почему громкость и массаж работают нормально

Громкость, массаж и PDC используют один базовый механизм `PartialPopupController`, поэтому сам класс partial popup не может объяснять разницу.

Сравнение:

| Popup | ID | HMI layer/slot | ZPM slot | Результат на CarPlay |
|---|---:|---|---:|---|
| PDC/OPS справа | 2100008 | layer 3, slot 4 | 1 (`PARKING`) | сопровождается `SMALL_STAGE`, CarPlay деактивируется |
| Громкость | 52 | layer 1 | 5 | обычный overlay |
| Массаж справа | 2100016 | layer 3, slot 2 | 5 | обычный overlay |
| Массаж слева | 2100017 | layer 3, slot 1 | 5 | обычный overlay |

Источники:

- `de/audi/tghu/system/hmi/evohigh/SystemScreenBag2.java:4183-4211`
- `de/audi/tghu/earlyapps/hmi/evohigh/EarlyAppsScreenBag1.java:2456-2565`

Громкость и массаж не приводят `MMICombiStageListener` в состояние `SMALL_STAGE`, поэтому `ExternalEventsListener.hmiDeactivated()` не вызывается и проекционный displayable остаётся видимым.

## Ложный след: parking callbacks 1004/1005

В TerminalMode действительно существуют методы:

```java
parkingActivatedTerminalMode()   -> action proxy method 1004
parkingDeactivatedTerminalMode() -> action proxy method 1005
```

Но в MU1316:

- в приложении TerminalMode нет слушателей `1004/1005`;
- штатный `TerminalModeSMM` не вызывает эти методы;
- реальное сворачивание подтверждается цепочкой `viewSizeChanged(1) -> updateKombiStage(false) -> hmiDeactivated()`.

Следовательно, перехват или отключение action-proxy событий `1004/1005` этот эффект не исправит.

Источники:

- `de/audi/app/terminalmode/evo/TerminalModeActionProxyImpl.java:62-69`
- `de/audi/app/terminalmode/actionproxy/IActionProxyIDsCore.java:7-8`

## Полноэкранная парковочная версия

Полноэкранный VPS/RVC — отдельный путь. Он активирует display-manager component для rear-view camera:

```java
displayManagerService.activateComponentRVC(17);
```

При деактивации выполняется `deactivateComponentRVC(17)`.

Источник:

- `de/audi/app/earlyfunc/core/parking/vps/AbstractParkingSystemVPSComponent.java:137-153`

Это уже настоящий takeover видеоконтекста/displayable, а не только HMI partial popup. Его нельзя считать аналогом правого OPS `2100008`.

## Природа проблемы

Поведение выглядит как пользовательский баг, но в коде оно является преднамеренным OEM fallback:

1. Parking policy требует малую сцену, чтобы освободить правую треть для OPS.
2. Штатные HMI-приложения умеют перестраивать содержимое под small stage.
3. Проекционный CarPlay screen объявлен как `OMISSION`, а не как экран, поддерживающий crop/resize.
4. TerminalMode интерпретирует small stage как HMI deactivation и скрывает проекцию целиком.

Поэтому корень проблемы — несовместимость двух политик: PDC ожидает совместимое с small stage фоновое приложение, а TerminalMode специально не разрешает CarPlay оставаться видимым в small stage.

## Реализованное исправление CarPlay

Исправление должно находиться на границе `SMALL_STAGE -> hmiDeactivated`, а не в отрисовке PDC.

Реализована parking-specific политика:

```text
если viewSize == SMALL
и активен CarPlay screen
и показан OPS partial popup 2100008
то оставить Resource.SCREEN у TerminalMode
и отключить только CarPlay key handler на время парковочного popup
иначе оставить штатный hmiDeactivated()
```

Патч состоит из трёх точек:

1. `java_patch/com/luka/carplay/pdc/PdcSmallStageGuard.java` хранит только состояние popup `2100008` на terminal `0`.
2. Replacement `java_patch/de/audi/app/earlyfunc/core/parking/ParkingPartialPopupHandler.java` поднимает guard **до** `showPartialPopup()`. Это закрывает race, при котором `SMALL_STAGE` может прийти раньше асинхронного `partialPopupVisible()`. После `partialPopupHidden`, `partialPopupRemoved`, unregister или deinit guard сбрасывается. Неподтверждённый show request автоматически протухает через 3 секунды.
3. Replacement `java_patch/de/audi/app/terminalmode/ExternalEventsListener.java` перехватывает только `updateKombiStage(false)` при одновременном выполнении четырёх условий: экран TerminalMode активен, активна именно CarPlay-сессия, стадия стала малой и guard `2100008` поднят. В этой ветке команда `HMIDeactivated` не создаётся, поэтому не выполняется `Resource.SCREEN -> MAINUNIT`.

Клавишный handler TerminalMode на время OPS отключается штатным `deactivate()`, чтобы ввод не уходил в CarPlay под парковочным интерфейсом. На `BIG_STAGE` существующая stock-ветка `hmiActivated()` возвращает ввод и подтверждает владельца `DEVICE`.

Почему патч узкий и не ломает остальные takeover:

- `AP_METHOD_HMI_DEACTIVATED` (`1002`) не перехватывается и всегда идёт по stock-пути;
- любой `SMALL_STAGE` без popup `2100008` по-прежнему вызывает `hmiDeactivated()`;
- Android Auto и остальные TerminalMode-типы не затрагиваются, потому что требуется `CarPlayApp.isActive()`;
- полноэкранный VPS/RVC по-прежнему активирует component `17`; его HMI/action-proxy деактивация не маскируется;
- громкость, массаж и другие partial popup не поднимают guard.

Таким образом, MMI всё ещё переходит в малую сцену и может использовать штатную геометрию/crop, но TerminalMode больше не отдаёт экранный ресурс только из-за правого OPS.

## Проект: CarPlay + partial 360/PDC

Штатный fullscreen VPS/OPS показывает изображение камер не внутри Java-виджета OPS, а отдельным displayable `17`. Полноэкранный parking context имеет композицию:

```java
this.dc[3] = new DisplayContext(3, new int[]{16, 17});
```

Здесь `16` — HMI/OPS поверх видео, а `17` — video surface камеры. Обычный CarPlay использует context `78={16,59}`, где `59` — video surface проекции. Поэтому для сохранения CarPlay слева и добавления 360°-камеры под правым partial popup требуется отдельный A5 context:

```java
// CarPlay + partial 360/PDC
this.dc[91] = new DisplayContext(91, new int[]{16, 17, 59});
```

Порядок элементов является z-order от переднего слоя к заднему:

```text
16  HMI и partial OPS 2100008       сверху
17  360°/RVC camera video           посередине
59  CarPlay video                   снизу
```

Использовать отдельный context `91` безопаснее, чем постоянно менять stock context `78` на `{16,17,59}`. В обычном CarPlay context `78` не должен содержать неактивный camera displayable: иначе возможны чёрный прямоугольник, последний замороженный кадр, постоянное резервирование второй video plane или конфликт аппаратных overlay-плоскостей.

Перед включением context `91` displayable `17` необходимо обрезать до геометрии правого popup:

```text
destination = (720, 0, 304, 480)
```

Для этого используется `IDisplayManager.setCropping()`. Source width/height нельзя жёстко считать равными `1024x480`: их нужно получить через `getExtends(17)` или подтвердить по DSI-логу на автомобиле.

Сам context только компонует поверхности и не запускает поток камер. Кадры появляются после штатной активации:

```java
displayManagerService.activateComponentRVC(17);
```

Однако VPS activation вызывает `notifyParkingSystemActive()`, после чего parking controller рассылает interapp message `108`. `HighPriorityResourceTracker` интерпретирует его как fullscreen RVC, выставляет `SMARTPHONE_SCREEN_BLOCKED_CHOICE=3` (`CARPLAY_BY_RVC`) и `MURVCActive=true`. Поэтому без дополнительного partial-mode guard CarPlay будет заблокирован независимо от исправления `SMALL_STAGE`.

Безопасная последовательность включения:

```text
1. Поднять отдельный guard PARTIAL_360_PDC.
2. Запросить/активировать VPS camera source и component 17.
3. Для guard-режима не применять fullscreen CarPlay resource takeover от message 108.
4. Получить source extents и выставить crop displayable 17 -> (720,0,304,480).
5. Переключить display context 78 -> 91.
6. Показать штатный partial OPS popup 2100008 на HMI layer 16.
```

Последовательность выключения должна быть обратной и не оставлять camera surface в CarPlay context:

```text
1. Скрыть OPS popup 2100008.
2. Вернуть display context 91 -> 78.
3. Деактивировать component 17 и восстановить штатный cropping.
4. Снять PARTIAL_360_PDC guard и очистить partial-only RVC state.
```

Подавлять message `108` глобально нельзя. При настоящей задней передаче/fullscreen RVC stock context `3={16,17}`, блокировка CarPlay и полный takeover должны сохраняться как штатная safety-функция. Исключение допустимо только при одновременном наличии CarPlay, popup `2100008` и отдельного `PARTIAL_360_PDC` guard.

Перед финальным включением функции требуется проверка на автомобиле, что MIB2 способен одновременно выделить video planes `17` и `59`. В штатной таблице MU1316 контекста с двумя такими video surfaces нет. Этот раздел фиксирует проект решения; context `91` и запуск partial camera пока не реализованы в `java_patch/`.

## Следствие для RetroArch context 90

Контекст RetroArch `90={16,43}` теперь также добавлен в replacement `DisplayManagerMIB2High` для поддерживаемой A5-конфигурации:

- HMI displayable `16` сверху;
- video displayable `43` снизу.

Поэтому чистый OPS `2100008` может рисоваться на HMI 16 поверх игры. В отличие от TerminalMode, `RaScreen` не регистрирует `MMICombiStageListener` и не превращает `SMALL_STAGE` в `hmiDeactivated()`. Чистый OPS также не активирует RVC component и не переключает display context в своей Java-ветке.

Ожидаемое поведение для RetroArch:

```text
pure OPS/PDC 2100008 -> правая панель поверх игры, без остановки RA
VPS/RVC fullscreen   -> штатный полный takeover экрана возможен
```

Это различие следует сохранить: копировать TerminalMode small-stage deactivation в RA не нужно.

Таблица A5 увеличена до 91 элемента и содержит точную запись:

```java
this.dc[90] = new DisplayContext(90, new int[]{16, 43});
```

Для `KOMBI_TYPE_G24` ID `90` уже занят stock-генератором контекстов как `11 + 79`; патч намеренно не перезаписывает этот stock KDK-контекст.

## Что проверять в живом логе

После установки исправленного JAR ожидается такая последовательность сообщений:

```text
ParkingPartialPopupHandler#showPopup popupID=2100008
MMICombiStageListener.viewSizeChanged SMALL_STAGE
ExternalEventsListener.updateKombiStage SMALL_STAGE pure OPS: keep CarPlay SCREEN owner
```

Между `SMALL_STAGE` и закрытием popup **не должно** быть `HMIDeactivated.execute`.

После исчезновения PDC ожидается:

```text
MMICombiStageListener.viewSizeChanged BIG_STAGE
ExternalEventsListener ... hmiActivated / HMIActivated
```

Для полноэкранной камеры, HOME или другого настоящего takeover по-прежнему ожидается `AP_METHOD_HMI_DEACTIVATED` и `HMIDeactivated.execute`.

Сборочная проверка выполнена: `./scripts/build_java.sh`, `unzip -t`, Java class major `48`, а `javap -protected -s` не выявил пропавших stock public/protected ABI-членов у трёх затронутых replacement-классов.
