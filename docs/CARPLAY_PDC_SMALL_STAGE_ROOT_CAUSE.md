# CarPlay сворачивается при правом PDC popup: причина и природа поведения

Дата исследования: 2026-08-31  
Платформа: Audi MHI2Q, MU1316  
Исходники для анализа: `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-combined-vf`

Текущий статус, 2026-09-14: **исправлен пропущенный путь через `PARKING_SYSTEM_ACTIVATED (108)`, собран и проверен локально; на HU пока не выгружен**. Пользователь подтвердил симптом: при появлении бокового PDC CarPlay уходит в Main Wizard. Этот же сбой воспроизведён host-тестом со штатным MU1316 tracker; с новым tracker тест проходит. SSH к `100.127.17.86` завершился по таймауту, поэтому подтверждения на автомобиле ещё нет.

Исторический статус на 2026-09-02: было записано подтверждение на автомобиле. Первая реализация на машине не сработала — при появлении правого OPS popup CarPlay всё равно закрывался. Вторая реализация закрывает дополнительные пути `SMALL_STAGE_OMISSION` и AP `1002`; после выкладки jar и перезагрузки сообщалось о сохранении проекции под правым OPS popup. Это не подтверждает все варианты активации парковочной системы.

Подтверждение — наблюдение на автомобиле, а не строка лога: маркер `/mnt/app/carplay_verbose` в тот момент был снят (его убирали при отладке звука), поэтому `Log.i` из `OPS guard` не писался. Маркер возвращён.

## Уточнение 2026-09-14: сообщение 108 приходит и от обычного OPS

Прежнее разделение «pure OPS — только small stage; VPS — сообщение 108» неверно. В актуальном `MU1316-vf` и байткоде `MU1316-final.jar` есть независимая цепочка:

```text
AbstractParkingSystemOPSComponent.setActive(true, content)
  -> controller.notifyParkingSystemActive(this, true)
     -> updateInterappClients(true)
        -> MsgDistributor.sendMessage(108)
  -> controller.addPopupRequest(...)
     -> ParkingPartialPopupHandler.showPopup(2100008)
        -> PdcSmallStageGuard.showRequested(...)   [до этого исправления guard включался только здесь]

HighPriorityResourceTracker.processMsg(108)
  -> SWITCH_TO_MAIN_WIZARD_CHOICE переключается для CarPlay
  -> SMARTPHONE_SCREEN_BLOCKED_CHOICE = 3
  -> rvcActive = true
     -> acquireHighPriorityResources(Resource.SCREEN)
        -> TMState.restrictAccessTo(SCREEN), requester MAINUNIT
  -> MURVCActive = true
```

В предыдущем варианте отправка `108` предшествовала включению guard. `MessageDistributorImpl.sendMessage` вызывает `processMsg` слушателей синхронно. Но даже доставка после включения guard не была защищена: штатный `HighPriorityResourceTracker` его не проверяет вообще. Кандидат `carplay-readiness-fix-20260914` содержал все три прежних PDC класса, однако не заменял этот stock tracker. До текущего исправления PDC-код не менялся после `cbc81bb` от 2026-09-01: последующие исправления renderer/lifecycle не удалили его.

Источники в `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-vf/`:

- `de/audi/app/earlyfunc/core/parking/ops/AbstractParkingSystemOPSComponent.java`, `setActive`.
- `de/audi/app/earlyfunc/core/parking/AbstractParkingSystemControllerComponent.java`, `notifyParkingSystemActive`, `updateInterappClients`.
- `de/audi/atip/msg/MsgDistributor.java`: 107/108 — деактивация/активация **парковочной системы**, не отдельные события камеры.
- `de/audi/app/terminalmode/interapp/HighPriorityResourceTracker.java`, `processMsg`, подписка на `rvcActive`, `acquireHighPriorityResources`.

`javap -c -p` подтвердил порядок вызовов `setActive` и действия `processMsg` непосредственно в `MU1316-final.jar`. Это подтверждённый пробел реализации и воспроизведённый механизм совпадающего симптома; живой лог последнего эпизода пользователя пока не получен.

Нельзя просто игнорировать все `108` или проверять только видимость `2100008`: этот popup может сосуществовать с VPS. Также нельзя полагаться только на `getCurrentDisplayContent()`: stock controller обновляет его после `activateParkingSystem(newContent)` в некоторых ветках.

## Третья реализация: ранняя классификация парковочного экрана

