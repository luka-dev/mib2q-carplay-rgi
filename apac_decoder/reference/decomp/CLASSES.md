# APAC decoder — recovered class layout

Source: `libSPF_CP_PlugInWrapper.so`, Pioneer SPHERA 1.030010 (AArch64 Linux ELF, image base 0).
Recovered with IDA Pro 9.3 / Hex-Rays from the intact `.dynsym` Itanium symbols, the
`_ZTV`/`_ZTI`/`_ZTS` RTTI records, and the decompiled member accesses.

Everything in this file is derived from the binary. Where a value could not be derived it
says so; nothing here is inferred from how other AAC-family codecs are built.

---

## 1. The bitstream reader

`TBitstreamReader<unsigned int>` (typedef'd `BitstreamReader` in Apple's source, per the
`__PRETTY_FUNCTION__` string at `0x30ea90`) is a **real class with out-of-line copies of every
method**, and it is *also* inlined at almost every call site. Both forms are in the binary.
Out-of-line at `0x000b1430`–`0x000b19a8`, decompiled in `TBitstreamReader.c`.

Layout (24 bytes), derived from `FillCacheFrom`, `GetBits`, `BitsRemaining` and the ctor:

```c
struct TBitstreamReader_u32 {   /* sizeof == 24 */
    const uint8_t *cur;      /* +0   next 32-bit word to pull into the cache        */
    const uint8_t *end;      /* +8   one past the last byte of the buffer           */
    uint32_t       cache;    /* +16  MSB-aligned; bits are taken from the top       */
    int32_t        bitsLeft; /* +20  signed; < 0 means the cache must be refilled   */
};
```

Semantics, exactly as decompiled:

* `FillCacheFrom(p)` — if `end - p > 3`, `cache = bswap32(*(uint32_t*)p)` (a **big-endian**
  32-bit load). Otherwise it shifts in the `end - p` bytes that remain, MSB first, and pads
  the rest of the word with `0xFF` bytes. Note the tail padding is `0xFF`, not zero.
* `GetBits(n)` — returns 0 for `n == 0` or `n > 32`. Otherwise
  `v = cache >> (32 - n); bitsLeft -= n;` and if `bitsLeft` went negative it refills:
  `v |= newcache >> (32 + bitsLeft); cache = newcache << (-bitsLeft); cur += 4;
  bitsLeft += 32;` with `bitsLeft == -32` special-cased to `cache = 0`.
  All shifts rely on AArch64 `LSR`/`LSL` taking the shift amount modulo 32.
* `GetBit()` — the `n == 1` specialisation of the above.
* `SkipBits(n)`, `AlignToNextByte()` — advance without producing a value;
  `AlignToNextByte` discards `bitsLeft & 7` bits.
* `BitsRemaining()` — `bitsLeft + 8 * (uint32_t)(end - cur)`. The pointer difference is
  **truncated to 32 bits** before the multiply; that is what the code does.
* `PutBack(n)` — rewinds: `bitsLeft = (bitsLeft + n) & 31; cur -= 4 * ((bitsLeft + n) >> 5);`
  then refills from `cur - 4`, or zeroes the cache when the new `bitsLeft` is 0.
* The constructor takes `(const uint8_t *buf, uint32_t len)`, sets `cur = buf`, `end = buf+len`,
  and if `buf` is **not 4-byte aligned** it pre-loads the leading 1–3 bytes into the cache and
  advances `cur` to the next 4-byte boundary. A port must reproduce this, because every
  subsequent refill assumes `cur` is word aligned.

`apac::GetEscBits<ESCBits, ESC2Bits>(BitstreamReader&, uint32_t numBits)` is instantiated three
times — `<16,20>` at `0x78150`, `<8,16>` at `0x96630`, `<6,12>` at `0x99220`. It reads
`numBits`; if the value is all-ones it reads a further `ESCBits` and adds; if *that* escape is
all-ones it reads `ESC2Bits` and adds again. Asserts `numBits <= 32`
(`APACStructs.h:198`, from the assert string at `0x30eb50`).

---

## 2. Class hierarchy (from `_ZTI` records)

`__class_type_info` (16 bytes) = no base; `__si_class_type_info` (24 bytes) = one public base
at word 2; 32 bytes = `__vmi_class_type_info`.

```
APACChannelRemapper
APACDecoderInterface
    APACDecoder
        APACASPDecoder
APACICSInfo
APACIndividualChannelStream
APACSynthesisFilterBank
IAPACBWEDecoder
    APACBWE2Decoder
IAPACBaseDecoder
    APACCoreDecoder
        APACCoreLBRDecoder
apac::APACAncillaryFrameDecoder
apac::APACAudioSceneDecoder
apac::APACDRCDecoder
apac::AncillaryConfig
apac::Autocorr
apac::DecElement
    apac::ChanDecElement
apac::GlobalConfig
apac::GlobalConfig const*
apac::GlobalConfig::ComponentConfigs
    apac::GlobalConfig::ASCConfigData
    apac::GlobalConfig::CodecConfigData
apac::IASCConfig
    apac::BaseASCConfig
    apac::chan::CoreDecoderConfig
        apac::lbr::DecoderConfig
apac::IExtFrameElementDecoder
    apac::ExtFrameUnkownDecoder
apac::LPCBase
    apac::LSFTools
        APACLRVQDecoder::LRVQLPC
apac::LPMDCTFilter
    APACLRVQDecoder::LPMDCT_LONG
    APACLRVQDecoder::LPMDCT_LTP
apac::TCEConfig
apac::drc::APACDRCConfigBase
    apac::drc::DynRangeCompressor
```

Classes with a `_ZTI` record but **no** `_ZTV` (abstract-with-no-vtable, or POD with RTTI only):
`APACDecoderInterface`, `IAPACBWEDecoder`, `IAPACBaseDecoder`, `apac::GlobalConfig`, `apac::GlobalConfig const*`, `apac::GlobalConfig::ComponentConfigs`, `apac::IExtFrameElementDecoder`, `apac::drc::APACDRCConfigBase`

---

## 3. Vtables

Slot 0 is offset-to-top, slot 1 is the `_ZTI` pointer; virtual function *index n* is at
slot `n + 2` (byte offset `16 + 8n`). Order is the compiler's, i.e. declaration order.

### `APACASPDecoder`  — vtable at `0x0042a278`, 88 bytes, 9 virtuals
base: `APACDecoder`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACASPDecoder` |
| 2 | 0 | `APACASPDecoder::~APACASPDecoder()` |
| 3 | 1 | `APACASPDecoder::~APACASPDecoder()` |
| 4 | 2 | `APACASPDecoder::Initialize(apac::GlobalConfig const&)` |
| 5 | 3 | `APACDecoder::Reset(void)` |
| 6 | 4 | `APACASPDecoder::DecodeFrame(unsigned char const*,unsigned int,apac::FrameOutRecord &)` |
| 7 | 5 | `APACDecoder::GetParameter(int,boost::any &)const` |
| 8 | 6 | `APACDecoder::SetParameter(int,boost::any const&)` |
| 9 | 7 | `APACDecoder::CreateDecoderElement(apac::ASCType)` |
| 10 | 8 | `APACASPDecoder::DecodeOneFrame(TBitstreamReader<unsigned int> &)` |

### `APACBWE2Decoder`  — vtable at `0x0042a868`, 64 bytes, 6 virtuals
base: `IAPACBWEDecoder`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACBWE2Decoder` |
| 2 | 0 | `APACBWE2Decoder::~APACBWE2Decoder()` |
| 3 | 1 | `APACBWE2Decoder::~APACBWE2Decoder()` |
| 4 | 2 | `APACBWE2Decoder::Initialize(unsigned int)` |
| 5 | 3 | `APACBWE2Decoder::Deserialize(TBitstreamReader<unsigned int> &,unsigned int,APACSyntacticElement &)` |
| 6 | 4 | `APACBWE2Decoder::DecodeFrame(APACICSInfo const&,APACSpectralData &,unsigned int)` |
| 7 | 5 | `APACBWE2Decoder::GetActiveFlag(unsigned int)const` |

### `APACChannelPairElement`  — vtable at `0x0042b528`, 80 bytes, 8 virtuals
base: `APACSyntacticElement`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACChannelPairElement` |
| 2 | 0 | `APACChannelPairElement::~APACChannelPairElement()` |
| 3 | 1 | `APACChannelPairElement::~APACChannelPairElement()` |
| 4 | 2 | `APACChannelPairElement::Deserialize(TBitstreamReader<unsigned int> &)` |
| 5 | 3 | `APACChannelPairElement::Reset(void)` |
| 6 | 4 | `APACChannelPairElement::ChannelData(bool)` |
| 7 | 5 | `APACChannelPairElement::GetICSInfo(bool)` |
| 8 | 6 | `APACChannelPairElement::DeserializeSQFrame(TBitstreamReader<unsigned int> &)` |
| 9 | 7 | `APACChannelPairElement::DeserializeLrvqFrame(TBitstreamReader<unsigned int> &)` |

### `APACChannelRemapper`  — vtable at `0x0042a8b8`, 48 bytes, 4 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACChannelRemapper` |
| 2 | 0 | `APACChannelRemapper::~APACChannelRemapper()` |
| 3 | 1 | `APACChannelRemapper::~APACChannelRemapper()` |
| 4 | 2 | `APACChannelRemapper::Initialize(std::vector<unsigned char,std::allocator<unsigned char>> const&)` |
| 5 | 3 | `APACChannelRemapper::Process(std::vector<float *,std::allocator<float *>> &)` |

### `APACCoreDecoder`  — vtable at `0x0042a9e0`, 88 bytes, 9 virtuals
base: `IAPACBaseDecoder`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACCoreDecoder` |
| 2 | 0 | `APACCoreDecoder::~APACCoreDecoder()` |
| 3 | 1 | `APACCoreDecoder::~APACCoreDecoder()` |
| 4 | 2 | `APACCoreDecoder::Initialize(apac::ParamList const&,apac::observer_ptr<apac::IASCConfig>)` |
| 5 | 3 | `APACCoreDecoder::Reset(void)` |
| 6 | 4 | `APACCoreDecoder::DecodeAPACFrame(TBitstreamReader<unsigned int> &,apac::FrameType,std::vector<float *,std::allocator<float *>> &)` |
| 7 | 5 | `APACCoreDecoder::Deserialize(unsigned char const*,unsigned int)` |
| 8 | 6 | `APACCoreDecoder::Deserialize(TBitstreamReader<unsigned int> &,bool)` |
| 9 | 7 | `APACCoreDecoder::GetBlockSize(void)const` |
| 10 | 8 | `APACCoreDecoder::GetNumChannels(void)const` |

### `APACCoreLBRDecoder`  — vtable at `0x0042aaa0`, 88 bytes, 9 virtuals
base: `APACCoreDecoder`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACCoreLBRDecoder` |
| 2 | 0 | `APACCoreLBRDecoder::~APACCoreLBRDecoder()` |
| 3 | 1 | `APACCoreLBRDecoder::~APACCoreLBRDecoder()` |
| 4 | 2 | `APACCoreLBRDecoder::Initialize(apac::ParamList const&,apac::observer_ptr<apac::IASCConfig>)` |
| 5 | 3 | `APACCoreDecoder::Reset(void)` |
| 6 | 4 | `APACCoreDecoder::DecodeAPACFrame(TBitstreamReader<unsigned int> &,apac::FrameType,std::vector<float *,std::allocator<float *>> &)` |
| 7 | 5 | `APACCoreDecoder::Deserialize(unsigned char const*,unsigned int)` |
| 8 | 6 | `APACCoreLBRDecoder::Deserialize(TBitstreamReader<unsigned int> &,bool)` |
| 9 | 7 | `APACCoreDecoder::GetBlockSize(void)const` |
| 10 | 8 | `APACCoreDecoder::GetNumChannels(void)const` |

### `APACDecoder`  — vtable at `0x0042ae08`, 80 bytes, 8 virtuals
base: `APACDecoderInterface`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACDecoder` |
| 2 | 0 | `APACDecoder::~APACDecoder()` |
| 3 | 1 | `APACDecoder::~APACDecoder()` |
| 4 | 2 | `APACDecoder::Initialize(apac::GlobalConfig const&)` |
| 5 | 3 | `APACDecoder::Reset(void)` |
| 6 | 4 | `APACDecoder::DecodeFrame(unsigned char const*,unsigned int,apac::FrameOutRecord &)` |
| 7 | 5 | `APACDecoder::GetParameter(int,boost::any &)const` |
| 8 | 6 | `APACDecoder::SetParameter(int,boost::any const&)` |
| 9 | 7 | `APACDecoder::CreateDecoderElement(apac::ASCType)` |

### `APACICSInfo`  — vtable at `0x0042b010`, 48 bytes, 4 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACICSInfo` |
| 2 | 0 | `APACICSInfo::~APACICSInfo()` |
| 3 | 1 | `APACICSInfo::~APACICSInfo()` |
| 4 | 2 | `APACICSInfo::Deserialize(TBitstreamReader<unsigned int> &,apac::CodingType)` |
| 5 | 3 | `APACICSInfo::Reset(void)` |

### `APACIndividualChannelStream`  — vtable at `0x0042b040`, 40 bytes, 3 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACIndividualChannelStream` |
| 2 | 0 | `APACIndividualChannelStream::~APACIndividualChannelStream()` |
| 3 | 1 | `APACIndividualChannelStream::~APACIndividualChannelStream()` |
| 4 | 2 | `APACIndividualChannelStream::Deserialize(TBitstreamReader<unsigned int> &,APACICSInfo &,bool)` |

### `APACLFEElement`  — vtable at `0x0042b4e8`, 64 bytes, 6 virtuals
base: `APACSyntacticElement`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACLFEElement` |
| 2 | 0 | `APACLFEElement::~APACLFEElement()` |
| 3 | 1 | `APACLFEElement::~APACLFEElement()` |
| 4 | 2 | `APACLFEElement::Deserialize(TBitstreamReader<unsigned int> &)` |
| 5 | 3 | `APACLFEElement::Reset(void)` |
| 6 | 4 | `APACSyntacticElement::ChannelData(bool)` |
| 7 | 5 | `APACSyntacticElement::GetICSInfo(bool)` |

### `APACLRVQDecoder::LPMDCT_LONG`  — vtable at `0x0042b158`, 80 bytes, 8 virtuals
base: `apac::LPMDCTFilter`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACLRVQDecoder::LPMDCT_LONG` |
| 2 | 0 | `APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG()` |
| 3 | 1 | `APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG()` |
| 4 | 2 | `apac::LPMDCTFilter::Initialize(unsigned short,unsigned short)` |
| 5 | 3 | `apac::LPMDCTFilter::Initialize(void)` |
| 6 | 4 | `apac::LPMDCTFilter::MDCTToAutoCorr(float const*,float *,unsigned int,unsigned int,unsigned int)` |
| 7 | 5 | `apac::LPMDCTFilter::LPCToSpecEnvelope(float const*,float *,unsigned int)` |
| 8 | 6 | `apac::LPMDCTFilter::LPImpulseToSpecEnvelope(float const*,float *,unsigned int)` |
| 9 | 7 | `apac::LPMDCTFilter::AutocorrFromPSD(float const*,float *,unsigned int)` |

