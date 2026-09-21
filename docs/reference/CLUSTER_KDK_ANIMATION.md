# KDK appearance in AU491 Virtual Cockpit

Verified offline, 2026-09-21. Scope: AU491 0379/1310 A4/A5/Q5 2018.
Firmware, model files and vehicle settings were not patched.

## Files

Firmware root:
`/Users/luka/Desktop/AUDI_2/Firmwares/VC/AU_C1_AU491_0379_1310_prod_8S0906961AL_A4-A5-Q5_2018/extracted/`

- Executable: `stage2_fs/STAGE2.1/bin/gtf2`.
- Shipped visuals: `stage2_fs/STAGE2.2/data/gtf/`.
- Engineer menu: `stage3_fs/data/gem/engdefs/AnimationParametersTubes.esd`.
- Correctly unpacked KZBs: `/Users/luka/Desktop/AUDI_2/Patches/UiModVC/A5_2018_unpacked/fullstream_kzb/`.
- Relevant KZBs: `AU491-5_Basic_Tubes.kzb`, `AU491-5_Sport_Tubes.kzb`,
  `AU491_GlassPlates.kzb`, `AU491-5_Sport_GlassPlates.kzb`.
- Viewer/parser: `/Users/luka/Desktop/AUDI_2/Patches/UiModVC/vc_native_studio/`.

`Firmwares/VC/gtf2.orig`, `UiModVC/A5_2018_unpacked/gtf2` and the firmware
executable above are byte-identical (SHA-256
`2ad9b41b39262168f0c42c7f17598ee8becb2d3fdc1ce8f72afa3bde2c8c6d10`).
Addresses below refer to this binary, image base `0x100000`.

## Confirmed appearance mechanism

`0x1765bc` (openKDKHole) sends animation event 15 to two targets, with payload
selectors (1,1) and (1,2). It requires the navigation/status/visibility guards
and sets the hole-open flag. `0x176780` closes it with event 14. These functions
do not contain movement coordinates or duration constants.

`0x6e66dc`, call at `0x6e68dc`, binds the SpeedTube widget member at offset 768
to the authored property `t_KDK_status`.

`0x6e70f0` (SpeedTube_setupKdkAnimation):

- animation types 15/17 prepare `t_KDK_status: 0 -> 1`;
- types 14/16 prepare `1 -> 0`;
- fade-in requires KDK-visible, not-follow, and widget state greater than 14;
- types 18/19 have separate flap handling, not the same scalar setup here.

`0x6e6f38` fixes the property at 1 or 0 on completion. Each frame,
`0x6deb04` writes the value returned by `0x721fa4`:
`start + (end-start) * AnimationManager.progress(animationId)`.
The local clock path is confirmed linear (`0x11ae3c`). An optional external
progress path exists; see the detailed supplement below.

## Authored Basic/Classic visual phases

Decoded using the existing read-only `vc_native.kzb.KzbArchive` parser from
`AU491-5_Basic_Tubes.kzb`. Let `p = t_KDK_status`, clamped to 0..1.

| Node/property | Binding | Effect during appearance |
| --- | --- | --- |
| `Layers/t_root/t_pr/t_viewport/t_DTM_placeholder.LayerOpacity` | `p` | Content opacity follows the transition |
| `Scenes/t_big_speed/t_root/t_center_right/KDK_hole_decorator.BlendIntensity` | `min(2.5*p,1)` | Frame reaches full intensity at p=0.4 |
| same parent, `KDK_hole_fix.BlendIntensity` | `clamp(5*p-2,0,1)` | Additional backing blends in between p=0.4 and p=0.6 |
| same parent, `t_KDK_hole.BlendIntensity` | `clamp(2.5*(1-p),0,1)` | Covering mask fades away between p=0.6 and p=1 |

These percentages refer to **animation progress**, not necessarily elapsed
time. The coefficients are simplified from serialized float32 values.
Visibility also depends on `t_morph` (stage transition); do not treat KDK as
an independently moving rectangle based only on this table.

The Sport Tubes package has the same last-phase `t_KDK_hole` intensity rule.
Its content placeholders instead use `t_follow_fade`; the Classic placeholder
binding must not be copied blindly into Sport. Sport glass-plate state uses
`gp_sd_KDK_state` (HubContent object constructor at `0x6e77f8`, not a getter).
Its independently verified timing and binding are detailed below.

