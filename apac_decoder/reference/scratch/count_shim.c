/* LD_PRELOAD shim: counts entry-point calls in the APAC entropy core so we can
   tell which paths a real capture actually exercises. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static long n_lrvq, n_spec, n_scf, n_lsf, n_ltp;
#define CHAIN(var, name) static void* var; if (!var) var = dlsym(RTLD_NEXT, name);

int _ZN15APACLRVQDecoder15LrvqDeserializeER16TBitstreamReaderIjEbbR16APACSpectralDataRN4apac13aligned_arrayIfLm1024EEES8_RNS6_IfLm256EEERNS6_IsLm64EEEPt(
        void* a, unsigned char b, unsigned char c, void* d, void* e, void* f, void* g, void* h, void* i)
{
    CHAIN(p, "_ZN15APACLRVQDecoder15LrvqDeserializeER16TBitstreamReaderIjEbbR16APACSpectralDataRN4apac13aligned_arrayIfLm1024EEES8_RNS6_IfLm256EEERNS6_IsLm64EEEPt");
    ++n_lrvq;
    return ((int(*)(void*,unsigned char,unsigned char,void*,void*,void*,void*,void*,void*))p)(a,b,c,d,e,f,g,h,i);
}
long _ZN16APACSpectralData11DeserializeER16TBitstreamReaderIjERK11APACICSInfoRK19APACScaleFactorDataRKN4apac10SFBOffsetsE(
        void* a, void* b, void* c, void* d, void* e)
{
    CHAIN(p, "_ZN16APACSpectralData11DeserializeER16TBitstreamReaderIjERK11APACICSInfoRK19APACScaleFactorDataRKN4apac10SFBOffsetsE");
    ++n_spec;
    return ((long(*)(void*,void*,void*,void*,void*))p)(a,b,c,d,e);
}
long _ZN19APACScaleFactorData11DeserializeER16TBitstreamReaderIjERK11APACICSInfo(void* a, void* b, void* c)
{
    CHAIN(p, "_ZN19APACScaleFactorData11DeserializeER16TBitstreamReaderIjERK11APACICSInfo");
    ++n_scf;
    return ((long(*)(void*,void*,void*))p)(a,b,c);
}
int _ZN15APACLRVQDecoder26lsfsToMdctSpectralEnvelopeEbRKN4apac13aligned_arrayItLm5EEERNS1_IfLm1024EEE(unsigned char a, void* b, void* c)
{
    CHAIN(p, "_ZN15APACLRVQDecoder26lsfsToMdctSpectralEnvelopeEbRKN4apac13aligned_arrayItLm5EEERNS1_IfLm1024EEE");
    ++n_lsf; return ((int(*)(unsigned char,void*,void*))p)(a,b,c);
}
int _ZN15APACLRVQDecoder25ltpToMdctSpectralEnvelopeEttRN4apac13aligned_arrayIfLm1024EEE(unsigned short a, unsigned short b, void* c)
{
    CHAIN(p, "_ZN15APACLRVQDecoder25ltpToMdctSpectralEnvelopeEttRN4apac13aligned_arrayIfLm1024EEE");
    ++n_ltp; return ((int(*)(unsigned short,unsigned short,void*))p)(a,b,c);
}
__attribute__((destructor)) static void report(void)
{
    fprintf(stderr, "[shim] LrvqDeserialize=%ld  SpectralData::Deserialize=%ld  ScaleFactorData::Deserialize=%ld"
                    "  lsfsToMdctSpectralEnvelope=%ld  ltpToMdctSpectralEnvelope=%ld\n",
            n_lrvq, n_spec, n_scf, n_lsf, n_ltp);
}
