# Issue 24: RCC/MMX boot, reset supervision, and recovery entry points

Analysis date: 2026-09-26. Constraint: affected CN MU1003 cannot enter RED or GEM.
All proposed SD entry steps are automatic; shell commands below require an already
obtained shell or a script execution entry and are not instructions to open GEM.

## Inputs and limits

Requested backup:
`/Users/luka/Desktop/AUDI_2/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398`.

MMX IFS binaries absent from that backup were inspected in the neighboring
`MHI2Q_US_AUG22_P5087_MU1316/extracted/` tree. These findings are verified for
the examined MU1316 binaries, not yet verified byte-for-byte against CN MU1003.

The backup's `advanced/MU1316-system/etc/boot/startup.sh` is modified: compared
with the neighboring extracted firmware, it adds `start_iohid` and removes
`start_hmi &` from `swdl_startup`. Do not infer that stock CN SWDL skips HMI from
this backup. Both copies launch normal HMI independently of autorunner.

All analyzed native targets are ARM32 little-endian QNX ELF (ldqnx.so.2).
`rabin2` labels some stripped QNX images as Linux; the interpreter identifies QNX.

| Binary | SHA256 | Base |
|---|---|---|
| MMX `ooc` | `372d0e934d3816c04ad01767ad292d71ce8d50b7415cf8aa661a9899a86bab10` | `0x100000` |
| MMX `servicemgrmibhigh` | `a913def041e7a7b014f2107e8755112692431381cbbee042ce6e7daea76d7fe6` | `0x100000` |
| MMX `libservmngt.so` | `53a406dda384a1fba790f6110243783a6587b37739fa121f3177d4061c692887` | `0` |

RCC `MIBRoot_RCC` fingerprint is recorded in the companion evidence manifest.
Pseudocode snapshots are under `output/r2/issue24-rcc-recovery/`. Addresses below
are virtual addresses. For the cited MIBRoot and ooc code in their first LOAD,
file offset = VA minus `0x100000`; do not apply this to every segment or library.

## Boot and reset are distributed

1. RCC IFS `/proc/boot/serverstarterboot` runs
   `/bin/starter -u -p30 -vv -g -e -c /etc/srv-starter.cfg`.
2. The starter config starts `MIBRoot` (process 5), `MMX2RCCEarlyApp` (41),
   RCC `watchdog` (49, `-d disableDSP=1`), and `copy_script` (38). MIBRoot's
   starter termination policy is `IGNORE_ALL`, not a loop respawning MMX.
3. RCC `start_pcie_cbc` starts Qnet/PCIe, assigns RCC `10.0.0.16`, and routes
   through `10.0.0.15`. MMX availability under `/net/mmx` is a native transport
   prerequisite, not proof that Java HMI is alive.
4. MMX `boot.sh` invokes `startup.sh`. The latter reads `/dev/ooc/startup` to
   choose normal/SWDL/ULP/test startup. Normal startup launches native services,
   hardware watchdog, `servicemgrmibhigh`, and `runHMI.sh` in separate processes.
   `runHMI.sh` sources `lsd.sh`, which launches j9 and builds the patched classpath.
5. `ooc` is the MMX On/Off controller communicating with IOC. RCC MIBRoot also
   exchanges On/Off IPC with IOC. IOC is the power/reset controller in this chain;
   a Java failure can cause a software reset request, not merely a direct RCC
   watchdog timeout. The exact IOC physical boot sequencer and retry exhaustion
   count have NOT been established by this analysis. The earlier claim that RCC
   necessarily gives up after a particular number of boots was unverified.

### HMI supervision on MMX

`etc/eso/production/dsistartup.json` configures a 70000 ms HMI supervision period
and 300 s for the expected DSISystem registration. These are different conditions;
do not interpret all frozen-logo cases as an already-armed 70 s heartbeat timeout.

`servicemgrmibhigh`:

- `0x10B2F0`: configures HMI and DSISystem supervision.
- `0x109A8C`: HMI `ResetRequest`, logs `REQUEST SYSTEM RESET` and invokes listener.
- `0x11B298`: system-reset listener writes/logs reset information, waits, then
  invokes the OOC reset proxy path.
- `0x10DE80`: explicit `resetMMXrequest` through the native proxy.

Useful unit evidence is `/mnt/ota/system/logs/sm_resetreason.txt`, HMI dumps,
and `/tmp/mmx-reset-requested` (the last is an indication, not a reset inhibitor).

### OOC heartbeat supervision and IOC command

`ooc`:

- `0x1072B8`: processes IOC frames; copies watchdog-enable into outgoing MMX
  state offset 8 and explicit reset request into offset 16. The keepalive counter
  is at offset 41. When enabled and SM alive becomes stale, it requests reset type
  2 and queues `AliveTimeout`. The threshold comes from the loaded framework
  callback; it is not derived here as a fixed number of seconds.
- `0x1098F4`: handles explicit reset types 1/2/3, stores the reset request and
  creates `/tmp/mmx-reset-requested`. This remains independent of the enable flag.
