# MMI Focus and Surround: baseline audit

Date: 2026-09-08. Read-only static inspection; no vehicle commands,
deployment, model changes, or runtime setting changes.

## User state and recommendation

User tentatively reports Surround at minimum or zero and Focus probably All.
Neither is a confirmed live readback. The previously selected target is stock
MMI 3D OFF, not forced DSP bypass.

For the proposed six-channel upmix using front and rear circuits, recommend
Focus All as a reproducible baseline. This is a goal-driven recommendation,
not a measured optimum at the driver's seat and not proof of flat DSP gains.

Audi A5 2018 owner's manual, Sound settings, printed p. 255 (viewer p. 256):
https://www.manualslib.com/manual/4255192/Audi-A5-2018.html?page=256
describes All as activating the complete speaker system with symmetric
distribution, and Front/Rear as restricting active speakers to that zone.
This is documented menu semantics, not a measurement of each physical output.

## Java evidence

Source root: `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-combined-vf`.

- `de/audi/tone/app/sound/SoundFocus.java`: UI indices All/Driver/Movie/Front/Rear
  are 0/1/2/3/4; DSI presets are 1/8/16/2/4. Visibility depends on the supplied
  preset capability mask. Generic support does not establish availability of
  Driver or Movie on this Audi.
- `de/audi/tone/app/sound/SurroundRange.java`: level model 1000040 and on/off
  model 1000116 are distinct. Automatic `level != -9` -> on/off coupling is
  guarded by the Bentley Bentayga Alpine Premium predicate. Do not generalize
  that exception to Audi; downstream Audi behavior is not excluded by this.
- `AbstractSoundRange.java`: limits come from the service; midpoint is
  `(min + max) / 2`. No fixed Audi range is established here.
- `de/audi/tone/app/dsi/DSISoundListenerImpl.java`: `updateSurrLevelRange`
  propagates supplied min/max to the model without a numeric offset.

## Native MIBRoot evidence

Binary: `/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/MHI2Q_US_AUG22_P5087_MU1316/RE/MIBRoot/MIBRoot`.
ARM32 LE ELF; addresses below are virtual addresses. SHA-256:
`e6e710d7467be935c3fea933ef9068a66786c392f3f7a0e78155ad9a1063acd1`.

Bounded disassembly via `R2_NOPLUGINS=1 r2 -q -e scr.color=0`:

- `requestSetSurroundLevel`, entry 0x1E4254, identifying string 0xDE2176:
  at 0x1E4324 loads the requested signed halfword unchanged; at 0x1E4334
  selects SoundSettings ID 0x0B; 0x1E4344 calls 0x1D4724. No minimum/zero
  special case or level offset in this setter. This does not prove the
  complete downstream DSP semantics of zero.
- `requestSetPresetPosition`, entry 0x1EB23C, string 0xDE3C87:
  ID 0x20 at 0x1EB310; converts the DSI mask with 0x1E2CD0 at 0x1EB320;
  passes the signed result to 0x1D4724 at 0x1EB340, WITHOUT subtraction.
  Lookup table 0xDDEE50 is [0,1,2,4,8,16,32,64,128]; matching array index
  is returned. Thus All/Front/Rear/Driver/Movie -> values 1/2/3/4/5 for
  this HU SoundSettings event. These are not asserted to be J525 wire values.
- For comparison, the 3D setter subtracts one at 0x1EB1F0 for ID 0x22.
  Do not reuse that conversion for Focus.
- Surround range callback branch 0x1FCDAC, string 0xDE8AD7:
  0x1FCDE4/0x1FCDE8 load received min/max; 0x1FCDF0 calls 0x1C8610,
  followed by client virtual callback at 0x1FCE08. No fixed numeric range
  is supplied by this branch.

## Remaining limits / next step

Do not encode UI minimum as numeric zero or as DSP OFF. The J525 clamp
[-9,+9] found previously is not itself proof of the range advertised to
this source/variant. Obtain an actual Surround value/range and Focus readback
or menu photo during CarPlay playback. Preserve the selected minimum for
the baseline pending that confirmation; do not arbitrarily move to midpoint.

Previously verified AA648 Surround gain addition is conditional on ASAD C0;
the target six-position FC stream does not enter that particular branch.
This is not proof that every Surround consumer is inactive for FC.
Exact Focus-to-J525 calibration bindings and full DSP effects remain open.
