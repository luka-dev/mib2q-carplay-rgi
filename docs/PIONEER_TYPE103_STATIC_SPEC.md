# CarPlay type 103: завершённая статическая спецификация для MHI2Q

Дата фиксации: 2026-08-29.

> **Обновление 2026-09-04:** протокольная статическая часть ниже остаётся
> справочной, но её выводы о недоступности AAC и ещё не реализованном тракте
> superseded. Машина выбрала AAC-LC/48 kHz/stereo index 23; Cinemo bridge теперь
> декодирует его, а `[65,23]` переключается APAC <-> AAC в обе стороны внутри
> одной type-103 сессии. APAP ID 2 несёт per-packet format index (23/65), ID 8
> несёт `spf=1024`; изменившийся inline cookie остаётся авторитетом выбора codec. Текущее состояние:
> [`DYNAMIC_BUFFERED_CODEC_SWITCH.md`](DYNAMIC_BUFFERED_CODEC_SWITCH.md) и
> [`AAC_BUFFERED_PATH_PLAN.md`](AAC_BUFFERED_PATH_PLAN.md).

## Итог

Статический контракт buffered-потока CarPlay type 103 восстановлен достаточно полно, чтобы больше
не угадывать протокол на HU. Из Pioneer SPHERA восстановлены SETUP/lifecycle, APAP framing,
аутентификация, расширения, cookie/format switching, очередь, clock, flush, teardown, APAC API и
штатный spatial downmix. Отдельно исследован нативный ARMv7/QNX путь декодирования AAC через уже
установленный в Audi Cinemo, но hardware negotiation показал, что iPhone не выбирает его для
Spatial Audio.

Первоначально рекомендуемый статикой AAC-тракт был:

```text
iOS type 103 / index 40 (AAC-LC 48 kHz 5.1.2)
  -> APAP + ChaCha20-Poly1305
  -> небольшой ARMv7 AudioConverter/Cinemo adapter
  -> PCM8 L R C LFE Ls Rs heightL heightR
  -> 5.1.2 -> 5.1 downmix
  -> существующий PCM6 / DIO5 / AMP
```

APAC 65/66 остаётся вторым, полностью описанным wire-путём и эталоном для проверки на macOS.
Pioneer-библиотеку нельзя переносить как готовый бинарник: это AArch64 Linux, а Audi MHI2Q — ARMv7
QNX. Патч матрицы Pioneer нужен только как проверяемый статический oracle.

## Live delta — Audi + iOS 27, 2026-08-31

Hardware изменил порядок реализации: iOS отказался от type 103 при рекламе index 40 и выбрал
стерео type 100, но при index 65 действительно создал type 103/APAP и показал Dolby Atmos. Финальный
dual-list тест объявил `[40,65]`; iOS увидел его, но выбрал `audioFormatIndex=65`,
`packetFormatAPAP=true`, а cookie подтвердил APAC 48 kHz/1024/6ch. Поэтому текущий рабочий probe
остаётся на **APAC index 65**, а AAC/Cinemo bypass для этой пары phone/HU закрыт.

Пятисекундное завершение сессии найдено и устранено на control-plane. Поздняя подмена callback
`HTTPServer+0x64` не перехватывала уже принятую control connection. Экспортированный interposer
`AirTunesServer_EnsureCreated` теперь ставит dispatcher сразу после создания HTTPServer, до первого
accept. После этого iOS успешно передал 33-byte `SETMAGICCOOKIE`; SETRATE был перехвачен, а после
разрешения media-only формы без `rtpTime` получил HTTP 200. Atmos/type 103 и playback counter
держались больше двух минут вместо teardown через 5 секунд.

Реальные pause-пакеты уточнили сериализацию:

```text
SETRATEANCHORTIME { rate = 0 }
FLUSHBUFFERED {
  flushUntilSeq = <seq>
  flushUntilMediaTimeValue = <value>
  flushUntilMediaTimeScale = 48000
}
```

`flushFrom*` и `flushUntilTS` при этом отсутствуют. Build `b8305141…` принимает опциональные
границы и RTP-компоненты; его live pause/resume проверка ещё не завершена. Decrypt/APAC decode,
5.1.2-to-5.1 downmix и DIO5 render по-прежнему следующие отдельные этапы.

Type 100 остаётся штатным realtime PCM/6 fallback. Никакой результат этой статической работы не
требует менять его формат, DIO5 или усилитель.

Текущий HU build и live-результаты описаны выше; прежнее состояние с выключенным marker и build
`13aee019…` больше не актуально.

## Повторный статический аудит negotiation (2026-08-29)

Negotiation-контракт проверен повторно по независимой цепочке iOS 27 AirPlaySender/APTransport,
Pioneer SPHERA и целевого Audi MU1316. Начальный и post-SETUP control flow закрыт end-to-end:

1. iOS предлагает `mainBuffered` в control SETUP `features`; receiver может вернуть его только в
   `enabledFeatures`. После этого iOS требует точное парное присутствие `/info.mainBufferedInfo`.
2. `/info.audioFormats` публикует отдельный `type=103`, `audioType=media`, один legacy codec family и
   extended index. Смешанный legacy mask может сорвать создание всего CarPlay format object, включая
   обычный type 100; одиночный AAC-LC bit `0x400000` этого дефекта не имеет.
3. При выбранном buffered format sender строит отдельный media SETUP именно с `type=103` и фактическим
   `audioFormatIndex`; для legacy APAP также передаются `packetFormatAPAP`, `spf` и `shk`.