- `0x10935C`: applies `EnableWatchdog`; `0x10928C`: applies `EnableKeepAlive`.
- `0x10CC70`: parses writes to `/dev/ooc/reset`.

| Command sent to MMX `/dev/ooc/reset` | Verified effect |
|---|---|
| `wdents` | `EnableWatchdog(false)` |
| `wdente` | `EnableWatchdog(true)` |
| `wdkats` or `wdts` | Disable keepalive generation; not the recovery command |
| `wdkate` or `wdte` | Re-enable keepalive generation |

Thus, **with a shell/script already running**, `echo wdents > /dev/ooc/reset`
disables this specific watchdog path. It does not stop explicit resets from SM,
remove a queued reset, stop the Qualcomm hardware watchdog, or create shell access.
From RCC, target MMX explicitly (`on -f mmx /bin/sh -c 'echo wdents > /dev/ooc/reset'`).

RCC MIBRoot `0x1ADF30` receives IOC IPC and echoes the received watchdog flag
unless a local override is set. `0x1AD24C` maps RCC reset requests into outgoing
IOC commands. `MSGTYPE_MMX_RESET_DEACTIVATE` is present in logging, but its name
alone is NOT a supported shell command or verified recovery interface.

### Persistent development watchdog flag

Stock GEM scripts use `/var/sm_disable_wd`. The path is set by `servicemgr.json`
`wd_disable_trigger_file`. `libservmngt.so::FWConfig::load` at `0x16CD4` tests the
configured path while initializing its cached config and updates
`m_watchdog_enabled`. SM `0x11BB00` returns its inverse; `0x1410F8` disables the
application watchdog object; startup includes a simulated-OOC watchdog path.

Creating the file is therefore **not an immediate switch in an already-running
SM**. It is a development-mode option for a subsequent SM startup/boot. Stock
`deactivateMMXWatchdog.sh` also swaps framework transport to TCP/IP; that broad
change is not included in rescue v2. Exact coverage of every HMI/hardware reset
path in CN requires validation. Rescue v2 does not install this persistent flag.

Do not blindly `slay ooc` or `slay watchdog`: removing a heartbeat sender can make
the upstream controller reset sooner.

## Autonomous execution without RED/GEM

### RCC SWDL autorun — the usable existing entry

RCC `mcd.cfg` scans `/net/mmx/fs/sd*`, `/net/mmx/fs/usb*`, and CD every 5000 ms.
The actual SWDL automatic-media selection uses the updater's `SWDL_UPDATE`
media list and checks `SwdlAutoRun.txt`. No user menu call appears in this path.

Verified MIBRoot gates:

- `0x568F48` initializes `CASWDLUtils::PERSISTENCE_FILE` to
  **`/HBpersistence/SWDL/update.txt`**.
- `0x5D46C8` skips new autorun discovery while that file exists, or an active
  test-media/autorun state blocks it. A cold boot does not delete persistent state.
- `0x5D3788` requires an update-classified medium with the marker and recognized
  source identity (CD/USB/SD1/SD2); mere SD mounting or `WMPInfo.xml` is insufficient.
- `0x555924` starts automatic SWDL; `0x556040` handles accepted SetMedium.
- `0x555F00` requires at least one device selection result 1 or 5; otherwise it
  reports `There's no device selected for update. SWDL cannot be started`.
  The FinalScript cannot repair anything if this stage never passes.

The old rescue reuses M.I.B.'s component `Launcher`, version 1, and destination
`/HBpersistence/SWDL/Log/service/MIB`. Existing version state is a plausible
selection problem, NOT a confirmed diagnosis from the affected HU.

The new [rescue card](../../rescue_Swdlautorun_v2/README.md) gives the component
an independent identity and logs both launcher entry and verified JAR quarantine.
It needs no RED/GEM and retains the original checksummed M.I.B. FinalScript.
It cannot bypass `update.txt`, repair an inaccessible MMX filesystem, or execute
when SWDL itself is dead. Do not describe it as a guaranteed unbrick.

### Why plain scripts on an M.I.B. SD are insufficient

M.I.B. V3.7.1 `apps/launcher` calls `. $VOLUME/mod/command.sh` while its installer
is already executing. That is a sourced hook inside the SWDL/GEM-launched program,
not an unconditional boot service. Our normal command.sh wrapper ignores sourced
execution, explaining the first issue log if that wrapper was used.

Two other native entry points were checked:

- MMX `autorunner` consumes `/dev/mcd/AUTORUN`, `/autorun`, `/autorun.sig`; its
  `main` at `0x101F70` calls `0x103114` and rejects results zero and -1 before
  decoding or spawning the script.
- RCC `/sbin/copy_script`, started by `starter`, handles `copie_scr.sh` and
  `copie_scr.sig`; media handler `0x100E6C` requires verifier `0x1013FC` to return
  zero before copying, decoding, and executing through `ksh`.

