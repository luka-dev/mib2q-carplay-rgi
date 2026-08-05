# Аудит Java-патча против stock MU1316

Дата исходного аудита: 2026-08-04
Повторный полный аудит текущего дерева: 2026-08-05
Проверенный вариант: `java_patch`
Авторитетный stock: `../jxe2jar/out/MU1316-combined-vf` и `../jxe2jar/out/MU1316-combined-final.jar`

## Итог повторного аудита

Patch JAR сохраняет stock public/protected ABI, собирается в bytecode Java 1.4 и больше не
содержит `ClusterProbe`, replacement-копию `AppState` или `ClusterViewMode`. Все восемь
подтверждённых runtime-дефектов повторного аудита исправлены в дереве 2026-08-05. Два безопасно
устранимых robustness-риска также закрыты. Остался один validation gap mapper'ов, для которого
нет наблюдаемого дефекта или авторитетного stock Java-аналога; он не маскируется как исправление.

Полезное изменение state machine при этом сохранено: `ctx 80` теперь не выбирается по одному лишь route intent.
Сначала полностью поднимается altScreen/stock-thread NavStatus gate и `ctx 81`, затем RGI
получает sticky snapshot, публикует BAP, ждёт реальный `FRAME_READY` текущего
`maneuver_render` peer и лишь после этого делает `setNavActive(true)`.

### Исправления по находкам повторного аудита (2026-08-05)

| Приоритет | Исходный дефект | Реализованное исправление |
|---|---|---|---|
| P1 | RGI имел конечное 10-секундное окно старта | Первые 10 s retry остаётся 200 ms, затем продолжается раз в 2 s до конца текущей CarPlay generation (`CarPlayApp.java:171-203`). Поздний Navigation service edge дополнительно будит controlled RGD rebind (`:112-140`). |
| P1 | NavStatus/RG wrapper ставился с TerminalMode dispatcher | `ClusterService.setCombiBAPService()` теперь оборачивает service синхронно на `NavigationJobs` **до** stock `setCombiService()->updateAll()` (`ClusterService.java:234-246`); остальные изменения маршалятся через `Navigation.getDispatcher()` (`AltScreenNavStatusGate.java:31-44`). |
| P1 | CarPlay перезаписывал stock Media cover-art provider type 0 | `CoverArtProviderMux` сохраняет уже зарегистрированный Media provider и направляет только source 21/CarPlay listRef в CarPlay cache (`CoverArtProviderMux.java:27-67`); при недоступной reflection seam остаётся direct push без повреждения stock provider. |
| P1/P2 | Неуспешный bind renderer IPC течёт FD каждые 500 ms | Оба локальных `ServerSocket` закрываются в catch (`RendererServer.java:96-110`, `CarplayBus.java:281-293`), renderer bind log ограничен одной записью за 10 s (`RendererServer.java:122-135`). |
| P2 | `source_supports_rg=0` не очищал slots при уже нулевом routeState | Authoritative transition в 0 всегда очищает count/order/all maneuver и lane caches; условным остался только route-state dirty bit (`RouteGuidance.java:611-640`). |
| P2 | Shutdown якобы сохранял native `rgActive`, но уже успевал сбросить его | Наличие stock route определяется **до** renderer teardown; `stopCustomRenderer(clearRouteInfo)` не сбрасывает route-info при живом native route (`BAPBridge.java:751-793,1849-1861`). |
| P2 | Частичный `BAPBridge.onStart()` не имел rollback | Catch теперь best-effort возвращает RGStatus/type/descriptor/distance/exit/maneuver, renderer, gfx и counters в retry-safe state (`BAPBridge.java:662-687`). |
| P2 | `CombiBAPServiceNavi` оставался one-shot stale handle | Service edge coalesces controlled stop/reacquire/start RGD (`CarPlayApp.java:112-140`), lookup пропускает stale first reference (`FrameworkRef.java:40-52`), старый handle по-прежнему парно release-ится в `RgdModule.stop()`. |

### Закрытые robustness-риски и оставшийся validation gap

- `CarplayDSILifecycleController.updateTouchEvents()` теперь partition-ит каждый mixed batch по
  `isTouchScreen()`: touchscreen entries идут в DSI input 1, touchpad — только в `CursorController`;
  тип больше не выбирается по element 0 (`CarplayDSILifecycleController.java:970-1019`).
