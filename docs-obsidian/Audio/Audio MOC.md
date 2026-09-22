# Audio MOC

*Map of content for the whole stereo→5.1 CarPlay audio pipeline — phone to the 19 speakers of the Audi B&O "Virtual Cabin".*

---

## 📋 What this section covers

Two problems had to be solved to play surround over CarPlay: **get the right six PCM channels** to the amp, and **synthesize six channels for the stereo content that is 99% of what people play**. The J525 B&O amplifier is already a complete 5.1 renderer, so the head unit only feeds it `L R C LFE Ls Rs` and lets it do the acoustics — that division of labour is the [[Virtual Cabin and Amp RE|Virtual Cabin]].

This MOC is the entry point. Start with [[Stereo to 5.1 Upmix]] for the end-to-end flow, then branch into the model, the amp, Dolby, or the discrete path. For the hub note and the trap list see [[Home]] and [[Landmines]].

## 📊 The whole pipeline

```mermaid
flowchart TB
  accTitle: Stereo to 5.1 CarPlay audio pipeline
  accDescr: Phone audio through two transports, decode, upmix or discrete, into PCM6, down through deva and MOST to the J525 amp and the speakers.
  phone["iPhone (CarPlay over USB-NCM)"]
  t100["type-100 realtime (PCM stereo)"]
  t103["type-103 buffered (AAC-LC / APAC)"]
  decode["decode (magic cookie = codec authority)"]
  classify["classify decoded PCM"]
  discrete["discrete 5.1 (reorder + 8 to 6 fold)"]
  hssr["HSSR upmix (STFT to GRU to rear synth)"]
  pcm6["PCM6 L R C=0 LFE Ls Rs (cabin guard)"]
  deva["deva-ctrl-qc to CSD to LPASS TDM"]
  most["MMX INIC to MOST150 ring to RCC INIC"]
  amp["J525 amp: V850 + SHARC (matrix + DSP)"]
  spk["19 speakers = the Virtual Cabin"]
  phone --> t100
  phone --> t103
  t100 --> pcm6
  t103 --> decode --> classify
  classify --> discrete --> pcm6
  classify --> hssr --> pcm6
  pcm6 --> deva --> most --> amp --> spk
  class phone,spk primary
  class discrete,hssr success
  class amp neutral
  classDef primary fill:#dbeafe,stroke:#2563eb,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,color:#14532d
  classDef neutral fill:#f3f4f6,stroke:#6b7280,color:#1f2937
```

## 🔗 The notes

| Note | What it covers |
|---|---|
| [[Stereo to 5.1 Upmix]] | End-to-end pipeline: ingest, where upmix vs discrete kicks in, the frozen calibration, what ships. |
| [[HSSR Model]] | The AI: why separation failed, the second-scene design, the runtime, every model iteration V9→q6d, calibration. |
| [[Virtual Cabin and Amp RE]] | The J525 amp reverse-engineered: two-SoC topology, the single-byte routing decision, the SHARC DSP chain. |
| [[Dolby Atmos]] | What Atmos is on CarPlay, why phone-side spatial is blocked, what HSSR borrowed and where it diverges. |
| [[Discrete 5.1 Routing]] | The native 5.1 path: deva mirror driver, CSD min_voices fix, SAD `asad=0xfc`, magic-cookie codec authority. |

## 💡 Truth levels

Throughout these notes, claims are tagged: **[car]** confirmed on the unit, **[model]** static-RE / offline render, **[open]** unverified. The honest headline: discrete 5.1 is **[car]**; the amp DSP is **[model]** static RE; no blind listening or cabin SPL has ever been measured.

Provenance lives in `docs/research/UPMIX.md` and its three companion papers.