### `APACLRVQDecoder::LPMDCT_LTP`  — vtable at `0x0042b1a8`, 80 bytes, 8 virtuals
base: `apac::LPMDCTFilter`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACLRVQDecoder::LPMDCT_LTP` |
| 2 | 0 | `APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP()` |
| 3 | 1 | `APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP()` |
| 4 | 2 | `apac::LPMDCTFilter::Initialize(unsigned short,unsigned short)` |
| 5 | 3 | `apac::LPMDCTFilter::Initialize(void)` |
| 6 | 4 | `apac::LPMDCTFilter::MDCTToAutoCorr(float const*,float *,unsigned int,unsigned int,unsigned int)` |
| 7 | 5 | `apac::LPMDCTFilter::LPCToSpecEnvelope(float const*,float *,unsigned int)` |
| 8 | 6 | `apac::LPMDCTFilter::LPImpulseToSpecEnvelope(float const*,float *,unsigned int)` |
| 9 | 7 | `apac::LPMDCTFilter::AutocorrFromPSD(float const*,float *,unsigned int)` |

### `APACLRVQDecoder::LRVQLPC`  — vtable at `0x0042b0b0`, 168 bytes, 19 virtuals
base: `apac::LSFTools`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACLRVQDecoder::LRVQLPC` |
| 2 | 0 | `APACLRVQDecoder::LRVQLPC::~LRVQLPC()` |
| 3 | 1 | `APACLRVQDecoder::LRVQLPC::~LRVQLPC()` |
| 4 | 2 | `APACLRVQDecoder::LRVQLPC::Initialize(void)` |
| 5 | 3 | `apac::LSFTools::Reset(void)` |
| 6 | 4 | `apac::LPCBase::LPC_Weighting(float const*,float *,float,unsigned short)` |
| 7 | 5 | `apac::LPCBase::ComputeLPCs(float const*,bool)` |
| 8 | 6 | `apac::LPCBase::LevinsonDurbin(float const*)` |
| 9 | 7 | `apac::LSFTools::QuantizeLSF(unsigned short *)` |
| 10 | 8 | `apac::LSFTools::get_LSFWeightingFactors(float const*,float *,unsigned short,float)` |
| 11 | 9 | `apac::LSFTools::DecodeLSF(unsigned short const*)` |
| 12 | 10 | `apac::LSFTools::LSF_To_LPC(float *,float *)` |
| 13 | 11 | `apac::LSFTools::LSP_To_LSF(float const*,float *,unsigned short,float)` |
| 14 | 12 | `apac::LSFTools::LSF_To_LSP(float const*,float *,float)` |
| 15 | 13 | `apac::LSFTools::LSF_To_LSP(float *,float)` |
| 16 | 14 | `apac::LSFTools::LSP_To_LPC(float const*,float *,unsigned short)` |
| 17 | 15 | `apac::LSFTools::LSP_To_LPC(void)` |
| 18 | 16 | `apac::LSFTools::Condition_LSFs(float *,float,float)` |
| 19 | 17 | `apac::LSFTools::ConditionLSFs(float *)` |
| 20 | 18 | `APACLRVQDecoder::LRVQLPC::Initialize(unsigned short)` |

### `APACSingleChannelElement`  — vtable at `0x0042b4a8`, 64 bytes, 6 virtuals
base: `APACSyntacticElement`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACSingleChannelElement` |
| 2 | 0 | `APACSingleChannelElement::~APACSingleChannelElement()` |
| 3 | 1 | `APACSingleChannelElement::~APACSingleChannelElement()` |
| 4 | 2 | `APACSingleChannelElement::Deserialize(TBitstreamReader<unsigned int> &)` |
| 5 | 3 | `APACSingleChannelElement::Reset(void)` |
| 6 | 4 | `APACSyntacticElement::ChannelData(bool)` |
| 7 | 5 | `APACSyntacticElement::GetICSInfo(bool)` |

### `APACSyntacticElement`  — vtable at `0x0042b468`, 64 bytes, 6 virtuals
base: `AlignedAllocator<16u>`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACSyntacticElement` |
| 2 | 0 | `0` |
| 3 | 1 | `0` |
| 4 | 2 | `__cxa_pure_virtual` |
| 5 | 3 | `__cxa_pure_virtual` |
| 6 | 4 | `APACSyntacticElement::ChannelData(bool)` |
| 7 | 5 | `APACSyntacticElement::GetICSInfo(bool)` |

### `APACSynthesisFilterBank`  — vtable at `0x0042b588`, 56 bytes, 5 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'APACSynthesisFilterBank` |
| 2 | 0 | `APACSynthesisFilterBank::~APACSynthesisFilterBank()` |
| 3 | 1 | `APACSynthesisFilterBank::~APACSynthesisFilterBank()` |
| 4 | 2 | `APACSynthesisFilterBank::Initialize(unsigned int,unsigned int)` |
| 5 | 3 | `APACSynthesisFilterBank::Reset(void)` |
| 6 | 4 | `APACSynthesisFilterBank::FrequencyToTimeInPlace(unsigned int,float *,unsigned int,unsigned int)` |

### `apac::APACAncillaryFrameDecoder`  — vtable at `0x0042a548`, 48 bytes, 4 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::APACAncillaryFrameDecoder` |
| 2 | 0 | `apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder()` |
| 3 | 1 | `apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder()` |
| 4 | 2 | `apac::APACAncillaryFrameDecoder::Initialize(apac::ParamList const&)` |
| 5 | 3 | `apac::APACAncillaryFrameDecoder::DecodeFrame(TBitstreamReader<unsigned int> &,std::vector<apac::ASCAudioOutData,std::allocator<apac::ASCAudioOutData>> &,apac::FrameType)` |

### `apac::APACAudioSceneDecoder`  — vtable at `0x0042a508`, 32 bytes, 2 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::APACAudioSceneDecoder` |
| 2 | 0 | `apac::APACAudioSceneDecoder::~APACAudioSceneDecoder()` |
| 3 | 1 | `apac::APACAudioSceneDecoder::~APACAudioSceneDecoder()` |

### `apac::APACDRCDecoder`  — vtable at `0x0042a528`, 32 bytes, 2 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::APACDRCDecoder` |
| 2 | 0 | `apac::APACDRCDecoder::~APACDRCDecoder()` |
| 3 | 1 | `apac::APACDRCDecoder::~APACDRCDecoder()` |

### `apac::AncillaryConfig`  — vtable at `0x0042a350`, 32 bytes, 2 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::AncillaryConfig` |
| 2 | 0 | `apac::AncillaryConfig::~AncillaryConfig()` |
| 3 | 1 | `apac::AncillaryConfig::~AncillaryConfig()` |

### `apac::Autocorr`  — vtable at `0x0042a848`, 32 bytes, 2 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::Autocorr` |
| 2 | 0 | `apac::Autocorr::~Autocorr()` |
| 3 | 1 | `apac::Autocorr::~Autocorr()` |

### `apac::BaseASCConfig`  — vtable at `0x0042afb8`, 56 bytes, 5 virtuals
base: `apac::IASCConfig`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::BaseASCConfig` |
| 2 | 0 | `apac::BaseASCConfig::~BaseASCConfig()` |
| 3 | 1 | `apac::BaseASCConfig::~BaseASCConfig()` |
| 4 | 2 | `apac::BaseASCConfig::Deserialize(TBitstreamReader<unsigned int> &)` |
| 5 | 3 | `apac::BaseASCConfig::GetNumChannels(void)const` |
| 6 | 4 | `apac::BaseASCConfig::GetChannelLayout(void)const` |

### `apac::ChanDecElement`  — vtable at `0x0042ac58`, 40 bytes, 3 virtuals
base: `apac::DecElement`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::ChanDecElement` |
| 2 | 0 | `apac::ChanDecElement::~ChanDecElement()` |
| 3 | 1 | `apac::ChanDecElement::~ChanDecElement()` |
| 4 | 2 | `apac::ChanDecElement::Initialize(apac::ParamList const&,apac::observer_ptr<apac::IASCConfig>)` |

### `apac::DecElement`  — vtable at `0x0042ac30`, 40 bytes, 3 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::DecElement` |
| 2 | 0 | `0` |
| 3 | 1 | `0` |
| 4 | 2 | `__cxa_pure_virtual` |

### `apac::ExtFrameUnkownDecoder`  — vtable at `0x0042af70`, 48 bytes, 4 virtuals
base: `apac::IExtFrameElementDecoder`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::ExtFrameUnkownDecoder` |
| 2 | 0 | `apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder()` |
| 3 | 1 | `apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder()` |
| 4 | 2 | `apac::ExtFrameUnkownDecoder::Initialize(apac::IExtCfgElement const&)` |
| 5 | 3 | `apac::ExtFrameUnkownDecoder::Decode(TBitstreamReader<unsigned int> &)` |

### `apac::GlobalConfig::ASCConfigData`  — vtable at `0x0042b930`, 48 bytes, 4 virtuals
base: `apac::GlobalConfig::ComponentConfigs`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::GlobalConfig::ASCConfigData` |
| 2 | 0 | `apac::GlobalConfig::ASCConfigData::configs(void)const` |
| 3 | 1 | `apac::GlobalConfig::ComponentConfigs::configs(void)` |
| 4 | 2 | `apac::GlobalConfig::ASCConfigData::~ASCConfigData()` |
| 5 | 3 | `apac::GlobalConfig::ASCConfigData::~ASCConfigData()` |

### `apac::GlobalConfig::CodecConfigData`  — vtable at `0x0042b900`, 48 bytes, 4 virtuals
base: `apac::GlobalConfig::ComponentConfigs`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::GlobalConfig::CodecConfigData` |
| 2 | 0 | `apac::GlobalConfig::CodecConfigData::configs(void)const` |
| 3 | 1 | `apac::GlobalConfig::ComponentConfigs::configs(void)` |
| 4 | 2 | `apac::GlobalConfig::CodecConfigData::~CodecConfigData()` |
| 5 | 3 | `apac::GlobalConfig::CodecConfigData::~CodecConfigData()` |

### `apac::IASCConfig`  — vtable at `0x0042a948`, 56 bytes, 5 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::IASCConfig` |
| 2 | 0 | `0` |
| 3 | 1 | `0` |
| 4 | 2 | `__cxa_pure_virtual` |
| 5 | 3 | `__cxa_pure_virtual` |
| 6 | 4 | `__cxa_pure_virtual` |

### `apac::LPCBase`  — vtable at `0x0042b750`, 72 bytes, 7 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::LPCBase` |
| 2 | 0 | `0` |
| 3 | 1 | `0` |
| 4 | 2 | `__cxa_pure_virtual` |
| 5 | 3 | `__cxa_pure_virtual` |
| 6 | 4 | `apac::LPCBase::LPC_Weighting(float const*,float *,float,unsigned short)` |
| 7 | 5 | `apac::LPCBase::ComputeLPCs(float const*,bool)` |
| 8 | 6 | `apac::LPCBase::LevinsonDurbin(float const*)` |

### `apac::LPMDCTFilter`  — vtable at `0x0042b700`, 80 bytes, 8 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::LPMDCTFilter` |
| 2 | 0 | `apac::LPMDCTFilter::~LPMDCTFilter()` |
| 3 | 1 | `apac::LPMDCTFilter::~LPMDCTFilter()` |
| 4 | 2 | `apac::LPMDCTFilter::Initialize(unsigned short,unsigned short)` |
| 5 | 3 | `apac::LPMDCTFilter::Initialize(void)` |
| 6 | 4 | `apac::LPMDCTFilter::MDCTToAutoCorr(float const*,float *,unsigned int,unsigned int,unsigned int)` |
| 7 | 5 | `apac::LPMDCTFilter::LPCToSpecEnvelope(float const*,float *,unsigned int)` |
| 8 | 6 | `apac::LPMDCTFilter::LPImpulseToSpecEnvelope(float const*,float *,unsigned int)` |
| 9 | 7 | `apac::LPMDCTFilter::AutocorrFromPSD(float const*,float *,unsigned int)` |

### `apac::LSFTools`  — vtable at `0x0042b798`, 160 bytes, 18 virtuals
base: `apac::LPCBase`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::LSFTools` |
| 2 | 0 | `apac::LSFTools::~LSFTools()` |
| 3 | 1 | `apac::LSFTools::~LSFTools()` |
| 4 | 2 | `apac::LSFTools::Initialize(void)` |
| 5 | 3 | `apac::LSFTools::Reset(void)` |
| 6 | 4 | `apac::LPCBase::LPC_Weighting(float const*,float *,float,unsigned short)` |
| 7 | 5 | `apac::LPCBase::ComputeLPCs(float const*,bool)` |
| 8 | 6 | `apac::LPCBase::LevinsonDurbin(float const*)` |
| 9 | 7 | `apac::LSFTools::QuantizeLSF(unsigned short *)` |
| 10 | 8 | `apac::LSFTools::get_LSFWeightingFactors(float const*,float *,unsigned short,float)` |
| 11 | 9 | `apac::LSFTools::DecodeLSF(unsigned short const*)` |
| 12 | 10 | `apac::LSFTools::LSF_To_LPC(float *,float *)` |
| 13 | 11 | `apac::LSFTools::LSP_To_LSF(float const*,float *,unsigned short,float)` |
| 14 | 12 | `apac::LSFTools::LSF_To_LSP(float const*,float *,float)` |
| 15 | 13 | `apac::LSFTools::LSF_To_LSP(float *,float)` |
| 16 | 14 | `apac::LSFTools::LSP_To_LPC(float const*,float *,unsigned short)` |
| 17 | 15 | `apac::LSFTools::LSP_To_LPC(void)` |
| 18 | 16 | `apac::LSFTools::Condition_LSFs(float *,float,float)` |
| 19 | 17 | `apac::LSFTools::ConditionLSFs(float *)` |

### `apac::TCEConfig`  — vtable at `0x0042a920`, 40 bytes, 3 virtuals

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::TCEConfig` |
| 2 | 0 | `apac::TCEConfig::~TCEConfig()` |
| 3 | 1 | `apac::TCEConfig::~TCEConfig()` |
| 4 | 2 | `apac::TCEConfig::Deserialize(TBitstreamReader<unsigned int> &)` |

### `apac::chan::CoreDecoderConfig`  — vtable at `0x0042a980`, 56 bytes, 5 virtuals
base: `apac::IASCConfig`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::chan::CoreDecoderConfig` |
| 2 | 0 | `apac::chan::CoreDecoderConfig::~CoreDecoderConfig()` |
| 3 | 1 | `apac::chan::CoreDecoderConfig::~CoreDecoderConfig()` |
| 4 | 2 | `apac::chan::CoreDecoderConfig::Deserialize(TBitstreamReader<unsigned int> &)` |
| 5 | 3 | `apac::chan::CoreDecoderConfig::GetNumChannels(void)const` |
| 6 | 4 | `apac::chan::CoreDecoderConfig::GetChannelLayout(void)const` |

### `apac::drc::DynRangeCompressor`  — vtable at `0x0042a330`, 32 bytes, 2 virtuals
base: `apac::drc::APACDRCConfigBase`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::drc::DynRangeCompressor` |
| 2 | 0 | `apac::drc::DynRangeCompressor::~DynRangeCompressor()` |
| 3 | 1 | `apac::drc::DynRangeCompressor::~DynRangeCompressor()` |

### `apac::lbr::DecoderConfig`  — vtable at `0x0042aa50`, 56 bytes, 5 virtuals
base: `apac::chan::CoreDecoderConfig`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'apac::lbr::DecoderConfig` |
| 2 | 0 | `apac::lbr::DecoderConfig::~DecoderConfig()` |
| 3 | 1 | `apac::lbr::DecoderConfig::~DecoderConfig()` |
| 4 | 2 | `apac::lbr::DecoderConfig::Deserialize(TBitstreamReader<unsigned int> &)` |
| 5 | 3 | `apac::chan::CoreDecoderConfig::GetNumChannels(void)const` |
| 6 | 4 | `apac::chan::CoreDecoderConfig::GetChannelLayout(void)const` |

### `boost::any::holder<apac::GlobalConfig const*>`  — vtable at `0x0042ae58`, 48 bytes, 4 virtuals
base: `boost::any::placeholder`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'boost::any::holder<apac::GlobalConfig const*>` |
| 2 | 0 | `boost::any::holder<apac::GlobalConfig const*>::~holder()` |
| 3 | 1 | `boost::any::holder<apac::GlobalConfig const*>::~holder()` |
| 4 | 2 | `boost::any::holder<apac::GlobalConfig const*>::type(void)const` |
| 5 | 3 | `boost::any::holder<apac::GlobalConfig const*>::clone(void)const` |

