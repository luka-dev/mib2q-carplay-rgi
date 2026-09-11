# Сравнение altscreen MHI2Q и MST2 rev1/rev2

Дата: 2026-09-10. Статическое сравнение исходников, состава пакета и ARM ELF.

Дополнен [полным сравнением общего и altScreen lifecycle](altscreen_lifecycle_comparison_2026-09-10.md): фактическая жизнь renderer между запусками dio, Java lifecycle, восстановление, ошибки и возврат OEM. В готовом rev1 дополнительно подтверждены stock-first SETUP и merge response.

Текущий репозиторий: `/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay`, ветка `master`, HEAD `44c772a`. Каталоги `hook/altscreen`, `altscreen_render` и `java_patch` без локальных изменений. Остальные незакоммиченные изменения репозитория не относятся к этому сравнению.

Второй объект: `/Users/luka/Downloads/rev1-rev2-withsource-edited-work`. Это пакет исходников и готовых файлов для MST2 H29 EU; истории Git, скриптов сборки и полного набора исходников Java в нём нет. Названия C-файлов и часть комментариев сохранили P0478/CN и раннее обозначение stage-1. Готовая библиотека уже содержит приём, AES и передачу видео, поэтому stage-1 в строке загрузки не описывает её фактическую функциональность.

## Главный результат

Оба решения реализуют CarPlay altScreen/type 111, но имеют разные пути вывода. Это не взаимозаменяемые версии одной библиотеки.

```text
MHI2Q:
iPhone H.264 → hook: TCP/AES/AVCC→Annex-B
            → TCP localhost:19831 → altscreen_render
            → Qualcomm OMX decode → EGL/GLES → RGBA Screen displayable 99
            → штатный путь cluster capture/encoder → приборка

MST2:
iPhone H.264 → probe в SAL: TCP/AES
            → /dev/shmem/carplay-alt-ring.bin (AVCC/config/timestamp)
            → bridge в DisplayManager: Annex-B/PES/MPEG-TS
            → штатный RingIO → MOST → приборка
```

MST2 не декодирует карту на HU: bridge перепаковывает сжатый поток. Это исключает дополнительное декодирование/отрисовку/кодирование именно карты и потенциально уменьшает задержку. При этом штатный OEM producer продолжает работать, а его записи при takeover направляются в discard buffer. Поэтому из отсутствия собственного декодера нельзя вывести конкретную экономию общего CPU или гарантированную задержку. Измерений в этой проверке не было.

Основания: [архитектура MHI2Q](../../docs/ARCHITECTURE.md), [OMX sink](../../altscreen_render/sink_omx_qnx.c), [bridge: AVCC→Annex-B, PES, очередь и RingIO](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:1726).

## Сопоставление

