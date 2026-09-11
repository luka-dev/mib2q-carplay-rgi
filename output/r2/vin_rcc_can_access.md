# MU1316 RCC VIN / CAN access: bounded static audit

Date: 2026-09-09. No vehicle connection, CAN transmission, deployment or binary
modification was performed. This establishes stock access paths, not a working
third-party CAN client or an authenticated vehicle identity.

## Inputs

Root: `/Users/luka/Desktop/AUDI_2/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/rcc_extracted/ifs3_out/`

Both binaries are ARM32 little-endian ELF, load base `0x100000`, interpreter
`/usr/lib/ldqnx.so.2`. Addresses below are virtual addresses unless labelled
otherwise. All cited code and string sites are in LOAD0, where file offset =
VA - `0x100000`.

| Binary | SHA-256 |
| --- | --- |
| `bin/car` | `f97e32e7602ea573c43769c45280bad3460187c1bde98894f0843ab0192c7f0c` |
| `usr/apps/MIBRoot` | `bab21755106ea5306f387102c0f81a5544f79f415de5564baec76fc7aaa05c81` |

## Confirmed in disassembly

`car` calls `CHBComponentInfo::addMountPointForIdentifier` with these pairs:

| Call VA | Device path | Identifier |
| --- | --- | --- |
| `0x11cec4` | `/dev/ipc/ipc8` | `IPC_CANsignals_DeviceName` |
| `0x11ced8` | `/dev/ipc/ipc9` | `IPC_CAN_PSDsignals_DeviceName` |
| `0x11cee8` | `/dev/ipc/ipc10` | `IPC_CAN_TP_DeviceName` |
| `0x11cef8` | `/dev/ipc/ipc11` | `IPC_CANdiagnosis_DeviceName` |
| `0x11cf08` | `/dev/ipc/ipc12` | `IPC_CANstatus_DeviceName` |

These are stock endpoint registrations. They do not establish that another
reader can open them concurrently, that the read stream is raw CAN frames, or
that a guessed request is valid.

`MIBRoot` at `0x374a1c` onward reads input fields through `0x36c200`, takes
their values at offset +4, and assembles bytes. At `0x374be4` onward it filters
characters while constructing a CHBString. The same path references
`[SERV] VIN[%d]: removed Character 0x%02x`, `[SERV] Send VIN: %s` and
`[SERV] VIN: %s`. At `0x374cc8` it assigns the string into object state,
and at `0x374cf0` it invokes an indirect callback with the constructed string.
This identifies a stock in-memory VIN processing path. Its upstream
subscription and exact source freshness have not been recovered here.

## Additional string evidence (not a recovered API)

- `car`: `eCPFuncVin`, file offset `0x168980`, VA `0x268980`.
- `car`: `CAN_IPC_TPWorker`, `sgw_sendCanMsg`, CAN TP registration/response logs.
- `MIBRoot`: `/dev/shmem/VIN.txt`, file offset `0xd43b51`, VA `0xe43b51`;
  literal pointer at VA `0x373db0`.
- `MIBRoot`: `VIN_NoResponse`, `onVINDataUpdate`, `listener_updateVINData`.

Separate prior inspection of the stock Java JAR confirmed
`DSICarVehicleStates.ATTR_VINDATA` and
`DSICarVehicleStatesListener.updateVINData(String,int)`.

## Implications and remaining work

Reading VIN through the stock RCC signal/service path can avoid the text-file
cache. A custom direct client still needs its registration protocol, VIN
identifier/payload, validity rules and coexistence behaviour established.
Neither `eCPFuncVin` nor a service message identifier is automatically a CAN ID.

Next bounded investigation: trace the stock VIN handler upstream to its
subscription, then recover the corresponding `car` IPC dispatch. Prefer a
supported subscription before considering another direct consumer of a device
already owned by `car`. No arbitration IDs, UDS addresses or raw CAN requests
were established by this audit.

For per-VIN build binding, changing the acquisition layer avoids dependence on
an editable VIN file; it does not by itself prevent runtime input substitution.
