# HU regression: decoded-frame age filter

Verified on MU1316 / APQ8064 on 2026-09-14, after deploying the `6d057db`
production candidate to `10.173.189.1`.

The 200 ms output-age cutoff in `alt_present_obsolete()` starved a healthy
decoder. The worker intake was only 0–5 ms old and EGL swap took about 0.3 ms,
but Qualcomm output retained approximately four pictures of input latency.
At 20 fps, the decoded pictures were already about 200–220 ms old; at lower
input rates they reached 275–283 ms. Newer *compressed* input did not mean a
newer *decoded* picture was available for presentation.

Before the fix, `outputSkipped` rose from 2700 to 3221 over 600 decoded
pictures, and later from 5920 to 6520 over another 600. GPU presentation
intervals reached 15.3 seconds while decode intake remained current.

The fix removes this cutoff. Every nonempty decoded output goes through the
existing direct presenter before its OMX buffer is recycled. Compressed-input
queue limits, continuity/reconnect handling, GPU buffer ownership, readiness,
loader pacing and decoder teardown are unchanged. Timestamps remain diagnostic.

Validation:

- Actual sink regression replays four-picture delay at 20/25/30 fps: each
  sequence presents all 600 pictures, including final/static and old first frames.
- Sink and timing tests pass under ASan/UBSan; QNX hostcheck and ARM build pass.
- Replaced only `altscreen_render`, verified its bytes, sent TERM to the exact
  old renderer PID and let the existing supervisor restart it.
- Live PID 18681993: 600 decoded / 600 GPU posts; output interval 50.62 ms,
  maximum 114.81 ms, with input interval 52 ms.
- Later: 3600 decoded / 3600 GPU posts; output interval 62.11 ms, maximum
  172.22 ms, with input interval 61 ms. No sustained frame starvation remained
  in these measurements. Visual feedback and broader endurance remain separate.

Hotfix binary SHA-256:
`76edae9be98109f4a38bd5eac71afd1e33d26af5dcb03dcacd32d341d45624dc`.
Evidence and build logs: `build/hotfix-alt-pacing-20260914/` and
`build/deployments/carplay-production-6d057db-20260914/`.
The complete corrected bundle is
`build/releases/carplay-production-6d057db-20260914-alt1.zip`.
The original bundle is superseded: do not redeploy its altScreen binary.
