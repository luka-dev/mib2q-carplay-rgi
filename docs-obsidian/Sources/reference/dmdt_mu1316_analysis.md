# MU1316 dmdt analysis

Binary:
`MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/bin/apps/dmdt`

IDA session: `mu1316_dmdt`

## What dmdt is

`dmdt` is a thin command-line client for the Display Manager debug IPC service.
It does not inspect Display Manager state locally. It starts a comm agent named
`DMRCClient` on domain `local`, connects a proxy to:

- `asi.displaymanager.DebugTool`
- `asi.displaymanager.DebugToolReply`

The client sends one request, waits for the async reply callback, prints the
reply, sleeps for 50 ms, then exits.

Main evidence:

- `0x106050`: main
- `0x103278`: DebugTool/DebugToolReply proxy construction
- `0x103C84`: reset reply-wait flag
- `0x103C04`: wait until reply callback signals condition

## Commands

`0x105B98` parses argv into a 32-byte command struct:

| CLI | id | Action |
| --- | -- | ------ |
| `gd` | 1 | list displayables |
| `dc <context_id> <displayable...>` | 2 | declare context |
| `sc <display_id> <context_id>` | 3 | switch context |
| `dm <display_id> on` | 4 | debug mode on |
| `dm <display_id> off` | 5 | debug mode off |
| `ts <display_id> <path>` | 6 | screenshot |
| `sb <display_id>` | 7 | switch buffered context |
| `gc` | 8 | list contexts |
| `gs` | 9 | system info |
| `gld on` | 10 | global debug/label mode on |
| `gld off` | 11 | global debug/label mode off |

Dispatch is at `0x105F48`.

Important request wrappers:

- `0x103D3C`: `gs`, sends request id `123`, then waits for reply.
- `0x103D00`: `gd`, sends request id `123`, then waits for reply.
- `0x103CC4`: `gc`, sends request id `123`, then waits for reply.
- `0x102DA4`: `sc`, calls virtual method with `(display_id, context_id, 123)`.
- `0x102E88`: `dc`, creates an Int32Array for displayables and sends it with context id.

## `dmdt gs` output format

The `gs` reply printer starts at `0x104914`.

Exact string sequence:

```text
displaymanager reports the following system information:
	number of displayables: %d
	number of displays: %d

	display: %s
	terminal: %s
	size: %d x %d
	context id: %d
		%d (%s)
```

The per-display loop prints:

1. display name from a `UnicodeString8`
2. terminal name from a `UnicodeString8`
3. width and height
4. current context id
5. each displayable in the active context, mapped through `0x102980`

MU1316 sample from `logs/dmtd_hook.log`:

```text
	display: Main Display
	terminal: LVDS1
	size: 1024 x 480
	context id: 0
		16 (DISPLAYABLE_HMI)

	display: Cluster Display
	terminal: LVDS2
	size: 1440 x 542
	context id: 74
		20 (DISPLAYABLE_MAP_ROUTE_GUIDANCE)
		102 (--)
		101 (--)
		33 (DISPLAYABLE_KOMBI_MAP_VIEW)
```

There is no numeric display id in `gs` output. `sc` uses display ids, but `gs`
only exposes display order, display name, and terminal name.

## Parser comparison

Renderer code: `maneuver_render/platform_qnx.c`

Old parser matched:

- `display:` line
- target if name contained `Cluster` or display block index matched `CR_DISPLAY_ID`
- `context id:` via `atoi`

That is compatible with the normal MU1316 sample. If the renderer logged
`dmdt focus unknown`, the likely cause was not this normal `gs` format. More
likely `popen("/eso/bin/apps/dmdt gs 2>/dev/null")` returned empty/failed
output, and stderr was hidden.

Current parser improvements:

- still accepts `Cluster Display`
- still accepts display block index `1`
- additionally accepts `terminal: LVDS2` for cluster and `terminal: LVDS1` for main
- merges stderr into the stream and logs status/line counts/sample on parse failure
- forces `sc <display_id> <context_id>` on parse failure instead of silently leaving focus unknown

Local replay of the MU1316 `gs` sample returns:

- `CR_DISPLAY_ID=1` -> context `74`
- `CR_DISPLAY_ID=0` -> context `0`

