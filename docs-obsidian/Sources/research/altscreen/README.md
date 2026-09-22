# altscreen/ — CarPlay 2nd-screen (instrument cluster) on MHI2Q

Self-contained knowledge + resources for driving a CarPlay AltScreen onto the
MU1316 cluster by reusing libairplay's screen primitives + `maneuver_render`.

**Start here:** [`MASTER_KNOWLEDGE.md`](MASTER_KNOWLEDGE.md) — A→Z (status, architecture, plan).

| File | What |
|---|---|
| [`MASTER_KNOWLEDGE.md`](MASTER_KNOWLEDGE.md) | Consolidated knowledge base — read first |
| [`feasibility.md`](feasibility.md) | Primitive inventory, iOS contract, phases, Cosmin recipe → MHI2Q port |
| [`impl_plan.md`](impl_plan.md) | Module design, interposer table, RE-resolved interposer bodies |
| [`re_findings.md`](re_findings.md) | Raw IDA reference — addresses, signatures, struct offsets, CFDict keys |
| [`gate_analysis.md`](gate_analysis.md) | Original 6-gate "Apple-intended wireless altScreen is impossible" writeup |
| [`reference/`](reference/) | MHI3 working hook + MHI3 master knowledge (Cosmin's proven recipe) |

**Code lives elsewhere (build needs it in place):**
- Hook module: `../c_hook/altscreen/` (`altscreen_hook.{c,h}`, `altscreen_av.{c,h}`)
- Cluster renderer: `../maneuver_render/` (separate process)
- Build: `../compile_hook.sh` (module already in `ALTSCREEN_SRCS`)

Default build = passive recon (`ALTSCREEN_INJECT=0`, safe). Injection is gated
until on-device validation.
</content>
