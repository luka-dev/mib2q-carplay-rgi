# CarPlay Video: принимающий плеер и пределы stock MU1316

Дата: 2026-09-14. Продолжение [исследования sender, экрана и допуска приложений](CARPLAY_VIDEO_TRANSPORT_2026_09_14.md).
Статический анализ; воспроизведение на HU не запускалось.

Продолжение от 2026-09-15: [штатный Cinemo / окно 43, URL API, блокировка картинки Audi и ответы playbackInfo](CARPLAY_VIDEO_MHI2_PLAYER_2026_09_15.md).

## Что удалось установить

1. В Apple AirPlayReceiver **stream 130 с video client UUID** имеет отдельную
   ветку `PlaybackV2`. Она запускает принимающий плеер через UI controller.
2. AirPlayReceiverKit реализует эту сторону через `APRKMediaPlayer` /
   `AVQueuePlayer` и `APRKPlayerItem` / `AVURLAsset`. Команды передают URL и
   управляют очередью; это отдельный путь от декодера экрана CarPlay.
3. **На проводе идентификатор называется `streamID`.** Sender называет его
   «transport token» в логах; отдельное поле `transportToken` не найдено.
4. В **stock libairplay MU1316** SETUP распознаёт только 100, 101, 110.
   Для 130 пишет `Unsupported stream type` и пропускает создание stream.
   Сам по себе этот случай не заставляет всю функцию вернуть ошибку.
5. Для HTTPS-медиа найдено получение 16-байтного PSK с меткой `Pair-TLS-PSK`
   и передача его в SSL-настройки `AVURLAsset`. Это отдельный механизм от
   авторизации защищённого контента FairPlay.

**Граница доказательства:** AirPlayReceiverKit из iOS — доступная реализация
Apple, а не автомобильный QNX-плеер. Она позволяет восстановить протокол и
разделение компонентов. Не доказывает наличие такого плеера или совместимой
QNX-поверхности в MHI2. Связь с логическим `mainScreen` установлена на стороне
CarPlay sender в предыдущем отчёте.

## 1. Образы и доказательства

Адреса Apple ниже относятся к iOS 27 `24A5418b` без ASLR.

| Образ | База / идентификатор |
|---|---|
| AirPlayReceiver | `0x250d0d000`, существующая `ios27_dsc/24A5418b__iPhone18,1/AirPlayReceiver.i64`; ключевые инструкции дополнительно прочитаны из исходного cache |
| AirPlayReceiverKit | `0x250ee8000`, извлечён через `ipsw dyld extract --slide --objc --stubs` |
| MU1316 libairplay.so | ARMv7, image base 0; адреса stock ниже являются смещениями в этом образе |

SHA256 извлечённого AirPlayReceiverKit:
`1f301254def11bc340604deaab31ce581347a842c4a9403bf8cc4532d19bc276`.

Stock binary:

```text
/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app/eso/lib/libairplay.so
SHA256: 01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2
```

Материалы в `output/carplay-video-re-20260914/`:

- [Core receiver: функции](../../output/carplay-video-re-20260914/receiver-core-evidence.json).
- [Ветки SETUP и ответ](../../output/carplay-video-re-20260914/receiver-setup-excerpts.json).
- [ReceiverKit: функции](../../output/carplay-video-re-20260914/receiver-kit-evidence.json).
- [Stock MU1316 SETUP](../../output/carplay-video-re-20260914/stock-mu1316-setup.json).
- [Ключи и selector stubs ReceiverKit](../../output/carplay-video-re-20260914/kit-annotations.json).
- [Скрипт аннотации](../../output/carplay-video-re-20260914/annotate_asm.py),
  [reader cache](../../output/carplay-video-re-20260914/read_cache.py).

Hex-Rays оставляет часть внешних вызовов как `MEMORY[...]` и теряет аргументы.
Поэтому фактические ключи и selectors проверены по инструкциям и исходным
данным cache. Скрипт аннотации разрешает только локальные ADRP/ADD и
ADRP/LDR/LDR последовательности; это не анализ потока данных. Он проверяет
CFString flags, чтобы не принимать block descriptors за строки.

## 2. SETUP: video UUID выбирает PlaybackV2

