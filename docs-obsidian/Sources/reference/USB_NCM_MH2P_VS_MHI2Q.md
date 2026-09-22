# USB / smartphone_integrator: MH2p vs MHI2Q

Compared because the connect failure is `USB eligible: N (No Network Interface)` — see
`CONNECT_FAILURE_ROOT_CAUSE.md`. Trees: `MH2p_US_LB636_P2712` and `MHI2Q_US_AUG22_P5087_MU1316`.

## Same on both — so not what distinguishes us

`smartphone_integrator` owns the OTG stack lifecycle and uses `stopncm.sh` as its cleanup on both
platforms. MH2p's SI binary contains the literal string `/etc/scripts/stopncm.sh` and
`SI_EVENT_ERROR_CREATING_CARLIFE_NCM_FLAG`, so the "SI tears the NCM interface down on child stop"
pattern is the eSolutions design, not an MHI2Q quirk. MH2p's `/etc/scripts` in this extraction has
only six files and no ncm scripts, but that is an extraction gap, not evidence of a different
design — the SI binary proves the script is expected.

Both platforms also ship `devnp-ncm.so`.

## Genuinely different

| | MH2p (Tegra3) | MHI2Q (MSM8960) |
|---|---|---|
| device-controller DLL | **`devu-iap2ncm-tegra3-ci.so`** — iAP2 **and** NCM in one driver — plus a separate `devu-iap2-tegra3-ci.so` | `devu-usbrndis-msm8960-ci.so` |
| usblauncher Apple plugin | **`usblauncher-Apple.so` present** | absent |
| NCM network driver actually used | `devnp-ncm.so` ships | `startncm.sh` mounts **`devnp-usbdnet.so`** with `protocol=ncm`, even though `devnp-ncm.so` ships at `/app/armle/lib/dll/devnp-ncm.so` |
| device stack entry | — | `Device_Stack.cmd = /etc/scripts/start_usb_device_stack.sh`, `connect_poll = 100` (commented "in order to improve CarPlay connection times"), `descriptors = {usbdesc_carlife, usbdesc_carplay, rndis}` |

`start_stack::device,1` selects the CarLife descriptor and `device,2` the CarPlay one
(`start_otg_stack_device_mode.sh` picks by the `/tmp/carlife_active` flag).

## Our gadget descriptor

`usblauncher_carplay_descriptor.lua` — VID `0x1A96`, PID `0x2018`, `AUDI AG` / `iAP2 NCM Accessory`.
One configuration, four interfaces:

| # | class | contents |
|---|---|---|
| 0 | `0xFF/0xF0/0x00` | iAP interface, BulkIn+BulkOut 512 |
| 1 | `0x02/0x0d/0x00` | CDC NCM control, InterruptIn 64, with the CDC header/union/ethernet/NCM functional descriptors (`iMACAddress = 000022446688`) |
| 2 | `0x0A/0x00/0x01` **alt 0** | CDC Network Data — **no endpoints** |
| 3 | `0x0A/0x00/0x01` **alt 1** | CDC Network Data — BulkIn+BulkOut 512 |

Two things follow. First, NCM is *always* in the descriptor, so "only one of two functions" cannot
happen at descriptor level — the phone always sees the NCM function offered. Second, the data path
only carries traffic once the host issues `SET_INTERFACE` to **alt 1**, and something on our side
has to service those endpoints: `devnp-usbdnet.so`, mounted by `startncm.sh`. If that mount did not
happen, the function is advertised but nobody answers on it.

## Candidates this comparison produces

1. **Use `devnp-ncm.so` instead of `devnp-usbdnet.so protocol=ncm`.** The purpose-built driver ships
   on our unit and the stock script does not use it. This is a candidate, not a known fix — the
   mount options may differ and the generic driver may be the deliberate choice. Cheap A/B: one line
   in `startncm.sh`.
2. MH2p's combined `devu-iap2ncm` DLL suggests the iAP2/NCM split is the fragile part of our design,
   but the Tegra3 DLL is not portable to MSM8960 (already recorded).

Neither is worth trying before the first question is answered: **why does the dio generation die at
~15 s**, since that churn is what forces the OTG teardown in the first place.