## Duration / speed

Engineer menu lines 63-67:

```
slider
    label "KDK Fade Duration (10ms)"
    value persys 0x2 52
    limits absolute 0 200 1
```

The menu exposes 0..200 units: **0..2000 ms, step 10 ms**. The recovered initial data-pool value is 20, i.e. 200 ms; runtime/persisted
settings may override it. The exact setter chain from this menu to the DP
array has not been recovered.
`poll 1000` is the menu polling interval, not animation duration.

Native timing chain:

1. `0x646310` reads indexed `m_kdkFadeAnimation` model values, either from
   a local array or from a data-pool binding.
2. `0x6e7ef4` (aboutToShow) and `0x6e87d0` (update) call `0x6e4d70`, which
   reads four values and multiplies every one by 10; stores at widget+912.
3. `0x6e7308` uses these timing parameters for animation types 14..19.
4. `0x1445ec`, forward=true, sets start=`params[0]`, end=`params[0]+params[1]`.

Thus the participating widget's fade interval is `10 * model[1]` ms, with
start offset `10 * model[0]` ms. Other animation participants can affect the
overall animation length. Model name string:
`MV_ClusterFull_SpeedTube_kdkFadeAnimation`.

Adjacent engineer controls separate the other parts of opening the panel:
Hub Morph Duration (`persys 0x2 56`), Gear/Speedo Move Duration (`57`),
Flap Morph Duration (`58`), all in 10 ms units. The KDK fade setting alone
does not describe every movement of the surrounding instrument layout.

## Detailed verified supplement — 2026-09-21

### Recovered default: 200 ms

The ClusterFull construction routine is `0x59f610`. Its very large decompilation
fails; the relevant ARM disassembly was checked directly:

- `0x5c62bc`: name `MV_ClusterFull.TubeContainer.SpeedTube`.
- Model base in the generated page: `page + 0x2ac80`.
- `0x5c64e8`: stores DPID `0x02000329` at page+0x2ae74 = model+500.
  LR is formed as `0x02000328 + 0x13 - 0x12`; hence no literal pointer search
  for `0x02000329` finds this assignment.
- Flags at model+594 are ORed with 0xf5, including bit0x40 selecting DP mode.
- Getter `0x646310` explicitly names this field `m_kdkFadeAnimation.m_DPID`.
- Namespace2 descriptor table `0xa8b84c`, entry index0x329, descriptor `0xa8eadc`:
  type0x82, list pointer `0xa89544`.
- List header has capacity/length/initialLength = 4/4/4 and data `0xa7f374`.
- Exact initial bytes: `00 00 14 00 00 00 14 00`; int16 values **[0,20,0,20]**.
- `ReadModelAnimationTiming10ms` converts them to **[0,200,0,200] ms**.
- `SpeedTube_getKdkAnimationParameters` passes forward=true for both incoming
  and outgoing KDK events, so this participant uses the first pair in both
  directions. Do not assert that fade-out here selects entries2/3.

This is the executable's initial DP configuration, not a measurement of the
current persisted configuration on a physical car.

### Event identities and start conditions

The native animation-name table at `0x7bef30` identifies:

| ID | Name |
|---|---|
|14|KDKFadeOut|
|15|KDKFadeIn|
|16|KDKHoleFadeOut|
|17|KDKHoleFadeIn|
|18|KDKFlapFadeOut|
|19|KDKFlapFadeIn|

Although the controller function is named openKDKHole in logs, `0x1765bc`
emits event15, **KDKFadeIn**, twice with target selectors(1,1) and(1,2).
This is not evidence of two sequential 200ms fades.

`0x6e70f0` configures the scalar only for14/16 and15/17. Types18/19
are separate cases. Incoming fade requires KDK-visible, not hideKdk/follow,
and widget state>14. The precise product meaning of all numerical states
has not been recovered; do not relabel them based on guesswork.

`0x6e6b2c` updateKDK has distinct preview, follow and normal branches:
preview clears KDK/follow/ETA/DTM fades; follow sets KDK=0 and follow/ETA/DTM=1;
normal visible enables ETA/DTM and sends KDKFlapFadeIn; normal invisible clears
KDK/ETA/DTM and sends KDKFlapFadeOut. Other inactive states clear the fades.
`0x176c5c` showKDKFlap sets DPID33555167 true under its guards and has a
special S-Design, non-bigStage, follow-case branch. It does not itself specify
a 200ms movement trajectory.