`ParkingSystemControllerComponentEvo` — полный stock replacement на 92 исходных строки плюс два узких override. `deactivateCurrentlyVisibleParkingSystems(newContent, nextSystems)` вызывается базовым `activateParkingSystem` **до** деактивации старых и активации новых компонентов. Этот override передаёт guard новый `DisplayContent` и уже вычисленный штатным контроллером список компонентов. Исключение разрешено только если список содержит ровно один OPS (`system ID 2`), его popup — partial `2100008`, CarPlay активен и текущий screen — `3200000`.

Любой VPS, PLA, ARA, неизвестный компонент, пустой/некорректный список оставляет штатный takeover. Дополнительный override `notifyParkingSystemActive` отзывает исключение перед самостоятельным уведомлением любого не-OPS компонента: это закрывает активацию камеры/PLA вне обычного обновления content. `deinit` контроллера также отзывает исключение.

`HighPriorityResourceTracker` сохранён как полный MU1316 replacement с Java 1.4 anonymous-классами. В `processMsg(108)` для разрешённого standalone OPS он не переключает Main Wizard и не захватывает SCREEN. После предыдущего RVC снимается только парковочное ограничение; stock реактивная комбинация eCall/clamp-S продолжает удерживать свои ограничения. Для остальных случаев и явной диагностической команды RVC исполняется штатная ветка. Событие `107` не стирает новый parking intent: оно может прийти от старого компонента в середине переключения.

Прежние small-stage и AP-перехваты используют тот же guard. Теперь он требует, чтобы текущим оставался именно защищённый экземпляр CarPlay screen. Поэтому первый HOME не поглощается даже если парковка вообще не присылала AP `1002`. Запоздалый `popupVisible` не может включить защиту после camera takeover. Начальный callback регистрации `hidden` не трактуется как отмена ещё не показанного popup; настоящий hidden/remove/unregister и 3-секундный таймаут восстанавливают small-stage policy.

Проверки:

- `scripts/test_pdc.sh`: реальный shipping tracker, штатные MU1316 reactive properties, `TMState` и команды. Штатный tracker воспроизводит `pure OPS 108 toggled Main Wizard`, новый проходит тот же сценарий. Проверены ранний/повторный `108`, old `107` после нового intent, OPS→VPS→OPS, поздние popup callbacks, HOME, отключение CarPlay, Android Auto, eCall, clamp-S, diagnostic RVC и таймаут отказавшего показа. Тест также читает байткод stock producer и replacement override, проверяя порядок вызовов до рассылки уведомления.
- `scripts/test_java_transports.sh`: существующие input/transport/lifecycle тесты и новый PDC-тест проходят.
- `scripts/audit_java_stock.sh`: сборка Java 1.4, все 116 классов проверены на linkage против `MU1316-final.jar` и `MU1316-combined.jar` с настоящей J9 class library; ошибок нет.

Кандидат: `build/releases/carplay-pdc-fix-20260914/carplay_hook.jar`, build ID `8ca2c73-pdc-fix`; манифест содержит SHA-256 JAR и всех Java-исходников. Для этого исправления меняется только JAR. Не заменять им старый readiness release без обновления манифеста того release.

Следующий шаг на доступном HU: установить этот JAR, перезагрузить для загрузки replacement-классов и проверить боковой OPS, HOME, полный RVC/VPS и закрытие парковки. С включённым `carplay_verbose` ожидается строка `OPS 108: keep CarPlay SCREEN; no Main Wizard takeover`; для полного takeover этой строки быть не должно. Факт локального прохождения тестов не считать подтверждением на автомобиле.

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

## Первая реализация CarPlay — не прошла проверку на автомобиле

Первая гипотеза состояла в том, что достаточно перехватить границу `SMALL_STAGE -> hmiDeactivated`, не меняя отрисовку PDC.

Была реализована parking-specific политика:

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

Тест на автомобиле опроверг вывод, что одного этого перехвата достаточно. CarPlay всё равно закрылся.

В первой реализации остались как минимум две независимые ветки, способные убрать проекцию:

1. Screen `TERMINALMODEIPODOUT` (`3200000`) по-прежнему создаётся с `setSmallStageType(1)`, то есть `SMALL_STAGE_OMISSION`. Поэтому HMI compositor может исключить сам screen/display controller при переходе в малую сцену, даже если `Resource.SCREEN` не был передан `MAINUNIT` через перехваченную ветку.
2. Action-proxy `AP_METHOD_HMI_DEACTIVATED` (`1002`) в replacement `ExternalEventsListener` намеренно оставлен безусловным и всё равно вызывает `hmiDeactivated()`. Если реальная parking policy на автомобиле посылает `1002` вместе со `SMALL_STAGE`, guard в `updateKombiStage(false)` не может сохранить CarPlay.

Поэтому точная следующая проверка — определить по живому логу, появился ли перед закрытием `AP_METHOD_HMI_DEACTIVATED`, и сработал ли вообще лог `SMALL_STAGE pure OPS: keep CarPlay SCREEN owner`. До этой проверки нельзя снова помечать исправление как готовое.

## Вторая реализация PDC-only — подтверждена на автомобиле 2026-09-02

Вторая реализация не меняет display context `78`, не добавляет camera displayable `17` и не касается RetroArch context `90`.

Перед `showPartialPopup(0, 2100008)` guard получает текущий screen terminal `0`. Только если активна CarPlay-сессия и текущий screen имеет ID `3200000`, его политика временно меняется:

```text
SMALL_STAGE_OMISSION (1) -> SMALL_STAGE_NO_CHANGE (6)
```

Это изменение выполняется на уже созданном `AbstractScreenWidget`; replacement для всего `TerminalModeScreenBag1` не нужен. После `partialPopupHidden`, `partialPopupRemoved`, unregister, deinit или истечения неподтверждённого show-request возвращается исходное значение small-stage policy.

Тот же узкий guard теперь проверяется в обеих ветках `ExternalEventsListener`:

1. `updateKombiStage(false)` не создаёт `HMIDeactivated`;
2. первый `AP_METHOD_HMI_DEACTIVATED` (`1002`), сопровождающий открытие popup, не создаёт `HMIDeactivated` и не сбрасывает `screenActive`.

Подавление AP `1002` одноразовое, потому что событие не содержит параметра source. Это позволяет следующему `1002` от HOME или настоящего fullscreen takeover работать штатно, даже если OPS ещё видим. В обеих parking-ветках key handler CarPlay временно выключается, чтобы ввод не проходил под popup. После возврата `BIG_STAGE` штатный `hmiActivated()` снова включает ввод. За пределами одновременного состояния `CarPlay active + popup 2100008 guarded` обе ветки работают штатно.

На момент второй реализации собранный JAR прошёл Java 1.4 compilation, ZIP integrity, сравнение public/protected ABI двух replacement-классов и host-тесты. Историческое подтверждение от 2026-09-02 не закрывает повторный сбой от 2026-09-14: теперь требуется проверить также раннюю цепочку `108`, последовательность `showPopup -> SMALL_STAGE/AP1002 -> popupVisible` и момент возврата `BIG_STAGE`.

### Почему замена display context сама по себе не исправляет обычный OPS

Для OPS без видеокамеры stock CarPlay context уже содержит нужную композицию:

```java
this.dc[78] = new DisplayContext(78, new int[]{16, 59});
```

`16` — HMI, на котором рисуется partial popup `2100008`; `59` — CarPlay video. Поэтому добавление другого context не отменяет ни `SMALL_STAGE_OMISSION`, ни `hmiDeactivated()`. Исправление OPS-only должно одновременно сохранить screen widget и владельца `Resource.SCREEN`.

Displayable `17` требуется только для отдельной задачи partial 360-camera. Добавление `17` в `78` или отдельный mixed context не является исправлением закрытия CarPlay при обычном OPS.

## Проект: CarPlay + partial 360/PDC

Обновлённая проверка от 2026-09-14: [подготовительный разбор и воспроизводимые тесты](reference/CARPLAY_PARTIAL_360_PREFLIGHT.md). Runtime-реализации и HU-проверки этой функции пока нет.

По решению пользователя от 2026-09-14 задача **отложена**: сохраняем результаты, реализацию и HU-эксперименты начинаем только по отдельной команде.

Предполагаемая композиция — HMI `16`, camera displayable `17`, CarPlay `59`; целевая область камеры справа `(720,0,304,480)`. Сосуществование двух видео на HU ещё нужно подтвердить. Новый context ID **не выбран**.

Первоначальный пример с `dc[91]` был некорректен: A5-массив имеет длину `82`, на G24 индекс `91` занят вариантом stock context `12+79`. Кроме того, callbacks stock `DisplayManager` передают `context % 79`. Просто добавить строку в таблицу недостаточно.

