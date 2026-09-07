# Полный rollback

Bundle содержит точные stock-файлы, поэтому rollback не зависит от того,
сохранилась ли ручная копия на HU. Скрипт всё равно отказывается работать, если
текущий target не совпадает ни со stock, ни с payload этого bundle.

## Только NCM

```sh
./rollback_over_ssh.sh ncm
# reboot выполняет владелец
./verify_over_ssh.sh stock
```

Одновременно возвращаются:

- `/mnt/system/etc/scripts/startncm.sh`;
- `/mnt/system/etc/scripts/stopncm.sh`;
- `/mnt/system/etc/usblauncher_otg.lua`.

Частичный rollback запрещён.

## Только SI

```sh
./rollback_over_ssh.sh si
# reboot выполняет владелец
./verify_over_ssh.sh stock
```

Возвращается
`/mnt/app/eso/bin/apps/smartphone_integrator` с exact stock SHA-256
`a25dc981e5591cb3c6b09f00fd95de23803dd3daa0f562e7e8d60b6d0e1b0d02`.

## Полный комплект

```sh
./rollback_over_ssh.sh all
# reboot выполняет владелец
./verify_over_ssh.sh stock
```

Rollback использует `.rollback-new`, readback и побайтный `cmp` до `mv`.
Скрипт не убивает SI/`dio_manager`, не перезапускает `usblauncher` и не вызывает
reboot.

## Если rollback отказался

Не обходить `REFUSE`. Сначала сохранить:

```sh
./capture_state_over_ssh.sh ./rollback-refuse-state.txt
```

Затем вручную прочитать неизвестный target на Mac и выяснить, какая версия или
сторонняя модификация уже установлена. Принудительное overwrite неизвестного
SI/Lua не является частью этого bundle.
