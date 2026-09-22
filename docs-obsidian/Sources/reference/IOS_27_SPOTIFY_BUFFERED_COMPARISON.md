# Spotify buffered policy in iOS 27 build 24A5418b

Verified 2026-09-19 against the local iPhone18,1 dyld cache. The explicit Spotify deny **remains** in this build; no claim is made about other iOS 27 builds.

Input: `/Users/luka/Desktop/AUDI_2/Refferences/ios27_dsc/24A5418b__iPhone18,1/mt_out/MediaToolbox`, SHA-256 `0213a91f451f319fb243a217d48c22d6408d257e6240cc3396132d5b07990c6e`.

`airplayRoute_updateAudioSessionPolicyForBuffered` at `0x196d44ecc` reads `HostApplicationDisplayID` and `AllowsAirPlayBuffered`. The corresponding import slots `0x1e0147250` and `0x1e0146f78` were resolved through two cache pointer indirections and decoded as CFStrings from the original cache.

## Фрагмент в рамке: запрет Spotify

> **iOS 27 · 24A5418b · MediaToolbox**
>
> Функция: `airplayRoute_updateAudioSessionPolicyForBuffered` @ `0x196d44ecc`.
> Ниже — читаемый псевдокод по декомпиляции, не исходный код Apple.
> Имена переменных раскрыты; служебное логирование и освобождение объектов
> опущены. Показана ветка после успешного получения свойств аудиосессии.
>
> ```c
> // Свойства текущей аудиосессии:
> // HostApplicationDisplayID — идентификатор приложения.
> // AllowsAirPlayBuffered — разрешение buffered audio для сессии.
> // Здесь отсутствующее свойство НЕ равно явному false.
> if (allowsAirPlayBuffered != NULL &&
>     allowsAirPlayBuffered == kCFBooleanFalse) {
>     disallowsBuffered = true;
> }
> else if (FigCFEqual(hostApplicationDisplayID,
>                     CFSTR("com.spotify.client"))) {
>     // ↑ ОТДЕЛЬНЫЙ ЗАПРЕТ SPOTIFY.
>     // Даже AllowsAirPlayBuffered == true не обходит эту проверку.
>     // CFString "com.spotify.client": 0x1ea95a478.
>
>     // Диагностика, если логирование включено:
>     // "Disallowing buffered for Spotify on session %{public}@"
>     // Строка в бинарнике: 0x1975a4c46.
>
>     disallowsBuffered = true;  // Запрет действует и без логирования.
> }
> else if (FigCFEqual(hostApplicationDisplayID,
>                     CFSTR("com.espn.ScoreCenter"))) {
>     disallowsBuffered = true;
> }
> else {
>     disallowsBuffered = false;
> }
>
> // Дальше результат реально сохраняется, а маршрут пересчитывается.
> // Поле audioSessionDisallowsBuffered: байт +148 в routeStorage.
> if (routeStorage->audioSessionDisallowsBuffered != disallowsBuffered) {
>     routeStorage->audioSessionDisallowsBuffered = disallowsBuffered;
>     // Запись поля: 0x196d45340.
>
>     airplayRoute_updateRoutingRegistry(route);
>     // Вызов: 0x196d45348.
> }
> ```
>
> **Область действия:** регистрация этого FigAirPlay-клиента/маршрута
> для buffered audio. Это не доказательство запрета всех возможных
> способов воспроизведения Spotify.

### Вырезка из декомпиляции с исходными именами IDA

Следующие строки скопированы из сохранённой декомпиляции. Между ними есть
пропуски, явно отмеченные комментариями; это не непрерывный фрагмент.
`v12` — HostApplicationDisplayID, `v30` — результат запрета,
`v2` — хранилище состояния маршрута. CFString `stru_1EA95A478` прочитан
из оригинального dyld cache и равен `com.spotify.client`.

> ```c
>   else if ( (unsigned int)MEMORY[0x198114210](v12, &stru_1EA95A478) )
>   {
>     if ( dword_1E488EBB0 )
>     {
>       // [Пропущено: подготовка и вызов диагностического логирования.
>       //  Этот путь тоже устанавливает v30 = 1 через LABEL_45.]
>     }
>   }
>   // [Пропущено: else с проверкой ESPN и служебные блоки.]
>   // Ниже — общий путь запрета при выключенном логировании:
>   v30 = 1; /*0x196d45394*/
> LABEL_46:
>   MEMORY[0x1980FB970](v5); /*0x196d45318*/
>   if ( v35 ) /*0x196d45324*/
>     MEMORY[0x1980FB970](); /*0x196d45328*/
> LABEL_48:
>   if ( v30 == *(unsigned __int8 *)(v2 + 148) ) /*0x196d45334*/
>   {
>     result = 0; /*0x196d45338*/
>   }
>   else
>   {
>     *(_BYTE *)(v2 + 148) = v30; /*0x196d45340*/
>     airplayRoute_updateRoutingRegistry(a1); /*0x196d45348*/
>     result = 1; /*0x196d4534c*/
>   }
> ```

