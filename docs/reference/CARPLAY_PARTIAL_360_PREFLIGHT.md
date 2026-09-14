# CarPlay + боковая 360°-камера: подготовительная проверка MU1316

Дата: 2026-09-14. База runtime-кода: `ec16682` (`master`). Это исследование и host-тесты, **функция ещё не реализована и на HU не проверена**. Производственные Java/C/SH файлы и готовый JAR в этой работе не менялись.

**Решение пользователя от 2026-09-14: отложено.** Сохраняем исследование, тесты и проект переходов. Реализацию, эксперименты на HU и выгрузку этой функции не начинаем до отдельной команды пользователя. Описанные ниже следующие шаги — план на будущее.

Цель: сохранить CarPlay слева, показать штатный вид сверху и OPS справа, уступать штатной полноэкранной парковке при takeover. Композиция `{16,17,59}` остаётся гипотезой для HU, номер нового контекста пока не выбран.

## Проверки и воспроизведение

Из корня репозитория:

```sh
bash scripts/test_partial360_preflight.sh
python3 tools/partial360_native_preflight.py
```

Первый скрипт заново компилирует текущую Java в **временный каталог** с target 1.4, исполняет штатные классы из `MU1316-combined.jar` и `MU1316-final.jar`. Подставлены DSI/HMI-интерфейсы, физические виджеты и инициализация native-сервисов. Байткод business-методов, модели OPS и таблица нашего DisplayManager настоящие. Тест не строит симулятор новой partial-camera логики.

Результат: **6 групп контрактов проходят на обоих JAR**:

| Проверка | Что подтверждено |
|---|---|
| Контексты A5/G24 | `3={16,17}`, `78={16,59}`; сочетания `17+59` нет; `91` непригоден; оба stock callback используют `%79` |
| Extents / HMI crop | Холодный cache возвращает `null`; cached array заимствованный; правильный порядок DSI-аргументов; без инициализации crop не отправляется |
| Camera service / interapp crop | RVC start/stop адресуют `17,0,0`, не меняют generic state; generic crop может продолжать адресовать `59` |
| Bird View | UI выбор `4` даёт `view=4, mode=9`, сохраняет popup/screen; повтор и неизвестный view игнорируются; stock VPS требует fullscreen `2100000` |
| OPS модели | Режим/фон зависят от типа системы, доступности TopView, выбранного view и подключения stock VPS screen |
| Screen bag | Правая область `(720,0,304,480)`, фон OPS управляется моделью, fullscreen парковка имеет собственный video-context controller |

Также проходит `PdcResourcePolicyTest` с текущим guard, включая takeover камерой. Контроль на **непатченном** tracker воспроизводит `pure OPS 108 toggled Main Wizard` — тест действительно различает старое и исправленное поведение.

Native-скрипт читает matching MU1316 ELF/config, проверяет fingerprint/архитектуру, известную capture-таблицу и разные RVC/TopView настройки. Сохраняет точечный disassembly и manifest с адресами. Это статические проверки артефактов, не исполнение QNX.

Локальные результаты: `output/partial360-preflight/host-tests.log`, `output/partial360-preflight/native/manifest.json`. `output/` не требуется хранить в git; данные воспроизводятся скриптами. Нужны существующий JDK8/stock JAR из `../../Tools/jxe2jar`, для native — `r2/rabin2` и распакованная прошивка.

## Что нужно исправить в первоначальном проекте

### 1. Нельзя просто добавить context 91

Наш `DisplayManagerMIB2High.defineContexts()` создаёт A5-массив длиной **82**. Индекс `91` вне него. На G24 индекс `91` уже означает stock context `12+79`, состав `{20,16,22,19}`.

Дырки таблиц: A5 `[34,65,70,79]`, G24 `[34,65,70,113,144,149]`. Это **не список свободных ID**: например, `70` отдельно классифицируется как video-only в stock `DisplayManager.isVideoOnlyContext()`, а `79` занят на G24. Возможные сторонние патчи тоже нужно учитывать.

Оба пути stock подтверждения — `DisplayManager.switchContext()` и `processEvent()` — передают слушателю `context % 79`. Простое увеличение массива не решает проблему: `91` становится `12`. Новый ID требует согласованной проверки таблиц, callback-семантики и владельцев контекстов. Не менять общий `%79` вслепую: G24 использует нормализацию для своих вариантов.

