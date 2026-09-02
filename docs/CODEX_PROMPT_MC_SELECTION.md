# Task: what selects the multichannel amplifier connection, and does this car take it?

## Where we are

The six-voice rendering defect is **fixed and confirmed on the car**: `mpl5_dio_ent`'s caps
record now mirrors `mpl1_int_ent`, the CSD session is built six-channel at route attach
(`pcm format => sample_rate=48000, channels=6` in the unit's own slog, where it read
`channels=2` for a month), the continuous 62.5 Hz tick is gone, and both 5.1 and stereo play
correctly. See `deploy/deva-ctrl-qc/BUILD_CHAIN.md`.

What remains is **channel delivery**. A one-hot test — six channels, one 440 Hz tone at a time,
three seconds each, everything else at digital zero — was played into `/dev/snd/mpl5_dio_ent`
with `qsa_play`, no CarPlay in the path:

    device limits: voices 6..6 rate 48000..48000 frag 18432..18432 formats 0x1000
    TOTAL 1152000 frames in 23879 ms -> 48243 frames/s

    channel 1 (L)   audible
    channel 2 (R)   audible
    channel 3 (C)   silent
    channel 4 (LFE) silent
    channel 5 (Ls)  silent
    channel 6 (Rs)  silent

So a correct, correctly-clocked six-channel stream reaches the driver and only the front pair
arrives at speakers. The amplifier is not short of outputs: `EV_AMPMst16C2Gen2ALPI` is a
16-channel MOST amplifier whose crossover set (subwoofer pair, woofer/mid pairs, a 336 Hz
nine-channel mid/tweeter bank, three tweeter bands) is a multi-speaker system, not a front pair.

## The question

Your sink audit found that connection 162 resolves to `eSRC_ENT_AMP_DIO_MEDIA_MC` +
`eSNK_DTCP51` — physical `SinkNr 0x26`, `BlockWidth 14`, six channels — and recorded that the
value comes from "the routing table and amplifier coding". That last part is now the whole
question.

1. **In `CAudioManagementPresCtrl::requestRequestConnection` (`FUN_001f1790`) and the routing
   table around it: what actually decides between the multichannel source/sink pair and a
   stereo one?** Is there a non-`_MC` variant for `ENT_AMP_DIO_MEDIA`, and what condition
   selects between them — amplifier coding, a PR code, a capability query, the number of
   channels the source declares, or something else?
2. **Where does that condition's input come from at runtime** — EEPROM/coding data, a CAN/MOST
   query of the amplifier, a config file, or a property? Name the storage and the field.
3. **Does anything on the head-unit side declare a channel count for the CarPlay source**, and
   if so where? If the HU tells audio management that DIO media is stereo, that would explain
   the measurement exactly, and it would be the thing to change.
4. If the multichannel pair is genuinely selected unconditionally for this amplifier, say so
   plainly. Then the remaining suspect is the SHARC source-to-output matrix inside the
   amplifier, which `Firmwares/SoundAmp/AMP_ENTERTAINMENT_7_1_MOST.md` already notes is absent
   from the V850 image and needs the external SHARC flash — and that is a hardware step, not a
   static one, so say that rather than guessing at it.

## Constraints

Static analysis of the head-unit firmware at
`/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/` and the
amplifier material in `/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/`. Do not modify any
binary. Do not propose flashing the amplifier.

Accepted and closed, do not revisit: the deva caps and CSD geometry
(`docs/ROUTER_INPUT_WIDTH_AUDIT.md`, now fixed), the CSD device id
(`docs/DEVICE_TOPOLOGY_AUDIT.md`), the amplifier sink table
(`docs/DIO_MEDIA_SINK_STATIC_AUDIT.md`), and the whole CarPlay stack
(`docs/AUDIO_5_1_STATE.md` §11).

## Deliverable

The condition that picks multichannel over stereo for connection 162, where its input lives,
and whether this car satisfies it — or a clear statement that the choice is unconditional and
the remaining suspect is inside the amplifier's SHARC image.