4. Pioneer `AirPlayReceiverSessionSetup@0x120100` передаёт stream в helper `0x11b290`; ветка
   `type == 103` непосредственно вызывает `_BufferedAudioSetup@0x12c500`. Перед этим `shk` читается
   как ровно 32 байта и без derivation копируется в stream context.
5. Legacy SETUP response требует `dataPort` и `audioBufferSize`. `arrivalToRenderLatencyMs` опционален
   и при отсутствии трактуется как ноль; `streamID` также опционален — AirPlaySender генерирует его,
   если accessory его не вернул. Новый TCP/6033 для этого потока не нужен.
6. APTransport package type `0x6d616763` формирует custom RTSP method `SETMAGICCOOKIE` с MIME
   `application/x-apple-binary-plist` на уже существующем control connection. Sender передаёт
   `magicCookieID` и `magicCookie`; Pioneer ACK-ает запрос даже до появления stream context, а при
   активном context хранит первый CFData для каждого ID. APAP TLV 17 затем выбирает сохранённый ID.
7. В Audi hook повторно проверен private seam: `AirTunesServer_EnsureCreated@0x2e9ac` кладёт stock
   handler `0x302e0` в callback-table offset `+0x24`; `HTTPServerCreate@0x6c324` копирует таблицу в
   `HTTPServer+0x40`, поэтому request callback действительно находится в `HTTPServer+0x64`.
   PIC-base в этой функции даёт outer-server global ровно `image+0xbabcc`. Поля stock HTTPMessage
   также подтверждены кодом handler: method `+0x201c/+0x2020`, body `+0x209c/+0x20a8`.
8. После prime buffered engine требует ещё четыре RTSP control method на том же соединении:
   `GETANCHOR`, `SETRATE`, `SETRATEANCHORTIME` и `FLUSHBUFFERED`. Stock Audi 210.81 не знает ни одного
   из них. Sender повторяет `SETRATEANCHORTIME`/anchor sequence 50 раз, затем вызывает
   `bufferedAudioEngine_handleTerminalSetRateError -> sendSessionFailureEvent`, что точно объясняет
   прежний wall-clock teardown через примерно 4.5–5 секунд независимо от объёма принятого APAP.

Это означает, что negotiation и обязательный playback-start control contract больше не содержат
неизвестного шага. Отдельное уточнение: `SETMAGICCOOKIE` остаётся обязательным protocol operation,
но именно его отсутствие **не** является доказанной причиной пятисекундного teardown. Ошибка уже
отправленного cookie HTTP-запроса приходит в completion handler, который главным образом логирует
результат. Точный teardown path — 50 неуспешных buffered rate/anchor retries из-за отсутствующих
методов выше. Полные sender/receiver адреса и plist-схемы зафиксированы в
`HANDOFF_SPATIAL_AUDIO.md`, §10.

Остаются динамические доказательства: успешная связка `SETMAGICCOOKIE -> 200 -> TLV 17/APAP`,
корректный NTP anchor/clock lock и непрерывное playback после
реального decrypt/decode/downmix/render. Текущий hook является negotiation/capture sink; production
receiver ещё должен валидировать `audioFormatIndex`, `packetFormatAPAP`, `spf` и 32-байтный `shk`,
реализовать четыре buffered-control метода и payload pipeline. Диагностический log key исправлен на
`audioFormatIndex`; функциональное поведение при этом не изменилось.

## Проверенные бинарники

| роль | файл / платформа | SHA-256 | результат |
|---|---|---|---|
| полный type-103 donor | Pioneer SPHERA 1.030010 `libSPF_CP_PlugInWrapper.so`, AArch64 Linux | `d8ac9aa2247d43627228d6538496ee0757e8f6eaa50526babc44dd697b971790` | полный receiver, APAC и downmixer |
| целевой AirPlay | Audi MHI2Q MU1316 `libairplay.so`, ARMv7 QNX | `01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2` | type 103 отсутствует; `AudioConverter*_compat` — заглушки |
| ARMv7 decoder donor | MH2p US LB636 P2712 `libairplay.so`, ARMv7 QNX | `f94f1b661be0ff9c1131dbaa2c74c7fe924cd7548e7eb794b5b8b44f15ddc5dc` | рабочие AudioConverter, AAC и ChaCha20-Poly1305; type 103/APAC отсутствуют |
| Audi codec factory | MHI2Q `libNmeSDK.so` | `a34a968e10b5193d9bd8ee80abd89d29e987d1b573a72f44d85865fd11ace73e` | `CinemoCreateAudioCodec` @ `0x4ea44` |
| Audi AAC codec | MHI2Q `libNmeAudioAAC.so` | `89536dcdfddeab67e98ce8945fc7e4346fa6d71da0de170f8c36d98475a6cd46` | `NmeCreateCodecAAC` @ `0x4a20` |

MPR3 AirPlay 450.14, MHI3 AirPlay 450.14.2 и MH2p AirPlay 320.17.1 не содержат ни
`packetFormatAPAP`, ни `APACDecoder`, ни type-103 setup. Новые VAG-библиотеки полезны как
API/crypto/codec references, но замена Audi 210 на 320/450 не добавит 103 и создаст ABI-риск.

## Форматы, которые имеют практический смысл

`103` — тип потока. `40`, `42`, `65`, `66` — индексы формата внутри этого потока.

