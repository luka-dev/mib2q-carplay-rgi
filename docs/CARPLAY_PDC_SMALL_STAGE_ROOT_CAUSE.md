# CarPlay сворачивается при правом PDC popup: причина и природа поведения

Дата исследования: 2026-08-31  
Платформа: Audi MHI2Q, MU1316  
Исходники для анализа: `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-combined-vf`

Текущий статус, 2026-09-17: **CarPlay остаётся рядом с OPS; после установки `b0653c6` и перезапуска надпись `Check surroundings!` исчезла, но штатная нижняя строка с часами/LTE осталась.** Новая фотография пользователя подтверждает частичный результат APS-фильтра: чёрная строка перекрывает нижние кнопки CarPlay. Для оставшейся строки подготовлена правка `b0653c6-ops-statusline-r3`, пока без проверки на автомобиле. Точный загруженный runtime build ID не снимался; версия установки и перезапуск известны из выгрузки и сообщения владельца.

### Остаток полосы: системный status line, ревизия r3

APS content **6001/6002** и системная нижняя строка — разные элементы.
`SystemScreenBag2` создаёт **popup 62**, slot **3**, layer **1**, с
`StatusbarControllerHigh` и `StatusbarRendererHigh`. Последний рисует prefab
`Prefabs/mib2_statusBar`, включая фон. Поэтому маскирование APS в модели 538
убирает предупреждение, но не гарантирует исчезновения часов, LTE и фона.
Штатный экран TerminalMode **3200000** сам использует
`StatusBarStubController.renderStyle=2`: его нормальное состояние — без этой
строки. Точный источник повторного show на автомобиле пока не захвачен.

Минимальное изменение в `PartialPopupManagerEvoHigh` сохраняет это состояние
на переднем терминале во время уже существующей защиты pure OPS:

- перед показом **2100008** штатный `hidePopup(62)` убирает видимую или
  анимирующуюся строку и её запрос из очереди;
- повторный `showPopup(62)` в том же защищённом режиме возвращает HIDDEN;
- `doShowPopup` проверяется отдельно: восстановление stock-очереди после
  полноэкранного popup может обходить публичный `showPopup`;
- проверяется идентичность конкретного защищённого Screen, а не только его
  номер. Другой экран с тем же ID и задний MU не наследуют исключение;
- вне guard очередной штатный show/hide работает обычным образом. Нет
  глобального отключения status line или принудительного показа старой строки
  при закрытии OPS. APS-фильтр продолжает восстанавливать свою модель отдельно.

`OpsStatusLineTest` запускает shipping-менеджер вместе с настоящей stock-базой,
очередями и dispatch. Подменены физические popup show/hide и HMI/service edges.
Покрыты уже видимая строка, незавершённая анимация, повторный show, прямое
восстановление очереди, сохранение OPS/volume popup и возврат stock-поведения
при HOME, камере, disconnect, timeout, другом экземпляре Screen и rear MU.
Stock-first negative control воспроизводит `MMI status line 62 remained over
CarPlay`. Эти проверки не запускают Kanzi и не доказывают итоговую картинку HU.

Локальная проверка r3: `scripts/test_java_transports.sh` и финальный
`scripts/test_pdc.sh` прошли; linkage audit с обоими stock JAR и библиотекой
J9 — **0 ошибок**, 121 класс Java 1.4. `check_local_protocols.py` подтвердил
39 общих констант. JAR, снимок исходников и логи сохранены в
`build/pdc-statusline-20260917-r3/`. На HU эта ревизия пока не установлена.

Следующая проверка на машине: исчезновение **всей** нижней строки при боковом
OPS, доступность нижних кнопок CarPlay и штатная строка в HOME/камере после
перехода. Будильник/teardown — отдельный открытый дефект, этой правкой не лечится.

## Нижняя строка `Check surroundings!`

Ниже — история APS-кандидата до установки `b0653c6`. Предыдущая попытка
`efc3020` полосу не убрала. Реализация `2e536a6-ops-aps-drawer-r2`, включённая
в `b0653c6`, убрала надпись; оставшийся status line разобран выше.