### `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>`  — vtable at `0x0042abd0`, 56 bytes, 5 virtuals
base: `std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>`

| slot | vidx | target |
|---:|---:|---|
| 0 | - | `(offset-to-top = 0)` |
| 1 | - | `(typeinfo) `typeinfo for'std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>` |
| 2 | 0 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace()` |
| 3 | 1 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace()` |
| 4 | 2 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::_M_dispose(void)` |
| 5 | 3 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::_M_destroy(void)` |
| 6 | 4 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::_M_get_deleter(std::type_info const&)` |

---

## 4. Member layout, harvested from the decompilation

Every access through a typed pointer (`this` or a typed parameter) was collected and grouped by
byte offset. **These are observed accesses, not a declaration.** A field that is never touched
by the code in this library does not appear; padding is invisible; and where the same offset is
read at two widths both are listed (usually a `uint8` inside a word, or a decompiler artefact).
`n=` is how many distinct read/write sites were seen. Object *size* is only given where an
`operator new` in a constructor or factory pins it.

### `APACLRVQDecoder`
observed extent: `0` .. `0x18` (24 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x010 (16) | u32 | 84 |
| +0x014 (20) | u32 | 82 |

### `DRCConfiguration`
observed extent: `0` .. `0x169` (361 bytes), 68 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x004 (4) | u8 | 2 |
| +0x008 (8) | u32 | 3 |
| +0x00c (12) | u8, u16 | 4 |
| +0x00d (13) | u8 | 2 |
| +0x010 (16) | u32, u64 | 4 |
| +0x018 (24) | u32, u64 | 4 |
| +0x01c (28) | u32 | 3 |
| +0x020 (32) | u8, u16 | 2 |
| +0x021 (33) | u8, u16 | 3 |
| +0x022 (34) | u8 | 3 |
| +0x024 (36) | u32 | 4 |
| +0x028 (40) | u8, u16 | 2 |
| +0x029 (41) | u8 | 1 |
| +0x02c (44) | u32 | 2 |
| +0x030 (48) | u8, u16 | 2 |
| +0x031 (49) | u8 | 1 |
| +0x034 (52) | u32 | 2 |
| +0x038 (56) | u8, u16 | 2 |
| +0x039 (57) | u8 | 1 |
| +0x03c (60) | u32 | 2 |
| +0x040 (64) | u8, u16 | 7 |
| +0x041 (65) | u8 | 3 |
| +0x044 (68) | u8, u32, u64 | 4 |
| +0x048 (72) | u64 | 3 |
| +0x04c (76) | u64 | 1 |
| +0x050 (80) | u32 | 2 |
| +0x058 (88) | u64 | 4 |
| +0x060 (96) | u32 | 6 |
| +0x068 (104) | u64 | 2 |
| +0x070 (112) | u64 | 1 |
| +0x078 (120) | u64 | 1 |
| +0x080 (128) | u64 | 1 |
| +0x088 (136) | u64 | 1 |
| +0x090 (144) | u64 | 1 |
| +0x098 (152) | u64 | 1 |
| +0x0a0 (160) | u8 | 2 |
| +0x0a4 (164) | u32 | 2 |
| +0x0a8 (168) | u8, u16 | 4 |
| +0x0a9 (169) | u8 | 2 |
| +0x0aa (170) | u8 | 1 |
| +0x0ac (172) | u32 | 1 |
| +0x0b0 (176) | u32, u64 | 3 |
| +0x0b4 (180) | u8 | 2 |
| +0x0b5 (181) | u8 | 2 |
| +0x0b8 (184) | u64 | 1 |
| +0x0bf (191) | u8 | 1 |
| +0x0c0 (192) | u64 | 3 |
| +0x0c8 (200) | u64 | 3 |
| +0x0d0 (208) | u64 | 1 |
| +0x0d8 (216) | u64 | 3 |
| +0x0e0 (224) | u64 | 3 |
| +0x0e8 (232) | u64 | 1 |
| +0x0f0 (240) | u64 | 2 |
| +0x0f8 (248) | u64 | 2 |
| +0x100 (256) | u64 | 1 |
| +0x108 (264) | u64 | 2 |
| +0x110 (272) | u64 | 2 |
| +0x118 (280) | u64 | 1 |
| +0x120 (288) | u64 | 3 |
| +0x128 (296) | u64 | 3 |
| +0x130 (304) | u64 | 1 |
| +0x138 (312) | u64 | 2 |
| +0x140 (320) | u64 | 2 |
| +0x148 (328) | u64 | 1 |
| +0x150 (336) | u64 | 2 |
| +0x158 (344) | u64 | 2 |
| +0x160 (352) | u64 | 1 |
| +0x168 (360) | u8 | 1 |

### `SC_SceneComposition`
observed extent: `0` .. `0x1e1` (481 bytes), 19 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x001 (1) | u8 | 3 |
| +0x030 (48) | u8 | 2 |
| +0x038 (56) | u64 | 10 |
| +0x040 (64) | u64 | 8 |
| +0x050 (80) | u64 | 11 |
| +0x058 (88) | u64 | 9 |
| +0x068 (104) | u64 | 9 |
| +0x070 (112) | u64 | 7 |
| +0x080 (128) | u64 | 14 |
| +0x088 (136) | u64 | 13 |
| +0x098 (152) | u8 | 4 |
| +0x0a0 (160) | u64 | 7 |
| +0x0a8 (168) | u64 | 7 |
| +0x0b8 (184) | u8 | 4 |
| +0x130 (304) | u8 | 7 |
| +0x138 (312) | u64 | 19 |
| +0x140 (320) | u64 | 8 |
| +0x152 (338) | u16 | 1 |
| +0x1e0 (480) | u8 | 1 |

### `SC_SceneProcessor`
observed extent: `0` .. `0xe8` (232 bytes), 18 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u32 | 1 |
| +0x004 (4) | u16 | 5 |
| +0x006 (6) | u16 | 21 |
| +0x008 (8) | u64 | 5 |
| +0x020 (32) | u64 | 1 |
| +0x038 (56) | u64 | 1 |
| +0x040 (64) | u64 | 2 |
| +0x050 (80) | u64 | 13 |
| +0x058 (88) | u64 | 2 |
| +0x068 (104) | u64 | 6 |
| +0x080 (128) | u64 | 7 |
| +0x098 (152) | u64 | 12 |
| +0x0a8 (168) | u64 | 1 |
| +0x0b0 (176) | u32 | 3 |
| +0x0c0 (192) | u64 | 5 |
| +0x0c8 (200) | u64 | 2 |
| +0x0d8 (216) | u64 | 10 |
| +0x0e0 (224) | u64 | 1 |

### `apac__LSFTools`
observed extent: `0` .. `0xe8` (232 bytes), 21 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x00c (12) | u16 | 4 |
| +0x018 (24) | u64 | 2 |
| +0x020 (32) | u64 | 2 |
| +0x030 (48) | u64 | 1 |
| +0x038 (56) | u64 | 2 |
| +0x050 (80) | u64 | 8 |
| +0x058 (88) | u64 | 2 |
| +0x068 (104) | u16 | 21 |
| +0x06a (106) | u16 | 1 |
| +0x070 (112) | u32 | 1 |
| +0x078 (120) | u64 | 2 |
| +0x080 (128) | u64 | 3 |
| +0x090 (144) | u64 | 4 |
| +0x098 (152) | u64 | 4 |
| +0x0a8 (168) | u64 | 3 |
| +0x0b0 (176) | u64 | 3 |
| +0x0c0 (192) | u64 | 5 |
| +0x0c8 (200) | u64 | 3 |
| +0x0d8 (216) | u64 | 2 |
| +0x0e0 (224) | u64 | 3 |

### `apac__drc__DynRangeCompressor__APACDRCExtensionPayload`
observed extent: `0` .. `0x992` (2450 bytes), 61 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x012 (18) | u8 | 1 |
| +0x018 (24) | u32 | 1 |
| +0x01c (28) | u16 | 1 |
| +0x020 (32) | u32 | 1 |
| +0x024 (36) | u16 | 1 |
| +0x028 (40) | u32 | 1 |
| +0x02c (44) | u8 | 1 |
| +0x030 (48) | u64 | 1 |
| +0x038 (56) | u32 | 1 |
| +0x068 (104) | u64 | 1 |
| +0x070 (112) | u64 | 1 |
| +0x090 (144) | u8 | 1 |
| +0x098 (152) | u8 | 1 |
| +0x09c (156) | u64 | 1 |
| +0x0a4 (164) | u8 | 1 |
| +0x0a8 (168) | u64 | 1 |
| +0x0b0 (176) | u32 | 1 |
| +0x0b4 (180) | u8 | 1 |
| +0x0b8 (184) | u64 | 1 |
| +0x0c0 (192) | u64 | 1 |
| +0x110 (272) | u64 | 1 |
| +0x118 (280) | u8 | 1 |
| +0x11c (284) | u32 | 1 |
| +0x168 (360) | u8 | 1 |
| +0x16c (364) | u32 | 1 |
| +0x170 (368) | u64 | 1 |
| +0x190 (400) | u32 | 1 |
| +0x1b0 (432) | u32 | 1 |
| +0x1b4 (436) | u8 | 1 |
| +0x1b8 (440) | u16 | 1 |
| +0x1d8 (472) | u8 | 1 |
| +0x1e0 (480) | u16 | 1 |
| +0x218 (536) | u16 | 1 |
| +0x21c (540) | u32 | 1 |
| +0x250 (592) | u8 | 1 |
| +0x258 (600) | u8 | 1 |
| +0x260 (608) | u8 | 1 |
| +0x264 (612) | u32 | 1 |
| +0x268 (616) | u64 | 1 |
| +0x270 (624) | u64 | 1 |
| +0x278 (632) | u8 | 1 |
| +0x27c (636) | u32 | 1 |
| +0x280 (640) | u8 | 1 |
| +0x284 (644) | u32 | 1 |
| +0x2b8 (696) | u8 | 1 |
| +0x2ba (698) | u16 | 1 |
| +0x2c0 (704) | u8 | 1 |
| +0x2e0 (736) | u32 | 1 |
| +0x2e4 (740) | u8 | 1 |
| +0x2e8 (744) | u8 | 2 |
| +0x5c0 (1472) | u8 | 2 |
| +0x5c4 (1476) | u64 | 2 |
| +0x600 (1536) | u32 | 2 |
| +0x638 (1592) | u8 | 1 |
| +0x640 (1600) | u8 | 2 |
| +0x678 (1656) | u8 | 1 |
| +0x67a (1658) | u16 | 1 |
| +0x6b0 (1712) | u8 | 2 |
| +0x6b1 (1713) | u8 | 1 |
| +0x6b2 (1714) | u8 | 1 |
| +0x991 (2449) | u8 | 3 |

### `SC_CompositionSelectionGroup__Presets`
observed extent: `0` .. `0x30` (48 bytes), 27 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u8, u64 | 2 |
| +0x001 (1) | u8 | 3 |
| +0x002 (2) | u8 | 3 |
| +0x004 (4) | u16 | 2 |
| +0x006 (6) | u16 | 2 |
| +0x008 (8) | u8, u64 | 4 |
| +0x00a (10) | u16 | 2 |
| +0x00c (12) | u16 | 2 |
| +0x00e (14) | u8 | 3 |
| +0x010 (16) | u16, u64 | 3 |
| +0x012 (18) | u16 | 2 |
| +0x014 (20) | u8 | 3 |
| +0x016 (22) | u16 | 2 |
| +0x018 (24) | u16, u64 | 3 |
| +0x01a (26) | u8 | 2 |
| +0x01b (27) | u8 | 2 |
| +0x01c (28) | u16 | 2 |
| +0x01e (30) | u8 | 3 |
| +0x020 (32) | u16, u64 | 3 |
| +0x022 (34) | u8 | 3 |
| +0x024 (36) | u16 | 2 |
| +0x026 (38) | u8 | 3 |
| +0x028 (40) | u16, u64 | 3 |
| +0x02a (42) | u8 | 3 |
| +0x02c (44) | u16 | 2 |
| +0x02e (46) | u8 | 3 |
| +0x02f (47) | u8 | 2 |

### `APACICSInfo`
observed extent: `0` .. `0x300` (768 bytes), 8 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u8 | 2 |
| +0x009 (9) | u8 | 3 |
| +0x00a (10) | u8, u16 | 15 |
| +0x00b (11) | u8, u16 | 22 |
| +0x00c (12) | u8 | 9 |
| +0x010 (16) | u32 | 8 |
| +0x2f0 (752) | u64 | 1 |
| +0x2f8 (760) | u64 | 1 |

### `SceneCompositionSettings`
observed extent: `0` .. `0x68` (104 bytes), 11 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x002 (2) | u16 | 2 |
| +0x004 (4) | u16 | 3 |
| +0x008 (8) | u64 | 7 |
| +0x010 (16) | u64 | 5 |
| +0x020 (32) | u64 | 21 |
| +0x028 (40) | u64 | 7 |
| +0x038 (56) | u64 | 3 |
| +0x040 (64) | u64 | 5 |
| +0x050 (80) | u64 | 2 |
| +0x058 (88) | u64 | 4 |
| +0x060 (96) | u64 | 1 |

### `apac__drc__DynRangeCompressor`
observed extent: `0` .. `0x44` (68 bytes), 10 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u8 | 3 |
| +0x00a (10) | u16 | 3 |
| +0x010 (16) | u64 | 21 |
| +0x018 (24) | u32, u64 | 5 |
| +0x01c (28) | u32 | 2 |
| +0x020 (32) | u32 | 2 |
| +0x028 (40) | u64 | 17 |
| +0x030 (48) | u64 | 3 |
| +0x038 (56) | u8 | 1 |
| +0x03c (60) | u64 | 1 |

### `SC_SceneController`
observed extent: `0` .. `0x260` (608 bytes), 21 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x004 (4) | u16, u32 | 5 |
| +0x008 (8) | u32 | 2 |
| +0x00c (12) | u8 | 2 |
| +0x00d (13) | u8 | 2 |
| +0x010 (16) | u64 | 1 |
| +0x018 (24) | u64 | 1 |
| +0x028 (40) | u64 | 3 |
| +0x030 (48) | u64 | 1 |
| +0x078 (120) | u64 | 1 |
| +0x100 (256) | u64 | 1 |
| +0x108 (264) | u64 | 1 |
| +0x180 (384) | u64 | 2 |
| +0x188 (392) | u64 | 1 |
| +0x1b0 (432) | u64 | 3 |
| +0x1b8 (440) | u64 | 1 |
| +0x220 (544) | u64 | 1 |
| +0x228 (552) | u64 | 3 |
| +0x238 (568) | u64 | 2 |
| +0x240 (576) | u32 | 5 |
| +0x250 (592) | u64 | 3 |
| +0x258 (600) | u64 | 2 |

### `APACBWE2Decoder`
observed extent: `0` .. `0x68` (104 bytes), 10 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 11 |
| +0x018 (24) | u64 | 1 |
| +0x020 (32) | u32 | 3 |
| +0x030 (48) | u64 | 2 |
| +0x038 (56) | u64 | 7 |
| +0x040 (64) | u64 | 2 |
| +0x048 (72) | u64 | 6 |
| +0x050 (80) | u64 | 1 |
| +0x058 (88) | u64 | 2 |
| +0x060 (96) | u64 | 1 |

### `apac__GlobalConfig`
observed extent: `0` .. `0x110` (272 bytes), 19 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 3 |
| +0x018 (24) | u64 | 1 |
| +0x020 (32) | u16 | 3 |
| +0x022 (34) | u8 | 2 |
| +0x024 (36) | u32 | 2 |
| +0x029 (41) | u8 | 2 |
| +0x02b (43) | u8 | 2 |
| +0x038 (56) | u8 | 1 |
| +0x058 (88) | u8 | 1 |
| +0x060 (96) | u64 | 1 |
| +0x068 (104) | u64 | 1 |
| +0x0b8 (184) | u64 | 1 |
| +0x0c8 (200) | u64 | 5 |
| +0x0d0 (208) | u64 | 1 |
| +0x0e8 (232) | u64 | 1 |
| +0x0f8 (248) | u8 | 2 |
| +0x0f9 (249) | u8 | 1 |
| +0x100 (256) | u64 | 3 |
| +0x108 (264) | u64 | 3 |

### `APACSynthesisFilterBank`
observed extent: `0` .. `0xb8` (184 bytes), 10 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 3 |
| +0x010 (16) | u64 | 3 |
| +0x018 (24) | u64 | 4 |
| +0x020 (32) | u64 | 5 |
| +0x028 (40) | u32 | 7 |
| +0x060 (96) | u32 | 7 |
| +0x098 (152) | u64 | 2 |
| +0x0a0 (160) | u64 | 2 |
| +0x0a8 (168) | u64 | 1 |
| +0x0b0 (176) | u64 | 2 |

### `APACDecoder`
observed extent: `0` .. `0x1d0` (464 bytes), 12 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u8 | 1 |
| +0x00c (12) | u64 | 1 |
| +0x010 (16) | u32 | 3 |
| +0x188 (392) | u64 | 4 |
| +0x190 (400) | u64 | 5 |
| +0x198 (408) | u64 | 1 |
| +0x1a0 (416) | u64 | 2 |
| +0x1a8 (424) | u64 | 2 |
| +0x1b0 (432) | u64 | 1 |
| +0x1b8 (440) | u64 | 5 |
| +0x1c0 (448) | u64 | 8 |
| +0x1c8 (456) | u64 | 2 |

### `APACChannelPairElement`
observed extent: `0` .. `0x210` (528 bytes), 9 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x010 (16) | u64 | 7 |
| +0x018 (24) | u64 | 5 |
| +0x020 (32) | u64 | 4 |
| +0x080 (128) | u8 | 2 |
| +0x081 (129) | u8 | 2 |
| +0x13e (318) | u8 | 2 |
| +0x13f (319) | u8 | 2 |
| +0x200 (512) | u64 | 5 |
| +0x208 (520) | u64 | 6 |

### `SC_ComponentModifier`
observed extent: `0` .. `0x24` (36 bytes), 11 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u8, u64 | 3 |
| +0x001 (1) | u8 | 3 |
| +0x004 (4) | u32 | 2 |
| +0x008 (8) | u8, u64 | 4 |
| +0x00c (12) | u32 | 2 |
| +0x010 (16) | u8, u64 | 4 |
| +0x014 (20) | u32 | 2 |
| +0x018 (24) | u8, u64 | 4 |
| +0x01c (28) | u32 | 2 |
| +0x020 (32) | u8, u32 | 4 |
| +0x021 (33) | u8 | 2 |

### `SC_CompositionSelectionGroup`
observed extent: `0` .. `0xcc` (204 bytes), 9 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u16 | 1 |
| +0x008 (8) | u64 | 3 |
| +0x010 (16) | u64 | 4 |
| +0x020 (32) | u64 | 3 |
| +0x028 (40) | u64 | 4 |
| +0x038 (56) | u64 | 5 |
| +0x040 (64) | u64 | 4 |
| +0x050 (80) | u8 | 4 |
| +0x0ca (202) | u16 | 1 |

### `SC_AudioScenes`
observed extent: `0` .. `0x134` (308 bytes), 8 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x001 (1) | u8 | 3 |
| +0x002 (2) | u8 | 3 |
| +0x003 (3) | u8 | 4 |
| +0x004 (4) | u8 | 4 |
| +0x008 (8) | u64 | 4 |
| +0x010 (16) | u64 | 4 |
| +0x0b8 (184) | u8 | 4 |
| +0x132 (306) | u16 | 1 |

### `APACCoreDecoder`
observed extent: `0` .. `0x108` (264 bytes), 19 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 1 |
| +0x010 (16) | u64 | 2 |
| +0x018 (24) | u64 | 1 |
| +0x020 (32) | u64 | 1 |
| +0x028 (40) | u64 | 1 |
| +0x030 (48) | u64 | 1 |
| +0x0a8 (168) | u64 | 1 |
| +0x0b0 (176) | u64 | 1 |
| +0x0b8 (184) | u64 | 1 |
| +0x0c0 (192) | u64 | 1 |
| +0x0c8 (200) | u32 | 2 |
| +0x0cc (204) | u8 | 1 |
| +0x0d0 (208) | u64 | 3 |
| +0x0d8 (216) | u64 | 3 |
| +0x0e0 (224) | u64 | 1 |
| +0x0e8 (232) | u64 | 1 |
| +0x0f0 (240) | u64 | 1 |
| +0x0f8 (248) | u64 | 1 |
| +0x100 (256) | u64 | 2 |

### `apac__APACAncillaryFrameDecoder`
observed extent: `0` .. `0x40` (64 bytes), 7 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 11 |
| +0x010 (16) | u64 | 3 |
| +0x018 (24) | u64 | 3 |
| +0x020 (32) | u64 | 1 |
| +0x028 (40) | u64 | 3 |
| +0x030 (48) | u64 | 1 |
| +0x038 (56) | u64 | 3 |

### `MP4LoudnessInfo`
observed extent: `0` .. `0x20` (32 bytes), 9 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u8 | 1 |
| +0x001 (1) | u8 | 2 |
| +0x002 (2) | u8 | 3 |
| +0x003 (3) | u8 | 2 |
| +0x004 (4) | u16 | 4 |
| +0x006 (6) | u16 | 3 |
| +0x008 (8) | u32 | 2 |
| +0x00c (12) | u32 | 2 |
| +0x018 (24) | u64 | 6 |

### `SC_Tag`
observed extent: `0` .. `0x3c` (60 bytes), 6 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x008 (8) | u64 | 4 |
| +0x018 (24) | u8 | 4 |
| +0x020 (32) | u64 | 5 |
| +0x028 (40) | u64 | 7 |
| +0x03a (58) | u16 | 1 |

### `apac__LPMDCTFilter`
observed extent: `0` .. `0x34` (52 bytes), 4 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u16 | 11 |
| +0x00a (10) | u16 | 5 |
| +0x010 (16) | u64 | 2 |
| +0x030 (48) | u32 | 2 |

### `SC_PresetDescription`
observed extent: `0` .. `0xf1` (241 bytes), 6 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x0c8 (200) | u8 | 2 |
| +0x0d0 (208) | u8 | 3 |
| +0x0d1 (209) | u8 | 5 |
| +0x0d8 (216) | u64 | 3 |
| +0x0e0 (224) | u64 | 4 |
| +0x0f0 (240) | u8 | 3 |

### `SC_SceneComposition__SC_Category`
observed extent: `0` .. `0x39` (57 bytes), 6 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x008 (8) | u64 | 4 |
| +0x018 (24) | u8 | 4 |
| +0x020 (32) | u64 | 2 |
| +0x028 (40) | u64 | 5 |
| +0x038 (56) | u8 | 3 |

### `APACASPDecoder`
observed extent: `0` .. `0x1e0` (480 bytes), 9 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x010 (16) | u32 | 3 |
| +0x188 (392) | u64 | 1 |
| +0x190 (400) | u64 | 1 |
| +0x1a0 (416) | u64 | 1 |
| +0x1b8 (440) | u64 | 3 |
| +0x1c0 (448) | u64 | 1 |
| +0x1d0 (464) | u64 | 2 |
| +0x1d8 (472) | u64 | 3 |

### `apac__chan__CoreDecoderConfig`
observed extent: `0` .. `0x58` (88 bytes), 7 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x018 (24) | u64 | 2 |
| +0x02c (44) | u32 | 2 |
| +0x030 (48) | u64 | 3 |
| +0x038 (56) | u64 | 2 |
| +0x048 (72) | u64 | 5 |
| +0x050 (80) | u64 | 1 |

### `void`
observed extent: `0` .. `0x61` (97 bytes), 7 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u16, u32, u64 | 8 |
| +0x002 (2) | u16 | 1 |
| +0x004 (4) | u32 | 1 |
| +0x008 (8) | u64 | 2 |
| +0x030 (48) | u64 | 1 |
| +0x038 (56) | u64 | 1 |
| +0x060 (96) | u8 | 1 |

### `CD_AudioSceneControlData__SceneCompositionData`
observed extent: `0` .. `0x48` (72 bytes), 8 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x001 (1) | u8 | 2 |
| +0x002 (2) | u8 | 1 |
| +0x003 (3) | u8 | 1 |
| +0x004 (4) | u8 | 3 |
| +0x008 (8) | u64 | 3 |
| +0x020 (32) | u64 | 1 |
| +0x038 (56) | u8 | 3 |
| +0x040 (64) | u64 | 1 |

### `SC_PresetDescription__SC_PresetCharacteristics`
observed extent: `0` .. `0x24` (36 bytes), 4 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u8 | 1 |
| +0x008 (8) | u64 | 4 |
| +0x018 (24) | u64 | 4 |
| +0x020 (32) | u32 | 6 |

### `apac__ASCAudioOutData`
observed extent: `0` .. `0x48` (72 bytes), 9 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x008 (8) | u32 | 1 |
| +0x010 (16) | u64 | 2 |
| +0x018 (24) | u64 | 2 |
| +0x020 (32) | u64 | 2 |
| +0x028 (40) | u32 | 1 |
| +0x030 (48) | u64 | 2 |
| +0x038 (56) | u64 | 2 |
| +0x040 (64) | u64 | 2 |

### `__int64`
observed extent: `0` .. `0x851` (2129 bytes), 10 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x810 (2064) | u32 | 1 |
| +0x818 (2072) | u32 | 1 |
| +0x81c (2076) | u16 | 3 |
| +0x824 (2084) | u32 | 1 |
| +0x838 (2104) | u32 | 1 |
| +0x83c (2108) | u32 | 1 |
| +0x840 (2112) | u32 | 1 |
| +0x84c (2124) | u16 | 2 |
| +0x84e (2126) | u16 | 1 |
| +0x850 (2128) | u8 | 2 |

### `SC_LanguageSpecificItem`
observed extent: `0` .. `0x4a` (74 bytes), 5 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x008 (8) | u64 | 4 |
| +0x040 (64) | u8 | 2 |
| +0x048 (72) | u8 | 3 |
| +0x049 (73) | u8 | 3 |

### `BWELPC`
observed extent: `0` .. `0xf8` (248 bytes), 7 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x030 (48) | u64 | 1 |
| +0x068 (104) | u16 | 1 |
| +0x0c0 (192) | u64 | 2 |
| +0x0d8 (216) | u64 | 3 |
| +0x0e0 (224) | u64 | 1 |
| +0x0f0 (240) | u64 | 4 |

### `ContentOutBuffer`
observed extent: `0` .. `0x30` (48 bytes), 4 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 4 |
| +0x010 (16) | u64 | 5 |
| +0x020 (32) | u64 | 2 |
| +0x028 (40) | u64 | 2 |

### `SceneCompositionSettings__CategorySettings`
observed extent: `0` .. `0x34` (52 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x018 (24) | u64 | 4 |
| +0x028 (40) | u32 | 4 |
| +0x030 (48) | u32 | 4 |

### `SC_Extension`
observed extent: `0` .. `0x38` (56 bytes), 6 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u8 | 1 |
| +0x008 (8) | u64 | 1 |
| +0x010 (16) | u64 | 3 |
| +0x020 (32) | u64 | 2 |
| +0x028 (40) | u64 | 4 |
| +0x030 (48) | u64 | 1 |

### `SC_LanguageSelectionSet`
observed extent: `0` .. `0x1b` (27 bytes), 4 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x008 (8) | u64 | 4 |
| +0x018 (24) | u16 | 3 |
| +0x01a (26) | u8 | 4 |

### `apac__LPCBase`
observed extent: `0` .. `0x50` (80 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u16 | 6 |
| +0x018 (24) | u64 | 5 |
| +0x048 (72) | u64 | 1 |

### `DCT_IV`
observed extent: `0` .. `0x30` (48 bytes), 6 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u32 | 3 |
| +0x008 (8) | u64 | 2 |
| +0x010 (16) | u32 | 1 |
| +0x018 (24) | u64 | 1 |
| +0x020 (32) | u64 | 1 |
| +0x028 (40) | u64 | 4 |

### `APACSyntacticElement`
observed extent: `0` .. `0x20` (32 bytes), 4 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 4 |
| +0x008 (8) | u32, u64 | 5 |
| +0x010 (16) | u64 | 1 |
| +0x018 (24) | u64 | 1 |

### `APACCoreLBRDecoder`
observed extent: `0` .. `0x110` (272 bytes), 5 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x0c8 (200) | u32 | 1 |
| +0x0cc (204) | u8 | 1 |
| +0x0d0 (208) | u64 | 1 |
| +0x0d8 (216) | u64 | 1 |
| +0x108 (264) | u64 | 7 |

### `apac__scene__BCP47LanguageString`
observed extent: `0` .. `0x20` (32 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 1 |
| +0x010 (16) | u64 | 9 |
| +0x018 (24) | u64 | 1 |

### `apac__AncillaryConfig`
observed extent: `0` .. `0x28` (40 bytes), 4 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u8 | 2 |
| +0x010 (16) | u64 | 4 |
| +0x018 (24) | u64 | 2 |
| +0x020 (32) | u64 | 2 |

### `APACTNSData`
observed extent: `0` .. `0xbe` (190 bytes), 5 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x001 (1) | u8 | 5 |
| +0x0ba (186) | u8, u16 | 2 |
| +0x0bb (187) | u8 | 1 |
| +0x0bc (188) | u8 | 1 |
| +0x0bd (189) | u8 | 1 |

### `SC_NonLanguageItem`
observed extent: `0` .. `0x19` (25 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x008 (8) | u64 | 4 |
| +0x018 (24) | u8 | 4 |

### `APACSingleChannelElement`
observed extent: `0` .. `0x2a` (42 bytes), 5 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x010 (16) | u64 | 3 |
| +0x018 (24) | u64 | 3 |
| +0x020 (32) | u64 | 1 |
| +0x028 (40) | u8 | 1 |
| +0x029 (41) | u8 | 1 |

### `apac__ASCChannelMap`
observed extent: `0` .. `0x18` (24 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 3 |
| +0x008 (8) | u64 | 5 |
| +0x010 (16) | u64 | 1 |

### `apac__LPCodeBook`
observed extent: `0` .. `0x38` (56 bytes), 5 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u16 | 1 |
| +0x002 (2) | u16 | 2 |
| +0x008 (8) | u64 | 1 |
| +0x028 (40) | u32 | 1 |
| +0x030 (48) | u64 | 4 |

### `MultiRadixRealFFT`
observed extent: `0` .. `0x20` (32 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u32 | 5 |
| +0x010 (16) | u64 | 1 |
| +0x018 (24) | u64 | 2 |

### `MP4LoudnessMeasure`
observed extent: `0` .. `0xc` (12 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u32 | 7 |

### `APACCACDecoder`
observed extent: `0` .. `0x58` (88 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x020 (32) | u64 | 2 |
| +0x038 (56) | u64 | 3 |
| +0x050 (80) | u64 | 1 |

### `apac__ChanDecElement`
observed extent: `0` .. `0x18` (24 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x010 (16) | u64 | 5 |

### `APACLFEElement`
observed extent: `0` .. `0x20` (32 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x010 (16) | u64 | 3 |
| +0x018 (24) | u64 | 2 |

### `APACChannelRemapper`
observed extent: `0` .. `0x18` (24 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 2 |
| +0x010 (16) | u64 | 2 |

### `apac__ProfileLevelConfig`
observed extent: `0` .. `0x3` (3 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u16 | 2 |
| +0x002 (2) | u8 | 2 |

### `apac__ElementInstance`
observed extent: `0` .. `0x10` (16 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u32 | 2 |
| +0x008 (8) | u64 | 2 |

### `apac__ParamList`
observed extent: `0` .. `0x18` (24 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x010 (16) | u64 | 3 |

### `APACExtFrameDecoder`
observed extent: `0` .. `0x18` (24 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 2 |
| +0x010 (16) | u64 | 1 |

### `APACInstanceConfig`
observed extent: `0` .. `0x20` (32 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x004 (4) | u32 | 1 |
| +0x010 (16) | u64 | 1 |
| +0x018 (24) | u64 | 1 |

### `MDCT`
observed extent: `0` .. `0x34` (52 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x028 (40) | u64 | 1 |
| +0x030 (48) | u32 | 2 |

### `MultiRadixFFT`
observed extent: `0` .. `0x20` (32 bytes), 3 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u32 | 1 |
| +0x010 (16) | u64 | 1 |
| +0x018 (24) | u64 | 1 |

### `apac__APACDRCDecoder`
observed extent: `0` .. `0x10` (16 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 2 |

### `apac__TrimmingData`
observed extent: `0` .. `0xc` (12 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x004 (4) | u32 | 1 |
| +0x008 (8) | u32 | 1 |

### `APACSpectralData`
observed extent: `0` .. `0x10` (16 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x008 (8) | u64 | 1 |

### `boost__any__holder`
observed extent: `0` .. `0x10` (16 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 2 |

### `APACLRVQDecoder__LPMDCT_LONG`
observed extent: `0` .. `0x18` (24 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x010 (16) | u64 | 2 |

### `APACLRVQDecoder__LPMDCT_LTP`
observed extent: `0` .. `0x18` (24 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x010 (16) | u64 | 2 |

### `apac__SFBOffsets`
observed extent: `0` .. `0x2` (2 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u16 | 2 |

### `CacQuantizer`
observed extent: `0` .. `0x10` (16 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x000 (0) | u64 | 1 |
| +0x008 (8) | u64 | 1 |

### `_QWORD`
observed extent: `0` .. `0x851` (2129 bytes), 2 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x844 (2116) | u64 | 1 |
| +0x850 (2128) | u8 | 1 |

### `apac__APACAudioSceneDecoder`
observed extent: `0` .. `0x10` (16 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 1 |

### `apac__GlobalConfig__ASCConfigData`
observed extent: `0` .. `0x10` (16 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 1 |

### `apac__TCEConfig`
observed extent: `0` .. `0xc` (12 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u32 | 1 |

### `apac__lbr__DecoderConfig`
observed extent: `0` .. `0x61` (97 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x060 (96) | u8 | 1 |

### `apac__GlobalConfig__CodecConfigData`
observed extent: `0` .. `0x10` (16 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x008 (8) | u64 | 1 |

### `apac__ExtensionConfig`
observed extent: `0` .. `0x18` (24 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x010 (16) | u64 | 1 |

### `apac__BaseASCConfig`
observed extent: `0` .. `0x38` (56 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x030 (48) | u64 | 1 |

### `APACIndividualChannelStream`
observed extent: `0` .. `0x18` (24 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x010 (16) | u64 | 1 |

### `apac__scene__SC_Language`
observed extent: `0` .. `0x29` (41 bytes), 1 distinct offsets

| offset | width(s) | n |
|---:|---|---:|
| +0x028 (40) | u8 | 1 |

---

## 5. Method list

### `APACASPDecoder` — 5 functions, 1172 bytes

| addr | size | signature |
|---|---:|---|
| `0x00072000` | 296 | `APACASPDecoder::DecodeFrame(unsigned char const*, unsigned int, apac::FrameOutRecord&)` |
| `0x00072130` | 416 | `APACASPDecoder::DecodeOneFrame(TBitstreamReader<unsigned int>&)` |
| `0x000722d0` | 228 | `APACASPDecoder::Initialize(apac::GlobalConfig const&)` |
| `0x000723c0` | 112 | `APACASPDecoder::~APACASPDecoder()` |
| | | `APACASPDecoder::~APACASPDecoder()` (alias, same code) |
| `0x00072430` | 120 | `APACASPDecoder::~APACASPDecoder()` |

### `APACBWE2Decoder` — 7 functions, 6592 bytes

| addr | size | signature |
|---|---:|---|
| `0x00078490` | 1484 | `APACBWE2Decoder::DecodeFrame(APACICSInfo const&, APACSpectralData&, unsigned int)` |
| `0x00078a60` | 1896 | `APACBWE2Decoder::Deserialize(TBitstreamReader<unsigned int>&, unsigned int, APACSyntacticElement&)` |
| `0x000791d0` | 1180 | `APACBWE2Decoder::InitInterFrameMemory(unsigned int)` |
| `0x00079670` | 192 | `APACBWE2Decoder::Initialize(unsigned int)` |
| `0x00079730` | 92 | `APACBWE2Decoder::GetActiveFlag(unsigned int) const` |
| `0x000799c0` | 888 | `APACBWE2Decoder::~APACBWE2Decoder()` |
| | | `APACBWE2Decoder::~APACBWE2Decoder()` (alias, same code) |
| `0x00079ff0` | 860 | `APACBWE2Decoder::~APACBWE2Decoder()` |

### `APACCACDecoder` — 2 functions, 1824 bytes

| addr | size | signature |
|---|---:|---|
| `0x0007b5a0` | 792 | `APACCACDecoder::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&)` |
| `0x0007b8c0` | 1032 | `APACCACDecoder::ProcessCac(APACICSInfo const&, APACIndividualChannelStream&, APACIndividualChannelStream&) const` |

### `APACChannelPairElement` — 8 functions, 3048 bytes

| addr | size | signature |
|---|---:|---|
| `0x000ac600` | 712 | `APACChannelPairElement::DeserializeLrvqFrame(TBitstreamReader<unsigned int>&)` |
| `0x000ac8d0` | 844 | `APACChannelPairElement::DeserializeSQFrame(TBitstreamReader<unsigned int>&)` |
| `0x000ad460` | 96 | `APACChannelPairElement::ChannelData(bool)` |
| `0x000ad4c0` | 88 | `APACChannelPairElement::GetICSInfo(bool)` |
| `0x000ad680` | 272 | `APACChannelPairElement::Reset()` |
| `0x000ad9b0` | 376 | `APACChannelPairElement::~APACChannelPairElement()` |
| `0x000adcf0` | 432 | `APACChannelPairElement::~APACChannelPairElement()` |
| | | `APACChannelPairElement::~APACChannelPairElement()` (alias, same code) |
| `0x000adea0` | 228 | `APACChannelPairElement::Deserialize(TBitstreamReader<unsigned int>&)` |

### `APACChannelRemapper` — 4 functions, 488 bytes

| addr | size | signature |
|---|---:|---|
| `0x0007bcd0` | 176 | `APACChannelRemapper::Process(std::vector<float*, std::allocator<float*> >&)` |
| `0x0007bd80` | 84 | `APACChannelRemapper::Initialize(std::vector<unsigned char, std::allocator<unsigned char> > const&)` |
| `0x0007bde0` | 116 | `APACChannelRemapper::~APACChannelRemapper()` |
| | | `APACChannelRemapper::~APACChannelRemapper()` (alias, same code) |
| `0x0007be60` | 112 | `APACChannelRemapper::~APACChannelRemapper()` |

### `APACCoreDecoder` — 11 functions, 3596 bytes

| addr | size | signature |
|---|---:|---|
| `0x0007cf50` | 68 | `APACCoreDecoder::Reset()` |
| `0x0007cfa0` | 120 | `APACCoreDecoder::Deserialize(unsigned char const*, unsigned int)` |
| `0x0007d020` | 352 | `APACCoreDecoder::~APACCoreDecoder()` |
| | | `APACCoreDecoder::~APACCoreDecoder()` (alias, same code) |
| `0x0007d180` | 84 | `APACCoreDecoder::~APACCoreDecoder()` |
| `0x0007d1e0` | 1476 | `APACCoreDecoder::Initialize(apac::ParamList const&, apac::observer_ptr<apac::IASCConfig>)` |
| `0x0007d7b0` | 256 | `APACCoreDecoder::APACCoreDecoder()` |
| | | `APACCoreDecoder::APACCoreDecoder()` (alias, same code) |
| `0x0007d8b0` | 148 | `APACCoreDecoder::DeserializeExtTools(TBitstreamReader<unsigned int>&)` |
| `0x0007d950` | 468 | `APACCoreDecoder::Deserialize(TBitstreamReader<unsigned int>&, bool)` |
| `0x0007db30` | 468 | `APACCoreDecoder::DecodeAPACFrame(TBitstreamReader<unsigned int>&, apac::FrameType, std::vector<float*, std::allocator<float*> >&)` |
| `0x0007dd10` | 88 | `APACCoreDecoder::GetBlockSize() const` |
| `0x0007dd70` | 68 | `APACCoreDecoder::GetNumChannels() const` |

### `APACCoreLBRDecoder` — 4 functions, 1496 bytes

| addr | size | signature |
|---|---:|---|
| `0x0007e440` | 944 | `APACCoreLBRDecoder::Deserialize(TBitstreamReader<unsigned int>&, bool)` |
| `0x0007e7f0` | 304 | `APACCoreLBRDecoder::Initialize(apac::ParamList const&, apac::observer_ptr<apac::IASCConfig>)` |
| `0x0007e920` | 120 | `APACCoreLBRDecoder::~APACCoreLBRDecoder()` |
| | | `APACCoreLBRDecoder::~APACCoreLBRDecoder()` (alias, same code) |
| `0x0007e9a0` | 128 | `APACCoreLBRDecoder::~APACCoreLBRDecoder()` |

### `APACDecoder` — 9 functions, 5308 bytes

| addr | size | signature |
|---|---:|---|
| `0x00093cc0` | 128 | `APACDecoder::Reset()` |
| `0x00093d40` | 68 | `APACDecoder::SetParameter(int, boost::any const&)` |
| `0x00093d90` | 192 | `APACDecoder::CreateDecoderElement(apac::ASCType)` |
| `0x00093e50` | 564 | `APACDecoder::DecodeFrame(unsigned char const*, unsigned int, apac::FrameOutRecord&)` |
| `0x00094090` | 292 | `APACDecoder::GetParameter(int, boost::any&) const` |
| `0x000941c0` | 1016 | `APACDecoder::~APACDecoder()` |
| | | `APACDecoder::~APACDecoder()` (alias, same code) |
| `0x000945c0` | 84 | `APACDecoder::~APACDecoder()` |
| `0x00094620` | 128 | `APACDecoder::APACDecoder()` |
| | | `APACDecoder::APACDecoder()` (alias, same code) |
| `0x000946a0` | 2836 | `APACDecoder::Initialize(apac::GlobalConfig const&)` |

### `APACExtFrameDecoder` — 2 functions, 524 bytes

| addr | size | signature |
|---|---:|---|
| `0x00096070` | 148 | `APACExtFrameDecoder::DecodeFrame(TBitstreamReader<unsigned int>&)` |
| `0x00096110` | 376 | `APACExtFrameDecoder::APACExtFrameDecoder(apac::GlobalConfig const&)` |
| | | `APACExtFrameDecoder::APACExtFrameDecoder(apac::GlobalConfig const&)` (alias, same code) |

### `APACICSInfo` — 4 functions, 944 bytes

| addr | size | signature |
|---|---:|---|
| `0x00099e60` | 736 | `APACICSInfo::Deserialize(TBitstreamReader<unsigned int>&, apac::CodingType)` |
| `0x0009a6e0` | 64 | `APACICSInfo::~APACICSInfo()` |
| | | `APACICSInfo::~APACICSInfo()` (alias, same code) |
| `0x0009a720` | 80 | `APACICSInfo::Reset()` |
| `0x0009a770` | 64 | `APACICSInfo::~APACICSInfo()` |

### `APACIndividualChannelStream` — 3 functions, 392 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009a610` | 200 | `APACIndividualChannelStream::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo&, bool)` |
| `0x0009a7b0` | 96 | `APACIndividualChannelStream::~APACIndividualChannelStream()` |
| | | `APACIndividualChannelStream::~APACIndividualChannelStream()` (alias, same code) |
| `0x0009a810` | 96 | `APACIndividualChannelStream::~APACIndividualChannelStream()` |