`AirPlayReceiverSessionSetup` — **`0x250e733d0`**.
Полная декомпиляция слишком велика и выдаётся с усечением; выводы о ветке
ниже основаны на сохранённых инструкциях.

Для `type = 130` в **`0x250e742ac`**:

```text
stream.clientTypeUUID == A6B27562-B43A-4F2D-B75F-82391E250194
  -> PlaybackV2Setup
  -> создать новый streamID
  -> прочитать обязательный clientUUID
  -> PlaybackV2Start
```

Это **тот же UUID**, который создаёт playback-сессия на iPhone. Иные UUID
переходят в общую ветку remote control в `0x250e74760`; её MediaRemote/XPC
реализацию нельзя автоматически считать видеоплеером.

Ветка также проверяет внутреннее состояние session по `+0x2c0 == 2`.
Смысл всего enum и переходы между состояниями здесь не восстановлены;
это не установление значения CarPlay capability.

`PlaybackV2Start`, инструкции **`0x250e75090`** и далее:

- требует UI controller в session;
- передаёт `deviceID`, `sessionID`, опциональный `networkClock`;
- устанавливает `_PlaybackV2EventCallback` (`0x250e7fc14`) и
  `_PlaybackV2TLSInfoRequestCallback` (`0x250e7fcf0`);
- вызывает start-функцию class vtable UI controller, slot `+48`.

После успешного старта **`0x250e75194`** формирует stream в ответе SETUP:

```text
{ type: 130, streamID: <новое числовое значение> }
```

Затем сохраняет отображение **streamID → clientUUID** и активную playback-сессию.
В данной специальной ветке нет создания отдельного media-data TCP listener
или возврата `dataPort`; она подходит под shared control/event connection.

### Исправление терминологии sender

В `ensureAndCopyTransportStreams` sender, statement **`0x25101cf2c`**,
raw CFString pointer разрешается в **`0x275afc700 = "streamID"`**.
Именно это значение логи называют transport token. Для внутренних transport
streams оно далее передаётся под ключом `StreamGroupID`.
Параметр `dataPort` относится к опциональной общей RCS-ветке dedicated socket;
его наличие в sender не означает обязательного отдельного сокета PlaybackV2.

## 3. Как команда попадает в плеер

Для исследованного shared-connection пути:

```text
POST /command, X-Apple-StreamID: <id>
  outer property list: { params: { data: <serialized player dictionary>, ... } }
  inner player dictionary: { type: "insertPlayQueueItem", item: {...}, ... }
```

Это схема структуры, не сетевой дамп и не готовый запрос: framing, CSeq,
session/security setup и все обязательные поля ещё требуют интеграции.

Подтверждённые переходы:

| Участок | Действие |
|---|---|
| `airplayReqProcessor_requestProcessCommand`, `0x250e46cdc` | При отсутствии внешнего `type` читает `X-Apple-StreamID` и `params`, передаёт `didReceiveData` |
| `AirPlayReceiverSessionControl`, `0x250e6fa20`, ветка `0x250e6fb00` | Проверяет ID в playback map, добавляет его к params, вызывает UI controller `controlVideoPlayback`; помечает режим `URLflinging` |
| `uiController_controlVideoPlayback`, `0x250f091ac` | Читает `data`, декодирует property list, учитывает `PerformSync`, передаёт в backingMediaPlayer с session ID |
| `processMessageWithIDAndDictionarySync:messageSessionID:`, `0x250eec420` | Проверяет совпадение playerSessionID, выполняет работу на последовательной message queue |
| `_processMessageWithDictionaryInternal:`, `0x250eec620` | Выбирает обработчик по **внутреннему** `type` |

`kUIControllerVTable` ReceiverKit — **`0x275ae86d0`**, class vtable
**`0x275ae8748`**. Проверены слоты:

- `+48`: `uiController_startVideoPlayback`, `0x250f08fd8`;
- `+56`: `uiController_stopVideoPlayback`, `0x250f090fc`;
- `+64`: `uiController_controlVideoPlayback`, `0x250f091ac`.

### Команды внутреннего протокола

