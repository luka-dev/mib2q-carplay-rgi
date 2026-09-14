# Java: сверка с обновлённым MU1316

Дата: 2026-09-14. Рабочее дерево: `mhi2-carplay-cursor`, ветка
`experiment/hu-touch-cursor`. Проверены все 43 исходника `java_patch/`, включая
9 полных замен штатных классов; сборка содержит 152 класса Java 1.4 (48.0).
Это аудит экспериментального дерева. Параллельные незакоммиченные изменения
в основной папке `mhi2-carplay` не включены и не изменялись.

## Эталон

Корень инструментов: `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar`.

| Вход | SHA-256 |
| --- | --- |
| `out/MU1316-final.jar` | `7ad6744c75985b7757be00a1a701b4efafbcef796b481bf34c535b9d351688f0` |
| `out/MU1316-combined.jar` | `bbfd4f4adca0f88852e4aa090f2b1da7c692c1b47e9279a0a8625572b5bd05dd` |
| `libs/jcl/MHI2Q_US_AUG22_P5087_MU1316/jcl.jar` | `33514c851b1be807acddf6bedf370e60e7f91e3ebf180acfcc38d21ce9a3abea` |

Исходники: `out/MU1316-vf`. Сборка и тесты переведены на новые имена.
`final.jar` — вход для декомпиляции/компиляции, его классы не копируются в
поставляемый JAR. AccessInline разворачивает `access$…` и меняет пригодность
байткода для исполнения, поэтому обратные ссылки дополнительно проверяются
по `combined.jar` до этого преобразования.

Свежий VF оставляет конструкции `class$… == null ? (class$… = class$(…)) : class$…`,
не всегда сохраняя сам метод `class$(String)`. Для сравнения и восстановления
тестового эталона `tests/java_reference.py` заменяет их обычными `Type.class`.
Исходный декомпилят не редактируется. Пользовательское правило для артефактов
`"isNativeLittleEndian"` / `"isNativeLittleEdian"` соблюдено: при переносе это `""`.
В поставляемых Java-исходниках этих строк и заглушек декомпилятора нет.

## Полные замены

Просмотрены нормализованные различия каждой замены с новым исходником:

| Класс | Сохраняемые отличия от stock |
| --- | --- |
| `AppConnectorTerminalMode` | CarPlay cover art и мультиплексирование со штатным Media provider |
| `ParkingPartialPopupHandler` | Защита SMALL_STAGE для конкретного OPS popup 2100008 |
| `ExternalEventsListener` | Парный OPS guard для SMALL_STAGE/AP 1002, сохранение штатных остальных событий |
| `TerminalModeBapCombi` | Lifecycle CarPlay, bus, merge метаданных/обложек с очисткой между сессиями |
| `TMDeviceControl` | Перехват модели идентичности/выбора Smartphone Integration через `HuMenuIdentity` |
| `CarplayDSILifecycleController` | Узкий NAVI-ignore, разделение MFW/DDS, touchpad/cursor; штатные ресурсы и аудио сохранены |
| `DisplayManagerMIB2High` | Контексты VC 80/81, владелец terminal 1, managed cursor 103 в HU ctx 78; G24 без курсора |
| `ClusterService` | BAP gate и altScreen listener, доступ к RGI, валидная дистанция одновременно с bargraph |
| `CombiMapController` | Передача штатной геометрии и View size в altScreen, сохранение управления stock KDK |

Неожиданной потери штатных ветвей в этих сравнениях не обнаружено.
Нормализация Java 1.4, имена переменных, именованные константы и восстановление
anonymous captures отделены от функциональных изменений при ручном просмотре.
Это не математическое доказательство эквивалентности всех девяти классов.

Для `TMDeviceControl` проверка строже: независимо восстановленный новый эталон
компилируется отдельно. Сравнение 30 классов семейства после удаления только
метода `Active.updateMainWizardModel` даёт одинаковый нормализованный байткод.
Проверены 16 одноаргументных конструкторов состояний, которые использует
`InnerClassInflator`, и 221 метод. ASM используется только в host-тесте;
`PatchMainWizard.java` в поставке/сборке отсутствует.

## Исправлено при этой сверке

### Реальный владелец dataRate