- `ClusterLayerController.apply()` теперь вызывается только для cluster terminal и sysconst
  `KOMBI_KDK_VIA_DISPLAYABLES`; terminal 0/G24 больше не получает обращения к 98/101/102
  (`CombiMapController.java:164-173`).
- `ManeuverMapper`, `RendererMapper` и `SideStreets` не имеют stock Java-аналога и host vector tests;
  статический bounds-аудит ошибок не показал, но полное соответствие MHI3 требует regression vectors.

### Ранее закрытые дефекты

| Исходная проблема | Текущее исправление |
|---|---|
| Cold-boot snapshot потерян до регистрации listener | После `bus.on(EVT_RGD_UPDATE)` Java отправляет `CMD_SYNC_REQ`; C hook повторяет sticky snapshot (`RouteGuidance.java:307-319`, `hook/framework/bus.c:544-558`) |
| Поздний READY/FRAME_READY не двигает state machine | `RendererServer.StateListener` будит generation-bound presentation worker, который переигрывает полный cached RGI (`RouteGuidance.java:507-570`) |
| Bus reader блокировался до 3.7 s | Renderer handshake полностью non-blocking; ожидание заменено edge/retry worker (`BAPBridge.java:1723-1796`) |
| RGI мог стартовать раньше altScreen reset/gate | `CarPlayApp` запускает ordered dependency chain и останавливает её в обратном порядке (`CarPlayApp.java:77-134`) |
| Layer 98/101/102 оставался opacity=0 | Последний KDK layout кэшируется и переигрывается после каждого физического ctx switch (`ClusterLayerController.java:44-145`, `AltScreenModule.java:375-378`) |
| Disconnect ломал общие stock KDK backings 101/102 | Кэшируются stock visibility/opacity и восстанавливаются при release CarPlay (`ClusterLayerController.java:64-115`) |
| `ClusterProbe` менял UI через 20 s и печатал stack traces | Класс и все call-sites удалены; `ClusterViewMode` снова берётся из stock JAR |
| Data races `connected/videoAvailable/navActive` | Cross-thread flags опубликованы как `volatile`, writes остаются под единым lock (`AltScreenModule.java:114-121`) |
| Неограниченный DEBUG log | Default `INFO`, BAP trace off, файл ротируется на 512 KiB (`Log.java:12-85`) |
| Глобальный patched `AppState` ломал PHONE/SPEECH/serializer | Replacement удалён; только incoming NAVI ID 2 нейтрализуется в converter (`CarplayDSILifecycleController.java:480-503`) |
| Stale VIDEO_ON удерживал ctx | Control clients preemptable и generation-bound; real disconnect имеет 1500 ms grace с authoritative clear (`AltscreenControlServer.java:49-121`, `AltScreenModule.java:273-307`) |
| Callback после `RouteGuidance.stop()` | Frame processing/teardown сериализованы, worker generation invalidated и joined (`RouteGuidance.java:325-351,370-505`) |
| OSGi service reference leak | `FrameworkRef.ServiceHandle` делает парный `releaseService`; `RgdModule.stop()` освобождает handle |
| Stale retry thread после fast reconnect | Retry generation + identity check; interrupt завершает worker (`CarPlayApp.java:138-171`) |
| NavStatus wrapper ставился напрямую из bus/module thread | Wrapper ставится до stock `updateAll()` на Navigation service edge; поздние apply/release идут через `Navigation.getDispatcher()` |
| Native status не восстанавливался | Wrapper кэширует compass/altitude/scale/text/destination fields и replay-ит последний snapshot при release (`GatedCombiService.java:61-125,140-222`) |
| Cover art жил дольше component/source/session | Paired start/stop, dispatcher handoff, CarPlay-source flag и session reset CRC/metadata сохранены; type-0 теперь multiplexed со stock Media provider |
| Parser молча резал 512 keys | Capacity 1024 + explicit overflow warning (`CarplayBus.java:160-257`) |
| Touch sink терялся после component reinit | `init()` повторно ставит sink и сбрасывает gesture state; touchscreen сохраняет stock coordinate conversion/sort |
| G24 получал чужой смысл ctx 80/81 | Feature явно disabled при `kombiType==4`; текущий MU1316/A5 остаётся поддержан |
| Build ID был `@BUILD_ID@` | Builder генерирует временный source и вшивает `git describe`; cleanup работает также при failed build |

## Текущий объём и validation

