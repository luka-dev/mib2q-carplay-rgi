# Полное сравнение lifecycle: MHI2Q и MST2 rev1/rev2

Дата: 2026-09-10. Сравнивается текущий код `mhi2-carplay` и пакет `/Users/luka/Downloads/rev1-rev2-withsource-edited-work`, включая оба декомпилированных JAR и проверенные ветки готовых ARM SO.

Текущий HEAD при завершении чтения — `50d911a`. Во время работы появился коммит `fix(rgd): restore VC route text with selected glyphs`; startup, framework, altScreen, renderer и основные Java lifecycle-классы относительно исходного `44c772a` не изменились. В рабочем дереве есть независимые изменения audio; здесь проверены только его общие AirPlay lifecycle callbacks, не весь DSP. Снимок хешей анализируемых файлов: [manifest](altscreen_lifecycle_inputs_2026-09-10.json).

**Вывод:** у нас отдельные lifecycle процесса, iAP2/RGD, AirPlay streams, decoder и Java display contexts, с защитой поколений на нескольких границах. MST2 использует два долгоживущих штатных процесса, общий video ring и файловые сигналы. Его прямой видеопуть короче, но завершение и восстановление слабее связаны с фактической жизнью сессии.

«Полное» здесь означает весь доступный путь модификации от загрузки до повторного подключения. Полный внутренний lifecycle штатных SAL, DisplayManager, smartphone_integrator и прошивки iPhone не входит в приложенный пакет и не восстановлен целиком. Где описан риск, это статический вывод, а не воспроизведённый автомобильный дефект.

## 1. Что чем владеет

| Объект | Наш MHI2Q | MST2 rev1/rev2 |
|---|---|---|
| Основная CarPlay-служба | `dio_manager`, запускаемый smartphone_integrator (SI) | Штатный `iMX6.SAL` |
| Native hook | `libcarplay_hook.so`: framework + RGD/coverart/altScreen/audio | `libmst2-h29-eu-altscreen-40fps-rev1.so` в SAL |
| Вывод карты | Отдельный `altscreen_render`, OMX → EGL/Screen → штатный capture/encoder → приборка | `libmst2-h29-eu-ringio-bridge-rev2.so` внутри `iMX6.DispMg`: H.264 → MPEG-TS → RingIO/MOST |
| Java | `CarPlayApp`, `AltScreenModule`, `RgdModule`, steering module | Замена штатных классов через HMI_Return_v10 и NavActiveIgnore |
| Межпроцессное видео | TCP localhost:19831; закрытие процесса закрывает peer socket | `/dev/shmem/carplay-alt-ring.bin`, 64 слота по 256 KiB; файл переживает SAL |
| Управление | Java bus :19810, renderer control :19832, номера поколений | showUI/ready/force-IDR marker-файлы без идентификатора сессии |
| Владение приборкой | Java переключает контексты 74/80/81 | Bridge подавляет OEM RingIO writes, перенаправляя их в discard buffer |

Да, **rev1 содержит настоящий hook**: экспортирует `AirPlayCopyServerInfo`, `AirPlayReceiverSessionSetup`, `AirPlayReceiverSessionStart`, вызывает оригиналы через `dlsym(RTLD_NEXT, …)`. Именно он объявляет второй display и принимает type 111. **Bridge rev2** перехватывает конструктор RingIO, `writeAquire` и `writeRelease`. JAR не загружает этот native hook — загрузку задаёт `cpu.conf`.

Источники: [установка preloads](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/package/custom/greenmenu/scripts/as_common.sh:186), [MST2 AirPlay hooks](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:1902), [RingIO hook](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:2348), [наш module table](../../hook/main.c:1).

## 2. Общий lifecycle: запуск → CarPlay → отключение

### Наш MHI2Q

