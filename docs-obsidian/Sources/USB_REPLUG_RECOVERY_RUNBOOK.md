# Полный runbook лечения зависания CarPlay после переподключения USB

Актуализация 2026-09-17: владелец подтвердил, что подключение и переподключения
на текущей машине закрыты. Ниже сохранён исторический статус отдельных
исследований и процедура A/B от 7 сентября; это не текущий список блокеров.
Сводный статус: [TODO](../../TODO.md).

Статус на 2026-09-07: обе точки отказа подтверждены статически, но какая из них
запустила конкретную пойманную мёртвую сессию, вживую не доказано. Оба
исправления подготовлены и обратимы, но ещё не прошли независимый live A/B на
головном устройстве. Поэтому ниже разделены доказанные факты, кандидаты
исправления и критерии, после которых их можно считать проверенными.

Целевая система:

- HU: Audi MHI2Q US, MU1316;
- прошивка: `MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398`;
- media connector: `0x0424:0x2530`;
- телефонный транспорт: iAP2 + NCM, интерфейс `carplay0`.

Этот документ касается USB/reconnect. Он не меняет аудиохук, APAC, рендереры,
Java, firewall или логику владения `dio_manager`.

## Короткий диагноз

Есть две независимые гонки, и одна может маскировать другую:

1. Старый `stopncm.sh` спит одну секунду, а затем безусловно уничтожает
   интерфейс по глобальному имени `carplay0`. За эту секунду новый телефон уже
   может создать новое поколение `carplay0`; старый teardown уничтожит уже его.
2. Если запуск CarPlay несколько раз заканчивается `TIMEOUT_STARTUP`, SI
   исчерпывает `maxSetupRetries`, ставит финальный
   `SwitchPort(direction=0)`, пишет `start_stack::none` и больше не запускает
   device stack. Телефон уже вставлен, а распаянный `0x2530`-хаб не переживал
   новое insertion-событие, поэтому система остаётся в стабильном тупике.

Типичная конечная цепочка:

```text
быстрый replug / USB-role-swap / NCM failure
                    ↓
carplay0 отсутствует или не имеет carrier
                    ↓
iAP2 не стартует, dio получает TIMEOUT_STARTUP
                    ↓
две штатные stop → 5 s → start попытки SI
                    ↓
финальный SwitchPort(direction=0)
                    ↓
start_stack::none, нет io-usb-dcd и /dev/io-usb/otg_dev
                    ↓
SI ждёт событие, которое уже произошло
```

Отсутствие `io-usb-dcd` после отказа не доказывает, что DCD упал первым: это
может быть нормальным результатом финального stop-only cleanup SI.

## Что именно лечит каждый компонент

| Проблема | Исправление | Статус |
|---|---|---|
| старый delayed teardown уничтожает новое `carplay0` | generation identity + общий transaction lock в `startncm.sh`/`stopncm.sh` | host-тесты пройдены, нужен live |
| новый mount сталкивается со старым интерфейсом того же имени | тот же lock сериализует destroy и mount | host-тесты пройдены, нужен live |
| `startncm.sh` пытается mount на отсутствующий `otg_dev` | обязательная проверка узла через `ls` | реализовано |
| stock-скрипт продолжает работу после неудачного mount | проверка кода mount, уборка только своей транзакции | реализовано |
| UTF-8 en-dash в аргументах `if_up` | ASCII `-s 2 -r 1` | исправлено; это не считалось корнем отказа |
| финальный error-cleanup CarPlay делает только stop | SI-патч направляет только CarPlay error-cleanup в штатный `direction=1` | статически проверено, нужен live |
| `sloginfo` вымывает момент поломки видеоштормом | решения NCM пишутся в тихий `/tmp/carplay_hook.log` с префиксом `NCM_FENCE` | реализовано |
| бесконечный внешний reset/relaunch может бороться с SI | не добавлять USB reset loop в `rc.local`; SI остаётся единственным владельцем OTG и `dio_manager` | обязательное ограничение |