- 28 Java-файлов, 11 081 строк: шесть необходимых полных замен stock-классов и 22 новых класса.
- `./scripts/build_java.sh` — успешно, target 1.4.
- `./scripts/build_hook.sh` — успешно, ARM QNX hook, `emutls=0`.
- `unzip -t build/carplay_hook.jar` — ошибок нет.
- `javap -verbose` — major version 48; в class constants присутствует реальный build ID, `@BUILD_ID@` отсутствует.
- `javap -protected` — ни одного пропавшего stock public/protected member в шести заменяемых outer classes.
- `ClusterProbe`, patched `AppState` и patched `ClusterViewMode` отсутствуют в JAR.
- `git diff --check` — ошибок нет.
- JAR содержит 80 class files. Build ID намеренно генерируется из текущего Git commit и поэтому
  проверяется в unit log, а не фиксируется в этом документе вместе с быстро устаревающим SHA-256.

### Обязательная on-unit матрица

1. HU cold boot с уже подключённым телефоном и активным route: `sync snapshot requested` →
   `RG presentation CONFIRMED` → один `ctx 81 -> 80`.
2. Убийство/перезапуск `maneuver_render`: немедленный fallback в 81 и возврат в 80 после
   нового `READY/FRAME_READY`, без нового iOS maneuver delta.
3. Быстрый unplug/replug и dio bounce: старые generations не могут выключить новую сессию.
4. Полный disconnect: ctx 74, stock KDK backings и NavStatus snapshot восстановлены.
5. Последовательность Android Auto → CarPlay и CarPlay reconnect: ни stale metadata, ни stale CRC
   обложки не переиспользуются.

Ниже сохранён исходный pre-fix аудит: он объясняет причины изменений и не описывает текущее
runtime-состояние.

## Исходный аудит до исправлений

### Объём проверки

- 30 Java-файлов, 10 916 строк: восемь полных замен stock-классов и 22 новых класса.
- Для восьми замен сравнивались исходники и `javap -protected -s` из stock/patch JAR.
- Проверены все вложенные `.class`, target bytecode, содержимое JAR, lifecycle `activate/deactivate`, socket reconnect, OSGi ownership, RGI/BAP path, display-context path, cover art и HID.
- `ARCHITECTURE.md` проверен как документация, но не входит в число 30 Java-файлов.

### Критические находки

### P0. Cold boot с уже активной навигацией может никогда не вызвать `setNavActive(true)`

`RouteGuidance` пытается начать RGI только когда текущий пакет содержит activation-relevant dirty bit: маска строится в `RouteGuidance.java:348-355`, а вся активация находится под `if (hasActivationDelta)` в `RouteGuidance.java:357-420`. После этого, если renderer/BAP ещё не готовы, состояние проверяется только в обработке того же пакета (`RouteGuidance.java:427-445`).

`BAPBridge.onStart()` не требует готового renderer: он вызывает `startCustomRenderer()` (`BAPBridge.java:624-633`), но затем всё равно устанавливает `bapSessionStarted=true` и возвращает `true` (`BAPBridge.java:660-663`). Повторный renderer-start существует лишь внутри следующего `BAPBridge.update()` (`BAPBridge.java:1114-1125`). `RendererServer` умеет принять позднее подключение (`RendererServer.java:125-167`), однако его `FRAME_READY` не инициирует повторную проверку presentation state.

Следствие: при загрузке HU единственный sticky RGI snapshot может прийти раньше `maneuver_render`. Позднее renderer подключится, но без нового iOS RGI delta `BAPBridge.update()` и `AltScreenModule.setNavActive(true)` больше не вызываются. Это точно согласуется с наблюдением «ctx уже выбран, но `setNavActive(true)` ни разу не был».

Минимальное исправление:

- сделать `RendererServer` readiness/death callback источником события для `RouteGuidance`, либо завести один короткий retry worker с generation token;
- сохранять последний полный `RouteGuidance.State` и при `READY/FRAME_READY` повторять `startCustomRenderer() -> bap.update(cachedState) -> isPresentationReady()`;
- менять `ctx 81 -> 80` только после успешного BAP update и `FRAME_READY`, как требует текущая политика;
- не выполнять ожидания renderer на единственном `CarplayBus` reader thread.

Проверка: boot HU с уже подключённым телефоном и активным route. Даже без нового манёвра лог обязан показать `renderer connected`, `FRAME_READY`, затем `RG presentation CONFIRMED`, после чего ровно один переход `81 -> 80`.

