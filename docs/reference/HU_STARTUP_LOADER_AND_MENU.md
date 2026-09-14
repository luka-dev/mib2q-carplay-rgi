# HU loader, cursor plane и пункт Apple CarPlay

Аудит обновлён 2026-09-14. Реализация `7fce1c4` объединена с PRISM и RGI
в основной `master` коммитом `7b6cc45`. Рабочая папка:
`/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay`. Проверки выполнены на хосте;
на автомобиль не установлено.

## Поведение

1. `CarPlayApp.onActivate()` начинает попытку с номером lifecycle generation.
   Штатный STARTED сразу сохраняет свой значок: готовность стока не ждёт loader.
2. `NativeCursorOverlay` отправляет loader в отдельный `cursor_render` на plane 103.
   Окно сначала скрыто и прозрачно; после native DM adoption процесс публикует
   первый кадр. Только успешный ACK с ещё действующей native lease разрешает следующий шаг.
3. В одном проходе HMI dispatcher `HuStartup` включает presentation identity
   Apple CarPlay через `HuMenuIdentity` и запрашивает штатный переход model 4095.
   На A5 контекст 78 содержит `{16,103,59}`; G24 сохраняет штатный `{16,59}`. Если экран 3200000 уже открыт, повторного
   запроса нет. Подтверждение контекста отслеживается отдельно.
4. Хук наблюдает успешный `CScreenRender::render` текущей конфигурации — то есть
   успешный `screen_post_window`. После подтверждения контекста и этого события
   native PRISM раскрывает видео через анимированную форму Apple. Таймер и завершение
   перехода принадлежат `cursor_render`; Java получает LOADER_DONE. Plane 103 затем обслуживает курсор.
5. При отключении попытка инвалидируется, menu возвращается в generic, транспорт
   закрывается и окно удаляется. При выходе из уже показанного CarPlay loader
   прекращается и не пытается снова захватить экран. Значок остаётся до отключения.

Это согласование порядка операций, не обещание атомарной смены пикселей разных
процессов на одном физическом vblank. ACK означает успешный post; реальное
наложение и timing на HU требуют проверки.

## Почему публикация значка предшествует подтверждению контекста

Модель 4239 используется не только MainWizard: штатный `TerminalModeSMM.checkGuard`
(например case 3200000, guard 0) при 4239=0 и 4384=0 выбирает ветку «нет подключения».
Ожидание готового контекста при удержании 4239=0 создаёт циклическую зависимость.
Поэтому готовый кадр 103 -> публикация identity -> запрос штатного HMI screen
выполняются последовательно в одном dispatcher callback. Иконка может смениться
на несколько кадров раньше физического появления loader во время HMI-перехода.

При этом не подменяются DSI CONNECTIONSTATE_STARTED, TMDevice.ConnectionState,
SPIServiceState, владельцы аудиоресурсов или статус готовности stream 110.

## Перехват штатной логики

`java_patch/de/audi/app/terminalmode/device/TMDeviceControl.java` полностью
компилируется вместе с остальными патчами в Java 1.4. В `Active.updateMainWizardModel`
изменены только записи model 4239 и model 138: они проходят через `HuMenuIdentity`.
ASM-патчер удалён; `build_java.sh` не модифицирует готовые class-файлы.

Это также исправляет ошибку предыдущей реализации: `MU1316-combined-final.jar`
подготовлен для декомпиляции (восстановленные константы и inline private accessors),
поэтому брать оттуда готовый inner class для исполнения было неправильно.

Исходник повторно проверен по обновлённому MU1316-vf / MU1316-final.jar (14 сентября 2026): state classes используют явные
конструкторы с одним parent аргументом, как ожидает штатный `InnerClassInflator`.
Сохранены оба метода Consumer.accept — Object и ApplicationOwner. Артефакт JXE
`"isNativeLittleEndian"` заменён на `""`, согласно указанию пользователя; то же
правило применяется к написанию `"isNativeLittleEdian"` при переносе других файлов.

Host-тест сверяет полный набор 30 классов и их сигнатуры с MU1316-combined.jar,
16 state-конструкторов и 221 метод. После повторной компиляции восстановленного
штатного исходника весь код за пределами метода меню совпадает после удаления
debug-атрибутов. ASM используется только в тесте для чтения/проверки class-файлов.
Это проверка структуры и изоляции изменений, не полный прогон машины состояний
с настоящими DSI/HMI сервисами.

`MenuIdentityState` хранит штатное значение и presentation ownership раздельно.
После готовности loader штатный CarPlay reset не вызывает мерцание generic/CarPlay.
Поздний STARTED не воскрешает закончившуюся попытку, старый loader ACK не принимает
новая попытка. Android Auto / CarLife остаются штатными; старый выход CarPlay не
стирает новый пункт другой системы. Чужое выделение меню не перехватывается.
Model 4552 (tabbar) остаётся полностью штатной.

## Native протокол и наблюдение видео

Private cursor TCP :19840 использует **v2: 40-byte state / 32-byte ACK**.
Java передаёт координаты, события ввода, geometry, разрешённый HMI context,
video-ready и epoch. В пакете нет alpha или elapsed time. Полная таблица —
[HU_TOUCH_CURSOR_EXPERIMENT.md](HU_TOUCH_CURSOR_EXPERIMENT.md).

`cursor_render/engine.c` сам ведёт idle/fade курсора и PRISM/handoff, рисуя около
30 кадров/с независимо от входящих пакетов. Heartbeat не считается новым вводом.
Java опрашивает контекст/lease, но не анимацию; завершение loader приходит в ACK.
Проверки HMI/menu и наблюдение готовности видео остаются в Java/хуке.