1. **SI запускает wrapper.** `carplay_child.json` задаёт startup 10 s, stop 8 s, watchdog 60 s, restart delay 3 s и retry/downtime policy. Wrapper запускает monitor и делает **`exec dio_manager`**, сохраняя PID, отслеживаемый SI. `LD_PRELOAD` задаётся только этому процессу. Подготовка renderer перенесена в monitor, чтобы не съедать стартовый бюджет dio.
2. **Renderer-процессы живут дольше соединения и dio.** Monitor принимает уже работающие процессы или запускает отсутствующие; первоначально проверяет порт alt renderer, затем преимущественно PID. При выходе renderer использует backoff 5 s. Обычный выход/замена dio оставляет оба renderer живыми. Текущий cleanup вызывает штатную mdns/PPS-очистку и также оставляет их живыми. Явный stop через monitor завершает снимок PID renderer.
3. **Native framework инициализируется лениво.** Первый Cinemo/AirPlay entry point вызывает `pthread_once`: регистрация и init модулей. Межмодульные AirPlay callbacks выполняются по priority. Bus стартует отдельно; неудачный старт повторяется на последующих входах с ограничением частоты.
4. **iAP2 — самостоятельный цикл.** `IdentifyStart` сбрасывает прошлое identify/auth/RGD-состояние и injection context; Identify/Auth разрешают дальнейшую работу. RGD объявляет компонент, подписывается/повторяет запросы после готовности. `STOP_LOCATION` не считается disconnect. **AirPlay TEARDOWN также не генерирует общий iAP2 disconnect**, поскольку iAP2 может пережить пересоздание AirPlay.
5. **Java transport постоянно готов.** Инициализация TerminalMode запускает bus и control server. `ACTIVATING` CarPlay публикует activate; `ACTIVE` восстанавливает activate после краткого провала. Потеря CarPlay-identity публикует deactivate. Эти callbacks передают намерение lifecycle worker и не выполняют длинный start/stop прямо в штатном callback.
6. **Java модули запускаются последовательно:** AltScreen → RGD → steering. При неготовом сервисе цепочка останавливается и повторяется: первые 50 интервалов по 200 ms, затем по 2 s, пока поколение актуально. Stop выполняется в обратном порядке, включая частично начатые модули; устаревший результат start отменяется. Замена navigation service запускает отдельное переинициализирование RGD. Deactivate имеет debounce 400 ms.
7. **Bus reconnect не равен перезапуску приложения.** Новое Java bus-соединение вытесняет старое; очереди команд привязаны к connection generation. Hook посылает HELLO и сохранённое состояние; новый RGD listener дополнительно запрашивает snapshot. Не нужно ждать новой навигационной команды от телефона для восстановления последнего состояния.
8. **Отключение проходит по нескольким независимым каналам.** AirPlay останавливает нужные media receivers; TMDev/Java освобождает RGD takeover и возвращает display context; EOF renderer очищает decoder-сессию. На обычном disconnect сами renderer и Java servers остаются доступны следующему соединению.
9. **Recovery USB отделён от video recovery.** Только три завершившихся достаточно долгих dio-запуска без успешного SETUP и с точной PPS-сигнатурой `drivers_matched=2/running=1` ставят запрос одноразового USB reset. Следующий wrapper перепроверяет состояние, выставляет latch и выполняет reset. Успешный SETUP снимает эту диагностику. Ошибка уже поднятого type 111 сама по себе такой reset не обосновывает.
10. **Штатный выход процесса имеет общий shutdown.** Framework публикует shutting_down под lock, освобождает lock до callbacks, сообщает SHUTDOWN, завершает модули в обратном порядке регистрации, затем injection worker и bus. RGD при этом очищает локальное состояние без синхронного SendIAP2 в destructor. Аварийное завершение/SIGKILL не обязано исполнять этот путь — сокеты и память освобождает ОС.

Источники: [SI config](../../deploy/smartphone_integrator/carplay_child.json), [startup/monitor](../../deploy/smartphone_integrator/carplay_startup.sh:1), [cleanup](../../deploy/smartphone_integrator/carplay_cleanup.sh:1), [USB policy](../../deploy/smartphone_integrator/carplay_processes.sh:39), [framework state](../../hook/framework/hook_framework.c:440), [AirPlay lifecycle](../../hook/framework/airplay_seams.c:217), [CarPlayApp](../../java_patch/com/luka/carplay/core/CarPlayApp.java:33), [TMDev callbacks](../../java_patch/de/audi/app/terminalmode/combi/TerminalModeBapCombi.java:89), [RGD start/stop](../../java_patch/com/luka/carplay/core/RgdModule.java:26), [bus preemption](../../java_patch/com/luka/carplay/bus/CarplayBus.java:359).

**Поправка к старым описаниям:** текущие исполняемые startup/cleanup оставляют renderer живыми и после обычного завершения dio. Комментарии, связывающие их жизнь исключительно с одним dio generation, и соответствующие старые описания в документации устарели. При разрыве hook-link `av_sink_session_end()` уничтожает decoder, но **сохраняет EGL/Screen presenter** для reconnect.