### P0. Плоскости 98/101/102 не переигрываются при переходе nav inactive -> active

Единственный вызов `ClusterLayerController.apply()` находится внутри stock KDK model-update path: `CombiMapController.java:147-166`. Сам controller скрывает плоскости при `navActive=false` и выставляет crop/opacity при `true` (`ClusterLayerController.java:40-63`).

Но `AltScreenModule.setNavActive()` лишь меняет флаг и публикует новый context (`AltScreenModule.java:179-186`); geometry/opacity он не применяет. Поэтому последний KDK update, пришедший до RGI readiness, оставляет opacity 98/101/102 равной нулю. Поздний переход в `ctx 80` сам по себе её не восстанавливает.

Минимальное исправление: `CombiMapController` должен кэшировать последний `popup/inTube` layout, а переход `setNavActive(false -> true)` должен переиграть этот layout через HMI/DisplayManager dispatcher. Вызов из произвольного bus thread напрямую нежелателен. Ошибка `ClusterLayerController.apply()` сейчас полностью проглатывается (`ClusterLayerController.java:64-66`); хотя бы первая ошибка должна логироваться.

### P0. `ClusterProbe` является активным управляющим кодом, а не пассивной диагностикой

Несмотря на комментарий «inert by default», `ClusterProbe.on()` безусловно возвращает `true` (`ClusterProbe.java:32-37`). При создании `ClusterService` запускается one-shot (`ClusterService.java:158-161`), который через 20 секунд вызывает `ClusterInputListener.setFavoredViewMode(3, false)` (`ClusterProbe.java:77-96`). Каждый cluster context switch также строит полный Java stack trace (`ClusterProbe.java:41-50`); вызовы стоят в `DisplayManagerMIB2High.java:61-62,291-304` и в patched cluster classes.

Это способно объяснять ровно 20-секундное самопроизвольное изменение UI и создаёт значительный log/GC pressure. Перед production `ClusterProbe` надо удалить либо сделать compile-time false; простой marker gate тоже не нужен в shipping JAR.

### P0. Shared state `AltScreenModule` читается без гарантии видимости между потоками

`connected`, `videoAvailable`, `navActive` объявлены обычными `boolean` (`AltScreenModule.java:104-110`). Записи выполняются под `LOCK` (`AltScreenModule.java:155-158,168-171,180-183,231-234,244-247`), но `isConnected()` и `isNavActive()` читают их без lock (`AltScreenModule.java:94,192`). Эти getters вызываются HMI/KDK thread, тогда как writers приходят с control/bus threads.

Это data race по Java Memory Model: pin контекста и opacity gate могут видеть старое значение. Минимум — сделать три флага `volatile`; лучше публиковать единый immutable presentation state или синхронизировать все getters на том же lock.

### P0. Production logging не ограничен

Уровень по умолчанию — DEBUG (`Log.java:11-16`), каждая строка одновременно пишется в stdout и append/autoflush `/tmp/carplay_java.log` (`Log.java:32-45`), без лимита и ротации. Дополнительно `BAP_TRACE_ENABLED=true` (`BAPBridge.java:34-39`) и почти каждый RGI delta логируется (`RouteGuidance.java:916-918`). `ClusterProbe` добавляет stack traces.

Это не доказывает, что прошлое зависание HU было вызвано логами, но код создаёт конкретный риск заполнения `/tmp`, нагрузки на `lsd/slog` и лишнего GC. Для production: уровень W/I, BAP trace false, bounded/rotated файл и никакого stack trace в hot path.

## Высокий приоритет

### P1. Глобальная подмена `AppState` подавляет PHONE и SPEECH вместе с NAVI

Stock getters возвращают поля (`stock org/dsi/ifc/carplay/AppState.java:20-25`), patched getters всегда возвращают ноль (`java_patch/org/dsi/ifc/carplay/AppState.java:30-37`). Входящий converter использует именно getters (`CarplayDSILifecycleController.java:477-488`), затем `TMState` обрабатывает ID 1 как PHONE, 2 как NAVI и 3 как SPEECH (`stock de/audi/app/terminalmode/statemachine/TMState.java:363-377`).

Подмена также затрагивает обратное направление: stock `AppStateSerializer` сериализует `getAppStateID()` и `getOwner()` (`stock de/esolutions/fw/comm/dsi/carplay/impl/AppStateSerializer.java:9-18`). Поэтому любой исходящий patched `AppState` получает на проводе `id=0, owner=0`.

