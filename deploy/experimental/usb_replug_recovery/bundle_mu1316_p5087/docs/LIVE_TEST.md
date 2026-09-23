# Independent live A/B

## General rules

1. Start with `./verify_over_ssh.sh stock`.
2. Test `ncm` and `si` separately.
3. Between phases, do a full rollback and reboot.
4. Before every reboot of a dead session, run `capture_state_over_ssh.sh`.
5. Do not clear `/tmp/carplay_hook.log`; add a text marker for each new series.

## NCM-only phase

```sh
./install_over_ssh.sh ncm
# reboot by the owner
./verify_over_ssh.sh ncm
```

Matrix:

- one cold ordinary connect;
- ten unplug/replug cycles with a pause of more than three seconds;
- twenty replugs with a pause of roughly 0.1–0.5 seconds;
- five replugs during CarPlay start;
- one CarLife cycle, if the feature is used.

PASS:

- every CarPlay connect completes without a reboot;
- `carplay0` is present, no orphan `carplay1`;
- `carplay0` gets carrier/`RUNNING`;
- `NCM_FENCE` shows different `busno:devno:iface` for new enumerations;
- on overlap, the new start waits for the lock or the stale stop logs `skip`;
- no `lock_timeout`, no repeated `mount_rc` and no endless
  `TIMEOUT_STARTUP`.

INCONCLUSIVE/STOP:

- `devno` is reused immediately, and the old/new session cannot be told apart;
- an overlap could not be produced;
- the test was run together with the SI payload.

## SI-only phase

First return NCM to stock and check `verify_over_ssh.sh stock`, then:

```sh
./install_over_ssh.sh si
# reboot by the owner
./verify_over_ssh.sh si
```

Repeat the replug matrix. A real PASS requires provably exhausting
`maxSetupRetries` at least once.

PASS:

- after exhaustion, the last stable state does not stay
  `start_stack::none`;
- SI brings back `io-usb-dcd` and `/dev/io-usb/otg_dev` the normal way;
- an already plugged-in phone gets a new CarPlay attempt;
- no restart storm;
- an ordinary unplug stays normal;
- Android Auto and CarLife show no regressions.

INCONCLUSIVE:

- all fast replugs succeed, but retry exhaustion is never reached;
- the failure happened before a proven entry into final cleanup;
- the NCM payload was installed at the same time.

## Combined

`all` is allowed only after separate NCM PASS and SI PASS. After installing,
repeat the whole matrix. On any new regression run
`./rollback_over_ssh.sh all`, then a reboot by the owner.

## Minimal snapshot

```sh
./capture_state_over_ssh.sh ./dead-session-N.txt
```

Key interpretation:

| Observation | Branch |
|---|---|
| `otg_dev` present, `carplay0` absent, `mount_rc` | NCM mount/start |
| `carplay0` without `RUNNING`, drivers matched 2/running 1 | phone did not enable both USB functions |
| after retries no `otg_dev`, `usb_ctrl1=start_stack::none` | SI terminal cleanup |
| no OTG `usblauncher` process | separate third launcher-lifecycle scenario |

Do not rely on `sloginfo` alone: its ring buffer is quickly flushed by the
video storm.