### Найденный отдельный источник: APS Audio Drawer, 2026-09-17

На новой фотографии видна центральная нижняя плашка с предупреждающим
треугольником. Поиск точного текста в `lang_data.zip` штатного MU1316 дал
системные label ID **3261** и **3928** (`Check surroundings!`). Они используются
в `SystemScreenBag6.aPSAUDIO()` и `aPSAUDIOREDUCED()` соответственно:

- screen/widget **182** и **190**;
- `EntertainmentDrawerContentController`, content **6001 / 6002**;
- иконка **925**, заголовок высотой **49**, ширина панели **460–468**;
- `setFixedColorScheme(4)` и `setHeaderVisible(true)`.

Это отдельное системное содержимое нижнего audio drawer. Цепочка в stock:

```
AbstractParkingSystemControllerComponent.updateInterappClients(active)
  -> private updateAudioDrawerContext(active)
  -> AudioDrawerContext.SOURCE_APS, ACTIVE / APS_ACTIVE_HIGH_PRIO
  -> highest-priority request column 0
  -> EntertainmentDrawerContentManager: content 6001 / 6002
  -> SystemScreenBag6: APS AUDIO / APS AUDIO REDUCED
```

`updateAudioDrawerContext()` также меняет choice model **3915**. Прежнее
`removePartialPopup()` для восьми ID `21000xx` эту ветку не затрагивает.
Предположение, что видимая на фото строка обязательно принадлежит PLA/ARA
message-popup, было неверным основанием для предыдущей правки. Наличие
параллельных PLA/ARA сообщений не исключается.

Источники: локальная прошивка
`Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app/eso/hmi/lsd/jars/lang_data.zip`
и `Tools/jxe2jar/out/MU1316-vf/`: `SystemScreenBag6.java:2800,3564`,
`EntertainmentDrawerContentManager.java:105`,
`AbstractParkingSystemControllerComponent.java:1221`.
Тот же текст встречается в phone labels 302185/302245; принадлежность
системной APS-плашки установлена по её отдельной цепочке, а не только строке.

### Исправление APS drawer

`AudioDrawerContextImpl` заменён минимально: на переднем MU только колонка
`SOURCE_APS` (**0**) модели **538** проходит через `OpsAudioDrawerPolicy`.
При защищённом CarPlay + pure OPS в модель передаётся `INACTIVE`; последнее
штатное значение сохраняется отдельно. Снятие защиты сразу публикует это
значение, включая `APS_ACTIVE_HIGH_PRIO` или уже полученный `INACTIVE`.
Поэтому повторные запросы парковочного контроллера не возвращают полоску,
а HOME, камера, активный PLA/ARA, disconnect, teardown и отклонённый
show-request не оставляют штатную APS-панель навсегда скрытой.

Фильтр включается существующим `PdcSmallStageGuard` до stock activation,
который публикует APS ещё до interapp message 108. Он выключается при
освобождении защищённого экрана. `ExternalEventsListener` дополнительно
отзывает защиту при настоящем уходе из CarPlay, включая ветки с уже
неактивным приложением или ограниченным SCREEN. Промежуточный SMALL_STAGE
при возврате в CarPlay до AP 1001 сохраняет действующую защиту.

Изменяется только визуальная модель audio drawer. Choice **3915**, OPS,
звуковые ресурсы парктроника и остальные колонки drawer остаются штатными.
На заднем MU фильтр не устанавливается. Обновления одной модели
сериализованы без удержания policy-lock при вызове model callbacks;
повторные и пересекающиеся запросы сохраняют последнее состояние.

### Повторная проверка, ревизия r2

Дополнительные сценарии выявили два пропуска предыдущей проверки:

1. **Disconnect без AP 1002/закрытия popup.** Настоящий lifecycle worker
   `CarPlayApp` завершался, но APS-фильтр оставался включённым до следующего
   HMI-события. Теперь worker вызывает `carPlayDisconnected()` и восстанавливает
   экран/панель вне горячего device callback. Повторная проверка `isActive()`
   под монитором guard не даёт уже отправленной старой cleanup-команде
   сбросить OPS новой сессии при быстром переподключении.
