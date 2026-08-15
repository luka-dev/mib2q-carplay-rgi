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

The stock MU1316 HMI already owns `DSICarKombi` instance 0 through
`de.audi.tghu.navi.app.car.kombi.DSICarKombiController`. Its listener originally
subscribed only to attribute 4 (`BCViewOptions`). The classpath override keeps
that logic and additionally observes:

- 96: display-preset list element count;
- 97: display-preset list updates (`pos -> type`);
- 99: active display-preset position;
- 100: display-view configuration (diagnostic only).

The DSI semantic types are declared by stock `DSICarKombi`:

- `DCDISPLAYPRESETSTYPE_CLASSIC = 2`;
- `DCDISPLAYPRESETSTYPE_DRIVINGPROFILE = 3` (Sport).

Attribute 99 is resolved through the attribute-97 record whose `pos` matches the
active value. Unknown/invalid updates do not change the last known style.

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
