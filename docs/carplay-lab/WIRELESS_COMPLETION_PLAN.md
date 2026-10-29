# Wireless implementation and acceptance ledger

_Current working-tree contracts, reviewed 2026-10-29. Test results below retain their original dates and profile boundaries._

[Laboratory overview](README.md) · [Wireless overview](../wireless-carplay/README.md) · [SI implementation](../wireless-carplay/SI_STAGE2_OFFLINE.md)

This is the shared status reference for lab and wireless guides. A passing codec test, native SI startup and a vehicle session establish different things. Implementation status comes from source; a test result applies only to its recorded artifacts and substituted peripherals. This documentation review did not run a new vehicle or full-system acceptance test.

## Test profiles

| Profile | Real components exercised | Explicit boundary |
| --- | --- | --- |
| Host unit/contract tests | Compiled parser, state, queue, codec and policy code | Synthetic inputs and substituted platform services |
| Native ARM/QNX fixtures | Stock SI/DIO/210 receiver and candidate native modules, depending on fixture | Synthetic phone, coding/FEC or peripheral providers must be listed in the receipt |
| Native vphone/QNX media | iOS sender, real bootstrap/receiver paths, captured video and codec PCM | VM Ethernet association, software-MFi fixtures and modeled QSA/OMX/HMI boundaries; inspect the selected profile |
| Real SI and J9 startup | Native SI, real Java HMI, controller queue and NOOP dispatch | Startup does not establish wireless media admission or a Java audio-focus grant |
| Physical HU | Actual radio, OEM authentication hardware, amplifier, microphone and display | Evidence applies to the installed/read-back kit; an earlier build does not accept the current candidate |

Synthetic HMI acknowledgements must be named as models. Receive-only MainHigh control is a codec-isolation model. Software-MFi success is not proof of authentication with an unmodified physical iPhone.

## Implemented baseline

- Existing SI owns the CarPlay child and admission. Rust owns bootstrap/recovery; the standalone DIO supervisor is an earlier integration path.
- SIW2 requests and CPRX operations bind work to SI/daemon/link/session generations. Native terminal state and child exit govern cleanup and re-admission.
- Paired stock210 control, main/AltScreen media and semantic channels have dedicated native fixtures. AAC receive and Opus transmit have independent execution paths; buffered APAC follows the existing type-103 path.
- Same-link recovery can restart media within a retained authenticated RFCOMM generation. It does not implement reopening a lost RFCOMM connection.
- Native identities remain separate for USB and WLAN. Java adds `Wired -` / `Wireless -` presentation and the initial remember-checkbox default; native handlers own Yes/No and persistence.

Sources: [SI ownership](../wireless-carplay/DIO_OWNERSHIP.md), [media contracts](WIRELESS_MEDIA_VALIDATION.md), [recovery.rs](../../wireless/daemon/src/recovery.rs), [TransportDevicePolicy.java](../../java_patch/com/luka/carplay/core/TransportDevicePolicy.java).

## Remaining gates

### MainHigh control

AAC data/codec support exists, but the production MainHigh 102 control/clock/retransmission contract is not complete. The [lab peripheral](../../tests/carplay_lab/qnx/vphone_audio_peripheral.c) opens a real UDP socket and receives control packets; it does not implement clock or retransmission behavior. A supplied `controlPort` and passing PCM capture do not close this gate.

Complete the protocol contract from native packets and stock/iOS evidence, including authentication, sequence/timestamps, wrap, loss, duplicates, clock mapping and teardown. Then integrate a generation-bound endpoint in [media.c](../../hook/wireless/media.c) / [audio_stream.c](../../hook/wireless/audio_stream.c), with bounded queues/retry windows and no shared long RX/TX lock. Accept it with native vphone playback and loss/reorder/drift profiles without the receive-only substitute. Session timing and buffered 103 control remain distinct protocols.

### Connection policy and lifecycle

| Area | What remains |
| --- | --- |
| Transport settings | Implement and enforce independent USB, Wireless and Auto preferences through native SI admission; prefixes and remember-checkbox defaults do not enforce these settings |
| Identity | Establish any shared USB/WLAN phone identity from verified native identifiers; names and assumed MAC equivalence are insufficient |
| Boot/bootstrap | Accept startup with an already connected BT peer and recovery of missing RFCOMM; same-link media recovery does not cover either |
| User refusal | Define/persist No with or without remember, trip suppression and manual disconnect/retry; rediscovery must not reset a refusal or cause repeated popups |
| Two phones | Preferred/last-used selection and explicit serial handover, with one CarPlay owner and no background takeover during a call |
| Late native events | Validate late ASI appState handling or establish a native sender/barrier contract; tagged IPC alone cannot identify an untagged native callback |
| SI restart/orphans | An existing kernel DIO owner blocks admission; automatic wireless orphan recovery is not implemented. Preserve guarded wired takeover and explicit recovery |
| Native providers | Validate authentic coding/adaptation and FEC exchange separately from positive fixtures that supply an RCC grant |