### 2. Bird View request не является активацией partial VPS

`AbstractParkingSystemVPSComponent.switchVPSView(4)` создаёт новый `DisplayContent`, меняет только `view=4, mode=9`, копирует прежние `popup/screen` и вызывает `controller.changeDisplayContent()`.

Если исходный popup — OPS (`1`), он остаётся OPS. Такой вызов сам по себе не добавляет VPS-компонент к активным parking systems. В stock controller involved components выбираются по popup/support, затем включаются. Доступность Bird View берётся из view options (`VPSSupportedViews.isBirdview()`), её нельзя выводить из одного наличия камеры.

Когда штатный VPS действительно активируется, порядок такой:

```text
update view/mode
activateComponentRVC(17)
addPopupRequest(FULLSCREEN, 2100000)
startupMgr.setRVCActive(true)
notifyParkingSystemActive -> interapp 108
```

Evo VPS возвращает fullscreen `2100000` для всех проверенных VPS popup IDs (`2,3,8,12,13,14,16,18`). Полноэкранный screen bag также управляет `PARKING_VPS_VIDEO_CONTEXT_CHOICE`. Поэтому подавить только `108` недостаточно: остаются popup, context controller и startup state.

Текущий PDC guard **намеренно** допускает только одиночный OPS component `2`; VPS component `4` снимает исключение. Новая функция должна иметь отдельное явно ограниченное состояние. Нельзя расширять существующий guard до «любая парковка при CarPlay».

### 3. Два несовместимых crop API

| API | Реальное действие |
|---|---|
| `HMIService.getDisplayManager().setCropping(displayable, terminal, srcX,srcY,srcW,srcH,dstX,dstY,dstW,dstH)` | Прямой адресат; перед DSI терминал преобразуется, первые два аргумента меняются местами |
| `IDisplayManagerService.setCropping(...)` | Первый аргумент не читается; queued `SetCroppingCommand` берёт displayable из generic `DisplayManagerState` |

Для HU `HMI setCropping(17,0,...)` превращается в `DSI setCropping(0,17,...)`. Для external cluster terminal `1` DSI terminal будет `4`.

`activateComponentRVC(17)` напрямую вызывает `startComponent(17,0,0)`, но **не обновляет generic state**. Host-тест показывает конкретную ошибку: при generic state `59` последующий interapp crop адресует CarPlay `59`, хотя RVC только что запускался через `17`. Для будущей функции нужен явный HMI crop.

### 4. Source extents недостаточно для картинки и восстановления

`getExtends(17)` при пустом cache вызывает DSI request и сразу возвращает `null`. При заполненном cache возвращается тот же mutable `int[]`; `DSIDisplayListener.getExtents()` обновляет его через HMI event. Сохранённое значение надо копировать. Сам метод не проверяет доступность DSI перед холодным запросом.

Extents — размеры поверхности, **не текущие source/destination rectangles**. HMI `setCropping()` запоминает только destination position. `getPosition()` также возвращает заимствованный массив. Нельзя объявить восстановление корректным, сохранив лишь `[width,height]`.

Целевая область подтверждена screen bag: **`x=720, y=0, w=304, h=480`**. Исходная область Bird View пока неизвестна: нужно определить, приходит отдельный вид сверху или составной кадр с несколькими зонами. Растягивание полного `1024×480` в `304×480` сильно сожмёт изображение по горизонтали; это не утверждённая геометрия.

Matching firmware production config содержит разные профили:

| Профиль | Source | Target |
|---|---|---|
| `rvc` | `(0,48,702,384)` | `(0,56,720,384)` |
| `zfas` / TopView | `(0,0,1024,480)` | `(0,0,1024,480)` |

Они одинаковы в IFS и system production copies. Presets не задаёт эти профили. Это значения прошивки, **не подтверждение активного режима конкретного HU**. Source rectangle и восстановление надо сверить со штатным состоянием на машине.

### 5. Фон OPS не переключается автоматически в нужный вид

`OPSViewModeHandler` использует `PARKING_VPS_SYSTEM_TYPE_CHOICE=2100180`: `0` при отсутствии VPS, `1` для rear-only конфигурации, `2` при наличии front/top/side view. Доступность TopView в choice-модели имеет значение `0` для available.