### MST2

1. Установщик дописывает probe в существующий preload SAL, bridge — в preload DispMg, а JAR — в `-Xbootclasspath/p` HMI. Исходные preloads сохраняются. Отдельного аналога нашего SI wrapper/renderer supervisor пакет не добавляет; запуск и restart штатных сервисов остаются обязанностью платформы.
2. Constructor probe в основном пишет сообщение загрузки. Реальная работа начинается на перехваченных AirPlay вызовах. Bridge запускает один detached worker при конструировании RingIO размера `0x50000` с ожидаемым параметром `writer==0`.
3. Основной CarPlay продолжает штатные control/setup/start, screen/audio/resource transitions. Приложенные native hooks не реализуют аналог нашего общего iAP2 Identify/RGD lifecycle framework.
4. JAR-классы постоянно подменяют соответствующие штатные классы на время жизни JVM. Их не включают и не снимают на каждую CarPlay-сессию. HMI_Return добавляет действие при штатном событии connection lost; NavActiveIgnore изменяет правила навигационного арбитража.
5. При нормальном завершении alt TCP probe ставит `session_active=0`, удаляет marker-файлы и закрывает listener. Bridge замечает это и выполняет fast release. Сам bridge worker не заканчивается и ждёт следующего поколения кольца.
6. **Возврат главного HMI и приборки — два разных механизма.** `ASLHandler` возвращает главный canvas через `takeScreen`, пока disconnect ещё не сбросил флаги borrow. Приборка возвращается через fast release native bridge. Наличие исправления HMI_Return не гарантирует возврат OEM-карты при зависшем bridge.

Источники: [JAR bootstrap](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/package/custom/greenmenu/scripts/as_common.sh:338), [декомпиляция и bytecode-проверка](mst2_jars_decompiled/README.md), [bridge worker](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:2145).

## 3. AltScreen lifecycle по этапам

| Этап | Наш MHI2Q | MST2 rev1 + bridge rev2 |
|---|---|---|
| Согласование возможностей | Control SETUP делает preflight полного display contract; сохраняет stock accepted features и добавляет altScreen/viewAreas. `/info` использует согласованный набор displays/displayPanels | `/info` дополняет существующий displays; успешный SETUP без streams отдельно заменяет enabledFeatures на iAPChannel/viewAreas/altScreen. Общего атомарного preflight нет |
| Геометрия | 1440×542, maxFPS 30, три viewArea | Готовый ELF: 1010×376, maxFPS 40, одна viewArea |
| Выбор type 111 | Ищет 111 во всём streams array после stock SETUP | Проверяет только первый stream. Если первым идёт другой тип, специальная ветка 111 не выполнится |
| Stock SETUP для 111 | Общий seam вызывает stock, затем модуль дополняет ответ; ошибка собственного setup возвращается вызывающему коду | **В готовом rev1 включены STOCK_FIRST_SETUP и STOCK_RESPONSE_MERGE:** вызывает stock, проверяет status, затем создаёт собственный listener и переносит его streams в stock response |
| AES/listener | Ключ из session+0x1c8, новый streamConnectionID; stock socket helper с preferred 6031, новая generation; descriptor/receiver/ownership commit согласованы | Ключ из session+0x1b8, streamConnectionID первого stream; собственный IPv6 listener на 7001. При невозможности derive может всё равно создать listener с aes_valid=0 |
| Ожидание телефона | Acceptor проверяет stop с шагом 1 s; у активного socket stop проверяется poll каждые 60 ms | Accept timeout 180 s; после accept `session_active=1` |
| Начальное video | avcC → SPS/PPS cache; AES → AVCC; до IDR кадры не отправляются decoder. IDR запрос при старте и ограниченные повторы | avcC/AVCC поступают в общий ring. Probe readiness означает валидные NAL + config; реальный takeover bridge ждёт config и IDR |
| Когда showUI | Первый полученный IDR запускает showUI + night + текущий viewArea; успешный showUI callback запрашивает следующий IDR | Успешный SETUP main stream 110 запускает detached задачу с задержкой 1 s; showUI ACK создаёт общий файл и запускает ForceKeyFrame |
| Роль SessionStart | Основной старт остаётся stock; отдельный alt lifecycle устроен вокруг SETUP, receiver и TEARDOWN | Hook SessionStart делегирует stock и логирует; рабочий запуск alt не находится целиком в нём |
| Условие видимости | Java заранее выбирает 81/loader; подтверждение VIDEO_ON приходит после успешной RGBA presentation. Наличие TCP само по себе не означает картинку | Bridge ждёт файл showUI-ready, затем config + IDR, включает takeover и начинает писать TS вместо OEM |
| Передача | Annex-B → TCP :19831 → decode queue → OMX → EGL/Screen 99 | AVCC ring → Annex-B/PES/TS → штатный RingIO; OEM producer продолжает работу, его writes уходят в discard |
| Runtime controls | Очередь команд с generation; ACK проверяется; Full/Sport/Classic, zoom, night, UI context | В данной SO runtime viewArea/HID из условного C не включены; есть showUI/ForceKeyFrame и файловая обратная связь |
| Потеря downstream | Hook сохраняет AES-последовательность, закрывает WAIT_IDR gate; новое соединение начинает с SPS/PPS + безопасного IDR | Ring overrun снимает takeover и ждёт IDR. Queue trim сохраняет takeover и просит IDR, но не закрывает пропуск P-кадров |
| Частичный TEARDOWN | Останавливает alt только при наличии 111; 103/audio или другой stream сохраняет alt. Full teardown останавливает alt | Hook TEARDOWN отсутствует. Ждёт EOF/error/timeout TCP независимо от RTSP teardown |
| Повторный SETUP | Старые same-pointer slots помечаются stop/closing, новые получают monotonic owner; поздний worker не закрывает новый renderer transport | Новый listener создаёт/обнуляет тот же ring и увеличивает generation. Старый writer/cleanup не проверяет, что всё ещё владеет ring |
| Обычный конец | Stop/listener close, revoke command owner, асинхронный receiver exit/GC; EOF → decoder teardown, presenter остаётся | Receiver cleanup → active=0; bridge reset → очистка очереди/config/TS state и fast release; worker остаётся |
| Возврат приборки | Java 80/81 → rate 0 → context 74 → rate 30; снимается RGD takeover | Reset shared RingIO indices → takeover off; снова проходят OEM writes |