`BAPBridge.forceGfxAvailable()` искал поле `dataRate` в `KOMOService` и его
родителях. Поля там нет. Оно принадлежит `ClusterViewMode`. Старый код мог
включить gfx с устаревшим сохранённым rate, а затем отдельно записать выходной
rate, оставив внутреннее состояние несогласованным.

Теперь сначала вызывается штатный `KOMOService.updateDataRate(rate, 1)`,
который обновляет cache в `ClusterViewMode` и вызывает `ClusterService.setKOMODataRate`.
Затем вызывается `updateGfxState(state, 1)`: при включении он уже использует
правильное сохранённое значение. При отсутствии ссылки на KOMO используются
публичные setter-методы через `csRef.getClusterViewMode()`.

Удалена цепочка из трёх reflection-обходов, включая попытку прямой записи
`gfxAvailable`, обходившую штатные побочные эффекты. Guard MOST/FPK внутри
`ClusterService.setKOMODataRate` не изменён: прямой вызов не отменяет его.

`KomoGraphicsStateTest` исполняет настоящие методы MU1316 `KOMOService` и
`ClusterViewMode`; заменены только запуск framework и выходной получатель rate.
Проверяются cache, gfx, каждое опубликованное значение rate, повторное
включение/отключение, переподключение и путь без ссылки на KOMO.

### Форматирование дистанции без reflection

В обновлённом JAR восстановлена метаинформация публичного вложенного типа
`BAPDistanceFormatter.BAPDistance`. Теперь используется этот тип и прямые
`getValue()` / `getUnit()`. Стоковые алгоритмы форматирования, invalid distance,
перевод единиц и сериализация BAP остались прежними.

Единственный оставшийся reflection-доступ к штатным полям —
`CoverArtProviderMux` → `AbstractPictureManager.pictureProviders`. Поле `Map`
подтверждено новым исходником; публичного getter для сохранения штатного
provider нет. Этот доступ сохранён, чтобы CarPlay не вытеснял Media cover art.

## Проверки

`bash scripts/audit_java_stock.sh` заново собирает JAR, сохраняет исходную
инвентаризацию/хеши/диффы в `build/java-stock-audit`, затем проверяет ссылки
в обе стороны, включая наследование, static/instance и доступ к членам.

| Проверка | Результат |
| --- | --- |
| Patch → final / J9 | 11 159 ссылок, 0 ошибок |
| Остальной final → patch | 340 ссылок, 0 ошибок |
| Patch → combined / J9 | 11 159 ссылок, 0 ошибок |
| Остальной combined → patch | 330 ссылок, 0 ошибок |
| Просмотр оставшегося stock | 35 095 классов на проход |
| `test_hu_startup.sh` | TM replacement, 57 startup и 14 реальных cursor lifecycle checks, native video observer — PASS |
| `test_route_info.sh` | Route presentation 1363, maneuver parity 1088, delta/sentinels, distance/bargraph/blink, viewport, lanes/recovery, C→Java contract — PASS |
| `KomoGraphicsStateTest` | Cache/pacing/repeat/reconnect/fallback по исходному combined JAR — PASS |
| `test_amp_control_contract.py` | 4 статических контракта — PASS |
| `check_local_protocols.py` | 41 Java/C константа, 152 class files версии 48.0 — PASS |
| `git diff --check` | PASS |

Ни JDK 8, ни его библиотека не подставляются как запасной источник API при
проверке ссылок: используются реальные J9-классы MU1316. Доступы оставшегося
stock проверяются по `combined.jar`, поскольку inline-accesses в `final.jar`
могут намеренно нарушать Java visibility ради читаемого декомпилята.

## Границы результата

Проверка ссылок не является полным JVM verifier: она не доказывает поведение
всех ветвей, порядок потоков или динамическую загрузку по внешним ресурсам.
Некоторые host-тесты stock JXE выполняются с `-Xverify:none`; поставляемые
классы собираются обычным компилятором с target 1.4. Для TM отдельно выполнен
ASM BasicVerifier.

На машине после этих исправлений проверок ещё не было. Перед выпуском остаются
реальные startup/context/plane 103, выбор курсором, map/popup start, renderer
respawn и отключение/возврат к штатной навигации. Ограничения эксперимента
описаны в [HU_STARTUP_LOADER_AND_MENU.md](HU_STARTUP_LOADER_AND_MENU.md).
Изменения аудита сохранены в `7fce1c4` и объединены в основной `master`
коммитом `7b6cc45`. На HU они пока не установлены.