Общий GPU код `common/prism_gl.c` используется HU и altScreen, с одинаковыми
baked shader/material/signature. HU всегда использует весь canvas. Курсор —
процедурное стеклянное кольцо ~74 px без текстуры/центральной точки, с цветной
каймой и нативной реакцией на направление движения; его входные координаты остаются
локальными к согласованному video viewport. Каждый GPU кадр сначала полностью
прозрачно очищается. При окончательном скрытии native удаляет окно. EGL context
и ассеты сохраняются между жестами; `eglTerminate` не вызывается.

`hook/hu_startup/main_video.c` добавляет два LD_PRELOAD seam:

- `_ZN3dio13CScreenRender6configERKNS_16st_screen_configE`;
- `_ZN3dio13CScreenRender6renderEPh`.

Оригинал вызывается с исходными аргументами, возвращается исходный результат.
Нет доступа к полям C++ объектов или содержимому OMX buffers, создания Screen
окон, установки opacity/visibility, GOT patching или дополнительного decoder.
Новый SETUP 110, reconfigure, Identify и teardown сбрасывают epoch/owner. Успешное
завершение старого render/config после смены epoch не помечает новую попытку готовой.
SETUP/TEARDOWN других типов потоков не сбрасывает основное видео. Ошибка текущего
SETUP сбрасывает наблюдение; поздний отказ старого SETUP не сбрасывает новый.

Java опрашивает статус каждые 200 ms во время загрузки и fade. В полёте один
запрос; потерянный ответ повторяется через 500 ms:

- CMD 0x0130: binary BE32 request nonce;
- EVT 0x0031: binary BE32 nonce, epoch, phase (0 absent / 1 starting / 2 posted).

Обе реальные таблицы bus dispatch имеют MAX_TYPES=0x0140: прежнее 0x0120
отбрасывало команду 0x0130 и в Java, и в C. Теперь границы проверяются тестами
настоящего транспорта и `check_local_protocols.py`.

Ответ не sticky, Java принимает только свой актуальный ожидающий nonce. Смена
epoch/phase отменяет старую готовность, в том числе во время fade. Источник истины
готовности видео — успешный post, а не SETUP, пакет H.264 или штатный READY.
Это не подтверждение физического scanout.

Бинарная база: MU1316 libairplay 210.81, ELF32 ARM LE, base VA 0,
SHA256 `01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2`.
Config: VA/file offset 0x8bcc8; render 0x8b5c0, post call 0x8b690.
Дизассемблирование: `output/r2/hu-startup-observer-20260913/render-config.txt`.

## Отказы и проверка

Без ACK, при native ошибке или если HMI не подтвердил контекст за 2.5 s после
запроса, loader освобождает показ штатному коду и возвращает сохранённую stock
identity. Без ответа observer более 1 s loader снимается; при живом observer
общий предел ожидания первого видео — 15 s. Потеря native lease тоже снимает
loader. Поздней DSI geometry даётся 750 ms. Повторного автоматического
входа в той же попытке нет. Это ограничивает влияние отсутствующего observer/renderer.

Проверено:

- Java 1.4, оба QNX ARM native artifacts и renderer builds;
- полная Java-замена: 30 классов / 16 state-конструкторов / 221 метод;
- 80 проверок ownership/ACK/context/native completion/disconnect/late events/fallback;
- 23 проверки настоящих HuCursor/NativeCursorOverlay: поколения сессий,
  запоздалые ACK/ошибки, loader не считается видимым курсором, lease/token;
- C observer: возвраты оригинала, first-post, selective setup/teardown,
  пересечения старых config/render с новой попыткой;
- native engine/session под ASan/UBSan + GPU: graceful/force hide, motion settling, premultiplied alpha и оба back buffers,
  loader -> cursor без следов; настоящий локальный TCP процесс проверяет mode 3,
  истечение lease, disconnect, adoption и watchdog;
- регрессии Java bus/renderer/altScreen transports и курсора (29 input/click checks);
- installer/uninstaller включает cursor_render; supervisor использует netstat
  для проверки listener и не вытесняет Java TCP-подключение health-пробой;
- LONGPRESSED остаётся в маршруте исходного touch down/up, не уходит второй
  штатной кнопкой. Проверка foreground выполняется и непосредственно при клике.
  Таймер lease в Java использует монотонное время framework.

Для проверки на HU нужны согласованные новый hook, JAR, cursor_render и supervisor
scripts. Проверить холодное подключение до создания 59, переход меню/loader,
первый кадр/затухание, HOME/popups, отключение во время loader и быстрое повторное
подключение. Различение физического нажатия touchpad-панели и rotary остаётся
отдельным незавершённым hardware-вопросом старого cursor experiment.

## Что остаётся неподтверждённым на машине

Реальные Screen adoption, z-order/alpha plane 103, вход в 78 до появления 59,
стоимость GPU PRISM и освобождение окон после watchdog не проверяются host-моками.
Также требуется подтвердить LD_PRELOAD двух CScreenRender символов на QNX и
принятие iOS абсолютных input-1 taps с дополнительной capability low-fidelity touch.

Nonce защищает от старых ответов, epoch — от наблюдённых native reset/config.
Это не общий session ID между Java TMDevice и AirPlay: точный порядок Java activate,
Identify, SETUP/config на быстром переподключении ещё нужно проверить в HU trace.
Выход на другой screen до/после entry отменяет loader; HOME в тот же screen ID
неразличим без отдельного события пользовательского ввода.

Никакой установки, commit, push или merge в основной dirty worktree во время
этого аудита не было. Для HW-проверки использовать согласованный комплект из
этого экспериментального worktree, сохранив возможность штатного запуска.

Повторная сверка всех Java-замен с обновлённым эталоном: [JAVA_DECOMPILE_CROSSCHECK_2026_09_14.md](JAVA_DECOMPILE_CROSSCHECK_2026_09_14.md).