Исходники: [наш negotiation](../../hook/altscreen/altscreen_hook.c:726), [receiver/IDR](../../hook/altscreen/altscreen_hook.c:1332), [SETUP](../../hook/altscreen/altscreen_hook.c:1567), [TEARDOWN](../../hook/altscreen/altscreen_hook.c:1912), [command ownership](../../hook/altscreen/altscreen_hook.c:2113), [renderer transport](../../hook/altscreen/altscreen_av.c:65), [decode queue](../../altscreen_render/main.c:204), [decoder session end](../../altscreen_render/sink_omx_qnx.c:808); MST2 [SETUP](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:1924), [activation](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:1797), [takeover](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:1858), [reset](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:1938).

## 4. Java, навигация и владение экраном

У нас `AltScreenModule` публикует желаемое состояние, а **один постоянный worker** пишет в DisplayManager. `connected || videoAvailable` удерживает наш контекст; без активной навигационной presentation это 81, с ней — 80. Начальный переход из stock/unknown идёт через 72 с паузой 180 ms; между 80 и 81 переход прямой. Каждые 250 ms активного владения worker сверяет физический context и исправляет рассогласование. Исчезновение навигации удерживается 250 ms, чтобы не мигать между 80/81.

Java также проверяет тип приборки: явный G24 отключает AltScreen/RGD. Эта Java-проверка сама по себе не является общей отменой native AirPlay advertisement.

После Java stop временный `VIDEO_ON` может удержать map-only context ещё до 1500 ms, позволяя пережить TMDev/dio bounce. Если повторного activate нет, grace истекает и принудительно снимает этот fallback. **Однако VIDEO_OFF при всё ещё `connected=true` сам по себе не возвращает stock:** остаётся наш контекст с loader/восстановлением. Это намеренная модель владения на время подключения.

RGD takeover — отдельный механизм: модуль ждёт штатный navigation service и готовый BAP gate, затем подписывается на маршрут. Stop прекращает подписку/presentation и снимает gate. Возврат RGD и переключение video context связаны порядком модулей, но не являются одной native операцией.