The intended user policy keeps two transport entries. USB/Wireless permission and automatic activation are separate preferences; native consent remains required. Settings changed during a session apply to the next connection; explicit Disconnect owns immediate teardown. A disallowed USB method must not preempt wireless just because a charging cable appears. Persisted refusal and temporary suppression must survive process restarts within the same trip, or remain until manual action if no reliable trip boundary exists.

Accept these rules through real menu/controller paths, including both No variants, opt-out, accidental refusal followed by manual retry, duplicate events, stale generations and two phones. The [earlier detailed specification](../archive/internal-reference-2026-10-29/carplay-lab/WIRELESS_COMPLETION_PLAN.md) retains the full scenario rationale.

### Native HMI and audio focus

Native SI and real J9/HMI startup are exercised, but one integrated wireless admission → SI-owned DIO → real Java resource grant/ACK session is still required. A fixture ACK cannot stand in for the stock resource chain. Check grant, refusal, timeout, release, stale ACK, mute/volume and interruption behavior.

A2DP/HFP handover also needs acceptance: keep bootstrap connectivity, coordinate media focus through the native stack and avoid background switching during a call. Siri/call preemption must follow the [existing audio routing contract](../audio/CALL_AND_SIRI_ROUTING.md). Physical microphone/speaker and cellular-call behavior require the HU.

### Combined laboratory session

Individual video, AAC/APAC PCM, mic and lifecycle tests do not establish their simultaneous operation with real HMI. Planned acceptance scope:

| Scenario | Required evidence; these are targets, not completed results |
| --- | --- |
| AAC + moving 110/111 + mic | 30 minutes, independent PCM comparison, Opus uplink, bounded queues, latency and drift |
| Buffered APAC 5.1 | 15 minutes, six-channel routing, pause/seek, codec transitions and UpMix changes |
| Combined load | 60 minutes with map motion, rapid zoom, music, mic and native HMI |
| Network faults | Loss/reorder/delay/duplicate/outage with protocol exchanges and explicit recovery budgets |
| Same-BT Wi-Fi recovery | 20 cycles, native cleanup, no extra consent when already permitted |
| Lifecycle and policy | Boot, SI/daemon/DIO restarts, two phones, USB priority, all permission combinations and refusals |
| Semantics | Now Playing, media keys, cover art, route/lanes and location together with media |

Record source/received/decoded/presented cadence separately, queue age, PCM gaps, clock offsets and generation cleanup. A bounded short PCM tap does not prove a long dropout-free run. Use corrected clock mapping; do not count repeated pictures as unique 30 fps output. Set numerical fault/recovery budgets before acceptance, and retain the actual manifest and models with every result.

### Physical HU

Earlier main-screen/input/AltScreen observations are build-specific; the latest recorded HU state is restored wired operation. Accept a fresh matched kit with read-back hashes and the running SI image/generation established after restart. Check wired connect/disconnect/reconnect, wireless from an existing BT pair, OEM authentication, actual 2.4 GHz radio, amplifier audio, microphone/Siri/calls, A2DP/HFP handover, rapid zoom, boot, two phones, refusals and Wi-Fi recovery. Emulator success cannot establish MOST/acoustics or physical OMX presentation.

## Evidence index

These are dated records, not a claim that the current working tree passed again. Generated results are local/ignored and may be absent from a fresh checkout.

| Recorded profile | Evidence location | Scope |
| --- | --- | --- |
| Receiver audio, 2026-10-23 | `build/wireless-validation-20261023/receiver-audio/result.json` | Six input hashes matched at review; codec/data-path PASS |
| Stock stream concurrency, 2026-10-23 | Media guide's stock-stream receipt | Five hashes matched; AAC RX continued while Opus TX was blocked |
| Native AAC/APAC, 2026-10-22 run 8 | [Audio/PCM guide](WIRELESS_MEDIA_VALIDATION.md) | PCM passed with named models; production/physical/RTCP flags remain false |
| Native SI + J9, run 8 | [HMI profile](WIRELESS_MEDIA_VALIDATION.md#real-hmi-profile) | Startup and SI NOOP passed; no media-focus acceptance |
| Transport stress | [Media stress](MEDIA_STRESS_TEST.md) | Five hashes matched at review; transport model, not Wi-Fi hardware |
| SI integrated features | [Archived SI receipts](../archive/internal-reference-2026-10-29/wireless-carplay/SI_STAGE2_OFFLINE.md) | Prior review found 297 matching inputs and 16 changed; not current-kit acceptance |
| Physical runs | [HU record](../wireless-carplay/STOCK210_HU_TEST.md), [SI boot record](../wireless-carplay/SI_STAGE1_HU_TEST.md) | Historical installed-kit observations |

For a new acceptance run, assemble one matched kit with `python3 wireless/si/build_si_review.py --full-cycle`, retain its manifest, then run the applicable native and physical profiles. Do not upgrade a dated PASS to current acceptance solely because a command still builds.