### `APACInstanceConfig` — 1 functions, 224 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009a870` | 224 | `APACInstanceConfig::Create(unsigned int, unsigned int)` |

### `APACLFEElement` — 4 functions, 988 bytes

| addr | size | signature |
|---|---:|---|
| `0x000ac2a0` | 324 | `APACLFEElement::Deserialize(TBitstreamReader<unsigned int>&)` |
| `0x000ad5d0` | 176 | `APACLFEElement::Reset()` |
| `0x000ad8a0` | 272 | `APACLFEElement::~APACLFEElement()` |
| | | `APACLFEElement::~APACLFEElement()` (alias, same code) |
| `0x000adc10` | 216 | `APACLFEElement::~APACLFEElement()` |

### `APACLRVQDecoder` — 7 functions, 7528 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009a950` | 184 | `APACLRVQDecoder::dequantizeLrvqGain(unsigned short, unsigned short, float*)` |
| `0x0009aa10` | 1232 | `APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> const&, apac::aligned_array<float, 1024ul>&)` |
| `0x0009aee0` | 536 | `APACLRVQDecoder::ltpToMdctSpectralEnvelope(unsigned short, unsigned short, apac::aligned_array<float, 1024ul>&)` |
| `0x0009b100` | 3264 | `APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData&, apac::aligned_array<float, 1024ul>&, apac::aligned_array<float, 1024ul>&, apac::aligned_array<float, 256ul>&, apac::aligned_array<short, 64ul>&, unsigned short*)` |
| `0x0009bdc0` | 408 | `APACLRVQDecoder::SpectrumDeserializeSCE(TBitstreamReader<unsigned int>&, APACICSInfo&, APACIndividualChannelStream&)` |
| `0x0009bf60` | 644 | `APACLRVQDecoder::SpectrumDeserializeCPE(TBitstreamReader<unsigned int>&, APACICSInfo&, APACICSInfo&, APACIndividualChannelStream&, APACIndividualChannelStream&)` |
| `0x0009c1f0` | 1260 | `APACLRVQDecoder::SpectrumDeserializeLFE(TBitstreamReader<unsigned int>&, APACICSInfo&, APACIndividualChannelStream&)` |