MST2 NavActiveIgnore действует иначе: меняет штатную конкуренцию OEM/телефонной навигации, включая Android Auto. Например, `stopGuidance(6)` игнорируется; CarPlay navigation-active branch не останавливает OEM guidance. У `DSICarplayProxy.requestModeChange` фильтр может отбросить **весь** объединённый запрос, если один AppStateRequest имеет `appStateID==2 && state && speechMode==0`. Если там же запрошено изменение screen/audio resource, оно также не дойдёт до native. Наличие такого смешанного запроса в реальной сессии не проверялось.

Источники: [Java context worker](../../java_patch/com/luka/carplay/core/AltScreenModule.java:489), [video grace](../../java_patch/com/luka/carplay/core/AltScreenModule.java:449), [RGD lifecycle](../../java_patch/com/luka/carplay/rgd/RouteGuidance.java:310), [фильтр mode change](mst2_jars_decompiled/NavActiveIgnore/de/esolutions/fw/comm/dsi/carplay/impl/DSICarplayProxy.java:111), [возврат HMI](mst2_jars_decompiled/MST2_H29_EU_HMI_Return_v10/de/vw/mib/asl/internal/carplay/target/ASLHandler.java:222).

## 5. Матрица отказов и переподключения

| Сценарий | Наше поведение | Поведение MST2 |
|---|---|---|
| Нормальный полный disconnect | Media teardown/EOF + Java deactivate; decoder сбрасывается, renderer остаётся; context возвращается отдельно | TCP cleanup ставит active=0; bridge освобождает OEM, JAR возвращает главный canvas |
| Перезапуск только AirPlay при живом iAP2 | RGD identify state сохраняется, 111 получает новый transport generation | TCP/listener/ring должны пересоздаться; собственного общего iAP2 reset hook в пакете нет |
| Поздний callback предыдущей сессии | Generation проверяется в очереди команд и ACK; renderer close только владельцем | Старый showUI ACK способен создать ready-файл уже для другого ring generation |
| Падение dio / SAL | ОС закрывает dio→renderer TCP; renderer получает EOF и заканчивает decoder-сессию. Возврат context зависит также от Java/TMDev | SAL не исполняет cleanup; shared file может остаться active=1. Bridge не проверяет живой producer и может продолжать takeover |
| Обычная длительная тишина video | Тишина не превращается в EOF; receiver ждёт stop/сокет, картинка может оставаться статичной | 10 s без readable data завершает receiver, даже если TCP ещё открыт |
| Падение renderer / DispMg | Monitor перезапускает renderer; hook при следующих кадрах восстанавливает link через SPS/PPS/IDR | Восстановление DispMg зависит от штатного supervisor. Новый bridge после RingIO ctor читает оставшийся ring и ждёт config/IDR; отдельного package supervisor нет |
| Ошибка декодера/presenter | Output recovery; после трёх неудач эскалация к decoder/presenter restart; тяжёлый hang завершает отдельный процесс | Собственного decoder нет; ошибка bridge RingIO write переводит в fail-open и пропускает OEM |
| Downstream переполнен | Ограниченная decode queue отвергает кадр и закрывает link; hook входит в WAIT_IDR | Queue trim выбрасывает TS и запрашивает IDR; пропускает следующие P-кадры, поэтому восстановление не гарантировано до IDR |
| Read error/magic mismatch у общего ring | Нет такого file-based канала | Ветка читателя закрывает/повторно открывает файл или ждёт; сама по себе не снимает takeover в этих ранних ветках |
| Нет новых кадров после recovery | Часть повторов IDR/decoder recovery запускается приходом кадров; нет обещания восстановления по одному таймеру | Force-IDR marker также проверяется на приёме video; периодический IDR считается по кадрам, не по независимому heartbeat |

## 6. Существенные пробелы MST2

