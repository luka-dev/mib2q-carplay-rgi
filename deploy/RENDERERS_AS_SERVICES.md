# Renderers as always-on framework services — change + deploy instructions

Move `maneuver_render` (id 98) and `altscreen_render` (id 99) off Java-controlled
spawn/kill/respawn and onto **always-on ESO-framework services** (`servicemgr:{external:true}`).
Result: framework launches + PID-monitors + restarts them; they survive dio's SIGKILL;
Java sends only data (`CMD_MANEUVER`) + cluster context + `setVideoAvailable`.

Two parts: **#2 Java (BAPBridge)** and **#3 firmware deploy (framework.json)**.

---

## #3 — Deploy the service entries (do this + QEMU-test FIRST)

### Service entries (the two objects to add to the `"applications"` array)

```jsonc
    {
        "name": "maneuver_render",  "exec": "/mnt/app/root/hooks/maneuver_render",  "node": "mmx", "id": 900,
        "servicemgr": { "external": true }
    },
    {
        "name": "altscreen_render", "exec": "/mnt/app/root/hooks/altscreen_render", "node": "mmx", "id": 901,
        "servicemgr": { "external": true }
    }
```

`external:true` = launch + PID-monitor + restart-on-exit, **no comm registration** (proven in
servicemgrmibhigh `sub_12EB78`/`sub_163A9C`: the external byte@+52 routes the process down the
exec-only path — no `comm::Proxy`, no `waitUntilAlive`, so no alive-timeout / reset).
Rules: NO `singleapp`, NO `apps` array (those make a *registering* app → alive-wait → the
`spawnfactory.reset_on_timeout:true` reset risk). `transport` NOT required for external — omit it.
ids 900/901 are free (max stock id = 516).

A ready, validated production file with these entries already inserted (after the `awp` entry)
is at **`deploy/framework.json`** — deploy that, or hand-insert the block above.

### Deploy to the unit
1. File on the unit: **`/etc/eso/production/framework.json`**
   (in the SWDL image: `.../efs-sys/10/default/system/etc/eso/production/framework.json`).
   Use `deploy/framework.json`, or insert the two objects into the `"applications"` array
   (e.g. right after the `asiaWordPredictionProc`/awp entry). The parser allows `#`/`//`
   comments + trailing commas, but keep element commas correct.
2. `servicemgr.json` — confirm `"restart_enabled": true` (stock: true; broker depends on it).
3. Ensure the binaries exist at the `exec` paths on the unit:
   `/mnt/app/root/hooks/maneuver_render`, `/mnt/app/root/hooks/altscreen_render`
   (same location Java used before). `/mnt/app` is mounted before app services (awp proves it).
4. Repack the `efs-sys` image + SWDL, or overlay the modified `framework.json` by your existing
   image-patch flow.
5. Verify on boot: both renderers get a PID; `slay maneuver_render` → servicemgr re-execs it
   (restart-on-exit); no `[externalProcessFailed]: restart is disabled`, no system reset.

### Notes
- **No `transport` needed** for `external:true` (SM never proxies it). The inert transport block
  in the jsonc is only for schema-strict loaders — you can delete it.
- **Do NOT** add `singleapp` or an `apps` array (those make a *registering* app → alive-wait → reset risk).
- `id` 900/901 are free (max stock id = 516). If you add more, keep ids unique.

---

## #2 — BAPBridge: drop process spawn/kill/respawn (Java = data only)

File (both trees): `com/luka/carplay/rgd/BAPBridge.java` in
`java_patch_new_tab/` and `java_patch_new_replace/`.

The renderer is now always-on (framework service). Java must NOT launch, kill, or respawn it.
Keep: the TCP link (to send `CMD_MANEUVER`), the cluster-context activation, and the BAP force flags.

### Remove
- Constants: `CR_LAUNCH_CMD`, `CR_KILL_CMD`, `CR_RESPAWN_FAIL_THRESHOLD`, `CR_RESPAWN_COOLDOWN_MS`,
  `CR_READY_TIMEOUT_MS`, `CR_FRAME_READY_TIMEOUT_MS`, and the `crLastRespawnMs` /
  `crConsecutiveSendFailures` respawn state.