Полная декомпиляция обеих функций без вырезок:
[ios27-comparison.json](../../output/ios266-buffered-eligibility/ios27-comparison.json).

### Где запрет применяется

> ```c
> // airplayRoute_updateRoutingRegistryInternal @ 0x196d44930.
> // Читаемый псевдокод условия регистрации клиента:
> if (routingContextUUID != NULL &&
>     playbackActive &&
>     !flag160 && !fpap_disableBuffered &&
>     !audioSessionDisallowsBuffered &&  // ← Результат проверки Spotify.
>     !flag147) {
>     registerClient(registry, route, routingContextUUID);
> }
> // flag160 / flag147 оставлены без предположений об их назначении.
> // registerClient — читаемое имя косвенного вызова через таблицу методов.
> ```

The Spotify comparison uses CFString `0x1ea95a478`, decoded to `com.spotify.client`; diagnostic at `0x1975a4c46` says `Disallowing buffered for Spotify on session %{public}@`. Both logging and nonlogging branches set the deny value. At `0x196d45340` the changed value is stored in route storage byte `+148`, then `0x196d45348` updates the registry.

`airplayRoute_updateRoutingRegistryInternal` at `0x196d44930` requires this deny byte to be false before registering the route/client, along with a routing-context UUID, active playback, no `fpap_disableBuffered` preference and other internal gates. Thus the Spotify string has an active behavioral effect.

Compared with the inspected 26.6 build, this function now consumes the `AllowsAirPlayBuffered` property instead of directly inspecting the earlier category/interruption/concurrent-AirPlay inputs. The Spotify and ESPN exceptions remain separate checks, so setting `AllowsAirPlayBuffered` true alone does not bypass the Spotify check here.

Scope remains this FigAirPlay route/client registration policy. This does not prove every possible Spotify playback API is blocked, nor identify which client owns a live type-103 stream. See [26.6 audit](IOS_26_6_SPOTIFY_BUFFERED_ELIGIBILITY.md) for the vehicle observations and limits.

## Проверка текстовой выгрузки iOS 26.1 / 23B85

В указанной пользователем папке `iPhone18-3_26.1_23B85_Restore` найдена
та же функция в `MediaToolbox_34.mm:8608`, а применение её результата —
в `airplayRoute_updateRoutingRegistryInternal`, строка 8389.
Полный относительный путь от этой папки:

```text
System/Library/PrivateFrameworks/CoreAccessories.framework/System/Library/
Frameworks/MediaToolbox.framework/MediaToolbox/MediaToolbox_34.mm
```

**Подтверждено:** проверки политики существуют, результат записывается
в поле состояния `+140`, а регистрация buffered-клиента требует, чтобы
это поле было нулевым.

**Конкретный запрет Spotify по этой выгрузке не доказан.** В ней у вызовов
сравнения потеряны аргументы: сохранено только `FigCFEqual()`.
Соответствующие диагностические строки тоже отсутствуют. Последовательность
проверок очень похожа на подтверждённый код 26.6, включая две отдельные
проверки после категорий, но приписывать им Spotify/ESPN без аргументов нельзя.

> ```c
> // Вырезки из 26.1 MediaToolbox_34.mm; между ними есть пропуски.
> if (FigCFEqual())  // Аргументы сравнения отсутствуют в выгрузке!
> {
>   if (dword_1EAF17000)
>   {
>     goto LABEL_38;
>   }
>   goto LABEL_39;
> }
> // ...
> LABEL_39:
>   v13 = 1;
>   goto LABEL_40;
> // ...
> *(DerivedStorage + 140) = v13;
> airplayRoute_updateRoutingRegistry(a1);
> ```

В этой папке обнаружены текстовые `.mm`, но не бинарник MediaToolbox,
его IDB или исходный dyld cache. Для такой же строгой проверки Spotify,
как в 26.6 и 27, нужен один из этих бинарных источников именно 23B85.