### `APACLRVQDecoder::LPMDCT_LONG` — 2 functions, 216 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009c830` | 100 | `APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG()` |
| | | `APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG()` (alias, same code) |
| `0x0009c8a0` | 116 | `APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG()` |

### `APACLRVQDecoder::LPMDCT_LTP` — 2 functions, 216 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009c920` | 100 | `APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP()` |
| | | `APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP()` (alias, same code) |
| `0x0009c990` | 116 | `APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP()` |

### `APACLRVQDecoder::LRVQLPC` — 4 functions, 944 bytes

| addr | size | signature |
|---|---:|---|
| `0x00070c6c` | 68 | `APACLRVQDecoder::LRVQLPC::Initialize(unsigned short)` |
| `0x00070cb0` | 68 | `APACLRVQDecoder::LRVQLPC::Initialize()` |
| `0x0009ca10` | 416 | `APACLRVQDecoder::LRVQLPC::~LRVQLPC()` |
| | | `APACLRVQDecoder::LRVQLPC::~LRVQLPC()` (alias, same code) |
| `0x0009cbb0` | 392 | `APACLRVQDecoder::LRVQLPC::~LRVQLPC()` |

### `APACScaleFactorData` — 1 functions, 1224 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009a140` | 1224 | `APACScaleFactorData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&)` |

### `APACSingleChannelElement` — 4 functions, 1192 bytes

| addr | size | signature |
|---|---:|---|
| `0x000ac3f0` | 528 | `APACSingleChannelElement::Deserialize(TBitstreamReader<unsigned int>&)` |
| `0x000ad520` | 176 | `APACSingleChannelElement::Reset()` |
| `0x000ad790` | 272 | `APACSingleChannelElement::~APACSingleChannelElement()` |
| | | `APACSingleChannelElement::~APACSingleChannelElement()` (alias, same code) |
| `0x000adb30` | 216 | `APACSingleChannelElement::~APACSingleChannelElement()` |

