# Углы CarPlay: проверка sentinels и изменений выбора HUD-иконок

> Исторический снимок до согласования renderer. Текущие исправления, проверки и новый лист 54 типов: [MANEUVER_RENDERER_PARITY_FIXES.md](MANEUVER_RENDERER_PARITY_FIXES.md).

Проверено 2026-09-11. **Защита `+1000`, `-1000`, `-1` в ManeuverMapper сохранена.**
При повторной проверке убрана лишняя новая политика для остальных углов вне ±180°:
она превращала их в straight. Теперь для них сохранён прежний fallback left/right
по знаку. Квантизация обычных углов и выбор базовых ramp-иконок остаются.

## Что именно проверено в iOS

- **26.1 / 23B85:** предоставленный каталог `Refferences/iPhone18-3_26.1_23B85_Restore`
  содержит декомпилированные `.mm`, а не исходники Apple или полный набор Mach-O.
  Проверены тела `CarClusterUpdateManeuverInfo`, `CPManeuver`, `CPAccNavUpdate`.
  Это проверка предоставленной декомпиляции; повторное дизассемблирование точного
  accessoryd 23B85 по этому каталогу не выполнено.
- **27.0 / 24A5418b:** BuildManifest указанного IPSW подтверждает iPhone18,1 и 24A5418b.
  Проверены Mach-O accessoryd и CarPlay из извлечённых файлов этого build;
  CarPlay повторно извлечён из соответствующего dyld cache с применением slide info.
  Неясные ObjC-вызовы дополнительно разрешены по инструкциям stub в самом cache.

Хеши источников: [source-sha256.json](../../output/ios-sentinel/source-sha256.json).
BuildManifest: [ipsw-build.json](../../output/ios-sentinel/ipsw-build.json).

### Apple Maps / CarPlay API

В `CarClusterUpdateManeuverInfo.mm` 23B85, строки 248–280 и 342–366,
exit angle устанавливается из junction element с role/type `2`. Для U-turn
корректируется знак с учётом driving side. Если такого элемента нет, эти ветви
не устанавливают exit angle; `1000` или `-1000` здесь не создаются.

В `CPManeuver.mm`, строки 636–643, `junctionExitAngle` связывается с параметром
`11` и единицей `NSUnitAngle.degrees`. `CPAccNavUpdate.mm`, строки 303–377,
сбрасывает объектные свойства в nil; строки 550–572 переводят measurement
в нужную единицу и далее в числовое представление.

В CarPlay 24A5418b подтверждена та же схема:

| Адрес | Наблюдаемое действие |
|---|---|
| `0x2471643f4` | selector `junctionExitAngle` |
| `0x24716440c` / `0x247164410` | key 11 / `paramKey:` |
| `0x247164424` / `0x247164438` | `degrees` / `copySettingDimension:` |
| `0x247123e0c` | nil при reset объектного свойства (`X2=0`) |
| `0x24712508c` | отсутствующее значение не проходит в numeric/measurement conversion |
| `0x247124f54` / `0x247124f60` | `measurementByConvertingToUnit:` / `doubleValue` |
| `0x247124f7c` | `NSNumberForCPAccNavTypeFromDouble` |

Инструкции: [параметр exit angle](../../output/ios-sentinel/cp_exit_angle_param.asm),
[reset](../../output/ios-sentinel/cp_reset.asm),
[measurement](../../output/ios-sentinel/cp_measurement.asm),
[разрешённые selectors](../../output/ios-sentinel/objc-stubs.asm).

### Реальная iAP2-сериализация

accessoryd 24A5418b:

- `_iap2_navigation_routeGuidanceManeuverUpdateHandler` начинается в `0x1001755a0`.
- `0x100175a00`–`0x100175a14`: dictionary lookup для ключа 11 и вызов
  `__addU16NumberParameterToMsg`.
- Helper `0x1000038d0`: при nil возвращает 0 без TLV; иначе вызывает
  `CFNumberGetValue(..., kCFNumberSInt16Type, ...)`. При успешном преобразовании
  передаёт сохранённые 16 бит в `iAP2MsgAddU16Param`.
- Здесь нет замены missing на ±1000, проверки диапазона ±180 или выбора направления.
  Переданные целые `1000`, `-1000`, `-1`, `0` проходят как обычные signed16 значения.

