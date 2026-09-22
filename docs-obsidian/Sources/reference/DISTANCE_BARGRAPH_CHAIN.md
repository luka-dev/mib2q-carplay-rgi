# Дистанция и bargraph: от iAP2 до видимости в VC

Дата: 2026-09-11. Исходный commit: `28327b6`.

**Дистанция не пропадает из нашего BAP-пакета при включении bargraph. Её скрывает
отдельное условие в исследованном `hmi_app.elf` приборки AU491/A5_2018.**
Предыдущее утверждение в `NAVSD_FCTID_MATRIX.md`, что исправленный
`ClusterService` уже разрешает совместное отображение в VC, было неверным:
этот класс находится на ГУ. Код CarPlay передаёт FctID 18 через другой путь.

Ни прошивка, ни режим отправки BAP в этом исследовании не изменены. Добавлены
воспроизводимые проверки и исправлены пояснения. Образ установленной в машине
приборки не считывался; адреса ниже относятся только к указанному локальному ELF.

## 1. Источник дистанции и момент включения шкалы

```text
iPhone: iAP2 0x5201, TLV 0x000A (метры до манёвра)
  → rgd_tlv.c: dist_to_maneuver
  → rgd_hook.c: dist_maneuver_m
  → CarplayBus → RouteGuidance.State.distManeuverM
  → BAPBridge: дистанция + bargraphOn + процент
       ├─ AppConnectorNavi → NavSD FctID 18 → приборка / другие BAP-потребители
       └─ RendererServer → CMD_BARGRAPH → maneuver_render → MOST-видеокадр VC
```

Исходники: `hook/routeguidance/rgd_tlv.c`, `rgd_hook.c`,
`java_patch/com/luka/carplay/rgd/{RouteGuidance,BAPBridge}.java`.

Отдельно 0x5202/TLV 0x0005 даёт исходную длину шага: `distance_between`
→ `mN_distance` → `State.mDistance[N]`. Она задаёт знаменатель шкалы,
а не заменяет оставшуюся дистанцию.

В `BAPBridge.getBargraphDenominatorM` окно шкалы ограничено 15% от prepare:
225 м для city и 450 м для highway. Известный шаг длиннее 2000 м считается
highway; при неизвестной длине используется эвристика типа манёвра.
При известном коротком шаге знаменатель равен длине этого шага; иначе —
соответствующему пределу 225/450 м. Без пригодного индекса манёвра шкала выключена.

При `0 < distM <= denominator` включается `bargraphOn`,
процент равен `distM * 100 / denominator`. Скорость автомобиля здесь не участвует.
Ниже 20% worker каждые 600 мс переключает процент между 100 и 0.
**В обеих фазах мигания `bargraphOn=true`, а положительная дистанция сохраняется.**
Поэтому пустая фаза шкалы сама по себе не возвращает число в VC.

## 2. Что действительно отправляет BAP

`BAPBridge.sendDistanceToManeuverRaw` передаёт результат штатного
`BAPDistanceFormatter` в:

```java
appConnectorNavi.updateDistanceToNextManeuver(value, unit, bargraphOn, percent);
```

В штатном `AppConnectorNavi.updateDistanceToNextManeuver` валидность расстояния
определяется только `value != -1`. `bargraphOnOff` и `bargraph` присваиваются
независимо. Отправляется `DistanceToNextManeuver_Status`, NavSD LSG 50 / FctID 18.

Реальный сериализатор формирует восемь байт status payload:

| Смещение | Поле | Пример: 150 м, шкала 75% включена |
|---|---|---|
| 0–3 | Distance, uint32 little endian | `dc 05 00 00` = 1500 |
| 4 | Unit | `00` = метры |
| 5 | BargraphOnOff | `01` |
| 6 | Bargraph, 0–100 | `4b` = 75 |
| 7 | ValidityInformation, bit 0 | `01` = дистанция действительна |

Полный payload: `dc 05 00 00 00 01 4b 01`.
Значение дистанции на проводе — отображаемое число × 10, согласно BAP;
это не сырые метры iAP2. Порядок байтов штатного BAP payload также отличается
от big endian заголовка нашего localhost CarplayBus.

Определение независимых полей в протоколе не обязывает HMI показывать их
одновременно. Значения BargraphOnOff 2–254 зарезервированы: использовать их
для обхода условия не следует.

Проверка `tests/DistanceBargraphChainTest.java` выполняет настоящий BAPBridge,
штатный AppConnectorNavi и штатный сериализатор. Подменены граница отправки
BAP и renderer; требующие работающего framework конструкторы пропущены через
`Unsafe`. Для этого отдельного host-процесса нужен `-Xverify:none`, поскольку
HotSpot отвергает `invokespecial` в реконструированном из IBM J9 JXE классе
`BAPFunctionPropertyFSG`. Это исследовательская проверка содержимого сообщения,
а не подтверждение совместимости реконструированных классов с HotSpot.

