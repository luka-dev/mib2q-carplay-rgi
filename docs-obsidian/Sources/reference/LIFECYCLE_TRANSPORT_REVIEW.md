# Дополнительная проверка жизненного цикла и локальных протоколов

Дата: 2026-09-11. База: `6fe1aa0` с исправлениями предыдущих проходов
[CatPlay](CATPLAY_REVIEW_FIXES.md) и [Showcase/AirPlay2](SHOWCASE_AIRPLAY2_REVIEW.md).
Изменения находятся в рабочем дереве. Развёртывания на ГУ не выполнялось.

## Найдено и исправлено

| Область / триггер | Ошибка | Исправление |
| --- | --- | --- |
| Java bus и сервер манёвров: stop → start до завершения старых потоков | Общий `running` снова становился true. Старые accept/reader/writer могли продолжить работу, занять порт, забрать новую очередь; stop мог обнулить ссылки на уже новые потоки. | Отдельное поколение запуска, проверка владения до публикации сокета/извлечения команды; stop атомарно забирает ссылки только на свои потоки. Bind bus сериализован с остановкой. |
| Bus: чтение старого пакета заканчивается после замены соединения | Уже прочитанный payload мог попасть в listener нового сеанса. Старый producer также мог вытеснить новую команду из очереди. | Проверка поколения запуска и конкретного сокета непосредственно перед dispatch; callbacks сериализованы между reader-потоками. Enqueue повторно проверяет connection generation, sequence присваивается в порядке постановки в очередь. Добавлены проверки длины payload/type/flags. |
| AltScreen control: старый EOF/close завершается после нового VIDEO_ON | Старое отключение публиковало VIDEO_OFF уже после показа нового видео. | Проверка владельца и упорядочивание callbacks после закрытия старого сокета; accept не держит блокировку во время HMI callback. |
| RGD bus: пустое, нечисловое или слишком большое значение в числовом списке | `out[idx++] = parseInt(...)` увеличивал индекс до исключения; catch увеличивал его второй раз. Получалось `ArrayIndexOutOfBoundsException`, обработка delta прерывалась. | Отдельная локальная переменная для разбора и ровно одно увеличение индекса. Невалидное значение остаётся 0 на своей позиции. |
| Maneuver renderer: CLEAR, MANEUVER, CLEAR с задержанными ACK | Один boolean разрешал FRAME_READY после подтверждения лишь первого CLEAR. При переполнении очереди CLEAR мог вообще потеряться, оставив ожидание несуществующего ACK. | Счётчик принятых CLEAR до их ACK; очередь сохраняет все CLEAR и порядок, вытесняя только обычные команды. Полная очередь из одних CLEAR возвращает false, не добавляя ложное ожидание ACK. |
| Native RGD 0x5201/0x5202/0x5204: правильное начало, повреждённый TLV далее | Парсер оставлял разобранное начало; обработчик применял частичный delta к маршруту/манёврам/полосам и прекращал ожидание первого ответа. | До разбора проверяются header, длина всего сообщения, каждый TLV и вложенные LaneInformations. Парсеры возвращают false при повреждении; обработчик ничего не публикует и не меняет debounce/first-response по такому сообщению. Штатная обработка iAP2 не блокируется. |
| Supervisor: смена владельца, смерть dio, stop или запуск renderer другим monitor во время backoff | После `sleep 5` monitor безусловно запускал ещё один renderer. Старый monitor мог продолжать наблюдение и менять USB recovery state новой генерации. | Проверки owner PID, жизни dio и stop marker до действий и после ожидания; повторная проверка PID renderer. Потерявший владение monitor выходит и не записывает результат USB recovery. |
| Supervisor: новый renderer появился во время трёхсекундной остановки dio | PID-файлы читались только после ожидания и могли уже содержать PID замены. | PID renderer фиксируются при получении stop, до ожидания; остановка использует сохранённые PID. |

Код: `java_patch/com/luka/carplay/{bus/CarplayBus.java,rgd/RendererServer.java,core/AltscreenControlServer.java}`,
`hook/routeguidance/rgd_{tlv.c,tlv.h,hook.c}`,
`deploy/smartphone_integrator/carplay_startup.sh`.

## Воспроизведение и проверки

Все перечисленные ниже проверки выполнены успешно на итоговом коде:

- `bash scripts/test_java_transports.sh`: реальные Java transport-классы,
  локальные сокеты на автоматически выбранных портах, 30 start/stop bus и
  30 connect/dispose сервера манёвров; фрагментация сообщения по одному байту,
  задержанное чтение старого сокета, старый writer, две CLEAR/ACK,
  переполнение обычной и barrier-only очереди, задержанный AltScreen disconnect.
  У AltScreen заменён только callback в HMI.
