# MHI2 CarPlay Route Guidance

CarPlay patch set for Audi MHI2 infotainment.
(Based on MHI2Q MU1316 firmware, but may need rebuild for different versions.)

**Disclaimer:** Use at your own risk. These patches modify firmware binaries and system configurations on your infotainment unit. Always back up all original files before making any changes. The authors are not responsible for any damage, bricked devices, or warranty issues resulting from use of these patches.

## Contents

- [Features](#features)
  - [Route Guidance](#route-guidance)
    - [Pipeline overview](#pipeline-overview)
    - [HUD (BAP)](#hud-bap)
    - [VC text overlays](#vc-text-overlays)
    - [VC MOST video (custom renderer)](#vc-most-video-custom-renderer)
    - [Bargraph synchronization](#bargraph-synchronization)
  - [Cover art](#cover-art)
  - [Touchpad input for CarPlay](#touchpad-input-for-carplay)
  - [Planned](#planned)
  - [Not possible — AltScreen](#not-possible--altscreen)
- [Architecture](#architecture)
  - [Patch components](#patch-components)
  - [Component map](#component-map)
  - [Boot / init sequence](#boot--init-sequence)
  - [Threading model](#threading-model)
  - [File system layout](#file-system-layout)
- [Why AltScreen is impossible (deep dive)](#why-altscreen-is-impossible-deep-dive)
  - [Diagram A — current state on MU1316 (broken)](#diagram-a--current-state-on-mu1316-broken)
  - [Diagram B — how it works on a properly equipped HU](#diagram-b--how-it-works-on-a-properly-equipped-hu)
  - [Component versions side-by-side](#component-versions-side-by-side)
- [Build](#build)
  - [Prerequisites](#prerequisites)
  - [libcarplay_hook.so (C hook)](#libcarplay_hookso-c-hook)
  - [maneuver_render (3D renderer)](#maneuver_render-3d-renderer)
  - [carplay_hook.jar (Java patch)](#carplay_hookjar-java-patch)
  - [Testing the renderer locally](#testing-the-renderer-locally)
- [Deploy](#deploy)
  - [Quick checklist](#quick-checklist)
  - [Step-by-step](#step-by-step)
- [Contributing](#contributing)
- [References](#references)

---

## Features

What this patch makes the head unit + cluster do that stock MHI2 doesn't:

- **Full HUD route guidance** from CarPlay nav (Maps, Waze, etc.) — maneuver icons, lanes, distance bargraph, ETA, destination
- **Custom 3D maneuver overlay** drawn into the cluster's MOST video stream (the same video plane the HU uses for its native map)
- **Album cover art** forwarded to the cluster's now-playing widget
- **MMI touchpad → DPAD bridging** so finger drags navigate CarPlay menus

What it does **not** do, with explanations:

- **CarPlay AltScreen on cluster** — architecturally blocked on this HU generation, [see deep dive below](#why-altscreen-is-impossible-deep-dive)

### Route Guidance

#### Pipeline overview

iOS RGD messages enter through the iAP2 hook and fan out into **two
independent rendering branches** on the VC:

- **HUD branch** (BAP LSG 50) — feeds the text/icon HUD widgets the
  cluster firmware already knows how to draw. Cheap, always-on.
- **Render branch** (custom MOST-video renderer) — draws full 3D maneuver
  scenes into the cluster's video pipeline. Spawned only while a
  route is active.

Both branches receive the same parsed state from `BAPBridge` and
update in lock-step.

```mermaid
flowchart LR
    iOS["📱 iOS<br/>CarPlay nav app"]

    subgraph hook["libcarplay_hook.so"]
        recv["recv() hook<br/>+ RGD parser"]
        bus["TCP bus :19810"]
        recv -->|"EVT_RGD_UPDATE"| bus
    end

    subgraph jar["carplay_hook.jar"]
        bus_cli["bus client"]
        bridge["BAPBridge<br/>(maneuver state machine)"]
        bus_cli --> bridge
    end

    iOS -->|"iAP2 RGD<br/>0x5200..0x5204"| recv
    bus -.->|"TCP :19810"| bus_cli

    bridge -->|"branch A"| hud_path
    bridge -->|"branch B"| render_path

    subgraph hud_path["Branch A — HUD (BAP LSG 50)"]
        direction TB
        hud_icons["Maneuver icons + lanes<br/>FctID 23, 24"]
        hud_text["Text overlays<br/>FctID 19, 21, 22, 46"]
        hud_dist["Distance + bargraph<br/>FctID 18"]
    end

    subgraph render_path["Branch B — Render (MOST video)"]
        direction TB
        spawn["spawn maneuver_render<br/>(slay -f -Q on stop)"]
        rend["maneuver_render<br/>EGL/GLES2 3D scene"]
        surf["displayable 200<br/>EGL surface"]
        enc["cluster video encoder<br/>(H.264)"]
        most["MOST video link"]
        spawn --> rend --> surf --> enc --> most
    end

    hud_icons --> vc
    hud_text --> vc
    hud_dist --> vc
    most --> vc

    vc[("🚗 Virtual Cockpit<br/>HUD + native map area")]

    style hook fill:#fff4e6,stroke:#cc7700
    style jar fill:#e6f3ff,stroke:#0066cc
    style hud_path fill:#fff4e6,stroke:#cc7700
    style render_path fill:#e6ffe6,stroke:#008800
```

| Branch | Cost when idle | Cost per maneuver | When active |
|--------|----------------|-------------------|-------------|
| HUD (BAP) | ~0 | small BAP frames | always while CarPlay nav running |
| Render (MOST) | renderer process not spawned | EGL frame draws + H.264 encode | only while route is set |

The renderer is spawned by `BAPBridge` on the first maneuver and
killed on `0x5203 StopRouteGuidance`, so the MOST-video branch costs
nothing when the user isn't actively navigating.

#### HUD (BAP)

All data sent via BAP protocol (LSG 50) to the VC, which drives the HUD.

```mermaid
flowchart LR
    iOS["📱 iAP2 RGD<br/>0x5201 RouteGuidanceUpdate<br/>0x5202 ManeuverUpdate<br/>0x5204 LaneGuidance"]

    subgraph mapper["ManeuverMapper (Java)"]
        direction TB
        in1["iAP2 turn_type<br/>(54 distinct types)"]
        in2["junction angle"]
        in3["driving_side (L/R)"]
        in4["lane_info[]"]
        in5["distance_meters<br/>+ HU unit setting"]
        out["BAP enums:<br/>ManeuverType, ExitNumber,<br/>SideStreets, LaneMask,<br/>distance + unit (m/ft)"]
        in1 --> out
        in2 --> out
        in3 --> out
        in4 --> out
        in5 --> out
    end

    iOS --> mapper

    subgraph bap["BAP LSG 50 (HUD-driving FctIDs)"]
        direction TB
        f23["FctID 23 — ManeuverDescriptor<br/>icon + side streets +<br/>exit number + multi-list (up to 3)"]
        f24["FctID 24 — LaneGuidance<br/>lane arrows + active highlight"]
        f18["FctID 18 — Distance + bargraph<br/>numeric value + fill +<br/>call-for-action blink"]
    end

    mapper --> f23
    mapper --> f24
    mapper --> f18

    subgraph hud["🚗 VC HUD widgets (drawn by stock cluster firmware)"]
        direction TB
        w_arrow["Big maneuver arrow"]
        w_extras["Side streets, exit number,<br/>upcoming maneuvers stack"]
        w_lanes["Lane indicator strip"]
        w_dist["Distance number + bargraph"]
    end

    f23 --> w_arrow
    f23 --> w_extras
    f24 --> w_lanes
    f18 --> w_dist

    style mapper fill:#e6f3ff,stroke:#0066cc
    style bap fill:#fff4e6,stroke:#cc7700
    style hud fill:#f5f5f5,stroke:#666
```

- [x] Maneuver icons - iAP2 turn type mapped to BAP ManeuverDescriptor (FctID 23). Supports turns, roundabouts, highway exits, merges, U-turns, ferry, etc.
- [x] Multi-maneuver list - up to 3 upcoming maneuvers sent in a single descriptor
- [x] Side streets at intersection - computed from iAP2 junction type + turn angle
- [x] Junction view - roundabout, highway interchange exit numbers
- [x] Left-hand / right-hand driving - auto-detected from iAP2 `driving_side`, affects icon mirroring and side-street layout
- [x] Distance to next maneuver - numeric display when far (FctID 18)
- [x] Distance bargraph - fills up on approach, with call-for-action blink at maneuver point (FctID 18)
- [x] Auto distance units - reads current HU setting (metric/imperial), no manual switch needed
- [x] Lane guidance - lane arrows with active-lane highlighting (FctID 24)

#### VC text overlays

Sent via the same BAP path. VC renders these as text bars over the native map area.

- [x] Turn-to street / exit name (FctID 23, part of ManeuverDescriptor)
- [x] Current road name (FctID 19)
- [x] Distance to destination (FctID 21)
- [x] ETA / remaining travel time - timezone-adjusted to HU local time (FctID 22)
- [x] Destination name (FctID 46)

#### VC MOST video (custom renderer)

Custom 3D renderer (`c_render/`) draws maneuver icons into the cluster's
**MOST video pipeline** (MOST150 isochronous channel — the same path
the HU uses to ship its native map render to the VC's Map tab).
Renders to QNX **displayable 200** via EGL/GLES2; the frames are
captured by the HU video encoder (H.264) and shipped over MOST to the
VC, where the cluster's TVMRCapture pipeline decodes them into a
texture composited by the Kanzi scene. Stock displayable 20 (the
native route-guidance widget that fights for the same screen region)
is periodically re-claimed by re-declaring context 74 — without that
watchdog the stock widget eventually wins back the screen.

> **Note on naming.** Earlier drafts of this README called this branch
> "LVDS video". That was wrong for MU1316 / MHI2Q — LVDS exists on the
> platform (`DISPLAYSTATUS_LVDS_DM_ACTIVE` / `LVDS_HMI_ACTIVE` bits in
> `DSIKombiSync`) but is reserved for full-screen mirror modes
> (startup logo, standby). The HU→VC video for the Map tab on this
> generation rides MOST150, not LVDS.

```mermaid
flowchart LR
    iOS[("📱 iOS<br/>CarPlay nav app")]

    subgraph hookbox["libcarplay_hook.so"]
        direction TB
        recv["recv() hook"]
        rgd["RGD parser<br/>(rgd_hook.c)"]
        idpatch["Identify patcher<br/>+0x001E component<br/>+ msg IDs"]
        bus["TCP bus :19810"]
    end

    subgraph javabox["carplay_hook.jar"]
        direction TB
        bridge["BAPBridge<br/>maneuver state machine"]
        bus_cli["bus client<br/>(EVT_RGD_UPDATE)"]
    end

    subgraph rendbox["maneuver_render (separate process)"]
        direction TB
        atlas["flag_atlas.rgba<br/>14 frames × 128×128"]
        render["EGL/GLES2 3D scene<br/>(road, arrow, flag)"]
        wd["watchdog<br/>re-declare context 74<br/>(reclaims displayable 200)"]
    end

    surf[("displayable 200<br/>EGL surface")]
    enc[/"cluster video encoder<br/>(H.264)"/]
    most([MOST video link])
    vc[("🚗 Virtual Cockpit")]
    stock["[!] displayable 20<br/>stock RG widget<br/>(fights for region)"]:::stock

    iOS -->|"0x5200<br/>StartRouteGuidance"| recv
    iOS -->|"0x5201<br/>RouteGuidanceUpdate"| recv
    iOS -->|"0x5202<br/>ManeuverUpdate"| recv
    iOS -->|"0x5203<br/>StopRouteGuidance"| recv
    iOS -->|"0x5204<br/>LaneGuidance"| recv

    recv --> rgd
    recv --> idpatch
    rgd -->|"parsed state"| bus
    bus -.->|"TCP :19810"| bus_cli
    bus_cli --> bridge

    bridge -->|"on first maneuver:<br/>spawn /mnt/app/root/hooks/<br/>maneuver_render &"| rendbox
    bridge -->|"on stop:<br/>slay -f -Q maneuver_render"| rendbox
    bridge -->|"TCP :19800<br/>CMD_MANEUVER (48-byte fixed)"| render
    bridge -->|"BAP LSG 50<br/>FctID 18, 19, 21..24, 46"| vc

    atlas --> render
    render --> surf
    surf --> enc --> most --> vc
    wd -. "periodic reclaim" .-> surf
    stock -. "competes for screen region" .-> surf

    classDef stock fill:#fee,stroke:#900,stroke-dasharray: 5 5
    style hookbox fill:#fff4e6,stroke:#cc7700
    style javabox fill:#e6f3ff,stroke:#0066cc
    style rendbox fill:#e6ffe6,stroke:#008800
```

- [x] Procedural 3D maneuver icons (turns, roundabouts, U-turns, merges, lane changes, arrival)
- [x] Side streets at junctions from iAP2 junction angles
- [x] Animated route path with curvature-aware speed
- [x] Smooth push transitions between consecutive maneuvers (crossfade + path chaining)
- [x] Camera follow/settle during transitions
- [x] Arrow-to-bulb tip morphing for arrival
- [x] Animated destination flag sprite
- [x] Distance bargraph overlay with blink mode
- [x] Perspective/orthographic view with animated blend
- [x] FXAA + 2x SSAA anti-aliasing
- [x] Painter's algorithm road rendering (white outline + grey fill + blue active route)
- [x] TCP command protocol (port 19800) - Java bridge sends maneuver updates, renderer handles all animation autonomously

#### Bargraph synchronization

Both branches above (HUD `FctID 18` bargraph and the renderer's
on-screen bargraph overlay) need continuous fill + attention-grabbing
blink near the maneuver, but iOS sends `0x5202 ManeuverUpdate` at
sparse intervals (~1–3 s, faster on approach). `BAPBridge` smooths
that gap with a small state machine + a dedicated blink timer thread.

**Phases:**

```mermaid
stateDiagram-v2
    direction LR
    [*] --> Far: route activated
    Far --> Approach: distM <= prepareThreshold
    Approach --> Blink: linBargraph% < 20<br/>(call-for-action zone)
    Blink --> Approach: distM/maneuver changed<br/>(bargraph >= 20% again)
    Approach --> Far: new maneuver pushed<br/>(distance jumps back up)
    Blink --> Far: new maneuver pushed
    Far --> [*]: route stopped (0x5203)
    Approach --> [*]: route stopped
    Blink --> [*]: route stopped
```

**Key parameters** (in `BAPBridge.java`):

| Constant | Value | Meaning |
|----------|-------|---------|
| `CITY_PREPARE_THRESHOLD_M` | 1500 m | enter Approach zone (city maneuver) |
| `HIGHWAY_PREPARE_THRESHOLD_M` | 3000 m | enter Approach zone (highway maneuver) |
| `BARGRAPH_BLINK_PERCENT` | 20% | enter Blink phase when bargraph drops below |
| `ACTION_BLINK_INTERVAL_MS` | 600 ms | blink toggle period (50% duty) |

**Linear fill formula:** `linBargraph% = distM * 100 / prepareThreshold`
clamped to `[0, 100]`. Highway maneuvers use the wider 3000 m
denominator so the bar fills more gradually on a long approach.

**Blink loop** runs on a dedicated `BAPActionBlink` daemon thread
(spawned only while a route is active). Every 600 ms it toggles the
bargraph between 100% and 0% and re-sends `FctID 18` (HUD) plus a
`CMD_MANEUVER` tick to the renderer (overlay). This is **independent**
of iAP2 update cadence — even if iOS goes silent for 2 seconds, both
HUD and renderer still see the blink animation in lock-step.

**FSG-sync workaround.** `AppConnectorNavi.sendStatusIfChanged()`
silently drops updates when nothing in `{FctID 23, 18, 49}` changed.
On every ManeuverDescriptor send we toggle the cosmetic
`exitViewNum` variant on FctID 49 to force a transmission — without
that toggle the cluster occasionally misses bargraph ticks during
fast approach.

### Cover art

Stock MHI2 CarPlay (TerminalMode) forwards track title, artist, and album to the VC, but the stock `AppConnectorTerminalMode` never pushes cover art to the BAP picture manager. The VC always shows a blank/default album icon.

```mermaid
%%{init: {'sequence': {'mirrorActors': false, 'showSequenceNumbers': true}}}%%
sequenceDiagram
    participant iOS as 📱 iOS
    participant recv as recv() hook<br/>(iAP2 thread)
    participant worker as Async worker<br/>(pthread)
    participant fs as coverart.png<br/>(tmpfs)
    participant bus as TCP bus<br/>:19810
    participant java as CoverArt +<br/>EventListener (Java)
    participant bap as AppConnector<br/>TerminalMode
    participant VC as 🚗 VC

    Note over iOS,VC: Track change — incoming cover art

    iOS->>recv: iAP2 transport frames<br/>(chunked JPEG)
    activate recv
    recv->>recv: append to per-fd buffer<br/>scan for FF 5A packets
    recv->>recv: reassemble JPEG (SOI..EOI)
    recv->>worker: enqueue raw JPEG<br/>(1-slot, latest wins)
    deactivate recv
    Note right of recv: hook returns in µs —<br/>iAP2 traffic not stalled

    activate worker
    rect rgba(255, 240, 200, 0.4)
        Note right of worker: ~50–100 ms decode work,<br/>off the iAP2 hot path
        worker->>worker: CRC32 of JPEG
        alt CRC == last_crc (duplicate)
            worker--xworker: skip — already saved
        else new image
            worker->>worker: stb_image decode<br/>+ resize 256×256<br/>+ PNG encode
            worker->>fs: write coverart.png<br/>(atomic rename)
            worker->>bus: EVT_COVERART<br/>crc:<u32> path:<str>
        end
    end
    deactivate worker

    bus->>java: deliver event
    java->>java: dedup by CRC,<br/>track current art-id
    java->>bap: push image<br/>(ResourceLocator)
    bap->>VC: BAP picture mgr -><br/>responseCoverArt()
    VC-->>VC: render album art

    alt Late cover art<br/>(track info already pushed)
        Note over java,VC: VC ignores second update<br/>with same picture id
        java->>java: tweak picture id (id+1)
        java->>bap: re-push now-playing<br/>with modified id
        bap->>VC: forces refresh
    end
```

The C hook intercepts iAP2 transport packets (`read()`/`recv()` hooks),
reassembles JPEG cover art from chunked transfers, hands the complete
JPEG to a **dedicated async worker thread** (single-slot pending queue,
latest-wins coalescing) which decodes and resizes to 256x256 PNG using
stb_image and writes to `/var/app/icab/tmp/37/coverart.png` (tmpfs —
regenerated each session, lost on reboot, which is fine since the
decode pipeline restarts with every CarPlay handshake). The async
hand-off keeps the recv()/read() hook thread free, avoiding ~50–100 ms
of synchronous decode latency per cover art that previously stalled
concurrent iAP2 traffic during handshake. A TCP bus event
(`EVT_COVERART`, port 19810) signals Java. The Java `CoverArt` module
subscribes to the bus, and `TerminalModeBapCombi$EventListener` pushes
the image through `AppConnectorTerminalMode` to the BAP picture manager
with proper `ResourceLocator` and `responseCoverArt()` calls,
mirroring the native `AppConnectorMedia` pattern. Late-arriving cover
art (after track info was already sent) triggers a re-send with a
modified picture ID to force the VC to refresh.

- [x] Cover art forwarding to VC

### Touchpad input for CarPlay

Stock MHI2 forwards the rotary, knob press, back and softkey buttons
to CarPlay natively — those work out of the box. The **MMI touchpad
is the only input device that stock leaves unbridged**: finger
gestures on the touchpad never reach the CarPlay session.

This patch adds the missing leg of the chain:

```mermaid
flowchart TD
    subgraph mmi["🎛️ MMI input devices"]
        direction LR
        pad["🖐️ Touchpad"]
        rotary["Rotary knob<br/>(turn left/right)"]
        knob["Knob press"]
        soft["▭ Softkeys / Back"]
    end

    subgraph dsi_layer["DSI input layer"]
        direction TB
        dsi_touch["updateTouchEvents<br/>(TouchEvent[])"]
        dsi_key["postButtonEvent<br/>(rotary, press, softkeys)"]
    end

    subgraph patch["This patch (touchpad-only)"]
        direction TB
        hook["DSI key events controller<br/>(class-replacement)"]
        ctrl["CursorController"]
        accum["accumulate Δx, Δy"]
        thresh{"|Δx| or |Δy|<br/>> threshold?"}
        emit["emit DPAD tick<br/>press + release"]
    end

    sink["DSI sink<br/>postDpad / postButtonEvent"]
    cp[("📲 CarPlay session")]

    pad --> dsi_touch
    rotary --> dsi_key
    knob --> dsi_key
    soft --> dsi_key

    dsi_touch --> hook
    dsi_key -.->|"stock — passes through"| sink

    hook --> ctrl
    ctrl --> accum
    accum --> thresh
    thresh -->|yes| emit
    thresh -->|no| accum

    emit --> sink
    sink --> cp

    style patch fill:#fff4e6,stroke:#cc7700
    style hook fill:#fff4e6
    style ctrl fill:#fff4e6
    style accum fill:#fff4e6
    style thresh fill:#fff4e6
    style emit fill:#fff4e6
```

The orange box is what this patch adds. Rotary, knob press and softkeys
already reach CarPlay through stock DSI (dotted path) — only the
touchpad needed bridging.

**Touchpad model.** A finger drag accumulates Δx / Δy; whenever either
axis crosses a speed-adaptive threshold, a `KEY_DPAD_*` press+release
pair is emitted and the threshold is subtracted. A long drag emits
multiple ticks so the user can traverse several list items in one
gesture.

(The class is named `CursorController` for legacy reasons — it once
drove an on-screen cursor that was abandoned because MU1316's H.264
encoder ghosted the overlay through motion compensation. See
`docs/` notes if curious.)

- [x] MMI touchpad drag -> DPAD navigation
- Knob rotary, knob press, back / softkey buttons — already work via stock DSI, no patch needed

### Planned

- [ ] Lane guidance on maneuver renderer (CMD_LANE_GUIDANCE protocol, arrow glyphs with status colors + dashed separators, see `docs/plan_lane_guidance_renderer.md`)

### Not possible — AltScreen

CarPlay second screen on the instrument cluster cannot be enabled on
MU1316 — and **no wireless dongle fixes it**. The blocker is on the
HU side (Cinemo iAP2 SDK + libairplay 210.81), not on the phone or
USB transport. Full reverse-engineering write-up + diagrams in
[Why AltScreen is impossible](#why-altscreen-is-impossible-deep-dive)
below.

---

## Architecture

### Patch components

| Component                    | Type                  | Purpose                                                                                          | Output                |
|------------------------------|-----------------------|--------------------------------------------------------------------------------------------------|-----------------------|
| `c_hook/`                    | C (ARM32 QNX)         | iAP2 hooks, route guidance + async cover-art bridge, TCP bus server (port 19810)                 | `libcarplay_hook.so`  |
| `c_render/`                  | C (ARM32 QNX / macOS) | Custom 3D maneuver renderer (EGL/GLES2), TCP command-driven                                      | `maneuver_render`     |
| `java_patch/`                | Java patch JAR        | Route guidance rendering, BAP bridge, cover art forwarder, MMI touchpad / D-pad -> CarPlay input  | `carplay_hook.jar`    |
| `dio_manager.json`           | System config patch   | Enables iAP2 route guidance message exchange with iOS                                            | Manual edit on device |
| `smartphone_integrator.json` | System config patch   | Loads `libcarplay_hook.so` via `LD_PRELOAD` in dio_manager process                               | Manual edit on device |

### Component map

```mermaid
flowchart LR
    iPhone[("📱 iPhone<br/>iAP2 / CarPlay")]

    subgraph mmi["MMI input"]
        direction TB
        touchpad["Touchpad"]
        dpad_btn["Knob / softkeys / back<br/>(stock — passes through)"]
    end

    subgraph cfg["System config (manual edits)"]
        direction TB
        cfg_int["smartphone_integrator.json<br/>(LD_PRELOAD declaration)"]
        cfg_dio["dio_manager.json<br/>(0x5200..0x5204 registration)"]
    end

    subgraph HU["🖥️ Audi MU1316 Head Unit (QNX 6.5, Qualcomm)"]
        direction TB

        subgraph dio["dio_manager process (Cinemo iAP2 SDK)"]
            direction TB
            subgraph hook["libcarplay_hook.so — LD_PRELOAD'd"]
                direction TB
                iap2["iAP2 transport hooks<br/>read() / recv() intercept<br/><i>(iAP2 thread)</i>"]
                idpatch["Identify patcher<br/>+0x001E EAGroupComponent<br/>+ msg-IDs runtime patch"]
                rgd["RGD parser<br/>0x5200..0x5204<br/>+ 0x52xx unknown warn"]
                cover["Cover-art bridge<br/>chunked JPEG reassembly"]
                worker["Async decode worker<br/>stb_image -> 256×256 PNG<br/>(pthread, coalesced queue)"]
                bus[("TCP bus server<br/>127.0.0.1:19810<br/><i>listener + writer + reader threads</i>")]
                cksum["iAP2 cksum<br/>(NEG, hard-coded)<br/>+ sanity log"]
            end
        end

        subgraph hmi["HMI process (lsd.jxe + class-replacements)"]
            direction TB
            subgraph jar["carplay_hook.jar"]
                direction TB
                carplayhook["CarPlayHook<br/>(lifecycle coordinator,<br/>retry thread)"]
                bapbridge["BAPBridge<br/>(maneuver state machine,<br/>BAPActionBlink thread)"]
                cursor["CursorController<br/>(touchpad Δx/Δy -> DPAD,<br/>speed-adaptive threshold)"]
                dsihook["TerminalModeDSIKeyEvents<br/>Controller (class-replaced)"]
                covjava["AppConnectorTerminalMode<br/>(class-replaced —<br/>cover art -> BAP picture mgr)"]
                buscli["TCP bus client<br/>(reader + writer threads)"]
                tmevent["TerminalModeBapCombi$<br/>EventListener (class-replaced)"]
            end
        end

        renderer["maneuver_render<br/>(separate ARM ELF process)<br/>EGL/GLES2 3D scene<br/>+ context-74 watchdog"]

        subgraph filesys["File system / tmpfs"]
            direction TB
            fs_cov[("/var/app/icab/tmp/37/<br/>coverart.png<br/>(tmpfs)")]
            fs_log_hook[("/tmp/carplay_hook.log<br/>(tmpfs, reset on reboot)")]
            fs_log_ren[("/tmp/maneuver_render.log<br/>(tmpfs, reset on reboot)")]
        end
    end

    subgraph VC["🚗 Virtual Cockpit (cluster, NVIDIA Tegra 2 SoC)"]
        direction TB
        most_rx["MOST RX +<br/>TVMRCapture"]
        kanzi["Kanzi scene composite<br/>(stock cluster firmware)"]
        hud_widgets["HUD widgets<br/>maneuver arrow,<br/>distance + bargraph,<br/>lane indicator,<br/>text overlays"]
        map_area["Native map area<br/>(receives MOST video<br/>+ our 3D overlay)"]
        most_rx --> kanzi
        kanzi --> hud_widgets
        kanzi --> map_area
    end

    cfg_int -.->|"declares LD_PRELOAD"| dio
    cfg_dio -.->|"registers RGD msg IDs<br/>in Cinemo SDK"| dio

    iPhone <-->|"USB iAP2 (MOST150)"| iap2
    iap2 --> idpatch
    iap2 --> rgd
    iap2 --> cover
    iap2 -.->|"verify on first stock frame"| cksum
    cover --> worker
    rgd -->|"EVT_RGD_UPDATE"| bus
    worker -->|"EVT_COVERART"| bus
    worker --> fs_cov
    hook -.->|"writes"| fs_log_hook

    bus <-->|"TCP :19810"| buscli
    buscli -->|"deliver events"| tmevent
    tmevent --> bapbridge
    tmevent --> covjava
    bapbridge -->|"TCP :19800<br/>CMD_MANEUVER (48B)"| renderer
    bapbridge -->|"BAP LSG 50<br/>FctID 18, 19, 21..24, 46"| hud_widgets
    covjava -->|"BAP picture mgr<br/>responseCoverArt()"| hud_widgets
    fs_cov -.->|"read PNG"| covjava

    touchpad -->|"DSI updateTouchEvents"| dsihook
    dsihook --> cursor
    cursor -->|"DSI postDpad<br/>KEY_DPAD_*"| iap2
    dpad_btn -.->|"DSI postButtonEvent<br/>(stock path)"| iap2

    renderer -->|"displayable 200<br/>(EGL surface)"| most_encoder
    most_encoder -->|"H.264 over MOST150"| most_rx
    renderer -.->|"writes"| fs_log_ren

    most_encoder([HU video encoder<br/>H.264])

    style hook fill:#fff4e6,stroke:#cc7700
    style jar fill:#e6f3ff,stroke:#0066cc
    style renderer fill:#e6ffe6,stroke:#008800
    style filesys fill:#f5f5f5,stroke:#999,stroke-dasharray: 3 3
    style cfg fill:#fff,stroke:#888,stroke-dasharray: 4 4
    style mmi fill:#fff,stroke:#888
    style VC fill:#f8f8ff,stroke:#666
```

The diagram shows a single CarPlay session at steady state. **Cinemo
SDK** is implicit — every iAP2 byte from iPhone first passes through
our `read()` / `recv()` hooks, then continues into the stock SDK code
underneath. We don't bypass the stock path; we intercept and (for
RGD / Identify / cover art) inject side effects.

### Boot / init sequence

```mermaid
%%{init: {'sequence': {'mirrorActors': false}}}%%
sequenceDiagram
    participant qnx as QNX init
    participant dio as dio_manager
    participant hook as libcarplay_hook.so<br/>(LD_PRELOAD'd)
    participant lsd as lsd.jxe (HMI)
    participant jar as carplay_hook.jar
    participant rend as maneuver_render
    participant ios as 📱 iPhone

    qnx->>dio: spawn (env from<br/>smartphone_integrator.json)
    Note right of dio: LD_PRELOAD picks up<br/>libcarplay_hook.so before<br/>main() runs
    dio->>hook: __attribute__((constructor))<br/>fires
    hook->>hook: spawn worker threads<br/>(async cover-art,<br/>bus listener/writer)
    hook->>hook: open TCP :19810

    qnx->>lsd: spawn HMI app
    lsd->>jar: load class-replacements<br/>(CarPlayHook, BAPBridge,<br/>CursorController, DSI hook,<br/>AppConnectorTerminalMode)
    jar->>jar: TCP bus client connects<br/>to :19810

    Note over qnx,ios: ... idle until iPhone plugged in ...

    ios->>dio: USB enumerate +<br/>iAP2 Identify start
    dio->>hook: read()/recv() with frames
    hook->>hook: Identify patcher injects<br/>+0x001E EAGroupComponent
    hook->>ios: patched Identify forwarded<br/>(via stock SDK)
    ios->>hook: Identify accepted (0x1D02)
    ios->>hook: Auth complete (0xAA05)

    Note over hook,jar: CarPlay session live

    hook->>jar: EVT_RGD_UPDATE / EVT_COVERART
    jar->>rend: spawn on first maneuver<br/>(if not already running)
    rend->>rend: open TCP :19800,<br/>load flag_atlas.rgba,<br/>create EGL context
    jar->>rend: CMD_MANEUVER packets
    jar->>jar: BAP traffic to VC starts
```

### Threading model

| Thread | Process | Role |
|--------|---------|------|
| iAP2 main | dio_manager | runs Cinemo SDK, calls `read()`/`recv()` — our hooks intercept on this thread |
| Cover-art worker | dio_manager | pthread — picks complete JPEGs off the 1-slot queue, decodes, writes PNG, emits `EVT_COVERART` |
| Bus listener | dio_manager | accepts TCP clients on :19810 |
| Bus writer | dio_manager | drains outbound event queue |
| Bus reader (per client) | dio_manager | one per connected Java client |
| HMI EDT | HMI process | UI loop — calls `setMMIDisplayStatus`, picture mgr, etc. |
| `BAPActionBlink` | HMI process | daemon — 600 ms ticks for bargraph blink animation |
| `CarPlayHook-Retry` | HMI process | retries `tryInit()` if OSGi services aren't ready yet |
| Bus reader (Java) | HMI process | reads bus events, dispatches to `TerminalModeBapCombi$EventListener` |
| Bus writer (Java) | HMI process | sends outbound bus messages (currently unused, reserved) |
| Renderer main | maneuver_render | EGL/GLES2 draw loop, accepts `CMD_MANEUVER` on TCP :19800 |
| Watchdog | maneuver_render | re-declares context 74 to reclaim displayable 200 from stock RG widget |

### File system layout

| Path | Type | Lifetime | Purpose |
|------|------|----------|---------|
| `/mnt/app/root/hooks/libcarplay_hook.so` | persistent | survives reboot | C hook binary |
| `/mnt/app/root/hooks/maneuver_render` | persistent | survives reboot | Renderer ARM ELF |
| `/mnt/app/root/hooks/flag_atlas.rgba` | persistent | survives reboot | Renderer flag sprite atlas |
| `/mnt/app/eso/hmi/lsd/jars/carplay_hook.jar` | persistent | survives reboot | Java patch JAR |
| `/mnt/system/etc/eso/production/smartphone_integrator.json` | persistent | survives reboot | declares `LD_PRELOAD` |
| `/mnt/system/etc/eso/production/dio_manager.json` | persistent | survives reboot | registers RGD msg IDs |
| `/var/app/icab/tmp/37/coverart.png` | tmpfs | until reboot | current album art (regenerated each session) |
| `/tmp/carplay_hook.log` | tmpfs | until reboot | C hook + Java event log |
| `/tmp/maneuver_render.log` | tmpfs | until reboot | renderer stderr |

---

## Why AltScreen is impossible (deep dive)

Branch closed 2026-04-23 after full static reverse of iOS 26.1, MHI3 and
MU1316. **AltScreen (CarPlay second screen on the instrument cluster)
cannot be enabled on MU1316 wired CarPlay — and no wireless dongle
fixes this**, because every gate that breaks the negotiation is on the
**head-unit side**, not on the phone or USB transport.

The full chain (see `docs/altscreen_gate_analysis.md`) requires the HU
to:

1. Emit iAP2 Identification ParamID 20/21 with subparam 17
   (themed-assets advertisement)
2. Run `libairplay >= 450.x` with `MainScreenDictCreate` /
   `AltScreenDictCreate` / `ScreenDictSetViewAreas` (MHI3-only
   helpers)
3. Stream type 111 + feature bit 26 (0x04000000) in the AirPlay
   /info dictionary

MU1316 ships:

- **Cinemo pre-2020 iAP2 SDK** — exposes only
  `SUPPORTS_IAP2_CONNECTION` / `SUPPORTS_CARPLAY` flags, no concept
  of Param 21 or the themed-assets subparam. iOS 26 sets
  `isIdentifiedForThemeAssets = 0` -> `_eaAccessoryCapabilities` bit
  21 cleared -> CarKit never adds `altScreen` to the proposed feature
  array.
- **libairplay 210.81** — flat dict from `CopyDisplaysInfo`, no
  AltScreen dict shape, no view-areas helpers. Gates 5 and 6 stay
  closed regardless of what the phone does.

A wireless adapter (Carlinkit / U2W / etc.) emulates a wired
carplay accessory **on the iPhone end of the link** — it speaks
the same iAP2 to the same `dio_manager` on MU1316, hits the same
Cinemo SDK, gets stuck at the same Gate 3. It cannot rewrite the HU's
SDK or libairplay version. **No dongle, present or future, will
unlock AltScreen on this generation of HU.**

### Diagram A — current state on MU1316 (broken)

Both wired iPhone and any wireless CarPlay dongle land on the same
HU stack — and both die at the same two gates.

```mermaid
flowchart LR
    iPhone["📱 iPhone 26.1<br/>CarKit + AirPlayReceiver"]
    dongle["📡 Wireless dongle<br/>Carlinkit / U2W / etc.<br/>(emulates iPhone-side iAP2)"]

    iPhone -->|"USB iAP2"| probe
    dongle -.->|"Wi-Fi / BT, then<br/>same iAP2 protocol"| probe

    probe(["altScreen handshake<br/>iAP2 Identification + AirPlay /info"])

    subgraph hu["🚗 MU1316 head-unit stack (BROKEN)"]
        direction TB
        sdk["iAP2 SDK<br/>Cinemo pre-2020"]:::bad
        g13["<b>Gates 1-3</b> (iAP2 Identification)<br/>[FAIL] no Param 21 / sub17<br/>(SDK only exposes<br/>SUPPORTS_IAP2_CONNECTION,<br/>SUPPORTS_CARPLAY)<br/>===<br/>themed-assets flag NEVER set<br/>caps bit 21 NEVER raised"]:::bad

        air["libairplay<br/>210.81"]:::bad
        g46["<b>Gates 4-6</b> (AirPlay /info)<br/>[FAIL] flat CopyDisplaysInfo dict<br/>NO Main/AltScreenDictCreate<br/>NO ScreenDictSetViewAreas<br/>NO stream type 111<br/>NO feature bit 26"]:::bad

        sdk --> g13
        air --> g46
    end

    end_state[("[X] altScreen NEVER ACTIVE<br/>cluster CarPlay second screen<br/>not exposed to iOS")]:::bad

    probe --> sdk
    probe --> air
    g13 --> end_state
    g46 --> end_state

    classDef bad fill:#fee,stroke:#900,color:#600
    style hu fill:#fff5f5,stroke:#900,stroke-width:2px
```

**Two independent failure points.** Even rewriting the Cinemo SDK to
fix Gates 1-3 leaves Gates 4-6 broken inside libairplay 210.81. A
wireless dongle replaces the iPhone-side iAP2 endpoint — the HU
stack stays exactly the same and hits the same two failures.
**The blocker is on the HU, not on the phone.**

### Diagram B — how it works on a properly equipped HU

For contrast: with iAP2 SDK >= 2020 + libairplay >= 450.x both gates
pass, and altScreen comes up. Wired and wireless behave identically
because both arrive at the same HU stack via the same iAP2 protocol.
This is what e.g. MHI3 looks like.

```mermaid
flowchart LR
    iPhone["📱 iPhone 26.1<br/>CarKit + AirPlayReceiver"]
    dongle["📡 Wireless dongle<br/>Carlinkit / U2W / etc.<br/>(emulates iPhone-side iAP2)"]

    iPhone -->|"USB iAP2"| probe
    dongle -.->|"Wi-Fi / BT, then<br/>same iAP2 protocol"| probe

    probe(["altScreen handshake<br/>iAP2 Identification + AirPlay /info"])

    subgraph hu["🚙 MHI3-grade head-unit stack (WORKS)"]
        direction TB
        sdk["iAP2 SDK<br/>>= 2020"]:::ok
        g13["<b>Gates 1-3</b> (iAP2 Identification)<br/>[OK] emits Param 21 + sub17<br/>(themed-assets advertisement)<br/>===<br/>iOS sets themed-assets flag<br/>caps bit 21 raised<br/>setSupportsThemeAssets:YES"]:::ok

        air["libairplay<br/>>= 450.14.2"]:::ok
        g46["<b>Gates 4-6</b> (AirPlay /info)<br/>[OK] Main + AltScreenDictCreate<br/>[OK] ScreenDictSetViewAreas<br/>[OK] stream type 111<br/>[OK] feature bit 26 (0x04000000)"]:::ok

        sdk --> g13
        air --> g46
    end

    end_state[("[OK] altScreen ACTIVE<br/>CarPlay second screen<br/>renders on cluster display")]:::ok

    probe --> sdk
    probe --> air
    g13 --> end_state
    g46 --> end_state

    classDef ok fill:#efe,stroke:#0a0,color:#040
    style hu fill:#f5fff5,stroke:#0a0,stroke-width:2px
```

### Component versions side-by-side

| Layer | iOS expects | MU1316 (broken) | MHI3-grade (works) |
|-------|-------------|-----------------|--------------------|
| Gates 1-3 (iAP2 Identification) — Cinemo SDK | Param 21 + subparam 17 | pre-2020 SDK, no Param 21 | SDK >= 2020, emits Param 21 |
| Gates 4-6 (AirPlay /info) — libairplay | Alt/Main dict helpers, stream 111, feature bit 26 | 210.81, flat dict only | 450.14.2, full helpers |
| Wireless dongle viable? | n/a | NO — same stack still fails | yes — wireless == wired here |

---

## Build

### Prerequisites

- **QNX SDP 6.5** - needed for cross-compiling C hook and renderer to ARM32 QNX. Easiest way is a QNX VM with the toolchain accessible over SSH.
  QNX VM image: https://archive.org/details/qnxsdp-65.7z (or any other QNX SDP 6.5 VM image available online).
  Set up SSH on the VM and configure the IP in the build scripts.

- **GLFW 3** (macOS only) - needed for local renderer development/testing. Install via `brew install glfw`.

- **lsd.jar** - original HU classes, needed to compile the Java patch against. Extract from your firmware dump:
  1. Get `/mnt/app/eso/hmi/lsd/lsd.jxe` from your firmware
  2. Convert with [jxe2jar](https://github.com/luka-dev/jxe2jar) (`../jxe2jar`)
  3. Output lands in `jxe2jar/out/lsd.jar` (default path used by `build_java.sh`)

### `libcarplay_hook.so` (C hook)

Cross-compiles on the QNX VM via SSH. Set `QNX_VM` IP in `compile_hook.sh`.

```bash
./compile_hook.sh
```

Output: `./libcarplay_hook.so`

Build flags:

| Flag                 | Default | Description                                 |
|----------------------|---------|---------------------------------------------|
| `LOG`                | `1`     | Enable/disable all logging                  |
| `LOG_RGD_PACKET_RAW` | `0`     | Full raw RGD packet dump (requires `LOG=1`) |

```bash
# default (logging on)
./compile_hook.sh

# silent build
LOG=0 ./compile_hook.sh

# verbose packet logging
LOG=1 LOG_RGD_PACKET_RAW=1 ./compile_hook.sh
```

### `maneuver_render` (3D renderer)

**For QNX (deployment):** Cross-compiles on the QNX VM via SSH. Set `QNX_VM` IP in `compile_render_qnx.sh`.

```bash
./compile_render_qnx.sh
```

Output: `./maneuver_render`

To build with the debug grid overlay:

```bash
./compile_render_qnx.sh grid
```

**For macOS (local development):** Builds natively with GLFW + OpenGL 2.1. Requires `brew install glfw`.

```bash
make -C c_render
```

Output: `c_render/c_render` (renderer) + `c_render/test_harness` (test client)

The macOS build also compiles a test harness — see [Testing the renderer locally](#testing-the-renderer-locally) below.

### `carplay_hook.jar` (Java patch)

Requires `lsd.jar` (see [Prerequisites](#prerequisites)). Check paths in `build_java.sh`.

```bash
./build_java.sh
```

Output: `./carplay_hook.jar`

All classes are compiled with `-source 1.2 -target 1.2` for MU1316 JVM compatibility. The bundled JDK from jxe2jar is used automatically.

### Testing the renderer locally

https://github.com/luka-dev/mib2q-carplay-rgi/raw/main/docs/test_manuver_render.mov

The macOS build includes a **test harness** (`c_render/test_harness`) that sends TCP commands to the renderer, letting you cycle through all maneuver types and verify animations without a real device.

Run both in parallel:

```bash
cd c_render
./c_render & ./test_harness
```

The renderer window opens and the harness connects to `127.0.0.1:19800`.

**Harness controls:**

| Key          | Action                                                                               |
|--------------|--------------------------------------------------------------------------------------|
| Left / Right | Cycle through maneuver presets (turns, roundabouts, U-turns, merges, arrivals, etc.) |
| R            | Send a random maneuver with random angle, junction streets, and bargraph             |
| P            | Toggle perspective / orthographic view                                               |
| D            | Toggle debug grid overlay                                                            |
| Space        | Save screenshot (PPM)                                                                |
| S            | Toggle sidescreen / popup viewport mode                                              |
| Q / Esc      | Quit                                                                                 |

The harness auto-cycles through a built-in preset list that covers all icon types with varying directions, exit angles, junction configurations, and bargraph levels. Each arrow key press sends a `CMD_MANEUVER` packet triggering a push transition.

---

## Deploy

### Quick checklist

1. `./compile_hook.sh` — build `libcarplay_hook.so`
2. `./compile_render_qnx.sh` — build `maneuver_render`
3. `./build_java.sh` — build `carplay_hook.jar`
4. `mkdir -p /mnt/app/root/hooks` on device
5. Copy `libcarplay_hook.so` to `/mnt/app/root/hooks/`, `chmod 755`
6. Copy `maneuver_render` to `/mnt/app/root/hooks/`, `chmod 755`
7. Copy `c_render/resources/flag_atlas.rgba` to `/mnt/app/root/hooks/`
8. Copy `carplay_hook.jar` to `/mnt/app/eso/hmi/lsd/jars/`
9. Set `LD_PRELOAD` in `smartphone_integrator.json`
10. Add `0x5200/01/02/03/04` IDs in `dio_manager.json`
11. Reboot (wait 30s after file changes)

### Step-by-step

#### Step 1: Create hooks directory

The hooks directory does not exist by default. Create it on the device:

```bash
mkdir -p /mnt/app/root/hooks
chmod 755 /mnt/app/root/hooks
```

#### Step 2: Deploy `libcarplay_hook.so`

```bash
cp libcarplay_hook.so /mnt/app/root/hooks/
chmod 755 /mnt/app/root/hooks/libcarplay_hook.so
```

Runtime log (resets on each reboot): `/tmp/carplay_hook.log`

#### Step 3: Patch `smartphone_integrator.json`

File: `/mnt/system/etc/eso/production/smartphone_integrator.json`

Add `LD_PRELOAD` under `$.children.carplay.envs`:

```json
"carplay": {
  "exec": "dio_manager",
  "envs": [
    "LD_LIBRARY_PATH=/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib",
    "IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production",
    "LD_PRELOAD=/mnt/app/root/hooks/libcarplay_hook.so"
  ]
}
```

#### Step 4: Patch `dio_manager.json`

File: `/mnt/system/etc/eso/production/dio_manager.json`

Add these iAP2 message IDs to the Cinemo SDK's Identify registration:

`MessagesSentByAccessory` — add:
- `0x5200` = `StartRouteGuidanceUpdates`
- `0x5203` = `StopRouteGuidanceUpdates`

`MessagesReceivedFromDevice` — add:
- `0x5201` = `RouteGuidanceUpdate`
- `0x5202` = `RouteGuidanceManeuverUpdate`
- `0x5204` = `LaneGuidanceInformation`

**Why this AND the C hook patches the Identify message:** Cinemo's
SDK has the message-ID list compiled into its `MessagesSentByAccessory`
/ `MessagesReceivedFromDevice` arrays — the JSON edit registers the
RGD message IDs so the SDK actually pumps them. The C hook
(`rgd_identify_patcher` in `c_hook/routeguidance/rgd_hook.c`)
separately patches the **EAGroupComponent declaration** in the
outgoing Identify message at runtime — adds component `0x001E` so iOS
recognises the accessory as a navigation component and starts sending
RG payloads. Both are required: without the JSON edit iOS sends RG
but the SDK drops it; without the hook patch iOS doesn't even start
sending. The runtime patch shows up in the log as
`Identify patched: 535 -> 621 bytes`.

Example (before):

```json
"MessagesSentByAccessory": [
  "0x5000", "0x5002", "0xAE00", "0xAE02", "0xAE03",
  "0x4154", "0x4156", "0x4157", "0x4159",
  "0xFFFB", "0x4C00", "0x4C02", "0x4C03", "0x4C05"
],
"MessagesReceivedFromDevice": [
  "0x4E09", "0x4E0A", "0x4E0C", "0x5001", "0xAE01",
  "0x4155", "0x4158", "0xFFFA", "0xFFFC", "0x4C01", "0x4C04"
]
```

After:

```json
"MessagesSentByAccessory": [
  "0x5000", "0x5002", "0xAE00", "0xAE02", "0xAE03",
  "0x4154", "0x4156", "0x4157", "0x4159",
  "0xFFFB", "0x4C00", "0x4C02", "0x4C03", "0x4C05",
  "0x5200", "0x5203"
],
"MessagesReceivedFromDevice": [
  "0x4E09", "0x4E0A", "0x4E0C", "0x5001", "0xAE01",
  "0x4155", "0x4158", "0xFFFA", "0xFFFC", "0x4C01", "0x4C04",
  "0x5201", "0x5202", "0x5204"
]
```

#### Step 5: Deploy `carplay_hook.jar`

```bash
cp carplay_hook.jar /mnt/app/eso/hmi/lsd/jars/
```

#### Step 6: Deploy `maneuver_render` and flag atlas

```bash
cp maneuver_render /mnt/app/root/hooks/
cp c_render/resources/flag_atlas.rgba /mnt/app/root/hooks/
chmod 755 /mnt/app/root/hooks/maneuver_render
```

The flag atlas (`flag_atlas.rgba`, source path `c_render/resources/flag_atlas.rgba`) must end up reachable from the renderer's working directory. The renderer searches in this order: `resources/flag_atlas.rgba`, `flag_atlas.rgba`, `<binary_dir>/resources/flag_atlas.rgba`, `<binary_dir>/flag_atlas.rgba`. Placing it next to the binary (as above) hits the second/fourth path.

Launched automatically by the Java bridge (`BAPBridge`) when CarPlay route guidance starts. Log: `/tmp/maneuver_render.log`.

#### Step 7: Reboot

Reboot the infotainment system. Give it 30+ seconds after file changes before rebooting — otherwise changes may not be flushed to disk.

---

## Contributing

**Maneuver icon testing.** The iAP2-to-BAP maneuver mapping covers all 54 CarPlay maneuver types, but has only been tested on a limited set of real-world routes. Edge cases like complex interchanges and multi-lane roundabouts need more road testing. If you see a wrong or missing icon on the HUD, a log from `/tmp/carplay_hook.log` of the exact moment + explanation of what's wrong would help. Note: the log resets on each device reboot, so grab it before restarting.

The hook automatically logs unrecognised iAP2 RGD-family messages as
`[HOOK] Unknown 0x52xx msgid=0xNNNN dir=IN len=N` followed by a hex
dump — these are the most useful starting point if iOS sends a
maneuver type we don't handle yet.

---

## References

Thanks for the previous work and knowledge that helped to figure this out.

- https://github.com/ludwig-v/wireless-carplay-dongle-reverse-engineering
- https://github.com/EthanArbuckle/iPhone18-3_26.1_23B85_Restore
- https://github.com/adi961/mib2-android-auto-vc
- [@fifthBro](https://t.me/fifthBro)