### `APACSpectralData` — 1 functions, 3820 bytes

| addr | size | signature |
|---|---:|---|
| `0x000ab3b0` | 3820 | `APACSpectralData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&, APACScaleFactorData const&, apac::SFBOffsets const&)` |

### `APACSyntacticElement` — 7 functions, 2556 bytes

| addr | size | signature |
|---|---:|---|
| `0x000acc20` | 176 | `APACSyntacticElement::DestroyList(std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >&)` |
| `0x000accd0` | 132 | `APACSyntacticElement::NumberOfChannels(std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > > const&)` |
| `0x000acd60` | 1620 | `APACSyntacticElement::CreateList(std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >&, APACInstanceConfig&, apac::observer_ptr<apac::IASCConfig>)` |
| `0x000ad3c0` | 72 | `APACSyntacticElement::ChannelData(bool)` |
| `0x000ad410` | 68 | `APACSyntacticElement::GetICSInfo(bool)` |
| `0x000adf90` | 272 | `APACSyntacticElement::~APACSyntacticElement()` |
| | | `APACSyntacticElement::~APACSyntacticElement()` (alias, same code) |
| `0x000ae0a0` | 216 | `APACSyntacticElement::~APACSyntacticElement()` |

### `APACSynthesisFilterBank` — 5 functions, 1736 bytes

| addr | size | signature |
|---|---:|---|
| `0x000ae530` | 124 | `APACSynthesisFilterBank::Reset()` |
| `0x000ae5b0` | 964 | `APACSynthesisFilterBank::FrequencyToTimeInPlace(unsigned int, float*, unsigned int, unsigned int)` |
| `0x000ae980` | 408 | `APACSynthesisFilterBank::Initialize(unsigned int, unsigned int)` |
| `0x000aeb20` | 116 | `APACSynthesisFilterBank::~APACSynthesisFilterBank()` |
| | | `APACSynthesisFilterBank::~APACSynthesisFilterBank()` (alias, same code) |
| `0x000aeba0` | 124 | `APACSynthesisFilterBank::~APACSynthesisFilterBank()` |

### `APACTNSData` — 3 functions, 2188 bytes

| addr | size | signature |
|---|---:|---|
| `0x000aed10` | 168 | `APACTNSData::APACTNSData(APACInstanceConfig&)` |
| | | `APACTNSData::APACTNSData(APACInstanceConfig&)` (alias, same code) |
| `0x000aedc0` | 1140 | `APACTNSData::ParseTNSData(TBitstreamReader<unsigned int>&, APACICSInfo const&)` |
| `0x000af240` | 880 | `APACTNSData::Apply(APACICSInfo const&, apac::SFBOffsets const&, APACSpectralData&) const` |

### `BWELPC` — 3 functions, 688 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b0d70` | 160 | `BWELPC::LPCToMDCTSpectrum(float const*, float*, unsigned int)` |
| `0x000b0e10` | 328 | `BWELPC::MDCTCoefficientsToLPC(float const*, unsigned int, unsigned int, unsigned int)` |
| `0x000b0f60` | 200 | `BWELPC::DecodeLSFsToLPCs(unsigned short const*)` |

### `CacDecoder` — 2 functions, 144 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b2070` | 72 | `CacDecoder::RepeatCodeTableDec() const` |
| `0x000b20c0` | 72 | `CacDecoder::GainIndexCodeTableDec() const` |

### `CacQuantizer` — 2 functions, 600 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b2110` | 188 | `CacQuantizer::GainTableEntry(unsigned int) const` |
| `0x000b21d0` | 412 | `CacQuantizer::Initialize()` |

### `ContentOutBuffer` — 5 functions, 2044 bytes

| addr | size | signature |
|---|---:|---|
| `0x00095980` | 208 | `ContentOutBuffer::~ContentOutBuffer()` |
| `0x00095a50` | 220 | `ContentOutBuffer::~ContentOutBuffer()` |
| | | `ContentOutBuffer::~ContentOutBuffer()` (alias, same code) |
| `0x000b2500` | 272 | `ContentOutBuffer::SetAudioData(apac::AudioDataType, unsigned int, __gnu_cxx::__normal_iterator<float* const*, std::vector<float*, std::allocator<float*> > >, __gnu_cxx::__normal_iterator<float* const*, std::vector<float*, std::allocator<float*> > >, unsigned int)` |
| `0x000b2610` | 312 | `ContentOutBuffer::GetAudioData(std::vector<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >, std::allocator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > > > const&, apac::FrameOutRecord&)` |
| `0x000b28f0` | 1032 | `ContentOutBuffer::Initialize(apac::GlobalConfig const&)` |

### `DCT_IV` — 5 functions, 1564 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b6e80` | 560 | `DCT_IV::PreModulation(float const*, DSPSplitComplex*, float) const` |
| `0x000b70b0` | 656 | `DCT_IV::PostModulationExtend(DSPSplitComplex const*, float*, float*) const` |
| `0x000b7340` | 100 | `DCT_IV::DCT_IV()` |
| | | `DCT_IV::DCT_IV()` (alias, same code) |
| `0x000b73b0` | 88 | `DCT_IV::~DCT_IV()` |
| | | `DCT_IV::~DCT_IV()` (alias, same code) |
| `0x000b7410` | 160 | `DCT_IV::Initialize(unsigned int)` |

### `DRCConfiguration` — 20 functions, 3288 bytes

| addr | size | signature |
|---|---:|---|
| `0x00082850` | 356 | `DRCConfiguration::~DRCConfiguration()` |
| | | `DRCConfiguration::~DRCConfiguration()` (alias, same code) |
| `0x000b3420` | 64 | `DRCConfiguration::LogStatus() const` |
| `0x000b3460` | 104 | `DRCConfiguration::SetCompressionMode(unsigned int)` |
| `0x000b34d0` | 72 | `DRCConfiguration::SetTargetLevelChanged(unsigned char)` |
| `0x000b3520` | 148 | `DRCConfiguration::SetTargetLevel(float)` |
| `0x000b35c0` | 72 | `DRCConfiguration::SetCutBoostChanged(unsigned char)` |
| `0x000b3610` | 156 | `DRCConfiguration::SetCutFactor(float)` |
| `0x000b36b0` | 156 | `DRCConfiguration::SetBoostFactor(float)` |
| `0x000b3750` | 72 | `DRCConfiguration::SetSensitivityChanged(unsigned char)` |
| `0x000b37a0` | 72 | `DRCConfiguration::SetPlaybackGainChanged(unsigned char)` |
| `0x000b37f0` | 72 | `DRCConfiguration::SetAudioScenePresetParamsChanged(unsigned char)` |
| `0x000b3840` | 72 | `DRCConfiguration::SetLoudnessEqStrengthChanged(unsigned char)` |
| `0x000b3890` | 72 | `DRCConfiguration::SetUniDRCCompressionProfileChanged(unsigned char)` |
| `0x000b38e0` | 132 | `DRCConfiguration::SetUniDRCCompressionProfile(unsigned int)` |
| `0x000b3970` | 108 | `DRCConfiguration::SetUniDrcDelay(unsigned int)` |
| `0x000b39e0` | 72 | `DRCConfiguration::SetUniDrcDelayChanged(unsigned char)` |
| `0x000b3a30` | 72 | `DRCConfiguration::SetUniDrcDelayIsSetOnCodec(unsigned char)` |
| `0x000b3a80` | 80 | `DRCConfiguration::SetAlbumMode(unsigned char)` |
| `0x000b3ad0` | 908 | `DRCConfiguration::SetAudioScenePresetParams(unsigned char, std::vector<bool, std::allocator<bool> > const&, std::vector<float, std::allocator<float> > const&, std::vector<float, std::allocator<float> > const&)` |
| `0x000b3e60` | 428 | `DRCConfiguration::DRCConfiguration()` |
| | | `DRCConfiguration::DRCConfiguration()` (alias, same code) |

### `MDCT` — 4 functions, 560 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b74b0` | 64 | `MDCT::MDCT()` |
| | | `MDCT::MDCT()` (alias, same code) |
| `0x000b74f0` | 64 | `MDCT::~MDCT()` |
| | | `MDCT::~MDCT()` (alias, same code) |
| `0x000b7530` | 132 | `MDCT::Initialize(unsigned int, float)` |
| `0x000b75c0` | 300 | `MDCT::InverseInPlaceTransform(float*, float*)` |

### `MP4LoudnessInfo` — 1 functions, 1948 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b7cd0` | 1948 | `MP4LoudnessInfo::Deserialize(TBitstreamReader<unsigned int>&, bool)` |

### `MP4LoudnessMeasure` — 1 functions, 788 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b77b0` | 788 | `MP4LoudnessMeasure::Deserialize(TBitstreamReader<unsigned int>&, bool)` |

### `MultiRadixFFT` — 1 functions, 156 bytes

| addr | size | signature |
|---|---:|---|
| `0x00103330` | 156 | `MultiRadixFFT::CplxOutOfPlaceTransform(DSPSplitComplex const*, DSPSplitComplex*, int)` |

### `MultiRadixRealFFT` — 3 functions, 612 bytes

| addr | size | signature |
|---|---:|---|
| `0x001033d0` | 244 | `MultiRadixRealFFT::RealInPlaceTransform(DSPSplitComplex*, int)` |
| `0x001034d0` | 124 | `MultiRadixRealFFT::RealOutOfPlaceForwardTransform(float const*, DSPSplitComplex*)` |
| `0x00103550` | 244 | `MultiRadixRealFFT::RealOutOfPlaceInverseTransform(DSPSplitComplex const*, float*)` |

### `SC_AudioScenes` — 4 functions, 2844 bytes

| addr | size | signature |
|---|---:|---|
| `0x00073840` | 1216 | `SC_AudioScenes::~SC_AudioScenes()` |
| | | `SC_AudioScenes::~SC_AudioScenes()` (alias, same code) |
| `0x000a55c0` | 344 | `SC_AudioScenes::operator==(SC_AudioScenes const&) const` |
| `0x000a5720` | 280 | `SC_AudioScenes::CheckConformance() const` |
| `0x000a8e60` | 1004 | `SC_AudioScenes::Deserialize(TBitstreamReader<unsigned int>&)` |

### `SC_ComponentModifier` — 1 functions, 296 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009da70` | 296 | `SC_ComponentModifier::operator==(SC_ComponentModifier const&) const` |

### `SC_CompositionSelectionGroup` — 4 functions, 5604 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a11c0` | 212 | `SC_CompositionSelectionGroup::~SC_CompositionSelectionGroup()` |
| | | `SC_CompositionSelectionGroup::~SC_CompositionSelectionGroup()` (alias, same code) |
| `0x000a4890` | 352 | `SC_CompositionSelectionGroup::operator==(SC_CompositionSelectionGroup const&) const` |
| `0x000a49f0` | 332 | `SC_CompositionSelectionGroup::CheckConformance(unsigned short, unsigned short) const` |
| `0x000a6ae0` | 4708 | `SC_CompositionSelectionGroup::Deserialize(TBitstreamReader<unsigned int>&, unsigned short)` |

### `SC_CompositionSelectionGroup::Presets` — 1 functions, 592 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a4640` | 592 | `SC_CompositionSelectionGroup::Presets::operator==(SC_CompositionSelectionGroup::Presets const&) const` |

### `SC_Extension` — 2 functions, 1184 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a3ca0` | 80 | `SC_Extension::operator==(SC_Extension const&) const` |
| `0x000a5840` | 1104 | `SC_Extension::Deserialize(TBitstreamReader<unsigned int>&)` |

### `SC_LanguageSelectionSet` — 3 functions, 1020 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a44b0` | 236 | `SC_LanguageSelectionSet::operator==(SC_LanguageSelectionSet const&) const` |
| `0x000a45a0` | 156 | `SC_LanguageSelectionSet::CheckConformance(unsigned short) const` |
| `0x000a6860` | 628 | `SC_LanguageSelectionSet::Deserialize(TBitstreamReader<unsigned int>&)` |

### `SC_LanguageSpecificItem` — 2 functions, 868 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a43c0` | 240 | `SC_LanguageSpecificItem::operator==(SC_LanguageSpecificItem const&) const` |
| `0x000a63d0` | 628 | `SC_LanguageSpecificItem::Deserialize(TBitstreamReader<unsigned int>&, bool)` |

### `SC_NonLanguageItem` — 2 functions, 748 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a42e0` | 220 | `SC_NonLanguageItem::operator==(SC_NonLanguageItem const&) const` |
| `0x000a6650` | 528 | `SC_NonLanguageItem::Deserialize(TBitstreamReader<unsigned int>&, bool)` |

### `SC_PresetDescription` — 2 functions, 852 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a4c30` | 292 | `SC_PresetDescription::operator==(SC_PresetDescription const&) const` |
| `0x000a7f80` | 560 | `SC_PresetDescription::Deserialize(TBitstreamReader<unsigned int>&, bool, bool)` |

### `SC_PresetDescription::SC_PresetCharacteristics` — 2 functions, 784 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a4b40` | 236 | `SC_PresetDescription::SC_PresetCharacteristics::operator==(SC_PresetDescription::SC_PresetCharacteristics const&) const` |
| `0x000a7d50` | 548 | `SC_PresetDescription::SC_PresetCharacteristics::Deserialize(TBitstreamReader<unsigned int>&)` |

### `SC_SceneComposition` — 5 functions, 5680 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a4fc0` | 224 | `SC_SceneComposition::GetSelectedItemIndex(unsigned int, unsigned int) const` |
| `0x000a50a0` | 836 | `SC_SceneComposition::operator==(SC_SceneComposition const&) const` |
| `0x000a53f0` | 456 | `SC_SceneComposition::CheckConformance() const` |
| `0x000a81b0` | 3236 | `SC_SceneComposition::Deserialize(TBitstreamReader<unsigned int>&, bool)` |
| `0x000aaa60` | 928 | `SC_SceneComposition::~SC_SceneComposition()` |
| | | `SC_SceneComposition::~SC_SceneComposition()` (alias, same code) |

### `SC_SceneComposition::SC_Category` — 3 functions, 1520 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a4d60` | 316 | `SC_SceneComposition::SC_Category::operator==(SC_SceneComposition::SC_Category const&) const` |
| `0x000a4ea0` | 288 | `SC_SceneComposition::SC_Category::CheckConformance(unsigned short, unsigned short) const` |
| `0x000a6030` | 916 | `SC_SceneComposition::SC_Category::Deserialize(TBitstreamReader<unsigned int>&, unsigned short)` |

### `SC_SceneController` — 4 functions, 2088 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009dce0` | 420 | `SC_SceneController::UpdateChannelActivity()` |
| `0x0009de90` | 268 | `SC_SceneController::UpdateInteractivityGains()` |
| `0x0009fd00` | 972 | `SC_SceneController::Initialize(unsigned int, unsigned int, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, SC_AudioScenes*, DRCConfiguration*)` |
| `0x000a1010` | 428 | `SC_SceneController::ProcessAudio(float const* const*, float**, unsigned int)` |

### `SC_SceneProcessor` — 3 functions, 3528 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009e150` | 216 | `SC_SceneProcessor::Update(std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > const&)` |
| `0x0009e230` | 2280 | `SC_SceneProcessor::Process(float const* const*, float**, unsigned int)` |
| `0x0009f130` | 1032 | `SC_SceneProcessor::Initialize(unsigned int, unsigned int, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > const&)` |

### `SC_Tag` — 2 functions, 1176 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a41d0` | 260 | `SC_Tag::operator==(SC_Tag const&) const` |
| `0x000a5c90` | 916 | `SC_Tag::Deserialize(TBitstreamReader<unsigned int>&)` |