Исправления находятся в
[`deploy/experimental/usb_replug_recovery`](../../deploy/experimental/usb_replug_recovery/README.md).
Готовые четыре payload-файла, точные stock-файлы и защищённые host-скрипты
собраны отдельно в
[`bundle_mu1316_p5087`](../../deploy/experimental/usb_replug_recovery/bundle_mu1316_p5087/README.md).

## Граница применимости и контрольные хеши

Нельзя переносить эти файлы на MH2p или другую версию MU. MH2p подтверждает ту
же политику direction 0/1, но использует другой ISA/layout.

Stock MU1316 P5087:

| Файл | SHA-256 |
|---|---|
| `startncm.sh` | `c533b63fa550126576cf3948b8060ef3f0f6d430ebe12c39769ef898e583713f` |
| `stopncm.sh` | `596b349c00ee992fa9755c76274eb14ab06062f512ad79f604527c5c8e520008` |
| `usblauncher_otg.lua` | `21ca8cb904448d24c0d13684d582ccd641139dd96758b8956080b9529b9a6b5c` |
| `smartphone_integrator` | `a25dc981e5591cb3c6b09f00fd95de23803dd3daa0f562e7e8d60b6d0e1b0d02` |

Кандидаты:

| Файл | SHA-256 |
|---|---|
| generation-aware `startncm.sh` | `e40ef9031ecbed1a34074a4213d76a0672aaeba2e7c18dc5ca88d52ce9a806ba` |
| generation-aware `stopncm.sh` | `804b3854a2e8460cf509f7eee3529cec8e0821766c273ee19d42e3344a033d68` |
| patched `usblauncher_otg.lua` | `76843b2dc0495e587336bc907bf2b3358b454ff2cad6e17c62ced42ae84de329` |
| SI re-arm candidate | `606236a81c7c4dafda6fdd08fe6eb27c43252796784da25d9760a7d552ee2af3` |

`patch_si_rearm.py` дополнительно проверяет размер SI `802798`, точный контекст
инструкции и запрещает in-place patch. Любое несовпадение — причина остановить
установку, а не обходить проверку.

## Правила безопасного испытания

1. Сначала зафиксировать полностью stock baseline.
2. Проверять NCM-набор отдельно, оставив SI stock.
3. Перед отдельной проверкой SI вернуть весь NCM-набор в stock.
4. Не ставить оба кандидата одновременно до независимого A/B: успешный replug
   иначе не покажет, какой дефект был устранён.
5. Каждый набор заменять атомарно и только целиком. Нельзя оставлять новый Lua
   со старым `stopncm.sh` или один новый NCM-скрипт со вторым старым.
6. Изменять persistent-копии, затем активировать их полной перезагрузкой HU.
   Не пытаться переучить уже запущенный `usblauncher` заменой RAM-копии `/etc`.
7. Перезагрузку выполняет владелец автомобиля. Скрипты из репозитория её не
   вызывают.

## Шаг 0. Host-side проверка артефактов

На Mac:

```sh
cd /Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay

sh -n deploy/experimental/usb_replug_recovery/startncm.sh
sh -n deploy/experimental/usb_replug_recovery/stopncm.sh
sh -n scripts/test_usb_replug_recovery.sh
./scripts/test_usb_replug_recovery.sh

python3 -B deploy/experimental/usb_replug_recovery/patch_si_rearm.py check \
  /Users/luka/Desktop/AUDI_2/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/bin/apps/smartphone_integrator
```

Ожидается `USB replug recovery shell tests: PASS` и
`state=stock ... sha256=a25dc9...`.

Тест NCM покрывает:

- обычное удаление текущего поколения;
- stale removal от старого USB-адреса;
- overlap, где stop держит штатный `sleep 1`, а новый start уже пришёл;
- повторный start одного поколения;
- reclaim lock, PID владельца которого уже умер;
- отказ mount без ложного успешного результата.

