# Virtual Cockpit Classic/Sport layout source

The loader must not infer the VC design from the CarPlay fullscreen/compact
state. They are independent axes in the stock system.

## VC-side verification

Authoritative binary:

`Firmwares/VC/AU_C1_AU491_0379_1310_prod_8S0906961AL_A4-A5-Q5_2018/extracted/stage2_fs/STAGE2.1/bin/gtf2`

- `gtf2:0x18F4B4` handles data-pool message `0x0100004E`
  (`INTERN_Designated_LayoutVariant_Choice`), passes the raw variant to
  `sub_12D208`, then publishes the four-byte design-change state.
- `sub_12D208@0x12D208` maps raw `0` to design 0 and raw `256` to design 1;
  other values take the unknown fallback.
- `gtf2:0x190124` handles the corresponding “layout choice ready” path and
  publishes the confirmed design change.
- The binary labels the two selectable designs with Classic (`0xA5104F`) and
  Sport (`0xA51082`).
- `isSportDesign@0x176740` reads the internal GTF condition through
  `sub_217B74`; condition values 4, 5, 14, 15 and 16 select Sport. This is a VC
  template condition, not a directly consumable HU API.

## HU-side seam

`DSICarKombi.DCDISPLAYPRESETSTYPE_CLASSIC = 2` is a real enum, but
`DCDISPLAYPRESETSTYPE_DRIVINGPROFILE = 3` is **not** an alias for Sport. It is a
dynamic display-preset class associated with the vehicle's drive profile and
must not be used as the currently rendered skin.

The authoritative stock-HU seam is the integer persisted by
`LastmodeStorage.getSkin()`:

- `DisplayManagerEvo.getSkin()` obtains it from
  `framework.getLastmodeHandler().getLastmodeStorage()`;
- `DisplayManagerEvo.isSportSkin(int)` returns true only for `skin == 1`;
- its KDK setup selects the explicit Sport assets for 1 and Classic assets for
  the normal `skin == 0` path;
- `DSICarTimeUnitsLanguageManager.updateSkin()` writes each live skin update to
  that same `LastmodeStorage`.

`AltScreenModule` reads this value on start and during its existing 250-ms
connected-context reconciliation. This follows live Classic/Sport changes
without adding a second DSI activator or overriding the stock CarKombi listener.

## CarPlay view-area mapping

The Audi View button remains a logical fullscreen/compact choice. The VC preset
selects the physical compact placement:

| Effective index | Name | Safe area |
|---:|---|---|
| 0 | fullscreen | `(270,90) 900x362` |
| 1 | sportscreen | `(90,135) 360x317` |
| 2 | classicscreen | `(540,135) 360x317` |

All three retain the full encoded/visible frame `(0,0) 1440x542`. The same
resolved state is sent to iOS (`updateViewArea`) and the local loader, preventing
the loader and Maps content from using different geometry.
