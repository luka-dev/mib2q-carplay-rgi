# MU1316 P5087 CarPlay USB replug recovery bundle

Самостоятельный комплект из четырёх готовых файлов для Audi MHI2Q US MU1316:

1. `startncm.sh` — generation-aware NCM start;
2. `stopncm.sh` — teardown с USB identity и общим lock;
3. `usblauncher_otg.lua` — передаёт `busno/devno/iface` обоим скриптам;
4. `smartphone_integrator` — one-instruction CarPlay error re-arm.

Bundle предназначен только для
`MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398`. Установщики сверяют
фактические файлы HU побайтно с `stock/` и `payload/` и отказываются менять
неизвестную версию.

Статус: offline/static verification и host-тесты пройдены; live A/B ещё не
закрыт. Скрипты никогда не вызывают reboot — его выполняет владелец HU.

## Состав папки

```text
bundle_mu1316_p5087/
├── payload/                  готовые четыре patched-файла
├── stock/                    точные четыре stock-файла для rollback
├── docs/
│   ├── TECHNICAL.md          механизм и границы исправлений
│   ├── LIVE_TEST.md          независимый A/B и критерии PASS
│   └── ROLLBACK.md           точный возврат stock
├── MANIFEST.sha256           контроль всех восьми бинарных/text payload
├── verify_bundle.sh          полностью локальная проверка
├── verify_over_ssh.sh        read-only проверка HU после reboot
├── install_over_ssh.sh       stage/readback/atomic install
├── rollback_over_ssh.sh      stage/readback/atomic stock restore
└── capture_state_over_ssh.sh read-only snapshot мёртвой сессии
```

## Локальная проверка

```sh
cd /Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay/deploy/usb_replug_recovery/bundle_mu1316_p5087
./verify_bundle.sh
```

Ожидается восемь строк `OK` и `bundle verification: PASS`.

По умолчанию используется
`/Users/luka/Desktop/AUDI_2/audi_ssh.sh`. Для другого helper:

```sh
AUDI_SSH_HELPER=/absolute/path/to/audi_ssh.sh ./verify_over_ssh.sh stock
```

## Режимы

| Режим | NCM scripts + Lua | SI | Назначение |
|---|---|---|---|
| `stock` | stock | stock | только `verify_over_ssh.sh` после полного rollback |
| `ncm` | payload | stock | первая независимая live-фаза |
| `si` | stock | payload | вторая независимая live-фаза |
| `all` | payload | payload | финал только после двух отдельных PASS |

## Рекомендуемый порядок

Проверить исходный HU без записи:

```sh
./verify_over_ssh.sh stock
```

Первая фаза:

```sh
./install_over_ssh.sh ncm
# владелец перезагружает HU
./verify_over_ssh.sh ncm
```

Провести матрицу из [`docs/LIVE_TEST.md`](docs/LIVE_TEST.md). Затем вернуть
stock до второго независимого теста:

```sh
./rollback_over_ssh.sh ncm
# владелец перезагружает HU
./verify_over_ssh.sh stock
```

Вторая фаза:

```sh
./install_over_ssh.sh si
# владелец перезагружает HU
./verify_over_ssh.sh si
```

Только если обе фазы отдельно дали PASS:

```sh
./rollback_over_ssh.sh si
# владелец перезагружает HU и проверяет stock
./install_over_ssh.sh all
# владелец перезагружает HU
./verify_over_ssh.sh all
```

`install_over_ssh.sh` выполняет:

- локальную проверку `MANIFEST.sha256`;
- read-only определение каждого target как exact `stock` или exact `payload`;
- отказ `ncm` при non-stock SI и отказ `si` при non-stock NCM, чтобы случайно
  не превратить независимый A/B в combined-тест;
- проверку уже существующих `.usb-replug-stock` backup;
- remount только требуемой persistent partition;
- upload в `.usb-replug-new`;
- readback и побайтный `cmp` до активации;
- `sh -n` NCM-скриптов на QNX;
- атомарный `mv` и `sync`;
- запись RAM-маркера `reboot_required`, из-за которого verifier не примет даже
  SI-only замену до полной перезагрузки;
- остановку без reboot или kill живых процессов.

## Быстрый snapshot перед reboot

Если сессия умерла, сначала:

```sh
./capture_state_over_ssh.sh ./dead-session-01.txt
```

Скрипт ничего не меняет на HU. Он сохраняет USB nodes, процессы, NCM,
`usb_ctrl1`, PPS topology и тихие строки `NCM_FENCE` на Mac.

## Жёсткие ограничения

- Нельзя ставить только два `.sh` без matching `usblauncher_otg.lua`.
- Нельзя применять SI к другому SHA, MH2p или другой MU-версии.
- Нельзя считать обычные успешные подключения доказательством SI: тест должен
  реально дойти до исчерпания `maxSetupRetries`.
- Нельзя добавлять USB reset/kill loop в `rc.local`.
- Единственное доказанное аварийное восстановление уже мёртвой stock-сессии —
  полная перезагрузка после сохранения snapshot.

Подробности механизма: [`docs/TECHNICAL.md`](docs/TECHNICAL.md). Полный rollback:
[`docs/ROLLBACK.md`](docs/ROLLBACK.md).