| индекс | wire codec | частота/каналы | PCM equivalent | роль |
|---:|---|---|---:|---|
| 40 | AAC-LC (`aac `) | 48 kHz, 5.1.2 / 8 ch | — | статически допустим, но live: один 40 дал type-100 fallback, `[40,65]` выбрал APAC 65 |
| 42 | AAC-ELD (`aace`) | 48 kHz, 5.1.2 / 8 ch | — | резервный AAC-вариант; тот же buffer class, отдельный decoder constructor |
| 65 | APAC | 48 kHz, 5.1 / 6 ch | 44 | первый APAC-кандидат, downmix не нужен |
| 66 | APAC | 48 kHz, 5.1.2 / 8 ch | 38 | целевой spatial master с downmix 8 -> 6 |
| 67 | APAC | 48 kHz, 7.1 / 8 ch | 78 | не нужен для первого этапа |
| 68 | APAC | 48 kHz, 7.1.4 / 12 ch | 84 | вне текущего hardware ceiling |
| 76 | APAC | 48 kHz, 5.1.4 / 10 ch | 80 | вне первого этапа |
| 77 | APAC | 48 kHz, 7.1.2 / 10 ch | 82 | вне первого этапа |

Индекс 40 не является догадкой: для индексов ниже 64 Apple отображает индекс ровно в legacy bit
`1ULL << index`, а таблица descriptor-ов **iOS 27** даёт `40=AAC-LC/48000/5.1.2` и
`42=AAC-ELD/48000/5.1.2`. Это не строки из старой Pioneer format table: Pioneer статически
подтверждает receiver-механику, а точное значение новых AAC index-ов задаёт sender. Статически
MH2p-конструктор AAC-LC
передаёт sample rate и channel count в Cinemo без найденного clamp до двух или шести каналов.
Однако этот decoder bench больше не является production gate: live sender не выбрал AAC.

В type-103 ветке `APCarPlayAudioFormatsCopyFormatsForAudioType` sender создаёт mutable copy
extended-массива и специально удаляет только legacy index 23; whitelist только из APAC-индексов
там нет, поэтому 40/42 сохраняются. Это закрывает только статическую допустимость AAC-кандидата на
данном этапе negotiation. Hardware capture уже поставил отрицательную точку: index 40 один не
открыл type 103, а `[40,65]` выбрал 65.

Индекс 65 уже был реально запрошен iOS после правильной рекламы: type 103, `audioFormatIndex=65`,
`packetFormatAPAP=true`, `spf=1024`, `shk=<32 bytes>`. Это доказывает negotiation и transport, но не
делает APAC-декодер доступным на Audi.
Самодостаточный private live corpus сохранён локально в
`captures/apac_5_1_live_20260831_0205`: raw APAP, matching SETUP/key, `dapa` cookie и control plists;
несекретный manifest находится в `docs/captures/APAC_5_1_LIVE_20260831.md`.
После исключения только оборванного финального 4-byte length prefix все 3980 полных кадров проходят
ChaCha20-Poly1305 authentication; plaintext APAC corpus содержит 1,148,875 байт.

Индекс 66 также подтверждён hardware-запросом iOS: type 103, `audioFormatIndex=66`,
`packetFormatAPAP=true`, `spf=1024`, `shk=<32 bytes>`. Его отдельный 34-байтный `dapa` cookie имеет
валидный sync `0x0800`; GlobalConfig декодируется как 48 kHz, 1024 samples и **8 channels**. Поэтому
это прямое доказательство реального APAC 5.1.2 на проводе, независимое от надписи Dolby Atmos в UI.
No-render receiver затем получил sender-driven `rate=0`, flush и per-stream teardown: decode/queue/
render всё ещё обязательны для стабильной сессии и звука.

## SETUP и транспорт

Минимальный входной descriptor от iOS:

```text
{
  type = 103;
  audioType = media;
  audioMode = default;
  isMedia = true;
  audioFormatIndex = <40|42|65|66>;
  packetFormatAPAP = true;
  spf = 1024;
  shk = <ровно 32 байта>;
  supportsDynamicStreamID = true;
}
```

Legacy-ответ receiver должен содержать массив `streams` и как минимум:

```text
{
  type = 103;
  dataPort = <APAP TCP port>;
  audioBufferSize = <5000000 для AAC index 40/42; 21168000 для APAC 65/66>;
  arrivalToRenderLatencyMs = <может быть 0>;
  streamID = <сгенерированный ID>;
}
```

`dataPort` и `audioBufferSize` обязательны. `arrivalToRenderLatencyMs` опционален и по умолчанию
равен нулю. `streamID` можно не возвращать: sender умеет создать его, но явный стабильный ID проще
сопоставлять с lifecycle.

`controlPort`/6033 не является условием работы этого legacy APAP-потока. Hardware показал, что iOS
подключается только к 6032; 6033 не открылся и не изменил пятиисекундный teardown. После статического
восстановления `SETMAGICCOOKIE` это стало окончательно понятно: codec configuration идёт отдельным
методом на уже существующем RTSP control connection, а не на 6033. Новый repository build больше не
публикует `controlPort` и не открывает RTCP listener. Production PF можно сократить до `6030:6032`
во всех четырёх профилях. Для текущей установленной диагностической сборки диапазон `6030:6033`
менять не надо: она не заменялась в ходе этой работы.

## APAP framing и криптография

В hook-capture каждый пакет начинается с big-endian `uint32`, причём длина включает собственные
четыре байта. После него идёт APAP packet.

Фиксированный APAP header имеет 15 байт:

| смещение | размер | значение |
|---:|---:|---|
| 0 | 8 | signed media-time value, big-endian int64 |
| 8 | 4 | signed media-time scale, big-endian int32 |
| 12 | 3 | sequence number, big-endian 24-bit |

Pioneer `AirPlayReceiverSessionConvertMediaTimeToSampleTime` вычисляет RTP/sample time как
`floor(mediaTimeValue / mediaTimeScale * sampleRate) mod 2^32`. Это подтверждено live-захватом:
`46670057988423 / 1e9 * 48000 = 2240162783 = 0x85862bdf`, и первый APAP fixed header содержит
ровно media value `0x0000000085862bdf`. Поэтому ответ `SETRATE` не может оставлять `rtpTime=0`,
даже когда iOS опускает его во входном media-only запросе.