Пять сценариев прошли: 150 м/75%, 30 м/100%, 30 м/0%, 300 м/шкала выключена,
нулевая дистанция/очистка. Включённая шкала и обе фазы мигания сохраняют valid=1.

## 3. Почему правка ClusterService не решает отображение VC

`core/RgdModule.java` получает framework-сервис `CombiBAPServiceNavi` и передаёт
его `RouteGuidance` → `BAPBridge`. Это прямой CarPlay-отправитель.

`core/AltScreenNavStatusGate.java` устанавливает `GatedCombiService` только
на штатный `Navigation.ClusterService.combiBAPListener.combiservice`.
Штатные записи FctID 18 блокируются при владении CarPlay; прямой отправитель
CarPlay остаётся доступен.

В `java_patch/de/audi/tghu/navi/app/cluster/ClusterService.java` условие
`!showBargraph || AltScreenModule.isConnected()` делает действительной
**Java-модель ГУ** `MODEL_DIST_TO_MANEUVER` (64); модель bargraph имеет номер 65.
Это не предикат ARM-приложения приборки.
Кроме того, штатная ветка MOST/KOMO сохраняет отдельный `flag1 = !showBargraph`,
передаваемый в `refreshDistanceToNextManeuverMOST` → `komoService.setDistanceToNextManeuver`.
Нельзя объявлять эти три пути одним и тем же потребителем.

## 4. Реальное условие скрытия в приборке

Исследованный файл:
`/Users/luka/Desktop/AUDI_2/Patches/UiModVC/A5_2018_unpacked/hmi_app.elf`.

- SHA-256: `d2df7d51e7dc6e9d16f0eb063142cb39212e32148b729041d318489bbbe9dc9b`.
- ELF32 ARM little endian, QNX interpreter `/usr/lib/ldqnx.so.2`.
- Первый PT_LOAD: file offset 0, VA `0x100000`; приведённый код находится в нём.

Имена DP из `dp_monitor.cfg`:

| DP ID | Значение |
|---|---|
| `0x02000066` | DistanceToNextManeuver: BargraphOn |
| `0x02000067` | DistanceToNextManeuver: Distance, строка |
| `0x02000068` | DistanceToNextManeuver: Unit |
| `0x02000069` | DistanceToNextManeuver: ValidityInformation |
| `0x02000070` | RG_Status |
| `0x02000071` | RG_Status_available |

Функция **VA `0x5eca80`** читает пять числовых DP через `0x69ecd0` и возвращает:

```c
visible = rg_available != 0
       && rg_status == 1
       && bargraph_on == 0
       && distance_valid != 0
       && unit <= 5;
```

Последнее сравнение в ARM знаковое, значения прочитаны как int16. Для допустимых
BAP unit 0–5 и unsupported=255 это соответствует показанному смыслу.

Решающая ветка:

```text
0x5ecaec  ldrsh r3, [sp, 0xa]     ; BargraphOn
0x5ecaf0  cmp   r3, 0
0x5ecaf4  bne   0x5ecb18          ; return false
```

Ветка находится по **file offset `0x4ecaf4`**, байты `07 00 00 1a`.
Callback `0x5f0d58` вызывает этот предикат и передаёт результат функции
видимости widget `0x654830`. Последняя содержит диагностическую строку
`m_visibleByModel.m_DPID` и в прямом режиме записывает visible по `widget+0xa0`.

`tools/verify_vc_distance_visibility.py` исполняет настоящий ARM-код предиката,
callback и setter через Unicorn. Подменены только чтение DP и виртуальное
получение widget. Проверены **336 комбинаций** RG available/status,
BargraphOn, distance validity, unit и исходной видимости widget:

| Active RG, valid distance, meters | Видимость после callback |
|---|---|
| BargraphOn = 0 | 1 |
| BargraphOn = 1 | 0 |

Проверка относится к прямому режиму setter; отдельная ветка model-bound setter
не исполнялась. Она не меняет факт, что callback передаёт false при BargraphOn=1.
Это проверка логики, а не рендер всего интерфейса. Хеш исходного ELF до и после
совпадает; исходный файл не менялся. Доставка BAP по реальному автомобилю и
геометрия всех вариантов экрана остаются предметом проверки на устройстве.

## 5. Где рисуется наш прогресс

В ветке `experiment/arrow-progress` после BAP-вызова
`BAPBridge.sendDistanceToManeuverRaw` отправляет уровень `percent * 16 / 100`
и явную фазу через `RendererServer.sendProgress`. Это прежняя 48-байтовая
команда `0x06`; флаг `0x20` отмечает фазу в payload[2]. Снимок манёвра
несёт ту же фазу в payload[42]. BAP-данные для штатного HUD сохранены.

`maneuver_render/main.c` передаёт прогресс в шейдер самой стрелки:
уровень 16 соответствует пустой заливке, 0 — полной. При мигании вся стрелка
меняет цвет по фазе Java/HUD. Отдельного столбика, выбора стороны и native-таймера
мигания больше нет. Для старых пакетов mode=1 управляет заливкой, mode=0
отключает прогресс; mode=2 без явной фазы оставляет базовый цвет.

