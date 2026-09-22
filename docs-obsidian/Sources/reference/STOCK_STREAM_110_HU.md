# Стоковый stream 110 на MHI2Q HU

Проверено 2026-09-13 по бинарнику MU1316, текущему коду хука/Java и
сохранённой трассе HU. Новая live-проверка не состоялась: SSH на
`100.127.17.86:22` завершился таймаутом. Код и файлы на автомобиле не менялись.

## Источник и границы проверки

Бинарник: `Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/libairplay.so`
от корня `/Users/luka/Desktop/AUDI_2`.

- Стоковая libairplay 210.81, ELF32 ARM little-endian, QNX, базовый VA `0`.
- SHA256: `01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2`.
- Приведённые ниже адреса функций/инструкций лежат в `.text`, где VA равен
  файловому смещению. Runtime-адрес получается прибавлением базы загрузки SO.
- Артефакты повторного разбора: `output/r2/stock-stream110-20260913/` — символы,
  секции и три файла дизассемблирования. Исторические планы from-source
  libairplay не использовались как описание текущей реализации.

## Полная цепочка

```text
iPhone: готовая картинка основного CarPlay, H.264
  -> RTSP SETUP, streams[{type:110, streamConnectionID, ...}]
  <- ответ HU: streams[{type:110, dataPort:...}]
  -> отдельное TCP-соединение на dataPort (предпочтительно 6030)
  -> dio_manager / libairplay.so / _ScreenThread
  -> AirPlayReceiverSessionScreen_ProcessFrames
       128-байтовый заголовок + тело, AES-CTR для видеоданных
  -> ScreenStreamProcessData
       avcC/AVCC -> H.264 Annex B
  -> dio::OmxVideoImpl -> dio::COMXVideoDecoder
  -> OMX.qcom.video.decoder.avc
       аппаратное декодирование в native Qualcomm 32m Screen buffers
  -> FillBufferDone -> очередь screenRenderThread
  -> dio::CScreenRender::render -> screen_post_window
  -> QNX Screen / аппаратный вывод, displayable 59
  -> основной экран HU

Java/HMI: context 78 = {16,59}
  16: HMI и наложенные popup
  59: видеослой CarPlay
```

Это путь основного экрана HU. `110` — тип AirPlay-потока, `59` — ID
видеослоя; они принадлежат разным пространствам идентификаторов. Наши
`111 / altscreen_render / 99` и `maneuver_render / 98` — отдельные пути.

## Transport и разбор пакетов

`_ScreenSetup @ 0x26f9c` использует единственный стоковый screen-slot сессии
`+5136/+5140`. Читает `streamConnectionID`, вызывает
`AirPlay_DeriveAESKeySHA512ForScreen` с ключом сессии `session+456`
(`0x27094–0x270b0`), затем открывает сокет.

В `0x270e4–0x27118` передаётся preferred port **-6030**. Отрицательное
значение здесь разрешает динамический порт при неудаче bind, поэтому
источником истины является возвращённый `dataPort`, а не предположение
«всегда 6030». Ответ `{type:110,dataPort}` формируется в `0x27134–0x27160`.
Семантика fallback ранее проверена в `ServerSocketOpen @ 0x7fb00`;
см. [аудит lifecycle](CARPLAY_SESSION_LIFECYCLE_AUDIT.md).

`_ScreenThread @ 0x24220` вызывает `SocketAccept` с таймаутом 10 секунд
(`0x242a0–0x242b0`), устанавливает синхронизатор времени, запускает
`StartSession @ 0x2a3b8` и входит в `ProcessFrames @ 0x2b154`.
`StartSession` получает основной экран через `ScreenCopyMain @ call 0x2a414`.

Видео передаётся собственными screen-пакетами по TCP, а не отдельными
RTSP-запросами на каждый кадр. `ProcessFrames` читает 128-байтовый заголовок
и указанное в нём тело. Opcode 0 — видеоданные, расшифровка
`AES_CTR_Update @ call 0x2b7b0`; opcode 1 — конфигурация AVC без этой
расшифровки, включая размеры и `avcc`.

`_ScreenStreamSetProperty @ 0x85590` переводит конфигурацию avcC в заголовок
Annex B (`H264ConvertAVCCtoAnnexBHeader @ calls 0x85830/0x85884`).
`ScreenStreamProcessData @ 0x84fec` разбирает length-prefixed NAL units и
передаёт H.264 в видеоабстракцию. У текущего стока нет выбранного здесь
HEVC-декодера: имя OMX-компонента — `OMX.qcom.video.decoder.avc`.

## Декодер и владение буферами