Статический разбор фактического iOS sender response parser уточнил success-форму ответа `SETRATE`:
`CFDictionaryGetAudioTransportTime` читает полный transport triple — `rtpTime`,
`mediaTimeValue`, `mediaTimeScale`; затем четыре поля
`networkTime{Secs,Frac,TimelineID,Flags}` и `status` (по умолчанию ноль). Последний live build
ошибочно убрал media-time пару и тем самым внёс регрессию. Текущий repository снова возвращает
полный triple и явно добавляет `status=0`.

В live A/B регрессивный ответ каждый раз давал окно ровно из 27 APAP-пакетов: RTP увеличивается на
1024 сэмпла, а `flushUntilSeq` совпадает с последним пакетом окна. Два запуска дали 54/54 успешно
аутентифицированных пакета. От `SETRATE rate=1` до автоматического `rate=0` прошло 9.298 секунды,
что соответствует следующей стене — 10-секундному timeout синхронизации network clock. После TLV
префикса в расшифрованном APAP находится настоящий APAC payload, а не только extension padding.

Инструментированный live build затем показал прыгающие на миллионы секунд anchors. Причина оказалась
не в layout `AirTunesTime`, а в ABI: целевой `MillisecondsToUpTicks` принимает `uint64_t` в `r0:r1`,
а hook объявлял `uint32_t`, оставляя `r1` случайным. Поэтому планируемые +150 ms превращались в
случайный 64-битный интервал. Текущий код исправляет прототип и явно передаёт 64-битное значение.

Тело защищено так:

```text
ciphertext || Poly1305 tag[16] || nonce[8]
```

Используется original ChaCha20-Poly1305 64x64 с восьмибайтным nonce. Значение SETUP `shk` — прямой
32-байтный ключ; HKDF и иная derivation здесь отсутствуют.

AAD:

* обычный audio packet: `fixed_header[4:12]`, 8 байт;
* extension/state packet: `fixed_header[0:12]`, 12 байт.

Пакет не должен попадать в очередь/decoder до успешной проверки Poly1305. Ошибка tag, неизвестный
активный format/cookie или выход длины за пределы должны закрывать только type-103 session и
оставлять type 100 stock fallback.

MH2p содержит совместимую ARMv7 реализацию:

| функция | VA |
|---|---:|
| `chacha20_poly1305_init_64x64` | `0x69a90` |
| `chacha20_poly1305_add_aad` | `0x69dfc` |
| `chacha20_poly1305_decrypt` | `0x69e74` |
| `chacha20_poly1305_verify` | `0x69fe4` |
| `chacha20_poly1305_decrypt_all_64x64` | `0x6a164` |

Это reference/source для маленькой QNX-реализации; загружать вторую полную `libairplay.so` в
процесс нельзя из-за одинаковых символов и конструкторов.

## APAP extension TLV

Extension body — последовательность `ULEB128 id`, `ULEB128 length`, `payload`; `id=0` завершает
список.

| id | смысл | payload |
|---:|---|---|
| 2 | transport `audioFormatIndex` в iOS sender | BE integer 1..8 bytes; AAC capture=`23`, APAC capture=`65` |
| 4 | trim start | media time: signed BE64 value + signed BE32 scale |
| 5 | trim end | media time: signed BE64 value + signed BE32 scale |
| 7 | SoundCheck records | N записей по 5 x BE32 |
| 8 | samples per frame (`spf`) | BE integer; обе capture=`1024` |
| 10 | `TransitionID` в iOS sender | BE integer; Pioneer 1.030010 пропускает как unknown |
| 11 | SoundCheck parameters | 3 x BE32 |
| 12 | `soundCheckMediaKind` | 1 byte |
| 17 | `magicCookieID` | 0..8 bytes, BE integer |
| 18 | inline `magicCookie` | codec configuration bytes |
| 19 | `SbufTracer` | opaque sender diagnostic payload |
| 20 | source/content format index | BE integer; AAC=`73`, APAC=`35` в сохранённых capture |
| 21 | extended `SbufTracer` | opaque sender diagnostic payload |

Поля 4/5 здесь не только распознаются. `_BufferedAudioThread` (`sub_123F00 @ 0x123f00`)
после вызова `APAPDecodeExtension @ 0x124a48` переводит их длительности в целое число
сэмплов текущей частоты: field 4 в `trimAtStartSamples` (`0x124ab0`,
`0x124c34..0x124c6c`), field 5 в `trimAtEndSamples` (`0x124ab8..0x124b00`).
`_BufferedAudioUpdateStateUsingAPAPExtensionInfo` (`sub_126160 @ 0x126160`) декодирует AU,
а затем в `0x12642c..0x126488` сдвигает начало PCM на `trimAtStartSamples` и уменьшает
длину на сумму start/end trim. Если trim покрывает весь AU, выходная длина становится нулевой.
Округление выполняется вниз отдельно для каждого пакета; накопителя дробного остатка нет.

Field 10 отсутствует в switch `APAPDecodeExtension`: общий путь `0x109bbc..0x109bc4`
просто пропускает его payload. Пустой APAP-пакет всё равно ставится в обычную очередь
(`0x124694..0x1246c4`), но `_AudioDecoderDecodeFrame` (`sub_123940 @ 0x123940`)
для пакета только с 24-байтовым crypto overhead возвращает success и 0 байт PCM
(`0x12394c..0x1239b0`). В этой Pioneer-реализации `TransitionID` не означает reset,
flush или rebuild декодера.