Другие уточнения после host/native проверки:

- `switchVPSView(4)` формирует `view=4, mode=9`, но сохраняет popup/screen. OPS-запрос от этого сам не превращается в VPS. Штатный Evo VPS требует fullscreen popup `2100000` и вызывает `108`; отдельное partial-состояние должно учитывать оба действия.
- Для адресного crop нужен HMI `IDisplayManager.setCropping(17,0,...)`. Одноимённый interapp API игнорирует переданный displayable и использует generic state, которое `activateComponentRVC(17)` не обновляет.
- `getExtends(17)` может вернуть `null`; cached `int[]` заимствованный. Extents не содержат предыдущие source/destination rectangles и не определяют область Bird View внутри кадра.
- Контекст может запускать локальный HU capture сам: native различает context-origin и component-origin. При закрытии и fullscreen takeover нужно учитывать владение, а не безусловно останавливать `17`.
- Штатный OPS overlay/background зависит от типа VPS, view и подключения stock screen `2100001`. Нельзя считать, что запуск камеры автоматически даст подходящий прозрачный фон справа.

Текущий PDC guard остаётся только для обычного OPS. Полноэкранная камера сохраняет штатный takeover. Матрица переходов, результаты тестов, native fingerprints и следующий ограниченный HU-эксперимент приведены в отдельном документе выше.

## Следствие для RetroArch context 90

Контекст RetroArch `90={16,43}` первоначально был добавлен в replacement `DisplayManagerMIB2High`, но затем удалён из CarPlay JAR коммитом `7590e88`. Его владельцем должен быть собственный Java-патч RetroArch, а не этот репозиторий:

- HMI displayable `16` сверху;
- video displayable `43` снизу.

Поэтому чистый OPS `2100008` может рисоваться на HMI 16 поверх игры. В отличие от TerminalMode, `RaScreen` не регистрирует `MMICombiStageListener` и не превращает `SMALL_STAGE` в `hmiDeactivated()`. Чистый OPS также не активирует RVC component и не переключает display context в своей Java-ветке.

Ожидаемое поведение для RetroArch:

```text
pure OPS/PDC 2100008 -> правая панель поверх игры, без остановки RA
VPS/RVC fullscreen   -> штатный полный takeover экрана возможен
```

Это различие следует сохранить: копировать TerminalMode small-stage deactivation в RA не нужно.

Требуемая запись для A5 остаётся такой, но в текущем `mhi2-carplay/java_patch` её нет:

```java
this.dc[90] = new DisplayContext(90, new int[]{16, 43});
```

Для `KOMBI_TYPE_G24` ID `90` уже занят stock-генератором контекстов как `11 + 79`; RetroArch-патч не должен перезаписывать этот stock KDK-контекст.

## Что проверять в живом логе

Для второй реализации ожидается такая последовательность сообщений:

```text
ParkingPartialPopupHandler#showPopup popupID=2100008
OPS guard: screen 3200000 smallStageType 1 -> 6
MMICombiStageListener.viewSizeChanged SMALL_STAGE
ExternalEventsListener.updateKombiStage SMALL_STAGE pure OPS: keep CarPlay SCREEN owner
```

Если parking policy дополнительно посылает AP `1002`, после его обычного входного лога должно появиться:

```text
ExternalEventsListener.actionProxyCallPerformed partial OPS: suppress HMI_DEACTIVATED
```

До закрытия popup не должно появиться `HMIDeactivated.execute`. Если нет лога `smallStageType 1 -> 6`, guard не увидел текущий TerminalMode screen или replacement-класс не был загружен.

После исчезновения PDC ожидается:

```text
MMICombiStageListener.viewSizeChanged BIG_STAGE
ExternalEventsListener ... hmiActivated / HMIActivated
OPS guard: restored screen 3200000 smallStageType 1
```

Для полноэкранной камеры, HOME или другого настоящего takeover по-прежнему ожидается `AP_METHOD_HMI_DEACTIVATED` и `HMIDeactivated.execute`.

Сборочная проверка выполнена: `./scripts/build_java.sh`, `unzip -t`, Java class major `48`, host audio/integration tests и `javap -protected -s` для изменённых stock replacement-классов. SHA-256 собранного `build/carplay_hook.jar`: `37bfcce592f32b5639e476c6b86929f56aa5f3f76d0275056a5a9b68e10c2448`.