Правильный seam — фильтровать только входящее состояние NAVI/ID 2 в `convertAppState()` или непосредственно перед `TMState.updateCurrentAppState()`. DTO `AppState` должен остаться byte-for-byte stock, чтобы PHONE, Siri/SPEECH и DSI serialization не менялись.

### P1. `videoAvailable` может удерживать CarPlay context после реального disconnect

Cluster считается занятым при `connected || videoAvailable` (`AltScreenModule.java:94,152-164`). `stop()` сбрасывает `connected` и `navActive`, но не `videoAvailable` (`AltScreenModule.java:240-250`). Control server обрабатывает только одного клиента синхронно (`AltscreenControlServer.java:55-63`) и блокируется в `read()` без timeout/heartbeat (`AltscreenControlServer.java:73-92`).

Half-open renderer connection может не дать принять новый клиент и навсегда оставить `ctx 81`/NavStatus gate после deactivation. Требуются session generation, heartbeat/read timeout, preemption нового клиента и авторитетный clear data-plane latch на подтверждённом CarPlay teardown с коротким grace для ожидаемого dio bounce.

### P1. `CarplayBus.off()` не отменяет уже захваченный callback

Listener копируется под lock, но вызывается вне lock (`CarplayBus.java:361-371`). `off()` только зануляет слот (`CarplayBus.java:110-118`). `RouteGuidance.stop()` после этого разбирает BAP/renderer (`RouteGuidance.java:285-297`), в то время как ранее захваченный `onFrame` может продолжить работу. Re-check `running` есть лишь в activation branch (`RouteGuidance.java:386-390`); update в `RouteGuidance.java:427-445` может пройти после stop.

Исправление: lifecycle generation в каждом frame, повторная проверка непосредственно перед BAP/renderer mutation и согласованное завершение текущего callback до `bap.onShutdown()`.

### P1. Cover art не привязан к lifecycle и типу projection source

Singleton запускается из конструктора `TerminalModeBapCombi.EventListener` (`TerminalModeBapCombi.java:232-246`), но `deinit()` его не останавливает (`TerminalModeBapCombi.java:68-74`). Повторный `CoverArt.start()` ничего не делает, если singleton уже running (`CoverArt.java:38-45`), поэтому recreated component сохраняет callback на старый inner object. Callback вызывается прямо с bus reader thread (`CoverArt.java:63-84`), а metadata fields `TerminalModeBapCombi.java:235-268` не синхронизированы.

`AppConnectorTerminalMode` проверяет лишь generic TerminalMode audio focus (`AppConnectorTerminalMode.java:64-73`), а не CarPlay source. Поэтому stale CarPlay artwork теоретически может попасть в Android Auto/другой TerminalMode source.

Нужно: stop/clear callback при deinit, source/session gate на CarPlay, синхронизация snapshot metadata и публикация BAP через stock dispatcher.

### P1. Java parser молча обрезает большие RGI snapshots

`CarplayBus.Data` имеет жёсткий `MAX=512` и молча выбрасывает хвост (`CarplayBus.java:163-172`). C snapshot допускает 16 манёвров (`hook/routeguidance/rgd_tlv.h:205-212`), пишет много ключей на каждый slot (`rgd_hook.c:472-644`) и затем проходит все 32 lane-cache slots, включая clear records (`rgd_hook.c:1137-1149`). Максимальный пакет способен превысить 512 пар, и первыми теряются поздние lane keys.

Минимум: 768/1024 entries и явный overflow flag/log. Предпочтительно динамический storage либо отказ от отправки пустых clear records для всех 32 slots.

### P1. OSGi service reference теряется на каждой новой сессии

`FrameworkRef.getService()` получает `ServiceReference[]`, вызывает `getService(refs[0])` и выбрасывает reference (`FrameworkRef.java:40-47`). `RgdModule` делает lookup при каждой сессии (`RgdModule.java:23-30`), а `stop()` ничего не release (`RgdModule.java:54-56`). Stock `IServiceManager` имеет парный `releaseService` (`stock de/audi/app/terminalmode/osgi/IServiceManager.java:16-26`), а `ServiceManagerImpl` реализует пару через `BundleContext.getService/ungetService` (`stock de/audi/app/terminalmode/osgi/ServiceManagerImpl.java:121-137`).