Статус Audi hook на 2026-09-06: TLV 4/5 реализованы в `apap_crypto.c` и применяются
после полного `decodeS16`, до channel reorder, stereo upmix и PCM ring. Положительный
CMTime переводится как `floor(value * decodedRate / timescale)` с overflow-safe
арифметикой и ограничением окном текущего AU; whole-AU trim даёт ноль PCM без ложной
ошибки декодера. TLV 10 и `FLUSHBUFFERED` этим изменением не затронуты. Host-проверка
включает 100000 сравнений с независимой формулой, 50000 malformed-parser cases,
ASan/UBSan и QNX ARM cross-build.

Поля 2 и 17 — state changes. Они применяются строго перед следующим соответствующим audio packet и
сбрасывают/rebuild converter. Cookie хранится словарём `cookieID -> retained CFData`; отсутствие
выбранного ID является ошибкой session, а не поводом продолжить со старым cookie.

## `SETMAGICCOOKIE`: доставка codec configuration

Magic cookie не находится в SETUP. Поле SETUP `ct` — это compression type, а не cookie table;
`soundCheckMediaKind` также не имеет отношения к codec config. После успешного media SETUP sender
асинхронно отправляет на **том же RTSP/HTTP control connection** custom method:

```text
SETMAGICCOOKIE <RTSP URI> RTSP/1.0
...

bplist00... {
  magicCookieID = <signed int64 / CFNumber>;
  magicCookie   = <CFData>;
}
```

Pioneer dispatcher @ `0x11386c` передаёт body pointer/length в
`CFBinaryPlistV0CreateWithData`, требует dictionary, читает `magicCookieID` через
`CFDictionaryGetInt64`, `magicCookie` как typed `CFData` и вызывает
`AirPlayReceiverSessionSetMagicCookie` @ `0x11a7c0`. Эта функция ищет stream context type `103` и
передаёт значения в `AirPlayAudioStreamSetMagicCookie` @ `0x12bd30`.

Receiver хранит первый `CFData` для каждого ID; повторный ID подтверждается, но старое значение не
заменяется. Если stream-103 context ещё отсутствует, Pioneer также возвращает success и ничего не
сохраняет. TLV 17 затем только выбирает один из уже доставленных ID перед следующим audio packet.
Следовательно, правильный порядок — принять и подтвердить `SETMAGICCOOKIE`, сохранить bounded copy,
затем разрешить TLV 17 выбрать config; неизвестный ID запрещено трактовать как пустой/default cookie.

Стоковый Audi AirPlay 210.81 не содержит строк `SETMAGICCOOKIE`, `magicCookieID`, `magicCookie` или
обеих setter-функций. Его список методов @ `0xa15a4` ограничен
`ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER, POST, GET`.
Однако добавлять custom method в OPTIONS/Public не надо: Pioneer Public list @ `0x34a0e8` также его
не рекламирует, а sender всё равно отправляет команду. Отсутствие dispatch branch/parser/storage в
Audi — доказанный protocol defect, но не найденный пятисекундный watchdog: exact teardown вызывают
50 неуспешных rate/anchor retries из-за отсутствующих `GETANCHOR`, `SETRATE`,
`SETRATEANCHORTIME` и `FLUSHBUFFERED`.

Для MU1316 найден seam без переписывания `.text`:

* `AirTunesServer_EnsureCreated` @ `0x2e9ac` формирует callback table;
* process callback равен private handler `0x302e0` и лежит в table на `+0x24`;
* `HTTPServerCreate` копирует table в heap-object на `server+0x40`, поэтому рабочий slot —
  `HTTPServer+0x64`;
* private global outer-server pointer находится по image VA `0xbabcc`.

Repository hook теперь ставит wrapper в этот slot только после проверки точного MU1316 prologue,
исходного callback и layout. Wrapper обрабатывает только точное `SETMAGICCOOKIE`, разбирает body
штатным target `CFCreateWithPlistBytes`, ограничивает cookie 1 MiB и map 16 IDs, повторяет Pioneer
first-ID-wins и отправляет обычный RTSP success/error. Любой другой method без изменений передаётся
оригинальному handler. Первая принятая CFData сохраняется mode-0600 как
`/tmp/spatial_magiccookie_<sequence>_<id-hex>.bin` для decoder bench. Изменение выключено вместе с
`SPATIAL_103=0`, а callback возвращается stock handler при teardown.

## APAC cookie и decoder API

Pioneer принимает:

* полный `dapa` atom;
* atom chain с `frma(apac)`, опциональным `apac` и затем `dapa`.

`frma` должен ровно содержать FourCC `apac`; у `apac` word на `+8` равен нулю. Внутри `dapa`
decoder payload начинается на `dapa+12`. Первые два байта payload — sync `0x0800`; GlobalConfig
word начинается только на `dapa+14`. Это важная поправка: metadata helpers нельзя вызывать на самом
sync word.

Из GlobalConfig:

```text
sampleRateIndex = (word >> 15) & 0x3f
channels        = (word >> 1)  & 0xff
frames          = 1024, если (word & 0x7e00) == 0
```

Таблица sample rate начинается `96000, 88200, 64000, 48000, 44100, ...`. Реальный Apple-generated
четырёхканальный cookie с word `0x14018008` даёт 48 kHz, 1024 frames и 4 channels; это проверено
инструментом `tools/pioneer_apac_cookie.py`.

Pioneer API:

| функция | VA |
|---|---:|
| `APACDecoder_DecodeFrame` | `0x105bb0` |
| `APACDecoder_GetCookieOffset` | `0x106030` |
| `APACDecoder_SampleRate` | `0x106150` |
| `APACDecoder_FrameLength` | `0x1061b0` |
| `APACDecoder_NumberOfChannels` | `0x106210` |
| `APACDecoder_SetParameter` | `0x106260` |
| `APACDecoder_Configure` | `0x106490` |
| `APACDecoder_Create` | `0x106800` |
| `APACDecoder_Dispose` | `0x106c10` |

`AirPlayAudioConverterSetProperty(..., 'dmgc', ...)` вызывает Configure. DecodeFrame выдаёт по 1024
frame; destination flags `12` выбирают четырёхбайтный float-like output, иначе используется
двухбайтный integer output.

На MHI2Q APAC implementation не найден. macOS `tools/apple_apac_oracle.c` — независимый
validation oracle для реального CarPlay cookie и packet boundaries, но не deployment-код для HU.

## Lifecycle, очередь и clock

Pioneer reference:

| стадия | функция | VA |
|---|---|---:|
| setup | `_BufferedAudioSetup` | `0x12c500` |
| reader thread | `_BufferedAudioThread` | `0x123f08` |
| consumer/read | `AirPlayAudioStreamReadAudio` | `0x1281b0` |
| session flush | `AirPlayReceiverSessionFlushBufferedAudio` | `0x11a710` |
| stream flush | `AirPlayAudioStreamFlushBufferedAudio` | `0x127e60` |
| teardown | `_TearDownBufferedStream` | `0x12c290` |

Setup читает `packetFormatAPAP`, compression type `ct`, `spf`, `audioFormatIndex` и legacy
`audioFormat`, опционально `soundCheckMediaKind`; создаёт bounded storage/queue, mutex, пустой cookie
dictionary, converter state, clock/skew state, server и reader thread. Сам dictionary заполняется
позднее командами `SETMAGICCOOKIE`. У Pioneer ring allocation — `0x800000`, плюс два буфера по
`0x61b0`; Audi implementation может выбрать меньшие bounds, но они должны быть жёсткими.

Reader должен:

1. собрать inclusive-BE32 frames без concatenation packet bodies;
2. проверить 15-byte header и защищённую длину;
3. аутентифицировать/decrypt;
4. применить extension state (format/cookie/SoundCheck/trim);
5. положить packet в timestamp/sequence queue;
6. разбудить consumer и остановить чтение при заполненной очереди.

Consumer привязывает media timestamp к синхронизированному AirTunes clock, учитывает trims,
underrun/gaps, late packets, flush target и медленную коррекцию drift (`SkewPlaySlower/Faster`).
Нельзя просто принимать burst и `sleep(1024/48000)`: hardware уже показал, что это меняет объём
прочитанных данных, но не предотвращает clean teardown через примерно пять секунд.

Flush хранит target timestamp + sequence и удаляет queued packets до target с wrap-aware compare.
Full reset также сбрасывает converter/APAP/cookie-current state. Teardown должен сначала остановить
session и listener, разбудить/join thread, затем закрыть sockets, уничтожить mutex, освободить queue,
buffers, converter, cookie dictionaries/current cookie и обнулить state. Повторный teardown должен
быть безопасным.

## Нативный AAC-путь на ARMv7/QNX

Текущий Audi `libairplay.so` не содержит рабочего AudioConverter:

* `AudioConverterSetProperty_compat` @ `0x3ddc8` возвращает ошибку `0xffffe5b1`;
* `AudioConverterFillComplexBuffer_compat` @ `0x3ddd4` возвращает ту же ошибку;
* `AudioConverterNew_compat` @ `0x3ddf0` выделяет пустой объект, тут же уничтожает его и возвращает
  ошибку (либо allocation error).

MH2p ARMv7/QNX содержит рабочий adapter:

| функция | VA | размер |
|---|---:|---:|
| `AudioConverterDispose` | `0xafab4` | 436 |
| `AudioConverterNew` | `0xafc68` | 856 |
| `AudioConverterReset` | `0xaffc0` | 196 |
| `AudioConverterSetProperty` | `0xb0084` | 102 |
| `AudioConverterFillComplexBuffer` | `0xb00ec` | 1344 |
| `CAudioDecoder::decode` | `0xb2874` | — |
| `CAudioDecoder::init` | `0xb2e08` | — |
| `CDecoder_AAC_LC` constructor | `0xb37b4` | — |
| `CDecoder_AAC_ELD` constructor | `0xb37e8` | — |

Converter поддерживает `aac `, `aace`, `opus` и PCM conversion. AAC-LC constructor создаёт Cinemo
decoder с MPEG-4 object type 2 и передаёт rate/channel count напрямую. Целевая Audi уже имеет ту же
codec factory и AAC plugin. Более того, целевой `dio_manager` уже содержит DT_NEEDED
`libNmeSDK.so`, то есть `CinemoCreateAudioCodec` присутствует в process image без загрузки второй
AirPlay-библиотеки. Поэтому production-решение — перенести минимальный adapter/interface в наш
hook/helper и resolve только установленный Cinemo API. Не надо заменять весь Audi `libairplay.so`
или `dlopen`-ить MH2p `libairplay.so`.

### Точный публичный ABI целевого Cinemo

У `libNmeSDK.so` есть стабильные C wrappers; именно их надо получать через `dlsym`, не копировать
MH2p C++ object/vtable и не вызывать внутренние target-адреса:

| export | VA | ABI |
|---|---:|---|
| `CinemoCreateAudioCodec` | `0x4ea44` | `int (void **outCodec)` |
| `ICinemoAudioCodec_Initialize` | `0x9eedc` | `int (codec, context, callback, inputType, outputType)` |
| `ICinemoAudioCodec_Write` | `0x9ef00` | `int (codec, bytes, size, arg3, int64 pts, arg6)` |
| `ICinemoAudioCodec_Drain` | `0x9ef2c` | `int (codec)` |
| `ICinemoAudioCodec_Flush` | `0x9ef40` | `int (codec)` |
| `ICinemoAudioCodec_Release` | `0x9eeb4` | release interface |

Factory действительно имеет один аргумент `ICinemoAudioCodec **out`: он создаёт временный объект,
делает `QueryInterface(..., ICinemoAudioCodec::iid)`, освобождает temporary reference и возвращает
`CinemoError`, где ноль означает успех. Wrapper `Initialize` передаёт ровно пять аргументов. Внутри
target codec сохраняет callback/context и выбирает decoder через
`NmeFindAudioDecoder(input.media_subtype)`. Во внешнем SDK-коде clamp количества каналов не найден.

Callback ABI точно совпал в target и MH2p:

```c
int decoded(void *context, const void *pcm, uint32_t bytes,
            const CinemoMediaType *format, int64_t pts, uint32_t flags);
```

`Write` передаёт семь ARM words с учётом 64-bit timestamp. Рабочий MH2p adapter для каждой AAC
access unit вызывает его как `Write(codec, bytes, size, 1, 0, 1)`. Семантические имена двух единиц
статически не доказаны, поэтому в implementation они остаются `mh2p_arg3`/`mh2p_arg6`, а не
придуманными flags. Для type 103 вместо постоянного нулевого PTS надо передавать восстановленный
APAP media time в согласованных единицах либо явно вести его в renderer clock; смешивать эти две
модели нельзя.

`CinemoMediaType` имеет точный размер `0x80`: target `MediaTypeInit` делает `memset(..., 0, 0x80)`.
Нужные поля:

| offset | input AAC | output PCM |
|---:|---|---|
| `+0x00` | media type `5` | media type `2` |
| `+0x04` | `0x101a` AAC-LC / `0x1022` AAC-ELD | subtype `0x1023` |
| `+0x08` | `0` | sample-format value `2` (S16) |
| `+0x0c` | `0` | `48000` |
| `+0x10` | `0` | `8` |
| `+0x74` | header type `5` | `0` |
| `+0x78` | pointer на ASC | `0` |
| `+0x7c` | ASC length `4` | `0` |

MH2p ASC builder статически и эмуляцией Thumb-инструкций даёт для 48 kHz / 8 channels точные четыре
байта:

```text
index 40 / AAC-LC / object type 2:   11 c0 00 00
index 42 / AAC-ELD / object type 39: 3e 39 c0 00
```

Этот контракт закреплён компилируемым C99 header
`hook/audio/cinemo_audio_codec_abi.h`: в нём есть fixed-width layout, offset/size assertions,
callback/factory/method typedefs и обе ASC-константы.

### Инициализация plugin registry

MH2p сначала вызывает `CinemoCreateConfig`, затем `ICinemoConfig::SetPluginDirectory`, и только
потом `CinemoCreateAudioCodec`. Target `dio_manager` уже делает ту же последовательность для своего
Cinemo iAP2 service. Значение читается из
`/system/etc/eso/production/dio_manager.json`:

```text
iap2.cinemoLib = /armle/usr/lib/cinemo
```

Именно там лежат `cinemo_classes.xml` и `libNmeAudioAAC.so`; XML регистрирует `NmeCodecAAC` и
`NmeCodecAACReference`. Поэтому hook работает в уже загруженном и сконфигурированном SDK process.
Production adapter всё равно должен fail closed: проверить все шесть wrapper symbols, нулевой
результат factory/Initialize и фактический callback format. Самостоятельно менять plugin directory
из hook не надо; это глобальное состояние процесса.

### Минимальный adapter lifecycle

1. Собрать zeroed input/output `CinemoMediaType[0x80]` по таблице выше; ASC должен жить до конца
   decoder session.
2. `CinemoCreateAudioCodec(&codec)`, затем `Initialize(codec, state, decoded, &input, &output)`.
3. На одну аутентифицированную APAP access unit вызвать
   `Write(codec, payload, payloadSize, 1, pts, 1)`.
4. Callback не должен писать в DIO из decoder-контекста: он копирует PCM в bounded PCM queue,
   проверив `format=PCM/S16/48000/8`, размер, frame alignment и session generation.
5. На format/cookie discontinuity вызвать `Flush`, пересобрать decoder; на нормальном EOS — `Drain`.
6. Teardown запрещает новые callbacks, освобождает queued PCM, вызывает `Release` ровно один раз и
   только затем уничтожает session state.

Первый обязательный host/bench gate для index 40: дать Cinemo настоящий AAC-LC 5.1.2 packet/config,
получить ровно 8 interleaved channels и проверить порядок channel-isolated материалом. До этого
поддержка восьми каналов считается сильным статическим выводом, но не runtime-фактом.

## 5.1.2 -> 5.1 downmix

Canonical Pioneer matrix `kAudioMixMatrixSpeakerConfig5_1_AInt` находится по VA `0x4316c8`, file
offset `0x4216c8`, размер 168 байт (`6 x 14` коэффициентов int16). Для входа 5.1.2 labels 0..7 она
даёт:

```text
L'   = L
R'   = R
C'   = C
LFE' = 0.5 * LFE
Ls'  = Ls + 0.5 * heightL
Rs'  = Rs + 0.5 * heightR
```

Иными словами, height energy сохраняется в surrounds на -6 dB, но вертикальная локализация
теряется. Auto attenuation в этой конфигурации выключен; при полностью коррелированных
full-scale surround + height возможна saturation. Production mixer должен использовать saturating
accumulate либо небольшой headroom/limiter, а не wraparound int16.