1. **Осиротевшее active-состояние после смерти SAL.** Последовательность по коду: SAL пишет active=1 → процесс падает без cleanup → bridge видит те же magic/generation/active → очередь заканчивается → `fill_output_block()` продолжает PAT/PMT/PCR/null TS → takeover остаётся включённым. `source_session_active()` перечитывает флаг, но не heartbeat/PID. Если RingIO продолжает принимать, write timeout не сработает. OEM может оставаться подавленным до нового ring generation/другого recovery. [Проверка источника](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:2010), [пустая очередь](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:2048).
2. **Общее generation не защищает producer.** Новый receiver обнуляет один и тот же ring; прежний receiver продолжает писать по тому же mapping. Его последующий cleanup безусловно ставит active=0 и удаляет глобальные markers. Delayed showUI и completion удерживают CF session, но не проверяют её актуальность. Retain защищает время жизни объекта, а не право управлять новой сессией. [Создание ring](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:881), [cleanup](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:1268).
3. **RTSP завершение не управляет receiver.** Нет hook TEARDOWN; до accept старый listener может удерживать фиксированный порт до 180 s. После accept отключение определяется сетевым read, включая 10 s silence timeout. Fast release ускоряет действие после обнаружения конца, но не его обнаружение.
4. **После latency trim нет WAIT_IDR.** Takeover остаётся true, следовательно условие отбрасывания non-IDR уже не действует. Более того, текущий кадр в этой ветке тоже выбрасывается (`free; return`), несмотря на комментарий об enqueue. Возможны повреждённые зависимости P-кадров до следующего IDR. [Ветка trim](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:1884).
5. **Fast release не полностью синхронизирован со штатным RingIO.** Проверяет pending операции и индексы, затем напрямую обнуляет read/write indices. Между проверкой и записью нет общей блокировки с consumer; комментарий требует остановленного target, но fast path перед reset не вызывает его stop. Следовательно, проверка pending сама по себе не доказывает отсутствие гонки. При неудаче reset takeover всё равно снимается — OEM разрешается, но чистота оставшегося TS не гарантируется. [Reset](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:1020), [fast release](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:1226).
6. **Начало сессии частично нетранзакционное.** Listener стартует до построения response; при последующей ошибке CF response не видна отмена созданной задачи. При невозможности derive AES setup не обязательно отклоняется. Перенос streams в stock response заменяет этот ключ; вместе с проверкой только streams[0] это требует отдельной проверки смешанных SETUP. Входящий body допускает 8 MiB, но ring отбрасывает сообщения больше 256 KiB.

## 7. Ограничения нашей реализации

1. **Изоляция renderer не изолирует весь CarPlay control.** Некорректный advertised contract или ошибка собственного 111 SETUP может сорвать общее подключение: `alt_on_setup_response` возвращает ошибку наверх. Политика preflight уменьшает этот риск, но не превращает altScreen в полностью независимую опцию.
2. **Тайм-ауты не являются общим deadline всей операции.** TEARDOWN до 2500 ms ждёт выполняющийся SETUP, потом откладывает очистку; команды отправляются отдельным worker без удержания alt_lock, но сам stock SendCommand может зависнуть. Shutdown ждёт worker 750 ms и затем рассчитывает на выход процесса. Java DSI calls также не имеют здесь жёсткого общего deadline.
3. **Есть условие исчерпания receiver slots.** Всего четыре слота; GC собирает только завершившиеся threads. Same-pointer reconnect ставит старому stop. Но при другом session pointer без корректного TEARDOWN старый receiver может остаться в молчащем `read_exact`: ownership проверяется снаружи read, а внутри poll проверяется только stop. Pending accept также проверяет stop, не смену владельца. Несколько таких старых сессий способны занять слоты; следующий SETUP корректно откажет, но автоматическое освобождение не гарантировано. [Read loop](../../hook/altscreen/altscreen_hook.c:1216), [accept](../../hook/altscreen/altscreen_hook.c:1501), [slot retirement](../../hook/altscreen/altscreen_hook.c:1567).
4. **Supervisor не проверяет всю pipeline.** В обычном цикле проверяет PID; проверка :19831 при adoption доказывает наличие LISTEN, а не декодирование. Decoder watchdog с порогом 20 s проверяется при новых кадрах/очереди, а не отдельным независимым watchdog-таймером. Молчащий зависший процесс может не попасть под этот критерий. Внутреннее EGL/OMX recovery покрывает другие обнаруживаемые ошибки.
5. **У shell monitor нет строгого generation fence.** OWNER_FILE записывается, но monitor его не проверяет; используется существование `/proc/<dio_pid>` и общие PID/STOP-файлы. При перекрытии startup/restart monitors и редком PID reuse нет такой же строгой защиты, как в native/Java. Cleanup сознательно не пишет global stop, что устраняет один конкретный поздний kill, но не доказывает невозможность всех гонок adoption.
6. **Java допускает краткий устаревший physical switch.** Намерение проверяется после bounce, но может измениться перед DSI IPC. Единственный writer исправит контекст следующим проходом; абсолютной атомарности с событиями stop/start нет. Grace 1500 ms действует после Java stop; если TMDev вообще не опубликовал потерю CarPlay identity, одного VIDEO_OFF недостаточно для возврата stock.
7. **WAIT_IDR не покрывает каждый внутренний restart decoder.** Hook ставит этот барьер при старте/потере TCP link. Но `av_sink_frame()` при внутреннем OMX error может пересоздать decoder с сохранением того же TCP, повторить SPS/PPS и передать текущий non-config frame без проверки, что это IDR. В этом пути не виден отдельный запрос IDR. Если текущий кадр — P, изображение может ждать естественного IDR; output-only EGL recovery, сохраняющее decoder, этого условия не создаёт. [OMX restart](../../altscreen_render/sink_omx_qnx.c:728).
8. **Alt shutdown рассчитан на process exit, а не hot unload.** `altscreen_shutdown()` останавливает command worker/renderer link, но не перебирает все receiver slots с stop/join. Обычная сессия должна пройти `on_teardown`, а при смерти процесса ресурсы заберёт ОС. Безопасная выгрузка SO в продолжающем работать процессе этим кодом не установлена. [Shutdown](../../hook/altscreen/altscreen_hook.c:2616).