### `SceneCompositionSettings` — 5 functions, 7844 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009dfa0` | 420 | `SceneCompositionSettings::Initialize(SC_SceneComposition const&, unsigned int)` |
| `0x0009eb20` | 320 | `SceneCompositionSettings::SetEffectiveInteractivityGains(SC_SceneComposition const&)` |
| `0x0009ec60` | 1224 | `SceneCompositionSettings::SetSceneComponentModifiers()` |
| `0x0009f540` | 1984 | `SceneCompositionSettings::SetFromPreset(unsigned short, SC_SceneComposition const&, unsigned short)` |
| `0x000a00d0` | 3896 | `SceneCompositionSettings::ApplyUserInput(unsigned int, SC_SceneComposition const&, CD_AudioSceneControlData::SceneCompositionData const&)` |

### `SceneCompositionSettings::CategorySettings` — 2 functions, 312 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009dba0` | 156 | `SceneCompositionSettings::CategorySettings::SetAllOn()` |
| `0x0009dc40` | 156 | `SceneCompositionSettings::CategorySettings::SetAllOff()` |

### `TBitstreamReader` — 8 functions, 1376 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b1430` | 168 | `TBitstreamReader<unsigned int>::TBitstreamReader(unsigned char const*, unsigned int)` |
| | | `TBitstreamReader<unsigned int>::TBitstreamReader(unsigned char const*, unsigned int)` (alias, same code) |
| `0x000b14e0` | 196 | `TBitstreamReader<unsigned int>::GetBits(unsigned int)` |
| `0x000b15b0` | 80 | `TBitstreamReader<unsigned int>::BitsRemaining() const` |
| `0x000b1600` | 176 | `TBitstreamReader<unsigned int>::FillCacheFrom(unsigned char const*)` |
| `0x000b16b0` | 176 | `TBitstreamReader<unsigned int>::GetBit()` |
| `0x000b1760` | 236 | `TBitstreamReader<unsigned int>::SkipBits(unsigned int)` |
| `0x000b1850` | 192 | `TBitstreamReader<unsigned int>::AlignToNextByte()` |
| `0x000b1910` | 152 | `TBitstreamReader<unsigned int>::PutBack(unsigned int)` |

### `apac::APACAncillaryFrameDecoder` — 5 functions, 4656 bytes

| addr | size | signature |
|---|---:|---|
| `0x00074120` | 1172 | `apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder()` |
| | | `apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder()` (alias, same code) |
| `0x000745c0` | 84 | `apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder()` |
| `0x00074a80` | 644 | `apac::APACAncillaryFrameDecoder::APACAncillaryFrameDecoder(apac::AncillaryConfig const&)` |
| | | `apac::APACAncillaryFrameDecoder::APACAncillaryFrameDecoder(apac::AncillaryConfig const&)` (alias, same code) |
| `0x00074d10` | 580 | `apac::APACAncillaryFrameDecoder::DecodeFrame(TBitstreamReader<unsigned int>&, std::vector<apac::ASCAudioOutData, std::allocator<apac::ASCAudioOutData> >&, apac::FrameType)` |
| `0x000757e0` | 2176 | `apac::APACAncillaryFrameDecoder::Initialize(apac::ParamList const&)` |

### `apac::APACAudioSceneDecoder` — 4 functions, 3580 bytes

| addr | size | signature |
|---|---:|---|
| `0x00074a30` | 76 | `apac::APACAudioSceneDecoder::DecodeFrame(float**)` |
| `0x00074f60` | 2164 | `apac::APACAudioSceneDecoder::Initialize(apac::ParamList const&, SC_AudioScenes*)` |
| `0x00076310` | 676 | `apac::APACAudioSceneDecoder::~APACAudioSceneDecoder()` |
| | | `apac::APACAudioSceneDecoder::~APACAudioSceneDecoder()` (alias, same code) |
| `0x000765c0` | 664 | `apac::APACAudioSceneDecoder::~APACAudioSceneDecoder()` |

### `apac::APACDRCDecoder` — 5 functions, 1848 bytes

| addr | size | signature |
|---|---:|---|
| `0x00074620` | 856 | `apac::APACDRCDecoder::Initialize(apac::ParamList const&, apac::drc::DynRangeCompressor&)` |
| `0x00074980` | 72 | `apac::APACDRCDecoder::Deserialize(TBitstreamReader<unsigned int>&)` |
| `0x000749d0` | 92 | `apac::APACDRCDecoder::DecodeFrame(float**)` |
| `0x00076860` | 424 | `apac::APACDRCDecoder::~APACDRCDecoder()` |
| | | `apac::APACDRCDecoder::~APACDRCDecoder()` (alias, same code) |
| `0x00076a10` | 404 | `apac::APACDRCDecoder::~APACDRCDecoder()` |

### `apac::ASCChannelMap` — 2 functions, 560 bytes

| addr | size | signature |
|---|---:|---|
| `0x000af5b0` | 448 | `apac::ASCChannelMap::InsertMap(std::tuple<apac::AudioDataType, apac::ASCChannelRange> const&)` |
| `0x000af770` | 112 | `apac::ASCChannelMap::AddMap(apac::AudioDataType, unsigned short, unsigned short)` |

### `apac::AncillaryConfig` — 4 functions, 1752 bytes

| addr | size | signature |
|---|---:|---|
| `0x00072cc0` | 92 | `apac::AncillaryConfig::AncillaryConfig()` |
| | | `apac::AncillaryConfig::AncillaryConfig()` (alias, same code) |
| `0x00072d20` | 212 | `apac::AncillaryConfig::~AncillaryConfig()` |
| | | `apac::AncillaryConfig::~AncillaryConfig()` (alias, same code) |
| `0x00072e00` | 84 | `apac::AncillaryConfig::~AncillaryConfig()` |
| `0x00072e60` | 1364 | `apac::AncillaryConfig::Deserialize(TBitstreamReader<unsigned int>&, unsigned int, unsigned int, unsigned int)` |

### `apac::AudioSceneConfig` — 1 functions, 1412 bytes

| addr | size | signature |
|---|---:|---|
| `0x00072620` | 1412 | `apac::AudioSceneConfig::Deserialize(TBitstreamReader<unsigned int>&)` |

### `apac::Autocorr` — 2 functions, 228 bytes

| addr | size | signature |
|---|---:|---|
| `0x00079790` | 116 | `apac::Autocorr::~Autocorr()` |
| | | `apac::Autocorr::~Autocorr()` (alias, same code) |
| `0x00079810` | 112 | `apac::Autocorr::~Autocorr()` |

### `apac::BaseASCConfig` — 5 functions, 756 bytes

| addr | size | signature |
|---|---:|---|
| `0x00097300` | 68 | `apac::BaseASCConfig::GetChannelLayout() const` |
| `0x000975f0` | 120 | `apac::BaseASCConfig::GetNumChannels() const` |
| `0x00098f00` | 68 | `apac::BaseASCConfig::Deserialize(TBitstreamReader<unsigned int>&)` |
| `0x00098f50` | 244 | `apac::BaseASCConfig::~BaseASCConfig()` |
| `0x00099050` | 256 | `apac::BaseASCConfig::~BaseASCConfig()` |
| | | `apac::BaseASCConfig::~BaseASCConfig()` (alias, same code) |

### `apac::ChanDecElement` — 3 functions, 476 bytes

| addr | size | signature |
|---|---:|---|
| `0x00093ad0` | 228 | `apac::ChanDecElement::Initialize(apac::ParamList const&, apac::observer_ptr<apac::IASCConfig>)` |
| `0x00093bc0` | 128 | `apac::ChanDecElement::~ChanDecElement()` |
| | | `apac::ChanDecElement::~ChanDecElement()` (alias, same code) |
| `0x00093c40` | 120 | `apac::ChanDecElement::~ChanDecElement()` |

### `apac::ExtFrameUnkownDecoder` — 4 functions, 696 bytes

| addr | size | signature |
|---|---:|---|
| `0x000955a0` | 64 | `apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder()` |
| `0x00095b30` | 64 | `apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder()` |
| | | `apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder()` (alias, same code) |
| `0x00095e70` | 500 | `apac::ExtFrameUnkownDecoder::Decode(TBitstreamReader<unsigned int>&)` |
| `0x00096290` | 68 | `apac::ExtFrameUnkownDecoder::Initialize(apac::IExtCfgElement const&)` |

### `apac::ExtensionConfig` — 1 functions, 388 bytes

| addr | size | signature |
|---|---:|---|
| `0x000964a0` | 388 | `apac::ExtensionConfig::Deserialize(TBitstreamReader<unsigned int>&)` |

### `apac::GlobalConfig` — 3 functions, 5676 bytes

| addr | size | signature |
|---|---:|---|
| `0x000978c0` | 220 | `apac::GlobalConfig::Reset()` |
| `0x000979a0` | 940 | `apac::GlobalConfig::SetChannelLayout()` |
| `0x00097d50` | 4516 | `apac::GlobalConfig::Deserialize(TBitstreamReader<unsigned int>&)` |

### `apac::GlobalConfig::ASCConfigData` — 3 functions, 216 bytes

| addr | size | signature |
|---|---:|---|
| `0x000760b0` | 88 | `apac::GlobalConfig::ASCConfigData::configs() const` |
| `0x00106e50` | 64 | `apac::GlobalConfig::ASCConfigData::~ASCConfigData()` |
| | | `apac::GlobalConfig::ASCConfigData::~ASCConfigData()` (alias, same code) |
| `0x00106f10` | 64 | `apac::GlobalConfig::ASCConfigData::~ASCConfigData()` |

### `apac::GlobalConfig::CodecConfigData` — 3 functions, 200 bytes

| addr | size | signature |
|---|---:|---|
| `0x000951c0` | 72 | `apac::GlobalConfig::CodecConfigData::configs() const` |
| `0x00106e90` | 64 | `apac::GlobalConfig::CodecConfigData::~CodecConfigData()` |
| | | `apac::GlobalConfig::CodecConfigData::~CodecConfigData()` (alias, same code) |
| `0x00106ed0` | 64 | `apac::GlobalConfig::CodecConfigData::~CodecConfigData()` |

### `apac::GlobalConfig::ComponentConfigs` — 1 functions, 76 bytes

| addr | size | signature |
|---|---:|---|
| `0x00106e00` | 76 | `apac::GlobalConfig::ComponentConfigs::configs()` |

### `apac::LPCBase` — 3 functions, 1208 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b4190` | 132 | `apac::LPCBase::LPC_Weighting(float const*, float*, float, unsigned short)` |
| `0x000b4620` | 400 | `apac::LPCBase::LevinsonDurbin(float const*)` |
| `0x000b4930` | 676 | `apac::LPCBase::ComputeLPCs(float const*, bool)` |

### `apac::LPCodeBook` — 2 functions, 948 bytes

| addr | size | signature |
|---|---:|---|
| `0x000b54b0` | 616 | `apac::LPCodeBook::VQSearch(float const*)` |
| `0x000b5720` | 332 | `apac::LPCodeBook::LSF_Quant_Weighting(float const*, short)` |

### `apac::LPMDCTFilter` — 8 functions, 1636 bytes

| addr | size | signature |
|---|---:|---|
| `0x00079940` | 116 | `apac::LPMDCTFilter::~LPMDCTFilter()` |
| `0x0007a350` | 100 | `apac::LPMDCTFilter::~LPMDCTFilter()` |
| | | `apac::LPMDCTFilter::~LPMDCTFilter()` (alias, same code) |
| `0x0009c740` | 108 | `apac::LPMDCTFilter::Initialize()` |
| `0x0009c7b0` | 116 | `apac::LPMDCTFilter::Initialize(unsigned short, unsigned short)` |
| `0x000b47b0` | 376 | `apac::LPMDCTFilter::AutocorrFromPSD(float const*, float*, unsigned int)` |
| `0x000b4be0` | 244 | `apac::LPMDCTFilter::LPCToSpecEnvelope(float const*, float*, unsigned int)` |
| `0x000b4ce0` | 248 | `apac::LPMDCTFilter::LPImpulseToSpecEnvelope(float const*, float*, unsigned int)` |
| `0x000b4de0` | 328 | `apac::LPMDCTFilter::MDCTToAutoCorr(float const*, float*, unsigned int, unsigned int, unsigned int)` |

### `apac::LSFTools` — 15 functions, 4776 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009c6e0` | 96 | `apac::LSFTools::ConditionLSFs(float*)` |
| `0x0009cd40` | 416 | `apac::LSFTools::~LSFTools()` |
| | | `apac::LSFTools::~LSFTools()` (alias, same code) |
| `0x0009cee0` | 392 | `apac::LSFTools::~LSFTools()` |
| `0x000b4220` | 268 | `apac::LSFTools::get_LSFWeightingFactors(float const*, float*, unsigned short, float)` |
| `0x000b4330` | 96 | `apac::LSFTools::LSP_To_LPC()` |
| `0x000b4390` | 200 | `apac::LSFTools::Condition_LSFs(float*, float, float)` |
| `0x000b4460` | 440 | `apac::LSFTools::LSP_To_LPC(float const*, float*, unsigned short)` |
| `0x000b4f30` | 224 | `apac::LSFTools::DecodeLSF(unsigned short const*)` |
| `0x000b5010` | 216 | `apac::LSFTools::LSP_To_LSF(float const*, float*, unsigned short, float)` |
| `0x000b50f0` | 204 | `apac::LSFTools::LSF_To_LSP(float const*, float*, float)` |
| `0x000b51c0` | 204 | `apac::LSFTools::LSF_To_LSP(float*, float)` |
| `0x000b5290` | 212 | `apac::LSFTools::Reset()` |
| `0x000b5370` | 312 | `apac::LSFTools::LSF_To_LPC(float*, float*)` |
| `0x000b5870` | 404 | `apac::LSFTools::QuantizeLSF(unsigned short*)` |
| `0x000b5a10` | 1092 | `apac::LSFTools::Initialize()` |

### `apac::LoudnessDRCConfig` — 1 functions, 272 bytes

| addr | size | signature |
|---|---:|---|
| `0x00072bb0` | 272 | `apac::LoudnessDRCConfig::Deserialize(TBitstreamReader<unsigned int>&, apac::ParamList const&, mpddrc::HeaderPayloadType)` |

### `apac::ParamList` — 1 functions, 576 bytes

| addr | size | signature |
|---|---:|---|
| `0x00083a50` | 576 | `unsigned int apac::ParamList::get<unsigned int>(int) const` |

### `apac::ProfileLevelConfig` — 4 functions, 2272 bytes

| addr | size | signature |
|---|---:|---|
| `0x0009d0d0` | 524 | `apac::ProfileLevelConfig::GetLevelFromCodecConfigs(std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > const&) const` |
| `0x0009d2e0` | 1412 | `apac::ProfileLevelConfig::InferProfileFromCodecConfigs(std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > const&, apac::ConstParamArray<apac::ProfileLevel> const&)` |
| `0x0009d870` | 108 | `apac::ProfileLevelConfig::IsDecodable(apac::Profile, apac::Profile)` |
| `0x0009d8e0` | 228 | `apac::ProfileLevelConfig::ValidateProfileLevelData(std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > const&) const` |

### `apac::TCEConfig` — 3 functions, 196 bytes

| addr | size | signature |
|---|---:|---|
| `0x0007c800` | 68 | `apac::TCEConfig::Deserialize(TBitstreamReader<unsigned int>&)` |
| `0x0007c850` | 64 | `apac::TCEConfig::~TCEConfig()` |
| | | `apac::TCEConfig::~TCEConfig()` (alias, same code) |
| `0x0007c890` | 64 | `apac::TCEConfig::~TCEConfig()` |

### `apac::TrimmingData` — 1 functions, 260 bytes

| addr | size | signature |
|---|---:|---|
| `0x00078040` | 260 | `apac::TrimmingData::Deserialize(TBitstreamReader<unsigned int>&)` |

### `apac::chan::CoreDecoderConfig` — 5 functions, 2500 bytes