Для PCM6 S16LE размер output всегда `frames * 6 * 2 = frames * 12`. Для максимального 8-channel
source buffer из six-channel output capacity требуется `floor(maxOut * 8/6) = floor(maxOut * 4/3)`;
Pioneer wrapper сначала резервирует общий worst case `floor(maxOut * 10/6) = floor(maxOut * 5/3)`,
затем применяет format-specific multiplier.

`tools/pioneer_type103_patch.py` фиксирует проверяемый AArch64 donor-only рецепт:

* output count 4/5 -> 6;
* обе GOT/RELA ссылки на 4.0/4.1 matrix -> 5.1 matrix;
* source/output capacity formulas -> six-channel-safe;
* subwoofer availability -> true.

Он принимает только исходный SHA-256 выше, никогда не меняет input и создаёт только новый файл.
Детерминированный SHA-256 patched copy:
`91893c9e201d9c3b3552bb18a42fec484cc70143fc368d91f30c1bcedac3d310`.
Этот файл нельзя устанавливать на Audi; рецепт нужен для статической проверки математики и
поведения Pioneer oracle.

## Контракт реализации в Audi hook/helper

Минимальная production-архитектура:

1. Marker/config рекламирует 103 только когда receiver полностью готов; default — выключено.
2. SETUP 103 валидирует только разрешённый индекс, `packetFormatAPAP=true`, `spf=1024`, `shk` ровно
   32 байта; открывает data listener до публикации response.
3. До публикации SETUP response установить fingerprint-locked RTSP wrapper, принять
   `SETMAGICCOOKIE` и сохранить bounded first-ID-wins map; 6033 для этого не открывать.
4. Тем же wrapper обработать `GETANCHOR`, `SETRATE`, `SETRATEANCHORTIME` и `FLUSHBUFFERED`; вернуть
   anchor на NTP timeline телефона и пережить следующий 10-секундный clock-lock gate.
5. Session state хранит key, stream ID, active format, cookie map/current cookie, seq/timestamp,
   bounded queue, clock anchor/rate, flush target и stop flags.
6. Reader decrypt/authenticates APAP и применяет TLV state до queue.
7. Для index 40 минимальный Cinemo adapter декодирует AAC-LC в PCM8; index 42 остаётся AAC-ELD
   fallback, а для 65/66 отдельный APAC backend
   добавляется позже и сверяется с macOS oracle.
8. PCM8 проходит canonical 5.1 matrix; PCM6 уже готов без downmix.
9. Пока 103 active, interposer stock `AirPlayReceiverSessionReadAudio` для type 100 выдаёт готовые
   PCM6 samples из buffered renderer в существующий DIO5 path. При inactive/error/teardown он
   немедленно возвращается к исходному stock поведению type 100.
10. Flush/teardown очищают ключи, cookies, decoder и очередь; никакой stale 103 state не переживает
    новую CarPlay generation.

Этот bridge через уже доказанный type-100/DIO5 output минимизирует изменения в QNX audio stack.
Нельзя передавать encrypted/compressed APAP как PCM, нельзя объявлять неизвестный индекс как
40/42/65,
и нельзя оставлять type 100 без stock fallback при ошибке 103.

## Граница статической готовности

Закрыто статически:

* точный SETUP response contract и роль 6032/6033;
* APAP outer framing, fixed header, 24-bit sequence;
* exact crypto/key/body/AAD;
* extension IDs и state transitions;
* exact `SETMAGICCOOKIE` method/body/storage semantics и MU1316 HTTPServer callback seam;
* пятисекундный sender retry-to-terminal path и обязательные buffered-control методы;
* APAC cookie offset, sync и metadata formulas;
* setup/read/queue/clock/flush/teardown lifecycle;
* точная 5.1.2 -> 5.1 matrix и buffer sizing;
* ARMv7 crypto source и точный public AAC/Cinemo decoder ABI, media layouts и ASC;
* type-100 PCM6 fallback boundary.

Остаются только динамические доказательства и реализация:

1. завершить buffered-control implementation и подтвердить стабильный hardware-run;
2. подтвердить NTP anchor/clock lock и `SETMAGICCOOKIE` ACK;
3. реализовать/портировать настоящий APAC backend для ARMv7/QNX и начать с index 65/PCM6;
4. подключить уже реализованный 5.1.2-to-5.1 helper для index 66 и проверить continuous playback;
5. проверять output 65/66 против macOS APAC oracle до подключения к DIO5.

До выполнения этих пунктов корректная формулировка статуса: **протокол и архитектура 103
доработаны статически до implementation-ready, но playable 103 на HU ещё не реализован**.

## Инструменты

* `tools/pioneer_apap_decrypt.py` — auth/decrypt, fixed-header parsing, TLV decode, per-frame output,
  `manifest.tsv` и `manifest.jsonl`.
* `tools/pioneer_apac_cookie.py` — atom grammar, `dapa+12`, sync и GlobalConfig на `+14`.
* `tools/pioneer_magic_cookie.py` — validate/build binary-plist body `SETMAGICCOOKIE`, extract CFData.
* `tools/pioneer_airplay_formats.py` — исполнение Pioneer format tables под Unicorn.
* `tools/apple_apac_oracle.c` — macOS APAC packet/cookie oracle.
* `tools/pioneer_type103_patch.py` — fingerprint-locked donor-only 5.1 adaptation recipe.
* `hook/audio/cinemo_audio_codec_abi.h` — target C ABI, `CinemoMediaType[0x80]`, callback и ASC.