- `bash scripts/test_route_info.sh`: 1363 проверки BAP-представления,
  направление стрелок и новый `RouteGuidanceDeltaTest` реального Java-парсера:
  metadata-only update, transient zero, reroute, явно пустой список,
  source_supports_rg=0 → 1, отсутствие воскрешения очищенного манёвра,
  невалидный числовой список. Парсер тестируется без запуска HMI и bus.
- `tests/rgd_tlv_test.c`: воспроизведение обрезанных внешних/вложенных TLV,
  неизвестные расширения, различие отсутствующего и пустого списка,
  120000 детерминированных случайных/повреждённых входов. AddressSanitizer/UBSan
  пройдены при `RGD_TRACE_RAW_FULL=0` и `=1`. Clang Static Analyzer без замечаний.
- `sh scripts/test_supervisor_lifecycle.sh`: выполняет функции из настоящего
  startup-скрипта с подставными process probes и renderer; проверяет смену owner,
  смерть dio, stop и чужой renderer во время backoff, отказ устаревшего monitor,
  снимок PID до stop grace. Проверка синтаксиса `/bin/sh` также пройдена.
- `sh scripts/run_tests.sh`: полный host-набор, включая audio/media-route,
  новые RGD/supervisor и предыдущие coverart/video/bus регрессии. APAC:
  3980 аутентифицированных кадров, 3977 пакетов совпали с принятым эталоном
  побайтно, 64 декодированы и проверены на реальном захвате.
- `SANITIZE=address,undefined sh scripts/test_audio_interposers.sh`:
  QSA rollback/partial-error, 64 гонки SETUP/teardown, 32 stale SETUP,
  rate/flush при смене поколения, ожидание PCM/QSA, области полного и выборочного
  TEARDOWN. Новых дефектов в проверенных аудиосценариях этот проход не выявил.
  `media_route_test` дополнительно покрывает перекрытие music/phone/Siri и
  отложенный возврат media после освобождения stock-потоков.
- `sh scripts/test_usb_replug_recovery.sh` и `sh scripts/test_usb_replug_bundle.sh`:
  поколения USB и mock install/rollback прошли; реальные интерфейсы не менялись.
- `python3 scripts/check_local_protocols.py`: совпали 31 константа Java/C
  (порты, opcodes, flags, размеры, лимиты); все 103 класса итогового JAR имеют
  class-file version 48.0 (Java 1.4). Wire-поля bus и 48-байтовой команды манёвра,
  включая отрицательные BE16-углы, дополнительно проверены transport-тестами.
- `LOG=0 bash scripts/build_hook.sh`: QNX ARM-сборка хука и APAC прошла, 26
  разрешённых interpose-экспортов, ABI v1 и проверки emutls/runtime сохранены.
  Java JAR пересобран под stock MU1316. Исходники renderer в этом проходе
  не менялись; их QNX-сборка проверена предыдущим проходом.
- `git diff --check`: без ошибок.

Одинаковые Java reproducer на исходном и исправленном коде показали:

```text
До:    intList FAILED: java.lang.ArrayIndexOutOfBoundsException: 6
После: intList length=6 second=2
До:    frameReady after first of two CLEAR acknowledgments=true
После: frameReady after first of two CLEAR acknowledgments=false
```

Тот же AltScreen-тест на исходном коде падает с `old disconnect hid new video`,
на исправленном проходит. RGD-тест до исправления падает на первой проверке
частично применённого active-state. Supervisor-тест до исправления фиксирует
запуск после смены owner во время backoff.

Логи, исходные версии для воспроизведения и SHA-256 итоговых артефактов:
`/Users/luka/Desktop/AUDI_2/LocalLogs/reviews/mhi2-lifecycle-2026-09-11/`.

## Границы проверки

Совпадение констант и успешная сборка подтверждают согласованность локальных
исходников/артефактов, но не версии файлов, уже установленных на ГУ. Автоматического
согласования версии wire-протоколов этот проход не добавляет.

Host-тесты проверяют порядок вызовов и состояние при подставных QSA/OMX/HMI;
они не заменяют выполнение на Qualcomm/QNX. На ГУ остаются проверка быстрого
replug, stop/restart renderer, переходов music → Siri/phone → music,
смены приложения навигации и отсутствия зависших потоков/лишних EGL-клиентов.
Проверки процессов используют определённые места смены поколений и не являются
доказательством отсутствия всех возможных межпроцессных гонок.

Различия scheduling по timestamps, отмеченные в предыдущем отчёте,
остаются отдельной задачей с захватом arrival/PTS/presentation на ГУ.
Изменения в UPMIX V15 и обучающих скриптах, появившиеся независимо во время
работы, в этот аудит и исправления не включены.