| addr | size | signature |
|---|---:|---|
| `0x0007c030` | 68 | `apac::chan::CoreDecoderConfig::GetChannelLayout() const` |
| `0x0007c080` | 120 | `apac::chan::CoreDecoderConfig::GetNumChannels() const` |
| `0x0007c100` | 1788 | `apac::chan::CoreDecoderConfig::Deserialize(TBitstreamReader<unsigned int>&)` |
| `0x0007c8d0` | 268 | `apac::chan::CoreDecoderConfig::~CoreDecoderConfig()` |
| | | `apac::chan::CoreDecoderConfig::~CoreDecoderConfig()` (alias, same code) |
| `0x0007c9e0` | 256 | `apac::chan::CoreDecoderConfig::~CoreDecoderConfig()` |

### `apac::drc::DynRangeCompressor` — 7 functions, 2832 bytes

| addr | size | signature |
|---|---:|---|
| `0x00073590` | 316 | `apac::drc::DynRangeCompressor::~DynRangeCompressor()` |
| `0x000736d0` | 360 | `apac::drc::DynRangeCompressor::~DynRangeCompressor()` |
| | | `apac::drc::DynRangeCompressor::~DynRangeCompressor()` (alias, same code) |
| `0x0007eac0` | 500 | `apac::drc::DynRangeCompressor::AdjustForHostInput()` |
| `0x0007ecc0` | 100 | `apac::drc::DynRangeCompressor::Deserialize(TBitstreamReader<unsigned int>&, mpddrc::HeaderPayloadType)` |
| `0x0007ed30` | 240 | `apac::drc::DynRangeCompressor::ProcessUniDrc(float const**, float**, unsigned int&)` |
| `0x0007fd50` | 604 | `apac::drc::DynRangeCompressor::Initialize(DRCConfiguration*, float, unsigned int, unsigned int, unsigned int)` |
| `0x0007ffb0` | 712 | `apac::drc::DynRangeCompressor::Initialize(apac::ParamList const&)` |

### `apac::drc::DynRangeCompressor::APACDRCExtensionPayload` — 3 functions, 4024 bytes

| addr | size | signature |
|---|---:|---|
| `0x0007ea20` | 160 | `apac::drc::DynRangeCompressor::APACDRCExtensionPayload::Deserialize(TBitstreamReader<unsigned int>&, mpddrc::HeaderPayloadType)` |
| `0x0007ee20` | 68 | `apac::drc::DynRangeCompressor::APACDRCExtensionPayload::~APACDRCExtensionPayload()` |
| | | `apac::drc::DynRangeCompressor::APACDRCExtensionPayload::~APACDRCExtensionPayload()` (alias, same code) |
| `0x0007ee70` | 3796 | `apac::drc::DynRangeCompressor::APACDRCExtensionPayload::APACDRCExtensionPayload(apac::drc::DynRangeCompressor&, DRCConfiguration const&, unsigned int, unsigned int, unsigned int, unsigned int, mpddrc::UniDrcDomain)` |
| | | `apac::drc::DynRangeCompressor::APACDRCExtensionPayload::APACDRCExtensionPayload(apac::drc::DynRangeCompressor&, DRCConfiguration const&, unsigned int, unsigned int, unsigned int, unsigned int, mpddrc::UniDrcDomain)` (alias, same code) |

### `apac::lbr::DecoderConfig` — 3 functions, 728 bytes

| addr | size | signature |
|---|---:|---|
| `0x0007e160` | 204 | `apac::lbr::DecoderConfig::Deserialize(TBitstreamReader<unsigned int>&)` |
| `0x0007e230` | 268 | `apac::lbr::DecoderConfig::~DecoderConfig()` |
| | | `apac::lbr::DecoderConfig::~DecoderConfig()` (alias, same code) |
| `0x0007e340` | 256 | `apac::lbr::DecoderConfig::~DecoderConfig()` |

### `apac::scene::BCP47LanguageString` — 3 functions, 632 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a3170` | 168 | `apac::scene::BCP47LanguageString::operator==(apac::scene::BCP47LanguageString const&) const` |
| `0x000a3220` | 76 | `apac::scene::BCP47LanguageString::Reset()` |
| `0x000a3620` | 388 | `apac::scene::BCP47LanguageString::Deserialize(TBitstreamReader<unsigned int>&)` |

### `apac::scene::BCP47LanguageString::BCP47LanguageChar` — 1 functions, 940 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a3270` | 940 | `apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(TBitstreamReader<unsigned int>&, char&, bool&)` |

### `apac::scene::SC_Language` — 2 functions, 864 bytes

| addr | size | signature |
|---|---:|---|
| `0x000a37b0` | 780 | `apac::scene::SC_Language::Deserialize(TBitstreamReader<unsigned int>&)` |
| `0x000a3ac0` | 84 | `apac::scene::SC_Language::Reset()` |

### `boost::any::holder` — 8 functions, 608 bytes

| addr | size | signature |
|---|---:|---|
| `0x00095250` | 64 | `boost::any::holder<SC_SceneController*>::~holder()` |
| | | `boost::any::holder<SC_SceneController*>::~holder()` (alias, same code) |
| `0x000952d0` | 64 | `boost::any::holder<apac::GlobalConfig const*>::~holder()` |
| | | `boost::any::holder<apac::GlobalConfig const*>::~holder()` (alias, same code) |
| `0x00095360` | 72 | `boost::any::holder<SC_SceneController*>::type() const` |
| `0x00095450` | 72 | `boost::any::holder<apac::GlobalConfig const*>::type() const` |
| `0x000954e0` | 64 | `boost::any::holder<SC_SceneController*>::~holder()` |
| `0x00095560` | 64 | `boost::any::holder<apac::GlobalConfig const*>::~holder()` |
| `0x00095730` | 104 | `boost::any::holder<SC_SceneController*>::clone() const` |
| `0x00095810` | 104 | `boost::any::holder<apac::GlobalConfig const*>::clone() const` |

### `std::_Sp_counted_ptr_inplace` — 5 functions, 424 bytes

| addr | size | signature |
|---|---:|---|
| `0x000802c0` | 64 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace()` |
| | | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace()` (alias, same code) |
| `0x00080340` | 64 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace()` |
| `0x00080380` | 64 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::_M_destroy()` |
| `0x000803c0` | 164 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::_M_get_deleter(std::type_info const&)` |
| `0x000884d0` | 68 | `std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::_M_dispose()` |

### `std::vector` — 28 functions, 14728 bytes

| addr | size | signature |
|---|---:|---|
| `0x0007cd40` | 524 | `void std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> >::_M_realloc_insert<apac::TCEConfig const&>(__gnu_cxx::__normal_iterator<apac::TCEConfig*, std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> > >, apac::TCEConfig const&)` |
| `0x00095cc0` | 432 | `void std::vector<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >, std::allocator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > > >::_M_realloc_insert<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > >(__gnu_cxx::__normal_iterator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >*, std::vector<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >, std::allocator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > > > >, std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >&&)` |
| `0x000962e0` | 440 | `void std::vector<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >, std::allocator<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> > > >::_M_realloc_insert<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >*, std::vector<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >, std::allocator<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> > > > >, std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >&&)` |
| `0x00099150` | 204 | `std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> >::~vector()` |
| | | `std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> >::~vector()` (alias, same code) |
| `0x00099ba0` | 400 | `void std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::_M_realloc_insert<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >&&)` |
| `0x00099d30` | 132 | `void std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::emplace_back<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > >(std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >&&)` |
| `0x000a15c0` | 316 | `std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::reserve(unsigned long)` |
| `0x000a1700` | 476 | `std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::operator=(std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > const&)` |
| `0x000a22c0` | 540 | `void std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::_M_realloc_insert<SC_ComponentModifier const&>(__gnu_cxx::__normal_iterator<SC_ComponentModifier*, std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > >, SC_ComponentModifier const&)` |
| `0x000a24e0` | 500 | `std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::_M_default_append(unsigned long)` |
| `0x000a26e0` | 352 | `std::vector<SC_SceneProcessor::ComponentTransitionFlags, std::allocator<SC_SceneProcessor::ComponentTransitionFlags> >::_M_default_append(unsigned long)` |
| `0x000a2840` | 360 | `std::vector<SC_SceneProcessor::Ramp, std::allocator<SC_SceneProcessor::Ramp> >::_M_default_append(unsigned long)` |
| `0x000a94f0` | 524 | `void std::vector<SC_CompositionSelectionGroup::Presets, std::allocator<SC_CompositionSelectionGroup::Presets> >::_M_realloc_insert<SC_CompositionSelectionGroup::Presets const&>(__gnu_cxx::__normal_iterator<SC_CompositionSelectionGroup::Presets*, std::vector<SC_CompositionSelectionGroup::Presets, std::allocator<SC_CompositionSelectionGroup::Presets> > >, SC_CompositionSelectionGroup::Presets const&)` |
| `0x000a9700` | 748 | `std::vector<SC_NonLanguageItem, std::allocator<SC_NonLanguageItem> >::_M_default_append(unsigned long)` |
| `0x000a99f0` | 608 | `std::vector<SC_LanguageSpecificItem, std::allocator<SC_LanguageSpecificItem> >::_M_default_append(unsigned long)` |
| `0x000a9c50` | 768 | `std::vector<SC_LanguageSelectionSet, std::allocator<SC_LanguageSelectionSet> >::_M_default_append(unsigned long)` |
| `0x000a9f50` | 960 | `std::vector<SC_CompositionSelectionGroup, std::allocator<SC_CompositionSelectionGroup> >::_M_default_append(unsigned long)` |
| `0x000aa310` | 1040 | `std::vector<SC_PresetDescription, std::allocator<SC_PresetDescription> >::_M_default_append(unsigned long)` |
| `0x000aa720` | 824 | `std::vector<SC_SceneComposition::SC_Category, std::allocator<SC_SceneComposition::SC_Category> >::_M_default_append(unsigned long)` |
| `0x000aae00` | 1456 | `std::vector<SC_SceneComposition, std::allocator<SC_SceneComposition> >::_M_default_append(unsigned long)` |
| `0x000ae180` | 380 | `std::vector<float, apac::AlignedAllocator<float, 16ul> >::_M_default_append(unsigned long)` |
| `0x000ae300` | 404 | `void std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >::_M_realloc_insert<apac::ElementInstance<APACSyntacticElement*> >(__gnu_cxx::__normal_iterator<apac::ElementInstance<APACSyntacticElement*>*, std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > > >, apac::ElementInstance<APACSyntacticElement*>&&)` |
| `0x000ae4a0` | 136 | `void std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >::emplace_back<apac::ElementInstance<APACSyntacticElement*> >(apac::ElementInstance<APACSyntacticElement*>&&)` |
| `0x000af7e0` | 408 | `void std::vector<std::tuple<apac::AudioDataType, apac::ASCChannelRange>, std::allocator<std::tuple<apac::AudioDataType, apac::ASCChannelRange> > >::_M_realloc_insert<std::tuple<apac::AudioDataType, apac::ASCChannelRange> const&>(__gnu_cxx::__normal_iterator<std::tuple<apac::AudioDataType, apac::ASCChannelRange>*, std::vector<std::tuple<apac::AudioDataType, apac::ASCChannelRange>, std::allocator<std::tuple<apac::AudioDataType, apac::ASCChannelRange> > > >, std::tuple<apac::AudioDataType, apac::ASCChannelRange> const&)` |
| `0x000b1240` | 496 | `void std::vector<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >, std::allocator<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> > > >::_M_realloc_insert<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >*, std::vector<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >, std::allocator<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> > > > >, std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >&&)` |
| `0x000b30e0` | 716 | `void std::vector<apac::ASCAudioOutData, std::allocator<apac::ASCAudioOutData> >::_M_realloc_insert<apac::ASCAudioOutData>(__gnu_cxx::__normal_iterator<apac::ASCAudioOutData*, std::vector<apac::ASCAudioOutData, std::allocator<apac::ASCAudioOutData> > >, apac::ASCAudioOutData&&)` |
| `0x000b5fb0` | 416 | `void std::vector<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >, std::allocator<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> > > >::_M_realloc_insert<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >*, std::vector<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >, std::allocator<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> > > > >, std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >&&)` |
| `0x00106f50` | 168 | `std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::~vector()` |
| | | `std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::~vector()` (alias, same code) |

### free functions / C API — 45

| addr | size | signature |
|---|---:|---|
| `0x00070c28` | 68 | `sub_70C28` |
| `0x00070cf4` | 72 | `sub_70CF4` |
| `0x00071fc0` | 64 | `DRCLoggingRegisterScopeOnce()` |
| `0x00078150` | 544 | `unsigned int apac::GetEscBits<16u, 20u>(TBitstreamReader<unsigned int>&, unsigned int)` |
| `0x00078370` | 288 | `sub_78370` |
| `0x0007b070` | 1316 | `sub_7B070` |
| `0x00096630` | 536 | `unsigned int apac::GetEscBits<8u, 16u>(TBitstreamReader<unsigned int>&, unsigned int)` |
| `0x00096850` | 2736 | `sub_96850` |
| `0x00097350` | 348 | `sub_97350` |
| `0x000974b0` | 308 | `sub_974B0` |
| `0x00097670` | 592 | `sub_97670` |
| `0x00099220` | 536 | `unsigned int apac::GetEscBits<6u, 12u>(TBitstreamReader<unsigned int>&, unsigned int)` |
| `0x00099650` | 836 | `int GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, __gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, std::vector<apac::observer_ptr<apac::IASCConfig const>, std::allocator<apac::observer_ptr<apac::IASCConfig const> > >&)` |
| `0x00099dc0` | 76 | `apac::GetHuffmanTable(unsigned int)` |
| `0x00099e10` | 76 | `apac::GetSCFHuffmanTable()` |
| `0x0009d070` | 92 | `apac::GetProfileConfig(apac::Profile)` |
| `0x0009d9d0` | 152 | `apac::GetSFBOffsetTable(unsigned int, unsigned int)` |
| `0x000a3b20` | 128 | `apac::scene::DecodeAngleOffset(unsigned short, unsigned short)` |
| `0x000a3ba0` | 144 | `apac::scene::DecodeDistanceRatio(unsigned short, unsigned short)` |
| `0x000a3c30` | 100 | `apac::scene::DecodeInteractivityGain(unsigned short, bool)` |
| `0x000aec20` | 232 | `sub_AEC20` |
| `0x000b0d20` | 76 | `apac::ConvertToAudioDataType(apac::ASCType)` |
| `0x000b19b0` | 316 | `sub_B19B0` |
| `0x000b1d90` | 724 | `InterleaveConvert(float const* const*, void* const*, unsigned int, unsigned int, bool, bool, float)` |
| `0x000b2750` | 412 | `apac::CreateContentMixer(apac::GlobalConfig const&)` |
| `0x000b33b0` | 104 | `GetDCTIVModulationCoefficients` |
| `0x000b40b0` | 220 | `sub_B40B0` |
| `0x00105590` | 104 | `GetSineWindowCoefficientPtr` |
| `0x00105600` | 108 | `GetKBDWindowCoefficientPtr` |
| `0x00105670` | 380 | `sub_105670` |
| `0x001057f0` | 244 | `sub_1057F0` |
| `0x001058f0` | 704 | `sub_1058F0` |
| `0x00105bb0` | 552 | `APACDecoder_DecodeFrame` |
| `0x00105de0` | 132 | `APACDecoder_Reset` |
| `0x00105e70` | 440 | `APACDecoder_GetParameter` |
| `0x00106030` | 124 | `APACDecoder_GetCookieOffset` |
| `0x001060b0` | 80 | `APACDecoder_GetVersion` |
| `0x00106100` | 80 | `APACDecoder_GetProfile` |
| `0x00106150` | 92 | `APACDecoder_SampleRate` |
| `0x001061b0` | 88 | `APACDecoder_FrameLength` |
| `0x00106210` | 80 | `APACDecoder_NumberOfChannels` |
| `0x00106260` | 552 | `APACDecoder_SetParameter` |
| `0x00106490` | 876 | `APACDecoder_Configure` |
| `0x00106800` | 1028 | `APACDecoder_Create` |
| `0x00106c10` | 492 | `APACDecoder_Dispose` |

