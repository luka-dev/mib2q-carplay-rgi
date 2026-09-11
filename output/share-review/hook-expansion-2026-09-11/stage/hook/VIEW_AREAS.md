# Visible frames, safe rectangles and view changes

The selected local view-area builder models two rectangles: a visible frame and
a nested safe rectangle. `view_area.c` preserves that representation and its
field names using portable data and diagnostic JSON. Validation and formatting
are new adaptations; platform dictionary allocation and sending are omitted.

The visible frame describes the area associated with the view. The safe rectangle
describes where important content should fit within it. It is geometry for the
consumer to honor. This helper does not crop pixels, change decoder buffers,
choose a display layer or make an application obey the rectangle by itself.

## Coordinate contract

Both rectangles use the same pixel coordinates. The origin is at the top left;
x increases rightward and y downward. A rectangle spans `[x, x+width)` and
`[y, y+height)`. The nested safe origin is not interpreted relative to the
visible origin. Positive dimensions and containment are required by this helper.
Intermediate arithmetic uses a wider integer type to avoid 32-bit edge overflow.

The following are deliberately synthetic dimensions:

| View | Visible rectangle | Safe rectangle |
|---|---|---|
| Wide | `(0, 0, 800, 480)` | `(40, 24, 720, 432)` |
| Compact | `(0, 0, 800, 480)` | `(200, 24, 400, 432)` |

The visible dimensions stay equal here while the safe region changes. This
illustrates a layout change within one fixed canvas. It is not a declaration
that every receiver supports that operation or that arbitrary dimensions will
be accepted. A rectangular safe region also cannot describe a nonrectangular
physical mask exactly; choose geometry that fits the intended layout.

`view_area_from_insets` constructs an origin-zero visible frame and subtracts
left/top/right/bottom insets to obtain the safe rectangle. Invalid inputs leave
the caller's existing output untouched. `view_area_write_json` exposes the same
rectangle field names as the selected local builder:

```json
{"originXPixels":0,"originYPixels":0,"widthPixels":800,"heightPixels":480,
 "safeArea":{"originXPixels":200,"originYPixels":24,"widthPixels":400,"heightPixels":432}}
```

This is diagnostic JSON, not a complete declaration or a wire message. No view
catalog registration, device-specific dimensions, identifiers or sender are
included. The existing renderer consumes frame records; it does not parse this
JSON. Adapting geometry to a real output or control path remains separate work.

## Requested and confirmed state

`view_switch.c` is a new educational model inspired by the local generation and
completion checks. It is not the production command implementation. Give it a
catalog size and initial desired index, then drive it from one owner thread:

1. A selection updates `desired`, even before a session is ready.
2. Once the caller's session is ready, `begin` starts a fresh generation.
3. `next` returns at most one pending request, tagged with generation, serial and
   area index. The pending state exists before the caller attempts its send.
4. A matching completion updates `applied` on success. Failure or uncertainty
   sets it to UNKNOWN. Unmatched, duplicate and old-session tokens do nothing.
5. Changes made while a request is pending coalesce into the latest desired
   index. Once that request finishes, `next` can produce the remaining change.
6. `end` invalidates pending work; the desired selection survives reconnects.

For example, selecting compact while wide is pending does not pretend compact
has already been applied. A successful wide completion first records wide as
confirmed; the next request targets compact. A completion from a previous
session cannot overwrite either value.

The caller owns transport behavior, callback delivery, timeouts and retry limits.
Immediate submission failure must complete that token as failed if no later
callback is promised. Do not retry in a tight loop. A timeout means the remote
state may be unknown: rejecting a late callback locally cannot undo a command
that a remote peer already applied. Your transport/adapter must establish command
ordering and recovery; this model does not solve remote reordering. A control
acknowledgement also does not prove that a new frame has reached the display.

All helper calls belong on one owner thread. Pass tokens by value from other
threads; do not reinitialize the state object while callbacks from its old
lifetime can still arrive. Counter exhaustion reports an error rather than
reusing identifiers. The host demo simulates completions locally.