2. **HOME → CarPlay с всё ещё открытым OPS без новой DSI notification.**
   Прежний тест сам повторно публиковал parking content и тем самым скрывал
   пропущенный переход. Теперь подтверждённая видимость OPS хранится отдельно
   от защиты CarPlay screen, а AP 1001 восстанавливает защиту только для
   актуального pure OPS и разрешённого CarPlay screen. Уже видимый popup не
   получает таймаут ожидания нового visible callback. Hide, unregistration,
   hidden registration snapshot и teardown parking service снимают признак
   видимости; camera/ARA, активный PLA, pending HOME и SCREEN restriction
   не позволяют повторно включить защиту.

Оба первоначальных сбоя воспроизведены до исправления; логи
`review-before.log` и `review-return-before.log` сохранены в каталоге кандидата.
Отдельный детерминированный тест блокирует старую cleanup на мониторе guard,
публикует новую сессию и проверяет, что запоздалое завершение не снимает её
защиту (`review-reconnect-before.log` фиксирует сбой до добавления проверки).
`CarPlayPdcLifecycleTest` исполняет настоящий `CarPlayApp`, guard и drawer
service; подменены только внешние модули/транспорт и физический HMI.

Также проверен настоящий stock `ListModel`, которым `SystemModelBank`
создаёт модель 538: все 22 не-APS колонки сохраняют исходные значения.
Расширены проверки возврата, в том числе OPS, впервые открытый вне CarPlay,
и удержание защиты после трёх секунд без повторного visible callback.

### Удалённые попытки

Удалены replacement-классы `PLAMessageHandler` и `ARAMessageHandler`,
список восьми PLA/ARA popup ID и повторные `removePartialPopup()` из worker
таймаута. Эти пути не управляли обнаруженной APS-плашкой. PLA/ARA message
handlers снова берутся из stock. Сохранён трёхсекундный таймаут отвергнутого
показа OPS, нужный для освобождения CarPlay screen.

Прежние host-тесты проверяли только предполагаемые PLA/ARA popup и потому
не доказывали исчезновение строки на фотографии. Их заменяет проверка
реального сервиса и штатного выбора APS content.

### Локальная проверка и границы результата

- `scripts/test_pdc.sh`: stock parking controller → `SOURCE_APS` → модель
  **538** → штатный `EntertainmentDrawerContentManager`. В protected OPS
  APS content **6001/6002** не выбирается; после снятия фильтра выбирается
  последнее запрошенное состояние. Проверены повторные/high-priority
  запросы, телефонный drawer, поздняя инициализация/замена сервиса, rear MU,
  HOME, pending HOME, камера, PLA, disconnect, deinit, закрытие и таймаут,
  reentrant/concurrent callbacks, возврат без новой DSI notification,
  disconnect без HMI-события и гонка cleanup с новым подключением.
- Два stock-first negative control воспроизводят отдельно захват Main Wizard
  и выбор APS-предупреждения **6001** поверх CarPlay.
- `scripts/test_java_transports.sh`: сборка shipping Java 1.4, транспорт,
  ввод, AltScreen startup, lifecycle и полный PDC regression.
- `scripts/audit_java_stock.sh`: linkage с `MU1316-final.jar` и
  `MU1316-combined.jar` + настоящей J9 class library, **0 ошибок**.

Build ID: `2e536a6-ops-aps-drawer-r2`. Артефакт: `build/carplay_hook.jar`;
логи повторной полной проверки: `build/pdc-aps-20260917/review-java-tests.log`
и `review-audit.log`; снимок и manifest — в том же каталоге.
Это локальная проверка Java-цепочки, не подтверждение картинки на автомобиле.
SSH на `10.173.189.1` при разборе дал timeout; новый JAR пока не установлен,
HU не перезапускался. Следующий шаг — установка кандидата и проверка
исчезновения именно нижней `Check surroundings!` при боковом OPS, затем
возврата штатного отображения при HOME/камере и повторном входе в CarPlay.

