# Audio MOC

*Map of content for the whole stereo→5.1 CarPlay audio pipeline — phone to the 19 speakers of the Audi B&O "Virtual Cabin".*

---

## 📋 What this section covers

Two problems had to be solved to play surround over CarPlay: get the **right six PCM channels** to the amp, and **synthesize six channels** for the stereo content that is 99% of what people play. The J525 B&O amplifier is already a complete 5.1 renderer, so the head unit only feeds it `L R C LFE Ls Rs` and lets it do the acoustics — that division of labour is the [[Virtual Cabin and Amp RE|Virtual Cabin]].

This MOC is the entry point. Start with [[Stereo to 5.1 Upmix]] for the end-to-end flow, then branch into the model, the amp, Dolby, or the discrete path. For the hub note and the trap list see [[Home]] and [[Landmines]].

## 📊 The whole pipeline

Both CarPlay audio streams reach the HSSR upmix: the buffered type-103 stream for decoded stereo, and the stock realtime type-100 stream for media apps such as Spotify (`AUDIO_REALTIME_UPMIX=on`, `deploy/smartphone_integrator/carplay_startup.sh:86`).

```mermaid
flowchart TB
  accTitle: Stereo to 5.1 CarPlay audio pipeline
  accDescr: Two phone transports feed decode or the realtime path, split into discrete 5.1 or the HSSR upmix, converge on PCM6 and travel through deva and MOST to the J525 amp and the speakers.
  subgraph phone_side["Phone and transports"]
    iphone["iPhone CarPlay over NCM"]
    realtime_100["type-100 realtime PCM stereo"]
    buffered_103["type-103 buffered AAC or APAC"]
  end
  subgraph hook_side["Hook in dio_manager"]
    cookie_decode["decode by magic cookie"]
    classify_pcm["classify decoded PCM"]
    discrete_51["discrete 5.1 reorder"]
    hssr_upmix["HSSR stereo upmix"]
    pcm6_out["PCM6 with C zero"]
  end
  subgraph car_side["Head unit and amp"]
    deva_csd["deva to CSD to LPASS"]
    most_ring["MOST150 ring to RCC"]
    j525_amp["J525 amp V850 and SHARC"]
    speakers_19["19 cabin speakers"]
  end
  iphone --> realtime_100
  iphone --> buffered_103
  realtime_100 -->|media only| hssr_upmix
  buffered_103 --> cookie_decode --> classify_pcm
  classify_pcm -->|APAC 5.1| discrete_51 --> pcm6_out
  classify_pcm -->|stereo| hssr_upmix --> pcm6_out
  pcm6_out --> deva_csd --> most_ring --> j525_amp --> speakers_19
  classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
  classDef neutral fill:#f3f4f6,stroke:#6b7280,stroke-width:2px,color:#1f2937
  class iphone,speakers_19 primary
  class discrete_51,hssr_upmix success
  class j525_amp neutral
```

## 🔗 The notes

| Note | What it covers |
|---|---|
| [[Stereo to 5.1 Upmix]] | End-to-end pipeline: ingest, where upmix vs discrete kicks in, the shipped calibration, what ships. |
| [[HSSR Model]] | The AI: why separation failed, the second-scene design, the runtime, every model iteration V9→q6d, calibration. |
| [[Virtual Cabin and Amp RE]] | The J525 amp reverse-engineered: two-SoC topology, the single-byte routing decision, the SHARC DSP chain. |
| [[Dolby Atmos]] | What Atmos is on CarPlay, why phone-side spatial is blocked, what HSSR borrowed and where it diverges. |
| [[Discrete 5.1 Routing]] | The native 5.1 path: deva mirror driver, CSD min_voices fix, SAD `asad=0xfc`, magic-cookie codec authority. |

## 💡 Truth levels

Throughout these notes, claims are tagged: `[car]` confirmed on the unit, `[model]` static-RE or offline render, `[open]` unverified. The honest headline: discrete 5.1 is `[car]`; the amp DSP is `[model]` static RE; no blind listening or cabin SPL has ever been measured.

Code state is checked against the source tree; every code claim cites `path:line`. Provenance lives in `docs-obsidian/Research/UPMIX.md` and its three companion papers.
