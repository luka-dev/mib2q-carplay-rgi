// scratch oracle driver #3: checks the closed forms for dequantizeLrvqGain and
// ltpToMdctSpectralEnvelope against the real library.
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
typedef int32_t (*fn_gain)(uint16_t, uint16_t, float*);
typedef int32_t (*fn_ltp)(uint16_t, uint16_t, float*);
typedef int32_t (*fn_lsf)(uint8_t, const uint16_t*, float*);
int main()
{
    setvbuf(stdout, 0, _IONBF, 0);
    void* h = dlopen(getenv("APAC_LIB") ? getenv("APAC_LIB") : "/opt/apac/lib/libSPF_CP_PlugInWrapper.so", RTLD_NOW);
    fn_gain G = (fn_gain)dlsym(h, "_ZN15APACLRVQDecoder18dequantizeLrvqGainEttPf");
    fn_ltp  L = (fn_ltp) dlsym(h, "_ZN15APACLRVQDecoder25ltpToMdctSpectralEnvelopeEttRN4apac13aligned_arrayIfLm1024EEE");
    fn_lsf  S = (fn_lsf) dlsym(h, "_ZN15APACLRVQDecoder26lsfsToMdctSpectralEnvelopeEbRKN4apac13aligned_arrayItLm5EEERNS1_IfLm1024EEE");
    if (!G || !L || !S) { fprintf(stderr, "sym\n"); return 2; }

    int bad = 0, n = 0;
    for (int mode = 0; mode < 2; ++mode)
        for (int i = 0; i < 512; ++i) {
            float got; G((uint16_t)i, (uint16_t)mode, &got);
            float t = (float)i / 511.0f;
            float want = (mode == 1) ? powf(10.0f, t + t) * 10.0f : 10.0f / powf(10.0f, t * 4.0f);
            want *= 65536.0f;
            ++n; if (memcmp(&got, &want, 4)) { if (bad < 3) printf("gain mode=%d i=%d got %.9g want %.9g\n", mode, i, got, want); ++bad; }
        }
    printf("dequantizeLrvqGain: %d/%d bit-exact against  g = (mode?10^(1+2t):10^(1-4t))*65536, t=idx/511\n", n - bad, n);

    // ltpToMdctSpectralEnvelope: predicted |1 - g e^{-j w lag}| on some bin grid,
    // normalised by 1/(1+g).  Solve for the grid empirically.
    static float out[1024];
    int lbad = 0, ln = 0, tailbad = 0;
    double worst = 0;
    for (int gi = 0; gi <= 127; gi += 7)
        for (int lag = 1; lag < 1024; lag += 37) {
            memset(out, 0x5a, sizeof out);
            L((uint16_t)gi, (uint16_t)lag, out);
            float g = ((float)gi / 127.0f) * 0.74f + 0.25f;
            for (int k = 0; k < 256; ++k) {
                double th = 2.0 * M_PI * (double)k * (double)lag / 2048.0;
                double m = sqrt(1.0 - 2.0 * g * cos(th) + (double)g * g) / (1.0 + g);
                double e = fabs(m - out[k]) / (m > 1e-6 ? m : 1e-6);
                if (e > worst) worst = e;
                ++ln; if (e > 2e-3) ++lbad;
            }
            for (int k = 256; k < 1024; ++k) if (out[k] != 0.f) ++tailbad;
        }
    printf("ltpToMdctSpectralEnvelope: %d/%d bins within 0.2%% of |1-g e^-jw*lag|/(1+g) at w=2*pi*k/2048"
           " (worst rel err %.3g); out[256..1023] nonzero: %d\n", ln - lbad, ln, worst, tailbad);

    // lsfsToMdctSpectralEnvelope: cheap invariants only
    uint16_t idx[5] = { 0, 0, 0, 0, 0 };
    int sbad = 0;
    for (int b = 0; b < 2; ++b) {
        memset(out, 0x5a, sizeof out);
        int r = S((uint8_t)b, idx, out);
        for (int k = 256; k < 1024; ++k) if (out[k] != 0.f) ++sbad;
        printf("lsfsToMdctSpectralEnvelope(bool=%d, idx=0): ret=%d out[0..3]=%.6g %.6g %.6g %.6g  tail-nonzero=%d\n",
               b, r, out[0], out[1], out[2], out[3], sbad);
    }
    idx[0] = 511; idx[1] = 511; idx[2] = 511; idx[3] = 511; idx[4] = 511;
    memset(out, 0x5a, sizeof out);
    printf("lsfsToMdctSpectralEnvelope(0, idx=511): ret=%d out[0..3]=%.6g %.6g %.6g %.6g\n",
           S(0, idx, out), out[0], out[1], out[2], out[3]);
    return bad || lbad ? 1 : 0;
}