Сохранены [декомпиляция сериализатора](../../output/ios-sentinel/ios27-maneuver-serializer.c)
и [helper с уточнёнными типами](../../output/ios-sentinel/ios27-signed16-helper.c).

**Вывод ограничен проверенными путями:** нет оснований называть ±1000 обязательным
Apple wire-значением «нет угла». Но это не повод удалять нашу защиту: optional поле
действительно может отсутствовать, и уже переданное sentinel-значение тоже может
дойти до получателя. `1000` не означает «ехать прямо».

## Наша цепочка и сохранённые гарантии

| Значение / ситуация | Обработка |
|---|---|
| TLV exit angle присутствует | `rgd_tlv.c` читает signed BE16 без изменения числа |
| Индексированный манёвр без TLV exit angle | Наш C-парсер создаёт **+1000**, совместимо с исследованным MHI3 |
| `m*_turn_angle`, `m*_exit_angle` | Hook публикует одно и то же `man->exit_angle` в оба поля |
| `-1000` | Сохранённый защитный случай Java; C-парсер не синтезирует отрицательный sentinel |
| `-1` | Legacy default незаполненного `mTurnAngle`; mapper сохраняет защиту |
| `0` | Настоящий нулевой угол; при выборе стороны сам по себе не задаёт left/right |

`-1` также **может быть настоящим signed16 числом на проводе**. Старое утверждение
«это не wire value вообще» было слишком сильным. Нынешний scalar `mTurnAngle`
не различает настоящий −1° и незаполненный слот; существующую политику mapper
в этом пересмотре не меняли. `mExitAngle` имеет другой initial default, +1000.

Для явных left/right/slight/sharp типов направление определяется типом даже при
неизвестном угле. Для U-turn и generic ramp используется прежний traffic-side
fallback. Для generic 11/16/51 sentinels дают прежний straight fallback.
В roundabout-квантизаторе sentinels отсекаются **до** clamp к ±180°.
Для end-of-road left `-1` явно исключён, чтобы сохранение fine directions
не превратило незаполненный слот в slight-left.

C-parser, hook, State, RendererMapper, SideStreets побайтно совпадают с HEAD.
В production изменён только ManeuverMapper в рамках выбора иконок.

## Проверка изменений

- Новая проверка всех 54 типов × 2 traffic sides × 3 junction types × 3 sentinels:
  **972 случая, 1944 проверки main/direction**. Каждый sentinel использует тот же
  categorical/traffic fallback, что и ноль; знак sentinel не становится стороной поворота.
- Дополнительно проверены границы квантизации и сохранение прежнего поведения вне ±180°,
  включая `Integer.MIN_VALUE/MAX_VALUE` без переполнения при взятии модуля.
- C-тест проверяет signed16 exit/junction values от −32768 до 32767, включая ±1000,
  −1, ноль и соседние значения, и отдельно отсутствие поля. **ASan/UBSan PASS**,
  существующие 120000 malformed-input cases также PASS.
- Java 1.4 build, RendererMapperDirectionTest, RouteGuidanceDeltaTest,
  RouteInfoPresentationTest и DistanceBargraphChainTest — **PASS**.
- После восстановления явных guards все **7890 bridge cases + 216 ramp cases +
  108 примеров** побайтно совпали с результатами до этого восстановления.
  [Хеши повторной проверки](../../output/ios-sentinel/matrix-recheck.json).
- Итог пересмотра иконок прежний: 8 типов уточнены, 46 сохранены; новых пустых
  3D-результатов нет. [Все 54 решения](CARPLAY_HUD_ICON_SELECTION.md).

## Существующие расхождения, не внесённые этим изменением

1. `SideStreets` отсекает только +1000. Для type 28 / roundabout / RHT / −1000
   тестовая матрица даёт side streets `60 40 20 00 e0 c0 a0`, а для +1000 — пусто.
   Требуется отдельное симметричное подавление неизвестной геометрии.
2. RendererMapper не распознаёт legacy −1 так же, как BAP mapper. Например,
   type 4 / U-turn / LHT / −1: BAP выбирает right, renderer получает direction −1.
   Перед унификацией нужно учитывать коллизию с настоящим wire −1°;
   для `mTurnAngle` полезно отделить наличие поля от числового значения.

Оба расхождения воспроизводятся до и после выбора HUD-иконок. Этот отчёт не
объявляет renderer и HUD полностью сопоставленными 1:1.