Нужно хранить `{reference, service}` и освобождать reference в `RgdModule.stop()`, либо использовать stock service tracker. Это также позволит корректно пережить замену сервиса во время component restart.

### P1. Retry thread переживает быстрый deactivate/reactivate

`stopRetry()` только делает `interrupt()` (`CarPlayApp.java:142-145`), а helper `sleep()` проглатывает `InterruptedException` (`CarPlayApp.java:148`). После быстрого reconnect старый worker видит снова `active=true` и продолжает работу параллельно с новым (`CarPlayApp.java:126-139`). `startPass()` сериализован, поэтому это не немедленный memory corruption, но появляются stale lifecycle work и лишние retries.

Нужен generation token; `InterruptedException` должен завершать worker, а не игнорироваться.

### P1. Renderer death callback написан, но не подключён

`RendererServer` объявляет `DeathListener` (`RendererServer.java:81-89`) и вызывает его при reader failure (`RendererServer.java:192-228`), однако `setDeathListener()` нигде не вызывается. `BAPBridge` замечает потерю только после трёх последующих ошибок отправки (`BAPBridge.java:1798-1819`). При неподвижной route data пустой `ctx 80` может остаться надолго.

Перед подключением callback надо исправить ownership: старый reader сейчас вызывает `notifyDeath()` до проверки `sock == owned` (`RendererServer.java:204-216`), поэтому socket preemption может породить ложную смерть нового connection. После этого death/ready/frame-ready должны пробуждать presentation state machine.

### P1. Неуспешный BAP start может не повториться

Если `bap.onStart()` вернул `false`, `RouteGuidance` оставляет `rgActive=false` (`RouteGuidance.java:392-405`), затем очищает dirty state и выходит (`RouteGuidance.java:422-425`). Следующий distance-only delta не попадает в activation mask (`RouteGuidance.java:348-355`) и start не повторяет.

Нужен явный `activationPending`, повторяемый на readiness event/таймере до успеха или реального route end.

### P1. NavStatus gate не восстанавливает полный native snapshot

При takeover wrapper инвалидирует compass/altitude/map scale/current position/turn-to (`GatedCombiService.java:83-96`) и затем блокирует новые значения (`GatedCombiService.java:150-153,178-183`). При release переигрывается только deferred initializing state (`GatedCombiService.java:73-80`), но не последние native values.

После отключения CarPlay нижний bar может оставаться пустым до случайного следующего native update. Нужно кэшировать каждое заблокированное значение и атомарно replay при release либо запросить stock producer resync.

### P1. Blocking renderer wait выполняется на единственном bus reader

`CarplayBus` вызывает listener синхронно из reader loop (`CarplayBus.java:335-371`). На этом пути `BAPBridge.startCustomRenderer()` ждёт renderer до 2500 ms и frame до 1200 ms (`BAPBridge.java:1717-1769`). В это время задерживаются все hook -> Java события, включая lifecycle и view-area ACK.

Renderer readiness нужно вынести в отдельный presentation worker; bus thread должен только обновлять cached state и сигнализировать worker.

### P1. Stock native navigation отменяется на весь CarPlay session

Это не случайная регрессия, а явная policy: `BAPBridge.engageTakeover()` вызывает `tryStopNativeNavigation()` и закрывает native RG gate уже при connect (`BAPBridge.java:529-555`); сам cancel вызывает `RouteManager.stopRouteGuidance()` (`BAPBridge.java:416-452`). Решение соответствует выбранному режиму REPLACE, но его нужно считать продуктовым контрактом: при подключённом CarPlay штатная Audi route guidance прекращается даже без активного Apple Maps route.

## Средний приоритет и переносимость

### P2. Изменена stock-семантика distance/bargraph для всех navigation sources

Patch делает distance model valid при `flag && i > 0`, независимо от `showBargraph` (`ClusterService.java:350-389`). Stock намеренно использует `flag && i > 0 && !showBargraph` (`stock ClusterService.java:346-359`). Изменение глобально действует и на Audi navigation. Если оно нужно только CarPlay, его надо ограничить активным takeover gate.

### P2. Custom contexts фактически поддерживают только A5/non-G24

`DisplayManagerMIB2High` выделяет расширенную таблицу только для non-G24 (`DisplayManagerMIB2High.java:61-67`) и создаёт реальные CarPlay contexts 80/81 только там (`DisplayManagerMIB2High.java:169-183`). На G24 `defineContextsForG24()` формирует обычные stock KDK twins (`DisplayManagerMIB2High.java:186-194`), но `AltScreenModule` всё равно выбирает 80/81.