| Звено | Подтверждение в libairplay |
|---|---|
| Создание видеоабстракции | `ScreenStreamCreate @ 0x85ce4` вызывает `IVideoAbstraction::createVideoImpl` |
| Создание рендера и декодера | `OmxVideoImpl::initialize @ 0x8c638`, calls `0x8c698/0x8c6ac` |
| Создание аппаратного AVC-компонента | `COMXVideoDecoder::open @ 0x871e8`, `OMX_GetHandle @ call 0x87220` |
| Native OMX output format | `COMXVideoDecoder::configure @ 0x89ffc`, константа `0x7f000004` в `0x8a148–0x8a17c` |
| Window ID 59 | `OmxVideoImpl::configure @ 0x8c388`, запись `59` в `0x8c4dc–0x8c4e0` |
| Screen format `0x1000c` | `convertColorFormatOMX2Screen @ 0x8c184`, преобразование native 32m |
| Получение общих буферов | `setOutputBuffers @ 0x86c4c` -> `CScreenRender::sharedBuffers @ 0x8b9e0` -> `getAllBuffers` |
| Подключение буферов к OMX | `goToIdleState @ 0x892fc` -> `attachBuffers @ 0x88f60` -> `CBufferPool::attach @ 0x88d40`, OMX `UseBuffer` через slot `+0x34` в `0x88e40–0x88e44` |
| Декодирование входа | `job_decodeFrame @ 0x88318`, копирование сжатых данных и вызов OMX `EmptyThisBuffer` |
| Готовый decoded buffer | `onFillBufferDoneCallback @ 0x86d18` -> `onJob_renderFrame @ call 0x86db0` |
| Очередь вывода | `onJob_renderFrame @ 0x8af08` -> `CRenderFrameCommand::execute @ 0x8b754` |
| Показ | `CScreenRender::render @ 0x8b5c0`, `screen_post_window @ call 0x8b690` |
| Возврат буфера | После успешного `render` команда вызывает reuse callback (`0x8b78c–0x8b7a0`) |

Декодированный кадр не копируется в наш RGBA-рендерер: OMX пишет в общие
буферы окна, затем сток публикует соответствующий Screen buffer. Это
передача без дополнительной CPU-копии **после декодера**, а не отсутствие
копий во всём сетевом тракте: сжатые данные копируются при подготовке OMX input.
Отдельного EGL/RGBA shader-pass, как у нашего `altscreen_render`, в этой
цепочке `CScreenRender` нет.

`CScreenRender::config @ 0x8bcc8` создаёт контекст, окно и группу
`dio_manager.airplay.qnx.screen_render`, задаёт ID, класс `media`, формат,
размеры, положение и alpha. `selectDisplay @ 0x8aa80` умеет перечислять
Screen displays и задавать `SCREEN_PROPERTY_DISPLAY`; это не вывод на MOST
и не наш маршрут к приборке.

## Что подтверждает сохранённая трасса автомобиля

`log/live_20260812_black110_restart/sloginfo.txt:5422–5498`:

- Начало screen-сессии: `NONE -> INITIALIZING`, параметр latency **25 ms**.
- Receiver/decoder priority **17**, decoder command slots **4**.
- Декодер и окно **1024×480**, **30 fps**, offset **(0,0)**.
- Window ID **59**; **12 input buffers** и **12 output buffers**.
- WFD создаёт **12 native images**, format **65548 = 0x1000c**.
- Декодер проходит Loaded -> Idle -> Executing; поступает avcC,
  затем screen-состояние переходит в READY.
- WFD создаёт источники из этих native images для аппаратного pipeline.

Это значения конкретного сохранённого запуска. Они не являются измерением
текущего FPS, полной задержки или текущего состояния автомобиля.
`latency=25` нельзя трактовать как измеренные 25 ms «телефон -> дисплей».

Также **READY не равно подтверждённому первому показанному кадру**:
`ProcessFrames` вызывает `ScreenStreamProcessData @ 0x2b7dc`, затем
`UpdateState @ 0x2b7f4`. OMX и очередь вывода находятся дальше асинхронно.
При диагностике чёрного экрана нужно отдельно проверять OMX output,
успех публикации буфера и активный display context.

## Java, текущий hook и остановка

В текущем [DisplayManagerMIB2High.java](../../../java_patch/de/audi/tghu/fwhmi/DisplayManagerMIB2High.java)
сохранён `dc[78] = {16,59}`. Java управляет контекстом и ресурсом экрана;
H.264 и декодированные кадры не проходят через Java.
`ExternalEventsListener.hmiDeactivated()` запускает команду HMIDeactivated;
исключение для partial OPS сохраняет владельца SCREEN, чтобы popup мог
остаться поверх CarPlay. См. [проверенный разбор PDC](../CARPLAY_PDC_SMALL_STAGE_ROOT_CAUSE.md).

В [airplay_seams.c](../../../hook/framework/airplay_seams.c) SETUP и TEARDOWN
проходят через обёртки, но вызывают оригинальные функции stock.
[altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c) создаёт собственный
receiver только для 111, на preferred port -6031, и не создаёт для него
стоковый ScreenStream. Выключение только 110 не останавливает 111;
полное завершение AirPlay-сессии очищает оба пути.

Старый main-screen loader и вмешательство в `CScreenRender` для 110 удалены;
см. [актуальное состояние loader](../CARPLAY_STARTUP_LOADER_PLAN.md).
Архивные советы удерживать 110 блокировкой TEARDOWN не описывают текущий код.
После выхода receive-loop `_ScreenThread` вызывает `StopSession @ call 0x24400`,
затем освобождает NetSocket. Потеря видимости HMI и teardown видео — разные
события; для конкретного случая нужно сопоставлять обе трассы.