### Clock and per-frame evaluation

Local animation time comes from `eal::CTimeProvider::m_time`. At elapsed t,
start offset S, and duration D, the primitive's incoming progress is:

```
p = clamp((t - S) / D, 0, 1)
```

Default: S=0, D=200ms. For disappearance use `p=1-clamp(t/200,0,1)`.
There is no ease-in, ease-out, spring, overshoot or bounce in this local scalar.
Changes of slope visible in the composition come from the KZB bindings below.

Native chain:

1. `0x11e128` collects participating widgets' start/end intervals.
2. `0x125be0` inserts starts/stops and determines the maximum overall end.
3. `0x11d980` starts the clock interpolation.
4. `0x11ae3c` evaluates linear elapsed/duration.
5. `0x124ca0` converts global progress to timeline position, starts primitives,
   and computes each primitive's own local fraction.
6. `0x14e6ac` stores primitive progress; `0x11ca14` retrieves it.
7. `0x721fa4` computes start+(end-start)*progress.
8. `0x6deb04` writes t_KDK_status; Kanzi evaluates authored bindings.
9. `0x6e6f38` fixes the final state at exactly0 or1.

A longer overall animation does not necessarily stretch this widget: each
participant retains its own interval. Activation and completion occur on
update ticks, not at a guaranteed raster scan timestamp.

### Basic/Classic: exact composition over progress

These are authored property values. BlendIntensity is a shader/material
input, not a measurement of final screen luminance. The descriptive words
frame/backing/cover are interpretations of node names and their composition.
They are not claims about a separately observed video.

```
contentOpacity = p
frameIntensity = min(2.5*p, 1)
fixIntensity   = clamp(5*p - 2, 0, 1)
holeIntensity  = clamp(2.5*(1-p), 0, 1)
```

The frame's complete rise takes0.4D; the fix rise takes0.2D; the hole's fall
takes0.4D. The content rises for allD. At D=200ms:

- **0ms:** p0; content0, frame0, fix0. HoleIntensity1, but the hole's own
  Visible rule evaluates false at exactlyp0; numeric intensity alone does
  not establish that the node is drawn.
- **0..80ms:** content0→40%; frame0→100%; fix0; holeIntensity1.
- **80..120ms:** content40→60%; frame1; fix0→100%; holeIntensity1.
- **120..200ms:** content60→100%; frame/fix1; holeIntensity100→0%.
- **200ms:** final p1; content/frame/fix1; holeIntensity0.

The hole Visible rule is `p * (step(1,m) + step(m,0))`, m=t_morph.
For m in[0,1], it is nonzero only when p>0 and m is0 or1. For0<m<1
(stage morph in progress), this node is disabled. Its binding is marked
`delayed=True`; the exact renderer scheduling latency is not established.
Consequently the table assumes a stable stage, not a simultaneous stage switch.

Direct t_KDK_status bindings in these tube packages affect opacity/intensity
and visibility; no direct translation/rotation/scale binding driven by this
property was found. Motion of surrounding components belongs to other
controls, including t_morph, t_hub_morph_Right and t_flapStatus_right.

### Calculated frame sampling at 60 Hz

This is mathematical sampling of the verified default at an **assumed**60Hz,
not captured frames and not a claim that the physical cluster runs at60FPS.
Time zero is the local primitive start, not a button press or CAN message.
Values are rounded percentages. See also `CLUSTER_KDK_60HZ.csv`.

|Sample|Time ms|p / content %|Frame %|Fix %|Hole intensity %|
|---:|---:|---:|---:|---:|---:|
|0|0.00|0.00|0.00|0.00|100.00|
|1|16.67|8.33|20.83|0.00|100.00|
|2|33.33|16.67|41.67|0.00|100.00|
|3|50.00|25.00|62.50|0.00|100.00|
|4|66.67|33.33|83.33|0.00|100.00|
|5|83.33|41.67|100.00|8.33|100.00|
|6|100.00|50.00|100.00|50.00|100.00|
|7|116.67|58.33|100.00|91.67|100.00|
|8|133.33|66.67|100.00|100.00|83.33|
|9|150.00|75.00|100.00|100.00|62.50|
|10|166.67|83.33|100.00|100.00|41.67|
|11|183.33|91.67|100.00|100.00|20.83|
|12|200.00|100.00|100.00|100.00|0.00|