| `type` | Обработчик APRKMediaPlayer |
|---|---|
| `insertPlayQueueItem` | `_insertPlayQueueItemWithDictionary:` |
| `removePlayQueueItem` | `_removePlayQueueItemWithDictionary:` |
| `setRate` | `_setRateWithDictionary:` |
| `seek` | `_seekWithDictionary:` |
| `stop` | `_stopWithDictionary:` |
| `playbackInfo` | `_processPlaybackInfoRequestWithDictionary:` |
| `setProperty` | `_setPropertyWithDictionary:` |
| `property` | `_getPropertyWithDictionary:` |
| `unhandledURL` | `_processUnhandledURLWithDictionary:` |
| `streamingKey` | `_processStreamingKeyWithDictionary:` |
| `authorizeItem` | `_processAuthorizeItemWithDictionary:` |
| `playbackCoordinationMedium` | одноимённый обработчик, за feature gate `CoordinatedAirPlayVideo` |

Это подтверждённый dispatcher, **не полный schema аргументов каждой команды**.

### События обратно на iPhone

`UIControllerSendUpstreamMessage` (**`0x250f07d84`**) сериализует словарь события
в binary property list и кладёт bytes в `data`. `_PlaybackV2EventCallback`
оборачивает результат в `params`. Его block (**`0x250e7fec0`**) вызывает
`AirPlayReceiverSessionSendCommandForObject` (**`0x250e72d0c`**).

Последний формирует `POST /command`, `RTSP/1.0`, `CSeq`,
`X-Apple-StreamID`, body `application/x-apple-binary-plist`.
Таким образом, здесь действительно **два уровня сериализации**: внешняя
команда транспорта и внутреннее сообщение плеера в `params.data`.

## 4. URL, queue item и TLS

`_insertPlayQueueItemWithDictionary:` — **`0x250eed7d4`**.
Берёт `item`, создаёт `APRKPlayerItem` и регистрирует/вставляет его в очередь.
Конструктор item — **`0x250effa04`** — создаёт `AVURLAsset` с URL и options,
подключает resource loader и content-key helper.

Некоторые точные ключи, проверенные через CFStrings:

| Ключ | Где / зачем |
|---|---|
| `item`, `itemAfter` | объект очереди и позиция вставки |
| `uuid` | идентификатор элемента; требуется конструктором |
| `Content-Location`, `HLS-Content-Location` | URL медиа; в выборе HLS-URL участвует reachability |
| `Start-Position` | начальное время при вставке |
| `Start-Date`, `Start-Position-Seconds` | дополнительные временные поля item; отдельные ключи |
| `IsTLSEnabled` | запрос TLS info и установка SSL options для asset |
| **`audioOnly`** | bool, сохраняется в `APRKPlayerItem._isAudioOnly` |
| `Play-Info`, `fpSessionID` | отдельная ветка FairPlay/авторизации |
| `timeToPauseBuffering`, `timeToPausePlayback` | программные условия приостановки item; сами по себе не автомобильный запрет |

Регистр существенен: подтверждены **`audioOnly`** и **`IsTLSEnabled`**.
`audioOnly` в конструкторе — флаг элемента, а не доказательство реализации
динамической команды CarPlay `changeVideoPlaybackToAudioOnly`.

### TLS для local playback

1. `IsTLSEnabled` приводит к запросу `mediaPlayerNeedsTLSInfo:` и использованию
   cached SSL properties в `AVURLAssetSSLPropertiesKey`.
2. Core callback **`0x250e7fcf0`** получает из security/key-holder объекта
   16 bytes по метке **`Pair-TLS-PSK`**. Возвращает `tlsPSK` и
   `tlsPSKClientIdentity`.
3. `_SSLPropertiesforTLSInfo:` (**`0x250eed348`**) преобразует их в
   `kCFStreamSSLPSKSharedSecret` / `kCFStreamSSLPSKIdentity`.
4. В **`0x250eed3b8`** создаётся двухбайтное значение cipher **`0x00AE`**;
   оно передаётся в `kCFStreamSSLEnabledCiphers`.
   Локальный Apple SDK `Security.framework/Headers/CipherSuite.h:204`
   определяет его как **`TLS_PSK_WITH_AES_128_CBC_SHA256`**.

Источник последнего соответствия:
`/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/Security.framework/Versions/A/Headers/CipherSuite.h`.
[Инструкции SSL-настроек](../../output/carplay-video-re-20260914/kit-tls.asm).
Это настройка исследованной реализации, не результат TLS handshake с MHI2.
PSK не является ключом расшифровки FairPlay-контента.