## Аудит порядка событий AP и жизненного цикла, 2026-09-15

Продолжение после `915c479`: проверенный локальный JAR с исправлениями ниже
имеет build ID `915c479-pdc-events`. На HU не устанавливался.
Наличие этих последовательностей в последнем эпизоде пользователя не доказано.

`PdcExternalEventsTest` исполняет настоящий replacement `ExternalEventsListener`,
штатные reactive properties/`DebounceOperation`, `HMIActivated`, `HMIDeactivated`
и `TMState`. Физические HMI, клавиши и часы очереди подменены; сама задержка
и отмена идут через штатный оператор debounce. Проверены следующие дефекты:

- Одноразовое исключение AP 1002 исчерпывалось после первого события OPS.
  Повторный AP и цикл AP 1002 → 1001 → 1002 отдавали SCREEN головному устройству.
- OPS успевал закрыться за 70 мс debounce. Его отложенный AP затем скрывал
  CarPlay и выключал уже восстановленный ввод. Быстрые повторные открытия
  и закрытия также проверены.
- Проверки только `RootWindow.currentScreen` не хватало: во время HOME fade-out
  он ещё равен CarPlay, хотя другой экран уже pending. Полноэкранный popup
  может заменить только `currentConnectedScreen`, сохранив underlying screen.
  В обоих случаях прежнее исключение поглощало настоящий уход из CarPlay.
- `deinit` не отменял подписку и её отложенную работу. Повторная инициализация
  также могла оставить старый callback; отдельно проверен callback, который
  уже попал в исполнение до отмены и был вытеснен новым AP.
- При возврате в CarPlay с уже открытым OPS AP 1001 в small stage не запрашивал
  SCREEN. Теперь запрашивает, сохраняя ввод за парковочным popup.

Исправление: повторные AP разрешены только при текущем защищённом CarPlay,
совпадающем connected screen и отсутствии pending перехода на другой экран.
При приёме AP 1002 сохраняется подтверждение парковочного перехода. После
нормального hide/DSI `POPUP_NONE` оно может пережить оставшиеся 70 мс очереди;
виджет при этом восстанавливается немедленно. Camera/PLA, неизвестный content,
таймаут, unregistration и teardown отзывают подтверждение. Перед исполнением
повторно проверяются экран, CarPlay, PLA и ограничения SCREEN. Новая подписка
ставится до регистрации слушателей; при deinit отменяется. Старые lifecycle
callbacks и AP, вытесненные более новым событием, не исполняют команды.

Проверки включают HOME до первого парковочного AP, HOME после закрытия OPS,
полноэкранный popup, camera → close, активный PLA, eCall, clamp-S, disconnect,
small stage без OPS и повторные init/deinit. `PdcResourcePolicyTest` дополнительно
проверяет подтверждение AP после **настоящего stock controller close**, а также
его отзыв по настоящему таймауту неудавшегося show. Negative control с исходным
stock tracker по-прежнему воспроизводит захват Main Wizard.

### Отдельный путь native display

Проверены MU1316 `TerminalModeSMMActions` (выход/вход состояния **3200004**),
`TerminalModeActionProxyImpl` (AP 1002/1001 с пустыми параметрами),
`MMICombiStageListener` (viewSize 1/2 → SMALL/BIG), `ScreenManager` и
`ScreenChangeManager` (pending хранится до завершения fade-out; partial popup
не заменяет connected screen).

Context **78 = [16 HMI, 59 CarPlay]** сохранён как в stock. Наш перехват
`DisplayManagerMIB2High.switchContext` защищает terminal **1** для VC;
он не является защитой HU terminal **0** от PDC. `setOpacity` передаётся stock.
`DisplayControllerEvo.setOpacityOnBackgroundLayers` может менять opacity 59
в зависимости от kombi type; `AnimationMIB2High` использует этот путь при смене
экрана и восстанавливает opacity в конце fade-in. В Sport ветка view-size 57
меняет фоновые displayables только при подключённом map grid — оснований
объявлять любое появление OPS причиной opacity=0 нет. Штатный
`CarplaySmartphoneProperties` также сохраняет запрос `rvc off` после перехода
MURVCActive true → false.