These are genuine independent native entry points but do not accept an arbitrary
new unsigned rescue script on stock firmware. Standard M.I.B. launcher installation
does not establish that these checks have been patched. No usable signed recovery
payload for these paths was found in this session.

## Decision from card evidence

1. No `rescue_v2_entered.txt`: the repair payload has not demonstrated entry.
   Investigate SWDL state/media visibility through available external access;
   modifying shell removal commands alone cannot solve this stage.
2. Entry marker but no payload log: inspect RCC-to-MMX dispatch and card logging.
3. Payload `FAILED`: the logged mount/quarantine error identifies the next action.
4. `VERIFIED`/`DONE`, then boot still fails: inspect matching native backups and
   HMI logs. The September 23 installer also replaced native components before
   the later CN firmware gates. Do not restore US binaries onto CN as a guess.

If `update.txt` blocks autorun, first preserve/inspect it and the SWDL logs through
an existing network/UART shell or a verified diagnostic recovery interface. Do not
blindly delete a potentially legitimate interrupted firmware-update transaction.
An ODIS reset/clear-user-data operation is not equivalent to removing the JAR.

## Exact CN MU1003 follow-up (2026-09-26)

User reports MMX resets before USB telnet becomes usable. Network login is therefore
not an available recovery prerequisite. No head unit access or reset-cause capture
was obtained; a broken JAR alone does not prove which native reset path fires.

Extracted the matching `MHI2Q_CN_AUG22_K1004_MU1003_4M0906961EG` system/app
and RCC IFS images into `output/r2/issue24-rcc-recovery/cn-*`. The requested
US dump's startup differs from stock: unlike that modified dump, stock CN and US
SWDL startup both launch HMI. Do not assume entering SWDL suppresses Java.

Verified on the CN binaries:

- RCC MIBRoot `0x561450` initializes `/HBpersistence/SWDL/update.txt`;
  discovery `0x5CC244` gates both SwdlAutoRun and TestAutoStart on absence of
  that file and inactive internal states. Its presence on the affected HU is
  still unverified. TestAutoStart is not a demonstrated bypass.
- CN MMX autorunner `0x101F70` invokes verifier `0x103114` before decoding
  and spawning shell. `0x103008` checks the complete RSA/PKCS#1 SHA1 encoded
  block. An ordinary unsigned SD autorun script is not accepted.
- Native `challenge` handler `0x1021F4` requires a verified response before
  `0x101C6C` overlays `/etc/passwd` with `/etc/nopasswd`; it is not a general
  script runner or watchdog control.
- RCC `/usr/bin/modifyPackage.sh:62` can launch internal
  `/mnt/efs-persist/prestart.sh`, but only under its password-disabled test.
  There is no SD pathname in that hook. Installation of standard M.I.B. alone
  does not establish that such an early recovery script exists.
- RCC initializes internal `/HBpersistence/SWDL/autoExec.sh` at `0x4D0D7C`.
  The US counterpart executes this internal file from the SWDL controller's
  initialization. This is not an external SD autorun hook; planting the file
  first remains an unsolved dependency.

### Native deletion candidate: real mechanism, not yet a recovery card

CN `0x530B04` reads `[DeleteFiles]` with `Entry` and `DisplayName`, followed
by numbered entries. `0x53091C` splits semicolon-separated paths and queues
`/net/mmx/` paths separately from RCC-local deletion. `0x5310F4` passes the
file and directory vectors through `0x530600` to proxy `0x5FCDF8` (request 8).
ARM disassembly confirms the vectors are actual call arguments omitted by the
initial decompiler prototype; that prototype was corrected in the saved IDB.

This can avoid invoking a remote shell, but still needs native MMX service
availability and an accepted SWDL transaction. For example, caller `0x4D2AD4`
is `resumeDownload`, checks transaction phase/media identity, then invokes
deletion through `0x4CE41C`. It is not an earlier unconditional media handler.
Writable-mount behavior and acceptance of modified metadata on the affected
unit are not established. No new destructive DeleteFiles card was produced.

The first issue log shows native M.I.B. working for roughly two minutes even
with broken HMI; later retries show no new entry log. These are distinct
observations. They do not establish that the present reset interval is long
enough, nor that the original repair shell ever ran. Need an early RCC/MMX
serial boot log (including reset reason and SWDL state), or evidence that a
native recovery entry point is reached, to distinguish these cases. USB telnet,
RED/GEM, repeated cold power removal, and a renamed script are not solutions to
the missing entry point.

### Validation and references

Host fault tests: `python3 scripts/test_rescue_v2.py`; all pass. Shell syntax and
FinalScript/token checksums verified. No head unit was connected or modified.

- [Issue 24](https://github.com/luka-dev/mib2q-carplay-rgi/issues/24)
- [M.I.B. V3.7.1 launcher](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/blob/V3.7.1/apps/launcher)
- [M.I.B. V3.7.1 SWDL payload](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/blob/V3.7.1/Launcher/stuff.sh)
- Local scripts/configs and decompilations listed above. Previous notes were
  treated as leads and checked against current binaries where conclusions rely on them.