## 5. Куда выводится картинка и где заканчивается эта реализация

`uiController_startVideoPlayback` передаёт session ID и version 2 в
`APRKStreamRenderer.processStartVideoPlaybackRequestWithWithSessionID:version:`
(**`0x250efcac4`**). Тот обеспечивает наличие внутреннего `APRKMediaPlayer`
через `_ensureInternalPlayerFor:` (**`0x250efdc28`**).

У renderer раздельные счётчики video, screen, audio.
В `_updateStreamingMode` block (**`0x250efe64c`**) наличие video выбирает
режим 4; иначе screen — 1, audio — 2, отсутствие — 0. Второй block
(**`0x250efe790`**) вызывает внешний delegate
**`didChangeStreamRendererModeTo:forRenderer:`**.

Это показывает границу между приёмом/плеером и приложением, которое управляет
его представлением. На Apple стороне есть backing `AVQueuePlayer`.
**ID дисплея MMI, QNX window/surface и переключение существующего HU compositor
этим кодом не устанавливаются.** Предыдущий sender-анализ по-прежнему указывает
на логический основной экран CarPlay, а не на наш stream 111 в приборку.

## 6. Кто исполняет запрет видео

На стороне iPhone уже прослежено:

```text
HU сообщает videoPlaybackAllowed=false
  -> iPhone устанавливает Forced audio-only
  -> при активном video отправляет changeVideoPlaybackToAudioOnly
  -> принимающая автомобильная сторона должна прекратить показ видео
```

В исследованном dispatcher APRKMediaPlayer **нет такой команды**. Точные строки
`changeVideoPlaybackToAudioOnly` и `setVideoPlaybackAllowed` также не найдены в
проверенном cstring-диапазоне AirPlayReceiver и полном извлечённом ReceiverKit.
Текстовая декомпиляция 26.1 согласуется с этим отрицательным результатом.

Вывод ограничен этими образами: **исполнитель автомобильного запрета здесь не
найден**. Он может быть в другом компоненте/автомобильной реализации.
Нельзя заменить эту недостающую часть обнаруженным флагом item `audioOnly`
или считать, что общий Apple receiver сам получает скорость автомобиля.

## 7. Что конкретно отсутствует в stock MU1316

`AirPlayReceiverSessionSetup` — **`0x272b8`**. После чтения stream type
в `0x27564` код принимает 100/101 (main/alt audio) и 110 (screen).
В **`0x27580`** любой другой тип выше 101 уходит в unsupported branch.

Эта ветка логирует и переходит к следующему элементу массива. Затем вызывается
`AirPlayReceiverSessionPlatformControl`; при его успехе общий SETUP возвращает 0.
Следовательно:

- **успешный RTSP SETUP сам по себе не доказывает поддержку stream 130**;
- без отдельного обработчика не будет playback stream / `streamID` в ответе;
- это вывод о stock core, а не о дополнительных обработчиках LD_PRELOAD hook;
- возврат пустого `videoPlaybackInfo` не добавит ни эту ветку, ни URL-плеер.

Старый SDK source подтверждает общую архитектуру switch/unsupported log, но
список поддерживаемых типов выше получен именно из целевого MU1316 бинарника.

## 8. Следующие проверки

1. Восстановить минимальный набор response/event dictionaries (`playbackInfo`,
   queue/current item, rate/seek completion, ошибки), включая поля и типы.
2. Найти пригодный плеер/decoder API и путь до основной поверхности MMI в HU.
   В firmware присутствуют `media`, `medialauncher`, `mediarouter` и
   `libasimmxmediadecodercontrolproxy.so`; пока это только кандидаты по именам.
3. Разобрать обработчик автомобильного audio-only, источник parked/движение
   и возврат владения mainScreen. Пороги и сигналы HU ещё не установлены.
4. Проверить, как media-only запрос и требуемое session state совмещаются с
   существующей CarPlay-сессией MU1316. Полный SETUP/control handshake не собран.
5. После этого подготовить тест собственного незашифрованного ролика: сначала
   очередь/статус/stop, затем URL fetch, TLS при необходимости, изображение и звук.
   Совместимость DRM-контента остаётся отдельной задачей.

Комментарии и понятные имена аргументов сохранены в IDB. Программный код hook
и файлы развёртывания в рамках этого исследования не менялись.
