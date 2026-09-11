# Декомпиляция MST2 JAR и дополнение к сравнению altscreen

2026-09-10. Декомпилированы **все 9 классов** из двух JAR пакета `/Users/luka/Downloads/rev1-rev2-withsource-edited-work/package/custom/java`.

## Файлы результата

- `MST2_H29_EU_HMI_Return_v10/`: Java, восстановленная CFR 0.152, один класс.
- `NavActiveIgnore/`: Java, восстановленная CFR 0.152, восемь классов.
- `vineflower/`: независимая декомпиляция тех же девяти классов Vineflower 1.12.0.
- `bytecode/`: `javap -p -c -s` для каждого исходного класса; дополнительно ASLHandler локального reference.
- `manifest.json`: исходные пути, SHA-256 JAR и точный состав классов.
- `reference_build_v8/`, `reference_build_v8_selected.jar`: выборка из локального MIBHMI.jxe для сопоставления.
- `diffs_vs_build_v8/`: текстовые diff для шести совпавших классов верхнего уровня.

Оба декомпилятора выдали тела всех классов без заглушек о невозможности декомпиляции. Это восстановленный код для анализа, не оригинальный buildable source project: исходные JAR не содержат всех зависимостей и некоторых nested classes. Например, CFR/Vineflower оставляют ссылки на отсутствующие `DSICarplayProxy$3/$4`. Не следует принимать различия в локальных переменных, форме условий и throws после повторной компиляции/декомпиляции за изменения поведения.

## HMI_Return_v10: конкретное изменение

[ASLHandler.java](MST2_H29_EU_HMI_Return_v10/de/vw/mib/asl/internal/carplay/target/ASLHandler.java:222) вызывает `handleAslApiLeaveCarPlayCanvas()` в начале `handleAslApiASL_SMARTPHONEINTEGRATION_CONNECTION_LOST_CARPLAY()`, **до изменения connectionState**.

Вызванный метод:

1. Выставляет `canvasVisible=false`.
2. При connectionState 0 или 1 возвращается.
3. В остальных состояниях, если экран принадлежит устройству (`currentScreenSource==2`) либо активен video borrow, выполняет `takeScreen("Take Screen: Carplay canvas was left")` и выставляет screen source 1 (HMI).

Вызов использует состояние владения экраном до последующей очистки флагов disconnect, включая videoBorrowState. На найденном reference метод disconnect **не содержит этого вызова**. Это подтверждено байткодом: в v10 `invokevirtual handleAslApiLeaveCarPlayCanvas` находится на offset **46**, затем `setConnectionState` на **131/153**; в reference есть только `setConnectionState` на **130/152**. Сам переход connectionState 3→2 не запрещает helper: его ранний выход относится только к 0/1.

Это управление главным CarPlay canvas/HMI. Сам класс не содержит отдельного приёмника altScreen, MOST muxer или нашего аналога Java AltScreenModule с viewArea/zoom. Возврат видеопотока приборки остаётся задачей native RingIO bridge.

Reference взят из `/Users/luka/Downloads/build-v8/fs/tsd/hmi/ifs/MIBHMI.jxe`, SHA-256 `5306639404b1050cf4f6c80ee2cde1544c2593197d99dabf82d204687b6626ee`. Из 19446 ROM-классов выбраны 7, включая один nested class. Это локальная сравнительная база, а не доказательство идентичности прошивке конкретного автомобиля. Три esolutions-класса в этом JXE отсутствуют, поэтому точный stock diff для них здесь не заявляется.

## NavActiveIgnore: фактическое поведение

| Класс | Что делает код |
|---|---|
| `CarPlayModeHandling` | В ветке новой активной навигации CarPlay логирует переход, но не вызывает остановку штатного ведения и не выставляет activeNavigation=2; неактивная ветка по-прежнему сбрасывает состояние |
| `DSICarplayProxy` | В `requestModeChange` проверяет все AppStateRequest; при `appStateID==2 && state && speechMode==0` блокирует **весь вызов** до RPC |
| Android Auto `NavigationListener` | `updateGuidanceActive` и `stopGuidance` только логируют ignored; `isGuidanceActive()` всегда false |
| Android Auto `RequestHandler` | При navFocus 1/2 помечает AA navigation active, отправляет `navFocusNotification(2,false)` при подключённом DSI2; штатную stopGuidance не вызывает |
| `ASLNavigationServices` | `stopGuidance(6)` не делегирует target; остальные причины делегирует; метод всегда возвращает 1 |
| `ExboxGuidanceManagerImpl` | Добавляет println/stack trace вокруг запросов/событий guidance; состояние и уведомления продолжает передавать |
| `DSIAndroidAutoDispatcher` | Продолжает dispatch событий; при setExternalDestination печатает параметры и stack trace |
| `DSIAndroidAuto2Dispatcher` | Продолжает dispatch, включая navFocus/next-turn; при setExternalDestination печатает параметры и stack trace |

Ключевые ссылки:

- [CarPlayModeHandling: обработка navigation](NavActiveIgnore/de/vw/mib/asl/internal/carplay/common/CarPlayModeHandling.java:212).
- [DSICarplayProxy: фильтр целого запроса](NavActiveIgnore/de/esolutions/fw/comm/dsi/carplay/impl/DSICarplayProxy.java:111).
- [NavigationListener: ignored/false](NavActiveIgnore/de/vw/mib/asl/internal/androidauto/target/NavigationListener.java:37).
- [ASLNavigationServices: stopGuidance](NavActiveIgnore/de/vw/mib/asl/internal/navigation/api/impl/ASLNavigationServices.java:199).

**Существенное следствие фильтра:** если в одном requestModeChange вместе с запрещённым AppStateRequest переданы ResourceRequest для экрана/аудио или другие app states, до native не дойдёт весь пакет, а не только один navigation элемент. Это прямо следует из `return` перед `remoteCallMethod(9, ...)`; наличие такого смешанного запроса на автомобиле здесь не проверялось.

В NavActiveIgnore также сохранены безусловные `System.out.println` и `new Exception("Stack trace").printStackTrace()` в ряде путей. Следовательно, подавление native capture/logging в rev1 не делает весь комплект тихим: Java всё ещё генерирует диагностику.

## Сравнение с текущим репозиторием

У MST2 Java выполняет возврат главного canvas и вмешивается в согласование/взаимное исключение навигации. По коду назначение NavActiveIgnore — позволить штатному навигационному пути сосуществовать с телефонной навигацией; фактический эффект зависит от связанной штатной прошивки.

В нашем MHI2Q `java_patch/com/luka/carplay/core/AltScreenModule.java` непосредственно владеет контекстами 80/81, переключает Full/Sport/Classic и посылает команды zoom через CarplayBus; RGD связывается с показом манёвров. Поэтому найденные JAR не эквивалентны Java-модулю текущего репозитория и сами по себе не добавляют эти функции.

Основной [отчёт о native altscreen](../altscreen_comparison_2026-09-10.md) дополнен ссылкой на эту декомпиляцию. Исходные JAR и исходники проекта не изменялись; выполнение на HU и пересборка не проводились.
