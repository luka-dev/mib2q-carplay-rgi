# Android Auto audio-quality roadmap

This project treats audio fidelity and surround presentation as separate goals. A convincing six-speaker
upmix is useful, but it does not restore resolution discarded before the head unit receives the stream.

## Confirmed boundaries

- The stock MU0918 `gal.json` requests Android Auto media as uncompressed PCM at 48 kHz, 16-bit,
  two-channel. Guidance is a separate 16 kHz mono stream.
- Current Android Auto phone builds validate media audio configurations to 48 kHz or 16 kHz, 16-bit,
  and one or two channels. Advertising 24-bit, 96 kHz or six channels is therefore not a safe
  configuration-only change: the phone can reject the AV setup before playback begins.
- The audited MHI2Q audio path has a 48 kHz transport ceiling, a 24-bit effective depth and a six-channel
  output path. Material above 48 kHz must ultimately be sample-rate converted on this hardware.

The existing Android Auto stream is not low-bitrate compressed audio: 48,000 x 16 x 2 is 1.536 Mbit/s
of linear PCM. The fidelity loss for high-resolution sources comes from conversion to 16-bit/48 kHz,
not from an MP3-style encoder. A 24-bit or 96/192 kHz source is not delivered bit-perfectly.

## Track A: compatible surround path

Keep the standard Android Auto PCM negotiation so every supported phone continues to connect, then:

1. Interpose the GAL media renderer at the 48 kHz/16-bit/stereo PCM boundary.
2. Convert to the existing float DSP domain without another lossy encode.
3. Reuse the audited stereo-to-5.1 engine and six-channel QSA/CSD route.
4. Dither once at the final output boundary and retain the head unit's 24-bit container internally.
5. Add an `Android Auto UpMix` MMI control with Off and bounded intensity levels.

This improves spatial presentation and uses the B&O system's discrete channels, but it must be labelled
**car-side upmix**, not native Dolby Atmos or restored high-resolution audio.

## Track B: genuine 24-bit source path

For local high-resolution files, add an optional companion player and receiver side-channel beside the
Android Auto session:

1. The Android player reads the source itself and sends FLAC or framed PCM to a head-unit daemon.
2. The head unit decodes to 24-bit PCM, performs one high-quality conversion to 48 kHz when required,
   and writes through the validated media-focus and six-channel output path.
3. Android Auto remains connected for maps, controls, metadata and navigation. The receiver owns
   ducking/mixing so guidance and safety audio remain audible.
4. Preserve stereo as stereo or apply the same optional upmixer after the final 48 kHz conversion.

This is the practical route to 24-bit playback on MHI2Q. It cannot transparently capture protected
audio from arbitrary streaming applications; the companion must own playback or use an authorized
provider API. A rooted-phone Android Auto patch is a separate research experiment, not a public
installation path.

## Acceptance evidence

Do not advertise a fidelity mode until tests prove all of the following:

- negotiated Android Auto format and codec are captured from the live session;
- no unintended AAC or other lossy transcode occurs;
- source, receiver and QSA sample formats are logged at every boundary;
- channel identity reaches the correct B&O speakers with no fold-down;
- navigation ducking, calls and safety announcements still work;
- underruns remain zero during simultaneous cockpit video and audio playback;
- spectral, noise-floor and level measurements are saved alongside an on-car listening result.