Для текущего MU1316 A5 это допустимо. Код должен явно отказать/заложить feature gate на G24, а не молча выбирать другой смысл context 80.

### P2. Touch sink не переустанавливается после полного component deinit/init

Sink устанавливается лишь в конструкторе nested key controller (`CarplayDSILifecycleController.java:826-853`), очищается в outer `deinit()` (`CarplayDSILifecycleController.java:105-113`), но outer `init()` его не ставит обратно (`CarplayDSILifecycleController.java:98-103`). Если один и тот же component object проходит deinit/init, HID останется отключён. Обычный CarPlay reconnect без component deinit этим не затронут.

### P2. Worker может сделать лишний ранний switch в stock context 74

`AltScreenModule.start()` сначала создаёт worker (`AltScreenModule.java:215-226`), и только затем публикует `connected=true`/`navActive=false` (`AltScreenModule.java:228-236`). При `currentCtx=-1` worker может успеть применить старый desired 74 перед republish 81. Это лишний switch и потенциальная задержка/flicker. Session state следует публиковать до старта worker.

### Resolved: build identity

`CarPlayApp.BUILD_ID="@BUILD_ID@"` остаётся шаблоном в source tree. `scripts/build_java.sh`
подставляет sanitized `git describe` в generated copy перед компиляцией, поэтому unit log однозначно
показывает deployed build без изменения исходника.

### P2. Thread affinity установки BAP wrapper не доказан

`AltScreenNavStatusGate.ensureInstalled()` напрямую заменяет `ClusterService.combiBAPListener.combiservice` (`AltScreenNavStatusGate.java:29-53`; accessors `ClusterService.java:1106-1112`). В stock поле не `volatile` (`stock CombiBAPListener.java:48-56`), а вызов приходит из module/bus lifecycle thread. Без stock dispatcher или общего lock HMI producer не обязан мгновенно увидеть wrapper. Установку/снятие нужно маршалить на stock event thread либо формально синхронизировать доступ.

## Проверка каждого Java-файла

### Полные замены stock-классов

| Файл | Изменение относительно stock | Вердикт |
|---|---|---|
| `AppConnectorTerminalMode.java` | Вставка cover-art перед stock station update | ABI сохранён; нужен CarPlay source/lifecycle gate |
| `TerminalModeBapCombi.java` | Always-on transport и cover-art merge | Stock flow в основном сохранён; callback leak/data race P1 |
| `CarplayDSILifecycleController.java` | Touchpad -> stock DSI DPAD; select оставлен stock | ABI сохранён; component reinit hole P2 |
| `DisplayManagerMIB2High.java` | Contexts 80/81 и защита от stock KDK remap | Корректно для A5; G24 unsupported; удалить probe |
| `ClusterService.java` | BAP accessors/hooks, probe, distance+bargraph | ABI сохранён; probe P0, global native-nav semantic change P2 |
| `ClusterViewMode.java` | Только вызовы диагностического probe | После удаления probe должен быть возвращён к stock |
| `CombiMapController.java` | View-area publish, stock context pin, layer callout | Stock map/KDK flow сохранён; missing layer replay P0 |
| `org.dsi.ifc.carplay.AppState.java` | Getters ID/owner всегда 0 | ABI формально сохранён, семантически слишком широкий P1; вернуть stock |

Во всех восьми классах отсутствуют пропавшие public/protected методы или поля. Единственные additions: override `updateCurrentStation` и шесть helper/hook методов в `ClusterService`. Все stock nested class files для заменяемых outer classes присутствуют в patch JAR; изменение номеров synthetic `access$...` ожидаемо после recompilation и не является внешним ABI.

### Новые классы