Phase boundaries80ms and120ms fall between samples, near samples4.8 and7.2.
At30Hz the same200ms contains6 frame intervals; at60Hz12; at50Hz10.
A slower rendering loop skips intermediate samples; the clock still advances
by elapsed time rather than counting a fixed number of frames.

### Disappearance

With the default local200ms fade-out, the same bindings run backward:

- 0..80ms: holeIntensity0→1; content1→0.6; frame/fix remain1.
- 80..120ms: fix1→0; content0.6→0.4; frame/holeIntensity remain1.
- 120..200ms: frame1→0; content0.4→0; fix0; holeIntensity1.
- At p0 the hole Visible rule turns it off.

For inverse events configured as a collision pair, `0x11eaa8` can begin the
new animation at1-oldProgress and scale remaining duration to preserve
continuity. Exact collision-pair membership and all concurrent participant
interactions were not fully enumerated; do not assume every state change
is this symmetric reversal.

### Sport: independent glass and follow content

Sport Tubes has the hole intensity/visibility rules described above, but
its content placeholders use **t_follow_fade**, not Classic's p binding.
Do not apply the entire Classic opacity table to Sport content.

`0x6e77f8` constructs HubContent id13 with property **gp_sd_KDK_state**.
Its vtable routes fadeIn→0x6e1138, fadeOut→0x6e1098, tick→0x6e101c.
`0x6e0ab4` initializes shared HubContent timing to **[0,300,0,200]ms**.
Thus its full-range local appearance takes300ms and disappearance200ms.
An immediate flag makes duration0. `0x6bbf74` scales duration by remaining
fractional distance from the current value; e.g. a rise from0.4 to1 takes
180ms of a nominal300ms fade. The transition remains linear.

In `AU491-5_Sport_GlassPlates.kzb`, node
`Scenes/generic_glassplate_mmik_back/centerTube_SD_KB_plate`:

```
BlendIntensity = max(1 - 3*t_morph, 0) * gp_sd_KDK_state
```

At m0, glass intensity follows the300ms scalar. At m0.1 its maximum is0.7;
at m0.2 maximum0.4; at m>=1/3 it is0, regardless of the scalar. This binding
is delayed. The300ms clock is relative to that object's own fade start;
its start-time offset versus the200ms KDK event has not been proved, so
adding the two durations or declaring a fixed300ms total would be unjustified.

Both Basic and Sport flap BlendIntensity simplify to
`5 * max(t_morph - 0.8, 0) * t_flapStatus_(left/right)` for the normal0..1
control ranges. During a stage morph the flap contribution is suppressed
until m>0.8, then rises to its flap-status value at m1. This formula specifies
blending, not the complete flap mesh's movement trajectory.

### External synchronization and exceptional paths

The AnimationManager can use annotation/external progress instead of the
local clock, conditional on manager state, DPID33555245==1, and an attached
annotation ID. `0x11d1f0` reads progress from DPID33555173[annotationId].
A manager mode either takes that value directly or uses its arrival only to
start a local linear clock. Therefore exact real-world time/easing cannot be
inferred solely from the local default in every synchronization mode.

There are fallback timeouts:5000ms when progress is absent and10000ms when
an external animation remains unfinished; fallback disables annotations and
restarts local clocks. These are watchdogs, not normal appearance durations.
The500ms 'KDK state may be invalid' recovery in0x6e87d0 is also a repair
mechanism, not the ordinary fade duration. Engineer-menu poll1000 is polling.

### Verified limits / work needed for actual filmed frame matching

Recovered: default200ms KDK scalar, local linear curve, authored piecewise
blending, event identities, separate Sport300/200ms glass clock, and key
preview/follow distinctions.

Not established: actual car's persisted overrides; exact menu setter chain;
external progress producer/curve for the active car configuration; render
cadence and delayed-binding latency; complete scheduling relationship between
flap/hub/glass/KDK participants; all movement keyframes during a simultaneous
stage morph. Recovering those is required for a claim of an exact complete
on-car pixel/frame sequence. No screenshot/video sequence was synthesized
and presented as authentic firmware output.