## Шаг 1. Read-only preflight головного устройства

До remount и записи нужно снять фактические persistent-файлы с HU и проверить
их на Mac. Это не меняет устройство.

Следующие фазы предполагают, что переменные `PROJECT_DIR`, `SSH` и `WORK_DIR`
остаются в том же окне shell.

```sh
PROJECT_DIR=/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay
SSH=/Users/luka/Desktop/AUDI_2/audi_ssh.sh
WORK_DIR=$(mktemp -d /tmp/mhi2-usb-recovery.XXXXXX)

"$SSH" exec 'cat /mnt/system/etc/scripts/startncm.sh' \
  > "$WORK_DIR/startncm.hu.stock"
"$SSH" exec 'cat /mnt/system/etc/scripts/stopncm.sh' \
  > "$WORK_DIR/stopncm.hu.stock"
"$SSH" exec 'cat /mnt/system/etc/usblauncher_otg.lua' \
  > "$WORK_DIR/usblauncher_otg.hu.stock"
"$SSH" exec 'cat /mnt/app/eso/bin/apps/smartphone_integrator' \
  > "$WORK_DIR/smartphone_integrator.hu.stock"

shasum -a 256 \
  "$WORK_DIR/startncm.hu.stock" \
  "$WORK_DIR/stopncm.hu.stock" \
  "$WORK_DIR/usblauncher_otg.hu.stock"
python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  check "$WORK_DIR/smartphone_integrator.hu.stock"
```

Все четыре результата обязаны совпасть с таблицей stock-хешей выше. Если нет,
сначала выяснить, какая модификация уже стоит на HU. Не создавать backup с
названием `stock` из неизвестного файла и не применять patch принудительно.

## Фаза A. Лечение NCM race

### A1. Собрать patched Lua локально

```sh
FW_SYSTEM=/Users/luka/Desktop/AUDI_2/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-system

cp "$FW_SYSTEM/etc/usblauncher_otg.lua" \
  "$WORK_DIR/usblauncher_otg.lua.stock"
cp "$FW_SYSTEM/etc/usblauncher_otg.lua" \
  "$WORK_DIR/usblauncher_otg.lua"
(
  cd "$WORK_DIR" &&
  patch --fuzz=0 -p0 \
    < "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/usblauncher_otg.lua.patch"
)

shasum -a 256 \
  "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/startncm.sh" \
  "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/stopncm.sh" \
  "$WORK_DIR/usblauncher_otg.lua"
```

Результаты обязаны совпасть с тремя candidate-хешами из таблицы.

### A2. Создать однократные persistent-backup

Этот шаг уже пишет на HU, но ещё не активирует новые файлы:

```sh
"$SSH" exec '
  set -e
  mount -uw /mnt/system
  [ -f /mnt/system/etc/scripts/startncm.sh.usb-replug-stock ] || \
    cp /mnt/system/etc/scripts/startncm.sh \
       /mnt/system/etc/scripts/startncm.sh.usb-replug-stock
  [ -f /mnt/system/etc/scripts/stopncm.sh.usb-replug-stock ] || \
    cp /mnt/system/etc/scripts/stopncm.sh \
       /mnt/system/etc/scripts/stopncm.sh.usb-replug-stock
  [ -f /mnt/system/etc/usblauncher_otg.lua.usb-replug-stock ] || \
    cp /mnt/system/etc/usblauncher_otg.lua \
       /mnt/system/etc/usblauncher_otg.lua.usb-replug-stock
  sync
'
```

Backup создаётся только после успешного read-only preflight, поэтому суффикс
`usb-replug-stock` имеет однозначный смысл.

### A3. Загрузить `.new`, прочитать обратно и проверить

