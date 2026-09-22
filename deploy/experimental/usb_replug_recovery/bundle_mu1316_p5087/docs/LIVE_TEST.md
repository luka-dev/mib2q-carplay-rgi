# Независимый live A/B

## Общие правила

1. Начинать с `./verify_over_ssh.sh stock`.
2. Тестировать `ncm` и `si` раздельно.
3. Между фазами выполнять полный rollback и reboot.
4. Перед каждым reboot мёртвой сессии запускать `capture_state_over_ssh.sh`.
5. Не очищать `/tmp/carplay_hook.log`; добавлять текстовый marker новой серии.

## Фаза NCM-only

```sh
./install_over_ssh.sh ncm
# reboot владельцем
./verify_over_ssh.sh ncm
```

Матрица:

- один холодный обычный connect;
- десять unplug/replug с паузой более трёх секунд;
- двадцать replug с паузой примерно 0.1–0.5 секунды;
- пять replug во время начала CarPlay;
- один CarLife цикл, если функция используется.

PASS:

- каждый CarPlay connect завершается без reboot;
- присутствует `carplay0`, нет orphan `carplay1`;
- `carplay0` получает carrier/`RUNNING`;
- `NCM_FENCE` показывает разные `busno:devno:iface` новых enumeration;
- при overlap новый start ждёт lock или stale stop пишет `skip`;
- нет `lock_timeout`, повторяющегося `mount_rc` и вечного
  `TIMEOUT_STARTUP`.

INCONCLUSIVE/STOP:

- `devno` немедленно переиспользуется, и старую/новую сессию нельзя различить;
- не удалось создать overlap;
- тест проводился вместе с SI payload.

## Фаза SI-only

Сначала вернуть NCM в stock и проверить `verify_over_ssh.sh stock`, затем:

```sh
./install_over_ssh.sh si
# reboot владельцем
./verify_over_ssh.sh si
```

Повторить матрицу replug. Для настоящего PASS необходимо хотя бы один раз
доказанно исчерпать `maxSetupRetries`.

PASS:

- после exhaustion последний устойчивый state не остаётся
  `start_stack::none`;
- SI штатно возвращает `io-usb-dcd` и `/dev/io-usb/otg_dev`;
- уже вставленный телефон получает новую CarPlay-попытку;
- нет restart storm;
- обычный unplug остаётся нормальным;
- Android Auto и CarLife не получают регрессии.

INCONCLUSIVE:

- быстрые replug все успешны, но retry exhaustion ни разу не достигнут;
- отказ произошёл до доказанного входа в final cleanup;
- одновременно был установлен NCM payload.

## Combined

`all` разрешён только после отдельных NCM PASS и SI PASS. После установки
повторить всю матрицу. При любой новой регрессии выполнить
`./rollback_over_ssh.sh all`, затем reboot владельцем.

## Минимальный snapshot

```sh
./capture_state_over_ssh.sh ./dead-session-N.txt
```

Ключевая интерпретация:

| Наблюдение | Ветка |
|---|---|
| `otg_dev` есть, `carplay0` нет, `mount_rc` | NCM mount/start |
| `carplay0` без `RUNNING`, drivers matched 2/running 1 | телефон не включил обе USB-функции |
| после retries нет `otg_dev`, `usb_ctrl1=start_stack::none` | SI terminal cleanup |
| нет процесса OTG `usblauncher` | отдельный третий launcher-lifecycle сценарий |

Не полагаться только на `sloginfo`: его кольцевой буфер быстро вымывается
видеоштормом.