Режимы OPS: `0=OVER_TOPVIEW_FULL`, `1=OVER_TOPVIEW_REDUCED`, `2=STANDALONE`. Фоны: `1=FULL`, `2=REDUCED`, `3=STANDALONE`. При системном типе `0` OPS остаётся standalone даже после `updateVPSView(4)`. Для типа `2`/view `4` stock выбирает full overlay/background `1`; reduced применяется при другом view и зависит от настройки.

Для reduced background `2` штатный handler ждёт именно `notifyScreenConnected(2100001)` (либо ARA), а не запуск displayable `17`. У нового partial режима этого screen нет. Нельзя подделывать его connected-событие: нужно отдельно определить presentation mode для боковой камеры. В popup `2100008` есть background icon controllers — их alpha/размещение потребуется визуально проверить, одного имени `OVER_TOPVIEW` мало.

## Native: камера запускается не только из Java

Файл: `Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app/eso/lib/libdm_modMain.so`.

SHA-256: `bd4c042457d4b3c0b9b5f3bcf8ef1e64a30842435e7bc70b477237acaa5ef6ec`; размер `661228`; QNX ARM32 LE EABI5 shared ELF, static base `0`. Ниже VA и file offset совпадают. Метка `linux` в generic ELF-выводе rabin2 не меняет принадлежность QNX-прошивке.

| Адрес | Проверенный участок | Следствие |
|---|---|---|
| `0x358ac` | `CContextManager::preContextSwitchHook`, вызов capture в `0x35c6c` с origin `0` | Включение контекста с capture displayable может само запускать HU capture |
| `0x60480` | `CDSIDisplaymanagement::evtStartComponent`, вызов capture в `0x607d4` с origin `1` | Явный component start — другой путь владения |
| `0x5a4ac` | `CCaptureInterface::stopCapturing`, сравнение origin в `0x5a4d4`–`0x5a4dc` | Stop зависит от владельца; порядок context/component операций важен |
| `0x59900` | `CCaptureInterface::isDisplayableReady` | Готовность capture проверяется отдельно; успешный request не равен первому кадру |
| `0x5dab0` | `CDSIDisplaymanagement::evtSetCropping` | Последовательно меняет position, size, crop и вызывает capture/decoder-specific update |
| `0x59c70` | `CCaptureInterface::evtSetRVCEnabled` | При соответствующей native ветке есть принудительный switch в context `-2` и возврат; Java — не единственный источник смены контекста |
| `0xa0aa8` | `m_captureConfigs`, 7 записей по 56 байт | IDs `[17,27,27,47,48,49,-125]`; `59` не входит в эту capture-таблицу |

Это уточняет раннее предположение «контекст только компонует и никогда не запускает камеру»: контекст действительно может запускать **локальный HU capture**. Выбор Bird View в parking ECU по-прежнему отдельная операция. Если сначала вызвать component start, смена контекста не обязательно станет владельцем capture: `startCapturing()` при уже активном том же профиле возвращает успех без переназначения origin. Нельзя считать переключение обратно на `78` гарантированным `stopComponent`.

`17` и `59` не являются двумя профилями одной найденной capture-таблицы. Это аргумент в пользу дальнейшего эксперимента, **но не доказательство**, что compositor/driver одновременно выведет оба видео без задержек и сброса DIO.

## Контракт будущего координатора — пока проект

Состояния: `OFF`, `REQUESTING_CAMERA`, `PARTIAL_VISIBLE`, `RESTORING`, `STOCK_TAKEOVER`. Исполнение на штатной очереди HMI, без нового постоянного poll-потока. Каждое включение имеет generation/session; поздние callbacks не должны возвращать уже закрытую камеру.

Вход возможен только при активном CarPlay screen, разрешённом боковом OPS, подтверждённой поддержке Bird View и отсутствии полного parking takeover. Нужно отдельно хранить: camera source request/ack, владение component start, geometry snapshot/profile, свой display context, popup instance и partial resource exemption. Флаг «камера включена» не заменяет эти состояния.