## 8. Временные границы: что именно они означают

| Константа/пауза | Значение и граница |
|---|---|
| Наш SI startup/stop/watchdog | 10 s / 8 s / 60 s, конфигурация штатного владельца процесса |
| Наш receiver stop | Проверка каждые 60 ms в read или каждую 1 s в accept; не общий deadline при произвольном зависшем stock call |
| Наш renderer transport | Connect 250 ms; send stall limit 1000 ms на send loop, не на весь lifecycle |
| Наш WAIT_IDR | Не чаще 2 s, максимум 6 запросов по этому счётчику; повторы проверяются при поступлении video |
| Наш Java disconnect | Debounce 400 ms; возможный video grace 1500 ms после stop; затем асинхронный DSI return. Складывать это в гарантированное время возврата нельзя |
| Наш context | Bounce 180 ms только stock/unknown → alt; reconcile 250 ms во время владения; nav-hide hold 250 ms |
| Наш decoder recovery | До 3 неудачных запусков, затем cooldown 30 s; retry на последующих кадрах. Работавшая сессия сбрасывает бюджет, неуспешная сохраняет guard |
| MST2 activation/read | showUI через 1 s после SETUP 110; accept до 180 s; readable timeout 10 s |
| MST2 ring output | 12.288 Mbit/s; блок 64×188 bytes примерно каждые 7.834 ms; RingIO write timeout 100 ms |
| MST2 latency trim | Порог 4096 TS packets ≈ 0.501 s при указанном bitrate. Это размер backlog, не измеренная end-to-end latency |
| MST2 periodic IDR | Каждые 120 кадров; ≈3 s только при реальных 40 fps, существенно дольше при редких кадрах |

## 9. Что подтверждено и что следует переносить

Готовый probe SHA-256 `5c59c6bb6dd57f22a482163318bd4b1c42a9f5833279168438b3d01fefdbed00`; bridge `3787f493cbed341b29f519b0fca7dfeaa4b274b5d69a7e5dc3e6a43cbd3b2b36`. В ELF подтверждены 40 fps/геометрия, AES offset, listener 7001, session rearm, отсутствие TEARDOWN, fast release и queue trim. Дополнительно в этом проходе подтверждены stock-first SETUP (`blx` 0x42dc) и merge stock response (`get_object`/`set_object` 0x4778/0x478c). Нельзя выводить build flags только из дефолтных `#define` приложенного C.

Оба JAR уже декомпилированы двумя декомпиляторами, существенные методы проверены по bytecode: [файлы и разбор](mst2_jars_decompiled/README.md). Они дополняют возврат главного HMI и правила navigation, но не заменяют native lifecycle приборки.

Для дальнейшего использования MST2 полезен его **прямой H.264→MPEG-TS backend**, если будет подтверждён соответствующий транспорт MHI2Q. Lifecycle целиком переносить из этого пакета не стоит: следует сохранить наши границы iAP2/AirPlay/Java, явный TEARDOWN, ownership generation, согласование display contract и IDR barrier. Если дорабатывать сам MST2, первыми нужны привязка producer/markers к владельцу, обнаружение смерти SAL, TEARDOWN и WAIT_IDR после trim; затем синхронизация RingIO handback.

Код установки, C/Java и исходные SO/JAR не изменялись. Это статическая проверка; сборка, запуск на HU, кабельные reconnect/crash-тесты и замеры задержки не выполнялись.