Эти механизмы проверены по локальному stock-коду, но native compositor не
исполняется в host-тестах. Глобально закреплять context 78/opacity 100 нельзя:
это нарушит HOME и полноэкранную камеру. Следующий HU trace должен включать
build ID, parking intent/108, SMALL/BIG, AP/HMI-команды, current/connected/pending
screen, фактический context terminal 0 и opacity 59. До этой проверки статус
**«исправлено на машине» не подтверждён**. Capture/videoencoderservice остаётся
штатным; эти изменения затрагивают только Java PDC и локальные тесты.

## Дополнительная проверка штатной цепочки, 2026-09-15

`PdcResourcePolicyTest.stockActivationLifecycle` исполняет настоящий приватный
`AbstractParkingSystemControllerComponent.activateParkingSystem`, replacement
override, штатные `OPS.setActive`, `OPSViewModeHandler`, `notifyParkingSystemActive`,
рассылку 108, добавление popup request и `PLAPopinHandler` в idle-состоянии.
Физические сервисы/HMI и отрисовка сообщений PLA заменены тестовыми границами;
решение о парковочном состоянии и порядок событий не подменяются. Начальный
OPS и обновления `4 -> 1 -> 7 -> 17 -> 1` сохраняют SCREEN и не переключают Main Wizard.
Это расширяет прежний тест, который собирал настоящий список компонентов,
но затем вручную вызывал guard и tracker.

Найден ещё один независимый дефект в унаследованном `ParkingPartialPopupHandler`:
любой `partialPopupHidden`, включая registration snapshot, отменял текущий
видимый popup без сравнения его ID и терминала. Теперь visible/hidden принимаются
только для main terminal; отмена контроллера выполняется лишь при совпадении
`visiblePPID`. Начальный hidden snapshot до показа по-прежнему не отменяет
in-flight guard. Новый regression test воспроизводит ошибку до исправления и
проверяет, что настоящий matching hidden освобождает guard и отменяет OPS.
Наличие такого несоответствующего callback в последнем эпизоде HU **не доказано**.

Граница выводов: `AbstractScreenWidget.setSmallStageType` только меняет поле.
Само значение `6` не доказывает видимость native plane. Тесты проверяют Java
SCREEN restriction, Main Wizard choice и запрос popup; они не исполняют GPU,
MMI compositor и реальный root-window переход. Отложенный AP теперь отдельно
проверяется описанным выше host-тестом со штатным debounce.
Если новый JAR снова не поможет, сначала нужны build ID и trace вокруг запроса:
DSI popup/mode + список компонентов, решение по 108, SMALL_STAGE/AP 1002,
`HMIDeactivated`, current screen и фактический display context. По ним можно
отличить Java takeover от скрытия native plane. Расширять исключение на все
108, игнорировать HOME или запрещать fullscreen VPS/PLA оснований нет.

Исторический статус на 2026-09-02: было записано подтверждение на автомобиле. Первая реализация на машине не сработала — при появлении правого OPS popup CarPlay всё равно закрывался. Вторая реализация закрывает дополнительные пути `SMALL_STAGE_OMISSION` и AP `1002`; после выкладки jar и перезагрузки сообщалось о сохранении проекции под правым OPS popup. Это не подтверждает все варианты активации парковочной системы.

Подтверждение — наблюдение на автомобиле, а не строка лога: маркер `/mnt/app/carplay_verbose` в тот момент был снят (его убирали при отладке звука), поэтому `Log.i` из `OPS guard` не писался. Маркер возвращён.

## Исправление 2026-09-15: участник списка не обязательно активная автопарковка