Заливка распределяется по видимой длине пути с учётом текущего crop VC.
Смена split/big сохраняет анимацию видимой области 250 мс. Кадр displayable 98
по-прежнему маршрутизируется на cluster/MOST через контексты ГУ 80/81.
Подробности и визуальные проверки: [Arrow progress](../ARROW_PROGRESS_EXPERIMENT.md).

Команды для передачи числовой дистанции/единицы и отрисовки этого текста
в текущем протоколе renderer нет. Исследование штатных HUD/VC и варианты
изменения видимости числовой дистанции ниже сохраняют прежний смысл.

## 6. Варианты реализации

### A. Сохранить BAP и разрешить число в прошивке VC

Убрать только `bargraph_on == 0` из найденного предиката. В этом образе точка
кандидатной правки — условный переход `0x5ecaf4`; остальные проверки RG,
validity и unit должны сохраниться. Тогда текущая отправка продолжает включать
bargraph, а этот callback перестаёт скрывать штатную дистанцию VC.

Преимущество: сохраняется семантика BAP для других потребителей, в том числе
HUD. Ограничения: привязка к конкретной прошивке, проверка расположения текста
и шкалы, показ во всех нужных экранах. Без отдельного условия CarPlay изменение
затронет и штатную навигацию. Патч прошивки не создавался и не применялся.

### B. Только ГУ: штатная дистанция VC плюс наша шкала

Разделить два выхода в `BAPBridge.sendDistanceToManeuverRaw`:

```text
BAP:      действительная distance/unit, BargraphOnOff=0, Bargraph=0
Renderer: исходные logicalBargraphOn и percent, включая мигание
```

Нельзя просто обнулить общий `bargraphOn`: сейчас от него зависит и renderer,
поэтому пропадут обе шкалы. При корректном разделении VC получает условие для
показа штатной дистанции, а наш видеокадр сохраняет bargraph. Для popup нужно
исправить размещение шкалы в crop.

Это наиболее прямой вариант для текущего VC-рендеринга без изменения прошивки
приборки. Но отправитель один для BAP-потребителей: выключение BAP bargraph
также отключит его у HUD, если тот показывает шкалу по этому сообщению.
Локальная шкала в MOST-кадре VC не заменяет шкалу HUD.

### C. Оставить BAP bargraph и рисовать число в нашем кадре

Передавать форматированную distance/unit отдельной командой renderer, добавить
отрисовку цифр и единиц, разметку для popup/sidescreen, очистку и повторную
отправку после подключения. Штатное число VC останется скрытым, но своё число
будет рядом со шкалой. Семантика BAP/HUD сохранится, прошивка VC не понадобится;
объём реализации больше, чем у B.

Выбор зависит от требования к HUD и к использованию именно штатного текста VC.
Сам BAP-пакет уже содержит оба значения; изменение порогов или повторная
отправка valid=1 не обойдут обнаруженный предикат.

Продолжение: [отдельная проверка прошивки HUD](HUD_DISTANCE_BARGRAPH_CHAIN.md).
Её GDC-renderer способен одновременно рисовать штатные число и шкалу
(216 проверок на двух версиях), но преобразование BAP в его внутренний пакет
пока не прослежено. Правка VC или добавление числа в наш MOST-renderer
не решает совместный показ на самой проекции.

## Воспроизведение и источники

```sh
bash scripts/test_route_info.sh
python3 tools/verify_vc_distance_visibility.py \
  /Users/luka/Desktop/AUDI_2/Patches/UiModVC/A5_2018_unpacked/hmi_app.elf
```

Для ARM-проверки использован host Python с Unicorn 2.1.4. Скрипт требует точного
SHA-256 и отказывается запускаться на другом образе.

Локальные источники вне репозитория:

- Штатные `AppConnectorNavi.java`, `BAPDistanceFormatter.java`,
  `DistanceToNextManeuver_Status.java` и `ByteArrayStream.java`:
  `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-combined-vf/`.
  Исполняемые stock-классы: соседний `MU1316-combined-final.jar`.
- BAP NAV SD v2.80, раздел 8 DistanceToNextManeuver:
  `/Users/luka/Desktop/AUDI_2/Refferences/476121999-BAP-FC-NAV-SD-P30DF48-v2-80-F.md`.
- DP monitor:
  `/Users/luka/Desktop/AUDI_2/Refferences/VC_AUDI/nand_extraction/extract_app/KI_FPK_AU491/gss-applications/8/default/app.img/dp/dp_monitor.cfg`.
- Индекс чтений сигналов того же хешированного ELF:
  `/Users/luka/Desktop/AUDI_2/Patches/UiModVC/vc_native_studio/assets/runtime/hmi_signal_usage.json`.
- Логи исполнения Java/ARM и radare2-дизассемблирование:
  `/Users/luka/Desktop/AUDI_2/LocalLogs/reviews/distance-bargraph-chain-2026-09-11/`.