| Свойство | Текущий MHI2Q | Пакет MST2 |
|---|---|---|
| Размещение hook | Общий `libcarplay_hook.so` в `dio_manager`; AirPlay seams делятся между модулями | Отдельные preload `.so` в SAL и DisplayManager |
| Формат после приёма | Annex-B по localhost TCP | AVCC + конфигурация + timestamp в кольце из 64 слотов по 256 KiB |
| Вывод | OMX → EGL/GLES → QNX Screen | MPEG-TS → RingIO/MOST, без собственного декодера |
| Объявленная геометрия | 1440×542, maxFPS=30, физический размер 292×110 мм | **В готовом ELF:** 1010×376, maxFPS=40, 202×75 мм |
| Safe areas | Full: (270,90) 900×362; Sport: (90,135) 360×317; Classic: (540,135) 360×317 | **В готовом ELF:** одна область (122,16) 606×344; visible frame 1010×376 |
| Переключение layout | `updateViewArea`, Java/BAP-состояние, ACK с номером поколения | Опциональные второй layout и файловый runtime control есть в C, но в данном ELF не включены |
| Zoom/ночной режим | `changeMapZoomLevel` с UUID приборки; перехват NightMode и команда приборке | В probe нет аналога текущего semantic zoom/night-mode пути; экспериментальный HID присутствует только как условный исходный код |
| Состав `/info` | `extendedFeatures`, `displays`, `displayPanels`; модернизация main; uid/uuid, initialVideoStreams, properties | Дополняет существующий `displays`, более короткая схема; не строит аналогичный `displayPanels` |
| `enabledFeatures` | Сохраняет существующие значения и добавляет altScreen/viewAreas после preflight экрана | Заменяет массив на `[iAPChannel, viewAreas, altScreen]` |
| `sourceVersion` | AltScreen-модуль не подменяет | Готовый probe подменяет на `950.7.1` |
| Активация UI | Первое IDR → showUI/nightMode/viewArea; showUI ACK → запрос IDR | Успешный SETUP 110 → задержка 1 сек → showUI; ACK → marker и запрос IDR |
| Переподключение | Поколения для receiver, транспорта, команд и ACK; явный TearDown для 111; чужой partial teardown сохраняет карту | Поколение общего кольца и session_active; завершение по TCP; session rearm не переиспользует старый listener/key |
| Возврат OEM | Java управляет штатными и CarPlay contexts | Fast release: очистка индексов RingIO и снятие подавления OEM writes |
| Интеграция приборки | Контексты 80/81, displayable 99, дополнительные слои RGI, startup loader | Перехват видеопотока; комплектные Java JAR патчат штатные VW-классы |
| Воспроизводимость | C/Java исходники, build scripts, Makefile/hostcheck, документы | 2 C-файла, protocol header, готовые SO/JAR и green-menu scripts; команды/defines сборки не приложены |

Исходники MHI2Q: [display declaration](../../hook/altscreen/altscreen_hook.c:407), [SETUP](../../hook/altscreen/altscreen_hook.c:1802), [TearDown](../../hook/altscreen/altscreen_hook.c:1912), [viewArea](../../hook/altscreen/altscreen_hook.c:2255), [zoom](../../hook/altscreen/altscreen_hook.c:2462), [Java](../../java_patch/com/luka/carplay/core/AltScreenModule.java:42).

Исходники MST2: [display declaration](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:482), [enabledFeatures](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:768), [activation](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:1797), [SETUP](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:1924), [deployment configuration](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/package/custom/greenmenu/scripts/as_common.sh:184).

## Что подтверждено в готовых SO

Все три SO — ELF32, little-endian ARM/EABI5, не stripped. Адреса ниже — VA относительно базы ELF 0; для перечисленных инструкций `.text` они совпадают с файловыми смещениями.

Probe SHA-256:

`5c59c6bb6dd57f22a482163318bd4b1c42a9f5833279168438b3d01fefdbed00`

В `AirPlayCopyServerInfo` (`0x23a8`) подтверждены:

| Адрес | Значение |
|---|---|
| `0x25d0` | maxFPS=40 |
| `0x25f0`, `0x2610` | width=1010, height=376 |
| `0x2630`, `0x2650` | physical=202×75 |
| `0x26e4`…`0x2708` | visible=(0,0) 1010×376; safe=(122,16) 606×344 |
| `0x2478`…`0x2494` | вызов set_cstring для sourceVersion=`950.7.1` |

`AirPlayReceiverSessionSetup`: `0x4408` использует session+`0x1b8` для AES key; `0x45c8` задаёт network-order порт **7001**. В MHI2Q ключ расположен по `0x1c8`, preferred listener port — **6031**. Совпадение версии AirPlay 210.81 не означает совпадение ABI/layout.

Экспортированные AirPlay interception points probe: CopyServerInfo, SessionSetup, SessionStart. Перехвата SessionTearDown нет. В ELF присутствует путь AES-CTR/forward ring, `alt-listener-closed-for-next-setup`; runtime `updateViewArea` и virtual HID из условного исходного кода не входят в эту сборку.

Bridge SHA-256:

`3787f493cbed341b29f519b0fca7dfeaa4b274b5d69a7e5dc3e6a43cbd3b2b36`

Корневой `bridge-rev2.so` и `package/custom/java/libmst2-h29-eu-ringio-bridge-rev2.so` **побайтно идентичны**. В bridge включены fast-release и latency-trim; `fast_session_release` находится по `0x2334`, `reset_session_state` — `0x248c`, worker — `0x2830`. Экспортируются RingIO ctor, writeAquire, writeRelease.

