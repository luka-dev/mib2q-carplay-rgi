# Full rollback

The bundle contains the exact stock files, so rollback does not depend on
whether a manual copy survived on the HU. The script still refuses to run if
the current target matches neither the stock nor the payload of this bundle.

## NCM only

```sh
./rollback_over_ssh.sh ncm
# reboot is done by the owner
./verify_over_ssh.sh stock
```

Restored together:

- `/mnt/system/etc/scripts/startncm.sh`;
- `/mnt/system/etc/scripts/stopncm.sh`;
- `/mnt/system/etc/usblauncher_otg.lua`.

Partial rollback is forbidden.

## SI only

```sh
./rollback_over_ssh.sh si
# reboot is done by the owner
./verify_over_ssh.sh stock
```

Restores
`/mnt/app/eso/bin/apps/smartphone_integrator` with the exact stock SHA-256
`a25dc981e5591cb3c6b09f00fd95de23803dd3daa0f562e7e8d60b6d0e1b0d02`.

## Full set

```sh
./rollback_over_ssh.sh all
# reboot is done by the owner
./verify_over_ssh.sh stock
```

Rollback uses `.rollback-new`, readback and a byte-for-byte `cmp` before `mv`.
The script does not kill SI/`dio_manager`, does not restart `usblauncher` and
does not call reboot.

## If rollback refused

Do not bypass `REFUSE`. First save:

```sh
./capture_state_over_ssh.sh ./rollback-refuse-state.txt
```

Then manually read the unknown target on the Mac and find out which version or
third-party modification is already installed. Forcibly overwriting an unknown
SI/Lua is not part of this bundle.