- All `CommandLineExecuter.executeCommand("/bin/sh", {"-c", CR_LAUNCH_CMD|CR_KILL_CMD})` calls
  (the launch + every `slay`).
- Methods: `handleRendererDied(...)`, `noteRendererSendResult(...)` respawn-escalation logic,
  and the `RendererServer.DeathListener` respawn wiring. (A send that fails now just logs;
  it does not respawn — the framework owns the process.)
- The startup gating `waitForReady(CR_READY_TIMEOUT_MS)` (process-boot wait). Optionally KEEP a
  short `waitForFrameReady(...)` purely as a "is the renderer connected + has a frame yet?" gate
  before switching the cluster to our context (avoids a blank first frame); it must no longer
  kill/respawn on timeout — just skip the context switch and let the state machine retry.

### Keep / change
- `RendererServer` (our TCP server, port 19800): **make it ALWAYS-ON.** Create + `connect()` (bind
  + accept thread) once from `CarPlayApp.startTransport()` at boot (same place the bus is brought
  up always-on), NOT per-session in `startCustomRenderer`. The always-on renderer connects to it
  and stays; add reconnect tolerance on the renderer (C) side (retry connect to 19800; idle-render
  blank until `CMD_MANEUVER`).
- `startCustomRenderer()` → shrink to **activate only**: `csRef.activateCustomRendererPipeline()`
  (which calls `AltScreenModule.setRgiActive(true)`) + `forceClusterRouteInfoState(true)` +
  `forceGfxAvailable(true)`. No spawn, no slay, no waitForReady.
- `stopCustomRenderer()` → **deactivate only**: `csRef.deactivateCustomRendererPipeline()`
  (`setRgiActive(false)`) + `forceGfxAvailable(false)` + `forceClusterRouteInfoState(false)`.
  No `slay`, no `rendererClient.dispose()` (the renderer stays up; only stop sending data).
- `updateRendererIfChanged(...)` / `sendBargraph(...)` etc. — unchanged (still send `CMD_MANEUVER`
  over the always-on TCP). `customRendererStarted` becomes "pipeline activated", not "process spawned".

### altscreen_render (id 99) — same model
- No Java launcher (there never was one). It is an always-on service too.
- `AltScreenModule.setVideoAvailable(true/false)` still needs a trigger for when the 111 video
  stream is actually producing frames (libairplay → shm → altscreen_render). Options: the renderer
  signals "have frames" over its TCP link → BAPBridge/AltScreen calls `setVideoAvailable(true)`;
  or a libairplay-side signal. **Still open — separate from this change.**

### C-side (renderers) — required for always-on
- **Reconnect loop**: the renderer starts at boot (before lsd's TCP server may exist) → it must
  retry `connect()` to 127.0.0.1:19800 until the server is up, and reconnect if the link drops
  (lsd restart). It must NOT exit on connect failure (framework would just re-exec it, but a
  reconnect loop is cleaner and avoids restart-rate-limit churn).
- **Idle render**: `maneuver_render` draws nothing/last-frame until `CMD_MANEUVER`; `altscreen_render`
  acquires its OMX H.264 decoder LAZILY only when 111 frames arrive (don't hold the HW decoder idle).

### Build
- After editing, rebuild both jars:
  `./build_variant.sh java_patch_new_tab carplay_hook_tab`
  `./build_variant.sh java_patch_new_replace carplay_hook_replace`
- Keep the two trees in sync (this change is identical in both — BAPBridge is a shared-base file).

---

## Order of operations
1. Apply #2 (BAPBridge) + the C reconnect/idle changes; rebuild both jars.
2. Deploy `deploy/framework.json` on the unit (#3) + the jars.
3. Verify: renderers up at boot, survive dio SIGKILL, restart on manual `slay`,
   KDK popup still works, no `restart is disabled` / no reset.