Это проверка конкретных механизмов/параметров ELF, а не доказательство полной воспроизводимости SO из приложенных C: исходные команды сборки отсутствуют.

## Существенные ограничения MST2, видимые в коде

1. **После latency-trim нет ожидания нового IDR.** [process_video_frame](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_ringio_bridge.c:1884) очищает накопленные TS, оставляет takeover активным и создаёт запрос ключевого кадра. Следующие P-кадры снова допускаются в очередь до прихода IDR. Их зависимости могли быть выброшены, поэтому возможны артефакты/зависание до нового IDR. Комментарий о постановке текущего кадра в очередь не соответствует коду: ветка заканчивается `free(annexb); return 1`. У MHI2Q при потере renderer link есть [WAIT_IDR barrier](../../hook/altscreen/altscreen_hook.c:1448) с повторным SPS/PPS перед восстановлением.

2. **10 секунд без данных трактуются как разрыв.** [read_exact](/Users/luka/Downloads/rev1-rev2-withsource-edited-work/mst2_p0478_altscreen_probe.c:1013) завершает receiver после тайм-аута select. Это потенциальный ложный disconnect при молчащем, но ещё открытом потоке. MHI2Q проверяет stop/EOF короткими poll и не назначает обычному отсутствию данных такой тайм-аут. Отдельно, fast-release bridge ускоряет возврат уже после обнаружения завершения, но не сокращает время самого обнаружения.

3. **Нет единого владельца поколения на стороне производителя и команд.** Кольцо имеет generation, но каждый listener пишет в общий mapped ring и cleanup безусловно ставит session_active=0/удаляет глобальные marker-файлы. Старые detached callbacks не проверяют текущее поколение. При перекрывающихся сессиях возможна порча состояния новой сессии. MHI2Q явно отбрасывает действия/ACK старого поколения. Это вывод по конкурентным путям исходника, не воспроизведённый дефект на автомобиле.

4. **Ограничения кадров и AVCC.** Размер одного forwarded сообщения ограничен 256 KiB, хотя TCP reader принимает до 8 MiB; большие кадры отбрасываются. `inspect_h264_avcc_frame` в probe всегда читает 4-байтовые NAL lengths, хотя bridge умеет читать length_size из avcC. MHI2Q использует полученный length_size также при определении IDR и принимает body до 4 MiB. При обычном 4-байтовом AVCC различие не проявится.

5. **Зависимость от конкретной прошивки.** MST2 bridge использует TSD RingIO symbols и внутренний layout RingIO; в условных ветках находятся абсолютные адреса P0478. Перенос на MHI2Q требует отдельного backend и подтверждения интерфейса штатного видеотранспорта, а не замены `.so` или одного AES offset. Аналогично Qualcomm OMX backend MHI2Q нельзя прямо перенести на MST2.

Это не исчерпывающий аудит безопасности или всех ошибок обеих реализаций. Сборки на HU, фактическая плавность, задержка и возврат OEM в этой работе не проверялись.

## Практический вывод

Для текущего Audi MHI2Q текущая реализация лучше соответствует существующим layout, Java/BAP, RGI и lifecycle. Главное полезное направление из MST2 — отдельный прямой H.264→MPEG-TS транспорт вместо рендеринга карты на HU. Его имеет смысл рассматривать как самостоятельный backend с сохранением текущих negotiation, управления экраном и защиты поколений. Готовый MST2-пакет не является обновлением, которое можно просто поставить поверх MHI2Q.

Дополнение: оба комплектных JAR декомпилированы CFR и Vineflower, все 9 классов проверены по составу, ключевые методы сверены с javap. В HMI_Return_v10 обнаружен вызов выхода из главного canvas при disconnect до сброса connectionState; NavActiveIgnore подавляет несколько путей навигационного арбитража и может отбрасывать целый requestModeChange. [Java-код и подробный разбор](mst2_jars_decompiled/README.md).

Исходники, библиотеки и конфигурации установки не изменялись. Созданы отчёты и файлы декомпиляции.