```sh
"$SSH" exec 'cat > /mnt/system/etc/scripts/startncm.sh.usb-replug-new' \
  < "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/startncm.sh"
"$SSH" exec 'cat > /mnt/system/etc/scripts/stopncm.sh.usb-replug-new' \
  < "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/stopncm.sh"
"$SSH" exec 'cat > /mnt/system/etc/usblauncher_otg.lua.usb-replug-new' \
  < "$WORK_DIR/usblauncher_otg.lua"

"$SSH" exec 'cat /mnt/system/etc/scripts/startncm.sh.usb-replug-new' \
  > "$WORK_DIR/startncm.readback"
"$SSH" exec 'cat /mnt/system/etc/scripts/stopncm.sh.usb-replug-new' \
  > "$WORK_DIR/stopncm.readback"
"$SSH" exec 'cat /mnt/system/etc/usblauncher_otg.lua.usb-replug-new' \
  > "$WORK_DIR/usblauncher_otg.readback"

shasum -a 256 \
  "$WORK_DIR/startncm.readback" \
  "$WORK_DIR/stopncm.readback" \
  "$WORK_DIR/usblauncher_otg.readback"
```

Только после совпадения candidate-хешей активировать все три файла одной
транзакцией:

```sh
"$SSH" exec '
  set -e
  sh -n /mnt/system/etc/scripts/startncm.sh.usb-replug-new
  sh -n /mnt/system/etc/scripts/stopncm.sh.usb-replug-new
  chmod 755 \
    /mnt/system/etc/scripts/startncm.sh.usb-replug-new \
    /mnt/system/etc/scripts/stopncm.sh.usb-replug-new
  chmod 644 /mnt/system/etc/usblauncher_otg.lua.usb-replug-new
  mv /mnt/system/etc/scripts/startncm.sh.usb-replug-new \
     /mnt/system/etc/scripts/startncm.sh
  mv /mnt/system/etc/scripts/stopncm.sh.usb-replug-new \
     /mnt/system/etc/scripts/stopncm.sh
  mv /mnt/system/etc/usblauncher_otg.lua.usb-replug-new \
     /mnt/system/etc/usblauncher_otg.lua
  sync
'
```

После этого владелец делает полную перезагрузку HU. До неё persistent-файлы уже
новые, но запущенный `usblauncher` и RAM-копия `/etc` могут оставаться старыми;
результат до reboot не считать валидным.

### A4. Проверка после reboot

Сначала проверить, что runtime-копии действительно обновились:

```sh
"$SSH" exec 'cat /etc/scripts/startncm.sh' \
  > "$WORK_DIR/startncm.runtime"
"$SSH" exec 'cat /etc/scripts/stopncm.sh' \
  > "$WORK_DIR/stopncm.runtime"
"$SSH" exec 'cat /etc/usblauncher_otg.lua' \
  > "$WORK_DIR/usblauncher_otg.runtime"
shasum -a 256 \
  "$WORK_DIR/startncm.runtime" \
  "$WORK_DIR/stopncm.runtime" \
  "$WORK_DIR/usblauncher_otg.runtime"
```

Затем провести минимум:

1. один обычный connect после холодной загрузки;
2. десять медленных unplug/replug с паузой более трёх секунд;
3. двадцать быстрых replug с паузой примерно 0.1–0.5 секунды;
4. пять replug во время начала CarPlay, когда сессия ещё поднимается;
5. один цикл CarLife, если он используется на этом HU.

Перед серией добавить понятный маркер, не очищая старые доказательства:

```sh
"$SSH" exec \
  'echo "USB_REPLUG_TEST phase=NCM run=01" >> /tmp/carplay_hook.log'
```

После серии снять данные:

```sh
"$SSH" exec '
  grep NCM_FENCE /tmp/carplay_hook.log
  ls -l /dev/io-usb/otg_dev
  ifconfig carplay0
  ifconfig -a | grep carplay
  pidin ar | grep -E "[i]o-usb-dcd|[d]io_manager|[u]sblauncher"
  cat /ramdisk/pps/device/usb_ctrl1
  ls /ramdisk/pps/device
' > "$WORK_DIR/ncm-phase-snapshot.txt"
```

Критерии успеха фазы A:

- после каждого connect CarPlay поднимается без reboot;
- существует ровно ожидаемый `carplay0`, нет orphan `carplay1`;
- `carplay0` получает рабочий carrier/флаг `RUNNING`;
- в `NCM_FENCE` видны разные `busno:devno:iface` для новых перечислений;
- при overlap виден `wait_lock` или stale removal получает `skip`;
- нет `lock_timeout`, бесконечного `mount_rc`, вечного `TIMEOUT_STARTUP`.

Если QNX немедленно повторно использует тот же `devno`, generation identity не
может отличить поздний removal старой сессии от новой. Такой результат означает
`INCONCLUSIVE/STOP`: не считать NCM-fence доказанным и не маскировать это
увеличением таймаутов.

### A5. Полный rollback NCM-набора

Возвращать только все три файла вместе:

```sh
"$SSH" exec '
  set -e
  mount -uw /mnt/system
  [ -f /mnt/system/etc/scripts/startncm.sh.usb-replug-stock ]
  [ -f /mnt/system/etc/scripts/stopncm.sh.usb-replug-stock ]
  [ -f /mnt/system/etc/usblauncher_otg.lua.usb-replug-stock ]
  cp /mnt/system/etc/scripts/startncm.sh.usb-replug-stock \
     /mnt/system/etc/scripts/startncm.sh.rollback-new
  cp /mnt/system/etc/scripts/stopncm.sh.usb-replug-stock \
     /mnt/system/etc/scripts/stopncm.sh.rollback-new
  cp /mnt/system/etc/usblauncher_otg.lua.usb-replug-stock \
     /mnt/system/etc/usblauncher_otg.lua.rollback-new
  chmod 755 \
    /mnt/system/etc/scripts/startncm.sh.rollback-new \
    /mnt/system/etc/scripts/stopncm.sh.rollback-new
  chmod 644 /mnt/system/etc/usblauncher_otg.lua.rollback-new
  mv /mnt/system/etc/scripts/startncm.sh.rollback-new \
     /mnt/system/etc/scripts/startncm.sh
  mv /mnt/system/etc/scripts/stopncm.sh.rollback-new \
     /mnt/system/etc/scripts/stopncm.sh
  mv /mnt/system/etc/usblauncher_otg.lua.rollback-new \
     /mnt/system/etc/usblauncher_otg.lua
  sync
'
```

После rollback снова нужна полная перезагрузка и проверка трёх stock-хешей у
runtime-копий `/etc`.

## Фаза B. Лечение terminal cleanup SI

Эту фазу выполнять с полностью stock NCM-набором. Патч не добавляет внешний
watchdog и не создаёт бесконечный retry. Он меняет ровно одну ARM-инструкцию:

```text
VA 0x17FDB4, file offset 0x7FDB4
stock:   00 30 a0 e3    MOV R3, #0
patched: 2e 33 09 e0    AND R3, R9, LR, LSR #6
```

На этой точке `R9` — error flag, а `LR == 0x40` только для CarPlay. Результат:

| Cleanup | direction |
|---|---:|
| нормальное отключение CarPlay | 0 |
| любой CarLife cleanup | 0 |
| финальный CarPlay cleanup после ошибки | 1 |

Штатный direction 1 выполняет activate-CarPlay, switch-port и
`start_stack::device,2`. Он устраняет stop-only финал, но live всё ещё должен
доказать, что этот путь успешно заканчивается именно в зависшем аппаратном
состоянии.

### B1. Создать и проверить кандидат

Использовать SI, прочитанный с HU на read-only preflight:

```sh
python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  apply \
  "$WORK_DIR/smartphone_integrator.hu.stock" \
  "$WORK_DIR/smartphone_integrator.rearm"

python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  check "$WORK_DIR/smartphone_integrator.rearm"
```

Ожидается `state=patched`, размер `802798` и candidate SHA-256
`606236a8...`.

Обратимость проверить до установки:

```sh
python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  revert \
  "$WORK_DIR/smartphone_integrator.rearm" \
  "$WORK_DIR/smartphone_integrator.reverted"
cmp \
  "$WORK_DIR/smartphone_integrator.hu.stock" \
  "$WORK_DIR/smartphone_integrator.reverted"
```

### B2. Backup, upload и атомарная замена

```sh
"$SSH" exec '
  set -e
  mount -uw /mnt/app
  [ -f /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-stock ] || \
    cp /mnt/app/eso/bin/apps/smartphone_integrator \
       /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-stock
  sync
'

"$SSH" exec \
  'cat > /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new' \
  < "$WORK_DIR/smartphone_integrator.rearm"

"$SSH" exec \
  'cat /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new' \
  > "$WORK_DIR/smartphone_integrator.readback"
python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  check "$WORK_DIR/smartphone_integrator.readback"
```

Только после `state=patched`:

```sh
"$SSH" exec '
  set -e
  chmod 755 /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new
  mv /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new \
     /mnt/app/eso/bin/apps/smartphone_integrator
  sync
'
```

Не убивать живой SI вручную. Владелец выполняет полную перезагрузку, после чего
runtime-бинарник ещё раз читается на Mac и проверяется командой `check`.

### B3. Live-критерии SI

Повторить ту же матрицу обычных и быстрых replug. Для этой фазы особенно важен
случай, который действительно исчерпывает `maxSetupRetries`. Если такой отказ
не возник, тест SI остаётся `INCONCLUSIVE`, даже если двадцать обычных
подключений были успешны.

Успешный результат SI-патча:

- после исчерпания retry stack не остаётся навсегда на `start_stack::none`;
- `io-usb-dcd` и `/dev/io-usb/otg_dev` возвращаются штатным SI-путём;
- уже вставленный телефон получает новую возможность поднять CarPlay;
- нет restart storm и SI не начинает бесконечно убивать `dio_manager`;
- нормальный unplug и CarLife по-прежнему выполняют direction 0 семантически.

### B4. Rollback SI

```sh
"$SSH" exec '
  set -e
  mount -uw /mnt/app
  [ -f /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-stock ]
  cp /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-stock \
     /mnt/app/eso/bin/apps/smartphone_integrator.rollback-new
  chmod 755 /mnt/app/eso/bin/apps/smartphone_integrator.rollback-new
  mv /mnt/app/eso/bin/apps/smartphone_integrator.rollback-new \
     /mnt/app/eso/bin/apps/smartphone_integrator
  sync
'
```

Затем полная перезагрузка и readback-проверка `state=stock`.

## Финальная production-комбинация

Устанавливать оба исправления вместе можно только после независимых фаз:

| NCM-only | SI-only | Решение |
|---|---|---|
| устраняет воспроизводимый NCM overlap | SI-тест не сумел достичь exhaustion | оставить NCM; SI считать непроверенным, не устанавливать «на всякий случай» |
| race остаётся | exhaustion восстанавливается через direction 1 | оставить SI, продолжить изучение NCM identity |
| оба отдельно закрывают свои воспроизводимые сценарии | оба PASS | допустима объединённая установка с повтором полной матрицы |
| любой вариант создаёт регрессию/шторм | — | немедленный rollback соответствующего слоя |

Даже после двух PASS сохраняется честное ограничение: если сам OTG
`usblauncher` погибнет, а one-shot hub flag не даст main launcher запустить его
повторно, эти два патча не доказывают восстановление такого третьего сценария.
Автоматический kill/relaunch wrapper уже показал, что может возвращать узел
«за спиной» владельца, не поднимая CarPlay. Для этой ветки сначала нужен
отдельный generation-aware lifecycle design и live-доказательство; нельзя
выдавать `rc.local` reset loop за готовое лечение.

## Диагностика оставшейся мёртвой сессии

До reboot сохранить:

```sh
"$SSH" exec '
  echo "USB nodes"
  ls -l /dev/io-usb/otg_dev /dev/io-usb/otg /dev/io-usb/io-usb
  echo "processes"
  pidin ar | grep -E "[i]o-usb-dcd|[i]o-otg|[u]sblauncher|[d]io_manager|[s]martphone_integrator"
  echo "NCM"
  ifconfig carplay0
  ifconfig -a | grep carplay
  echo "SI command"
  cat /ramdisk/pps/device/usb_ctrl1
  echo "PPS topology"
  ls /ramdisk/pps/device
  echo "quiet log"
  grep -E "NCM_FENCE|TIMEOUT_STARTUP|start_stack" /tmp/carplay_hook.log
' > "$WORK_DIR/dead-session-snapshot.txt"
```

Интерпретация:

| Состояние | Вероятная ветка |
|---|---|
| `otg_dev` есть, `carplay0` нет, в логе `mount_rc`/timeout | NCM start/mount |
| `carplay0` есть, но нет `RUNNING`; PPS показывает matched 2/running 1 | телефон не включил обе функции, iAP2 не начал работу |
| после retry нет `otg_dev`, последний `usb_ctrl1` — `start_stack::none` | terminal cleanup SI |
| новый start ждал lock, затем поднялся; старый stop получил `skip` | NCM fence отработал штатно |
| `usblauncher` отсутствует при живом SI/хабе | отдельный launcher-lifecycle отказ, текущие два PASS не доказывают его лечение |

`sloginfo` не использовать как единственный источник: его кольцевой буфер на
этой системе вымывается примерно за две минуты видеоштормом. Сначала сохранять
`/tmp/carplay_hook.log` и PPS/процессный snapshot.

## Аварийное восстановление до установки исправлений

Единственное доказанное полное восстановление уже пойманной мёртвой сессии —
полная перезагрузка HU. Ручной вызов device-stack start возвращал `otg_dev`, но
сам по себе CarPlay не поднял. Ручное переключение media-connector port однажды
сделало состояние хуже. Поэтому перед reboot собрать snapshot выше, после чего
перезагрузку выполняет владелец.

Не выполнять в цикле:

- `start_otg_stack_device_mode.sh` из каждого запуска `dio_manager`;
- kill/relaunch `usblauncher` без generation/latch;
- `reset port` или GPIO power-cycle из `rc.local`;
- параллельный перезапуск `dio_manager` внешним watchdog;
- увеличение `maxSetupRetries` как «исправление» — оно только удлиняет шторм.

## Что не требуется менять

- `verifyPortReset` state 12 → 14: CarPlay — app ID 0 и идёт по отдельной ветке;
- Tailscale, память и CPU: ранее подозревались, но были опровергнуты измерениями;
- LTE-правку в основном `usblauncher.lua`: она относится к модему;
- altScreen/APAC/renderers: отказ происходит до RTSP и media streams;
- `rc.local`: оставить универсальным supervisor для уже предназначенных ему
  сервисов, но не делать его вторым владельцем USB state machine.

## Связанные доказательства

- [`SI_RAPID_REPLUG_RACE_RE.md`](reference/SI_RAPID_REPLUG_RACE_RE.md) — точная
  SI state machine, адреса ARM и сравнение MH2p;
- [`USB_IPHONE_CONNECT_POSTMORTEM.md`](USB_IPHONE_CONNECT_POSTMORTEM.md) — live
  симптомы, опровергнутые гипотезы и ограничения логов;
- [`USB_STARTUP_RECOVERY_DIAGNOSIS.md`](reference/USB_STARTUP_RECOVERY_DIAGNOSIS.md)
  — ранняя диагностика и исправляющее примечание;
- [`NCM_AND_FIREWALL_IDEAL_START.md`](reference/NCM_AND_FIREWALL_IDEAL_START.md)
  — нормальный USB/NCM startup flow;
- [`deploy/experimental/usb_replug_recovery/README.md`](../../deploy/experimental/usb_replug_recovery/README.md)
  — реализация обоих обратимых кандидатов.