Условие предыдущей версии `systems.size() == 1` было неверным. Stock `AbstractParkingSystemPLAComponent.getSupportedDSIPopupIDs()` включает обычные OPS popup `1`, `4`, `7`, `17`. Контроллер добавляет PLA в `parkingSystemsForPopup`, а `getInvolvedParkingSystems()` возвращает его вместе с OPS, с PLA в конце списка. `EarlyFuncEvoApplication` создаёт PLA-компонент вместе с OPS/VPS при наличии parking-компонента. Следовательно, обычный боковой OPS может иметь список **`[OPS=2, PLA=16]`**, даже когда автопарковка выключена.

`AbstractParkingSystemPLAComponent.setActive(true, content)` лишь передаёт содержимое в `popinHandler.updateVpsOpsPopup()`. Фактическое состояние автопарковки читается через `isSystemActive()` и обновляется из `PDCPLAStatus.mode`. Пассивное присутствие PLA в списке нельзя приравнивать к захвату экрана автопарковкой.

Новая классификация:

- Допускаются только DSI `POPUP_OPS (1)`, `OPSAUTOACTIVATION (4)`, `OPSFLANKGUARD (7)`, `OPSOFFROAD (17)` и ровно один OPS с partial popup `2100008`.
- Допускается один дополнительный настоящий `AbstractParkingSystemPLAComponent`, только если `isSystemActive() == false`.
- Камера/VPS, ARA, явный `OPSFLANKGUARDPLA (15)`, неизвестные/неполные combined-запросы и trailer-assist mode `12` сохраняют stock takeover. Popup проверяется отдельно от списка: отсутствующий компонент не превращает VPS/ARA-запрос в обычный OPS.
- При проверках guard заново читается состояние PLA: оно может измениться без нового `DisplayContent`. Существующий перехват `notifyParkingSystemActive(nonOPS, true)` также отзывает защиту перед `108`.
- Проверки текущего CarPlay screen, HOME, отключения, eCall/clamp-S, скрытия popup и таймаута остаются прежними.

`PdcResourcePolicyTest` теперь использует настоящие MU1316 OPS/PLA/VPS popup-таблицы, `getInvolvedParkingSystems()` и `PLA.setActive()`; подменяется запуск native сервисов. Прежний искусственный pure-OPS fixture использовал popup `3`, который на деле означает `VPSOPS`; он исправлен на `1`.

Тот же новый сценарий запущен против **точного ранее установленного JAR** из `carplay-production-6d057db-20260914-alt1/bin/carplay_hook.jar`: воспроизводится `stock OPS + passive PLA toggled Main Wizard, popup=1`. После исправления сценарий проходит. Старый negative control со штатным tracker также по-прежнему воспроизводит takeover.

Runtime-тесты используют `MU1316-combined.jar`. `MU1316-final.jar` — результат преобразований для декомпиляции: его `NotifyOnAddProperty$2` падает на host с `IllegalAccessError` при обращении к private `sinks`. Это не ошибка shipping JAR; проверка linkage отдельно выполняется против обоих stock JAR с настоящей J9 class library.

Кандидат: `build/releases/carplay-production-6d057db-20260915-pdc1/bin/carplay_hook.jar`, build ID `6d057db-pdc-pla1`. Native-компоненты в комплекте сохраняют уже выгруженный altScreen hotfix `alt1`; для текущего HU достаточно заменить JAR и перезагрузить HMI/HU. При следующей установке создать `/mnt/app/carplay_verbose` **до перезапуска Java**: текущий `Log` считывает маркер только при инициализации, несмотря на устаревший комментарий про переключение без перезагрузки.

Новые INFO-строки содержат `parking intent popup=4 ... systems=2,16(idle) standaloneOPS=true` и решение `OPS 108: keep CarPlay SCREEN; no Main Wizard takeover` либо `parking 108: stock SCREEN/Main Wizard takeover`. На HU проверить боковой OPS, HOME при открытом OPS, закрытие OPS, полную камеру и активную PLA. До этого не считать проблему подтверждённо устранённой на автомобиле.

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