| Событие | Требуемое поведение |
|---|---|
| OPS появился в CarPlay | Сначала подготовить source/геометрию и ограниченный guard; показать partial camera только после нужных подтверждений |
| Camera unavailable / отказ / timeout | Оставить обычный OPS + CarPlay, убрать только собственные запросы и геометрию |
| OPS закрылся | Удалить partial state; вернуть `78` только если HU всё ещё принадлежит этому CarPlay-сеансу |
| HOME / другой app / CarPlay disconnect | Отменить pending callbacks; не переключать context обратно в CarPlay поверх нового экрана |
| Задняя передача / fullscreen camera / PLA/ARA takeover | Снять partial exemption до resource notification; отдать штатной системе camera ownership и геометрию; не отправлять запоздалый stop её камеры |
| Быстрый full → partial → full | Каждый callback сверять с актуальным владельцем/поколением; старая cleanup не трогает новый fullscreen |
| Повтор request/ack/visible | Идемпотентно, без повторного source start и без перезаписи snapshot partial-геометрией |
| DSI service пропал/вернулся | Сбросить уверенность в context/crop/кадре, восстановиться из актуального parking/CarPlay состояния |

Для обычного закрытия нужно убрать собственную композицию, подтвердить уход из неё, затем остановить принадлежащий ей capture и восстановить геометрию. Для takeover нельзя механически выполнять эту же обратную последовательность: source уже может требоваться штатному VPS.

Не использовать `controller.getCurrentDisplayContent()` как единственный источник нового назначения во время перехода: оно может ещё описывать предыдущий popup. Классификация должна следовать incoming content/involved systems до уведомлений `108/107`, как в текущем PDC исправлении.

## Что остаётся проверить на HU перед реализацией по умолчанию

1. Снять штатную геометрию displayable `17`, actual source/crop и содержание Bird View при доступных `screen/view/mode`; определить нужный ROI без изменения пропорций автомобиля.
2. Проверить ограниченную композицию `16+17+59` с выделенным, проверенным ID: camera + живой CarPlay, z-order, отсутствие чёрного/stale кадра, задержка ввода и отсутствие перезапуска decoder.
3. Подтвердить readiness/callback-сигнал для камеры и порядок native ownership. Не подменять это произвольной задержкой после start.
4. Проверить штатные OPS графики/фон над видео; они должны совпадать с масштабом и положением автомобиля в Bird View.
5. Прогнать таблицу переходов, особенно reverse/fullscreen во время ожидания кадра и позднюю cleanup после takeover. Сверить восстановление source и destination прямоугольников со штатными значениями.

После отдельной команды пользователя продолжение начнётся с **ограниченного HU-эксперимента**, не с готового релиза. Пока сохраняем обычный OPS; глобальную обработку `108` и stock RVC takeover не отключаем.

## Основные источники

Обновлённый decompile: `Tools/jxe2jar/out/MU1316-vf/`; исполнение/проверка контрактов по matching `MU1316-combined.jar` и `MU1316-final.jar`.

- `de/audi/app/earlyfunc/core/parking/vps/AbstractParkingSystemVPSComponent.java`: `switchVPSView`, `setActive`, `updateParkingSystemViewOptions`.
- `de/audi/app/earlyfunc/evo/parking/ParkingSystemVPSComponentEvo.java`: `getHMIPopupID`.
- `de/audi/app/earlyfunc/core/parking/AbstractParkingSystemControllerComponent.java`: `requestParkingPopup`, `activateParkingSystem`, `getInvolvedParkingSystems`, `changeDisplayContent`.
- `de/audi/app/earlyfunc/core/parking/ops/OPSViewModeHandler.java`: `updateOPSViewMode`, screen callbacks.
- `de/audi/tghu/earlyapps/hmi/evohigh/EarlyAppsScreenBag1.java`: `cARPOPUPPARKINGOPS`, `cARPOPUPPARKINGAPSOPSVPSRVC`.
- `de/audi/tghu/fwhmi/DisplayManager.java`, `DSIDisplayListener.java`: extents/crop/context callbacks.
- `de/audi/atip/displaymanager/DisplayManagerServiceImpl.java`, `SetCroppingCommand.java`, `StartComponentCommand.java`: отдельное состояние generic service и RVC путь.
- Наши `java_patch/de/audi/tghu/fwhmi/DisplayManagerMIB2High.java`, `java_patch/com/luka/carplay/pdc/PdcSmallStageGuard.java`.