| Файл/группа | Роль | Вердикт |
|---|---|---|
| `CarplayBus.java` | Один localhost transport hook <-> Java | Reconnect ownership сделан хорошо; callback teardown race и 512-key truncation P1 |
| `AltScreenModule.java` | Владелец ctx 74/80/81 и view area | Архитектура single writer разумна; state visibility, startup order, stale video latch P0/P1 |
| `AltScreenNavStatusGate.java`, `GatedCombiService.java` | Подавление stock NavStatus | Нужны stock-thread install и полный native replay |
| `AltscreenControlServer.java` | VIDEO_ON/OFF data-plane truth | Нет preemption, timeout, generation и session binding P1 |
| `CarPlayApp.java`, `Module.java`, `RgdModule.java`, `FrameworkRef.java` | Module lifecycle/retry/OSGi | Простая схема, но retry generation и OSGi release отсутствуют |
| `RouteGuidance.java`, `BAPBridge.java` | RGI state, BAP takeover, presentation gate | Функционально полный path; readiness не event-driven, bus блокируется, lifecycle races |
| `RendererServer.java` | maneuver renderer IPC/readiness | Heartbeat/reconnect есть; death listener не wired и ownership check поздний |
| `ManeuverMapper.java`, `RendererMapper.java`, `SideStreets.java` | Mapping iOS maneuver -> BAP/renderer | Очевидных bounds/ABI ошибок не найдено; stock аналога нет, нужны vector tests |
| `ClusterLayerController.java` | Plane 98/101/102 geometry | Геометрия централизована; отсутствует edge replay, ошибки молча теряются |
| `AltScreenCombiBAPListener.java` | Steering-wheel map-scale interception | Узкий перехват; central-console rotary остаётся stock |
| `CursorController.java` | Touch gesture -> DPAD | Основной seam корректен; есть dead API и reinit state hole |
| `CoverArt.java`, `AppConnectorTerminalMode$CoverArtProvider.java` | Artwork transport/provider | Нужны lifecycle/source gate и синхронизация |
| `ClusterProbe.java` | Временная диагностика | Не должен присутствовать в production; сейчас активно меняет UI |
| `Log.java` | Общий stdout + file logger | Неограниченный DEBUG/autoflush неприемлем для production |

## Dead/stale code

- `AltScreenModule.setDesired()` не вызывается (`AltScreenModule.java:253-256`).
- `CarPlayApp.fw()` не используется (`CarPlayApp.java:102`).
- `CursorController.isFeatureEnabled()`, `onTwoFingers()` и `shutdown()` не используются (`CursorController.java:88,221-238`).
- `RendererServer.setDeathListener()` существует, но не подключён (`RendererServer.java:81-89`).
- `CarplayBus.EVT_DEVICE_STATE`, `CMD_ALT_APPEARANCE`, `CMD_ALT_UICTX` не имеют Java consumers (`CarplayBus.java:49,56-57`).
- Комментарии `CursorController` всё ещё упоминают старый `CarPlayHook` seam.

Удалять `setDeathListener` не следует: после исправления ownership он нужен для cold-boot/recovery state machine. Остальное можно убрать после подтверждения отсутствия внешнего reflection/config использования.

## Build и ABI validation

Выполнено:

```text
./scripts/build_java.sh
Compiling 27 files (target 1.4)...
Output: build/carplay_hook.jar
```

Результат:

- build успешен;
- `unzip -t build/carplay_hook.jar` — ошибок нет;
- major class version 48 (Java 1.4);
- `javap -protected -s` — ни одного пропавшего stock public/protected member в восьми заменах;
- `git diff --check -- java_patch` — whitespace errors нет;
- Java source tests в репозитории отсутствуют. Сейчас mapping, reconnect и lifecycle проверяются только сборкой и on-unit интеграцией.

## Рекомендуемый порядок исправлений

1. Удалить/выключить `ClusterProbe` и production DEBUG/BAP trace.
2. Сделать единый event-driven `RouteGuidance` presentation worker: cached snapshot + renderer READY/FRAME_READY/death + generation; не блокировать bus reader.
3. На `setNavActive(false -> true)` переигрывать cached layer geometry на stock HMI thread; сделать state visibility корректной.
4. Вернуть stock `AppState`; подавлять только incoming NAVI ID 2 в узком converter seam.
5. Добавить session binding/heartbeat/preemption для alt control и авторитетно сбрасывать stale `videoAvailable`.
6. Закрыть callback teardown race, OSGi reference leak и retry-thread generation.
7. Исправить cover-art lifecycle/source gate и увеличить/сделать явным RGI parser capacity.
8. Добавить host-side tests: cold boot with sticky snapshot, fast reconnect, renderer late-ready/death, 600+ RGI keys, AppState PHONE/NAVI/SPEECH serialization и maneuver mapping vectors.

Production gate: до выполнения пунктов 1–4 текущий Java JAR нельзя считать стабильной заменой stock, несмотря на сохранённый ABI и успешную сборку.
