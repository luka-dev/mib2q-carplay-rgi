# `reference/decomp` — index

Decompiled reference material for the APAC port: **396 functions, 179 120 bytes of `.text`**,
one file per class. Start with `NOTES.md` (method, coverage, what is unreliable), then
`CLASSES.md` (hierarchy, vtable order, member offsets, signatures) and `RODATA.md` (the
constant tables and how they were found).

| file | class / group | fns | bytes | port stage |
|---|---|---:|---:|---|
| `apac_GlobalConfig.c` | `apac::GlobalConfig` | 10 | 6168 | a |
| `apac_ProfileLevelConfig.c` | `apac::ProfileLevelConfig` | 4 | 2272 | a |
| `TBitstreamReader.c` | `TBitstreamReader` | 8 | 1376 | a |
| `apac_BaseASCConfig.c` | `apac::BaseASCConfig` | 5 | 756 | a |
| `APACScaleFactorData.c` | `APACScaleFactorData` | 1 | 1224 | b |
| `APACICSInfo.c` | `APACICSInfo` | 4 | 944 | b |
| `APACLRVQDecoder.c` | `APACLRVQDecoder` | 15 | 8904 | c |
| `APACSpectralData.c` | `APACSpectralData` | 1 | 3820 | c |
| `apac_LSFTools.c` | `apac::LSFTools` | 15 | 4776 | d |
| `APACTNSData.c` | `APACTNSData` | 3 | 2188 | d |
| `APACSynthesisFilterBank.c` | `APACSynthesisFilterBank` | 5 | 1736 | d |
| `apac_LPMDCTFilter.c` | `apac::LPMDCTFilter` | 8 | 1636 | d |
| `DCT_IV.c` | `DCT_IV` | 5 | 1564 | d |
| `apac_LPCBase.c` | `apac::LPCBase` | 3 | 1208 | d |
| `apac_LPCodeBook.c` | `apac::LPCodeBook` | 2 | 948 | d |
| `MultiRadixRealFFT.c` | `MultiRadixRealFFT` | 3 | 612 | d |
| `MDCT.c` | `MDCT` | 4 | 560 | d |
| `apac_Autocorr.c` | `apac::Autocorr` | 2 | 228 | d |
| `MultiRadixFFT.c` | `MultiRadixFFT` | 1 | 156 | d |
| `APACBWE2Decoder.c` | `APACBWE2Decoder` | 7 | 6592 | e |
| `BWELPC.c` | `BWELPC` | 3 | 688 | e |
| `APACDecoder.c` | `APACDecoder` | 9 | 5308 | f |
| `APACCoreDecoder.c` | `APACCoreDecoder` | 11 | 3596 | f |
| `APACChannelPairElement.c` | `APACChannelPairElement` | 8 | 3048 | f |
| `APACSyntacticElement.c` | `APACSyntacticElement` | 7 | 2556 | f |
| `apac_chan.c` | `apac::chan` | 5 | 2500 | f |
| `APACSingleChannelElement.c` | `APACSingleChannelElement` | 4 | 1192 | f |
| `APACASPDecoder.c` | `APACASPDecoder` | 5 | 1172 | f |
| `APACLFEElement.c` | `APACLFEElement` | 4 | 988 | f |
| `APACChannelRemapper.c` | `APACChannelRemapper` | 4 | 488 | f |
| `apac_ChanDecElement.c` | `apac::ChanDecElement` | 3 | 476 | f |
| `APACIndividualChannelStream.c` | `APACIndividualChannelStream` | 3 | 392 | f |
| `APACInstanceConfig.c` | `APACInstanceConfig` | 1 | 224 | f |
| `apac_drc.c` | `apac::drc` | 10 | 6856 | g |
| `DRCConfiguration.c` | `DRCConfiguration` | 20 | 3288 | g |
| `apac_APACDRCDecoder.c` | `apac::APACDRCDecoder` | 5 | 1848 | g |
| `apac_LoudnessDRCConfig.c` | `apac::LoudnessDRCConfig` | 1 | 272 | g |
| `std.c` | `std` | 33 | 15152 | — |
| `_free_functions.c` | `_free_functions` | 33 | 14380 | — |
| `apac_APACAncillaryFrameDecoder.c` | `apac::APACAncillaryFrameDecoder` | 5 | 4656 | — |
| `apac.c` | `apac` | 9 | 2500 | — |
| `ContentOutBuffer.c` | `ContentOutBuffer` | 5 | 2044 | — |
| `MP4LoudnessInfo.c` | `MP4LoudnessInfo` | 1 | 1948 | — |
| `APACCACDecoder.c` | `APACCACDecoder` | 2 | 1824 | — |
| `apac_AncillaryConfig.c` | `apac::AncillaryConfig` | 4 | 1752 | — |
| `MP4LoudnessMeasure.c` | `MP4LoudnessMeasure` | 1 | 788 | — |
| `apac_ExtFrameUnkownDecoder.c` | `apac::ExtFrameUnkownDecoder` | 4 | 696 | — |
| `boost.c` | `boost` | 8 | 608 | — |
| `CacQuantizer.c` | `CacQuantizer` | 2 | 600 | — |
| `apac_ParamList.c` | `apac::ParamList` | 1 | 576 | — |
| `apac_ASCChannelMap.c` | `apac::ASCChannelMap` | 2 | 560 | — |
| `APACExtFrameDecoder.c` | `APACExtFrameDecoder` | 2 | 524 | — |
| `apac_ExtensionConfig.c` | `apac::ExtensionConfig` | 1 | 388 | — |
| `apac_TrimmingData.c` | `apac::TrimmingData` | 1 | 260 | — |
| `apac_TCEConfig.c` | `apac::TCEConfig` | 3 | 196 | — |
| `CacDecoder.c` | `CacDecoder` | 2 | 144 | — |
| `SceneCompositionSettings.c` | `SceneCompositionSettings` | 7 | 8156 | skip |
| `SC_SceneComposition.c` | `SC_SceneComposition` | 8 | 7200 | skip |
| `SC_CompositionSelectionGroup.c` | `SC_CompositionSelectionGroup` | 5 | 6196 | skip |
| `apac_APACAudioSceneDecoder.c` | `apac::APACAudioSceneDecoder` | 4 | 3580 | skip |
| `SC_SceneProcessor.c` | `SC_SceneProcessor` | 3 | 3528 | skip |
| `SC_AudioScenes.c` | `SC_AudioScenes` | 4 | 2844 | skip |
| `apac_scene.c` | `apac::scene` | 9 | 2808 | skip |
| `SC_SceneController.c` | `SC_SceneController` | 4 | 2088 | skip |
| `SC_PresetDescription.c` | `SC_PresetDescription` | 4 | 1636 | skip |
| `APACCoreLBRDecoder.c` | `APACCoreLBRDecoder` | 4 | 1496 | skip |
| `apac_AudioSceneConfig.c` | `apac::AudioSceneConfig` | 1 | 1412 | skip |
| `SC_Extension.c` | `SC_Extension` | 2 | 1184 | skip |
| `SC_Tag.c` | `SC_Tag` | 2 | 1176 | skip |
| `SC_LanguageSelectionSet.c` | `SC_LanguageSelectionSet` | 3 | 1020 | skip |
| `SC_LanguageSpecificItem.c` | `SC_LanguageSpecificItem` | 2 | 868 | skip |
| `SC_NonLanguageItem.c` | `SC_NonLanguageItem` | 2 | 748 | skip |
| `apac_lbr.c` | `apac::lbr` | 3 | 728 | skip |
| `SC_ComponentModifier.c` | `SC_ComponentModifier` | 1 | 296 | skip |

Port stages are the ones in `docs/Audio/APAC Codec.md`. `skip` is the
Ambisonics-scene and low-bitrate code the CarPlay 5.1 / 5.1.2 target does not use; `—` is
container, allocator and plumbing code with no stage of its own.
`rodata/` holds every constant table as a byte-exact C array and a matching `.bin`.

