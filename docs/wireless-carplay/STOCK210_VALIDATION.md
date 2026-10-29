# Stock210 validation profiles and evidence

Use the [shared acceptance ledger](../carplay-lab/WIRELESS_COMPLETION_PLAN.md) for current implementation status and remaining gates. A passing pairing fixture accepts its control protocol, not a complete vehicle session.

| Profile | Guide | What it establishes |
| --- | --- | --- |
| Pairing and encrypted control | [Pairing](STOCK210_HTTP_PAIRING_ABI.md) | Authenticated identity, records and stock HTTP dispatch |
| SI-owned lifecycle | [SI implementation](SI_STAGE2_OFFLINE.md) | Generation-bound native admission, child and cleanup paths |
| Native vphone media | [Native wireless QNX](../carplay-lab/NATIVE_WIRELESS_QNX.md) | Real sender through explicit VM association/peripheral models |
| Audio and HMI | [Media validation](../carplay-lab/WIRELESS_MEDIA_VALIDATION.md) | Separate codec/PCM, concurrency and real J9 startup profiles |
| Semantic channels | [Production channels](WIRELESS_PRODUCTION_CHANNELS.md) | Paired AltScreen, metadata and input transport |

Build the matched candidate with `python3 wireless/si/build_si_review.py --full-cycle`. Retain the manifest and each receipt's models, skips and input hashes; do not reuse an old PASS as current-kit acceptance.

The [2026-09-29 validation record](../archive/internal-reference-2026-10-29/wireless-carplay/STOCK210_VALIDATION.md) retains independent SRP/PairVerify, crypto/framing tests, exact artifact hashes and earlier media/lifecycle results. Its remaining-work lists describe that milestone.
