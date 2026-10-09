/* scene_simd.h — four bins at a time: the lane abstraction and the per-bin kernels of the scene renderer
 * (scene_engine.c, scene_engine_math.c, scene_clusters.c), NEON on ARMv7 / arm64 and a scalar fallback
 * that performs the same operations in the same order on four lanes, so every build computes the same bits
 * (the host build is -ffp-contract=off; the QNX build has no fused multiply-add).
 *
 * Rules the kernels keep so the two backends cannot drift:
 *  - clamps and min/max are compare + select (vbsl), never vmax/vmin (which order -0 and +0 differently);
 *  - division and square root are the IEEE scalar VFP / AArch64 vector instructions in every lane (ARMv7
 *    NEON has no vector divide or square root; the reciprocal estimates are not correctly rounded and were
 *    not used), so a lane is exactly what the scalar C expression yields;
 *  - atan2, sin/cos and exp are the polynomial forms of scene_engine_math.c, evaluated left to right;
 *  - partial quads (the 513th bin, the 134th group) load n lanes and zero the rest, store n lanes:
 *    a padded lane may hold NaN or inf, it is never stored.
 * Header-only (static inline), no allocation, no libm beyond sqrtf / expf / logf in the lane loops.
 */
#ifndef SCENE_SIMD_H
#define SCENE_SIMD_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SCENE_SIMD_NEON 1
typedef float32x4_t scene_v4;
typedef uint32x4_t  scene_m4;
#else
#define SCENE_SIMD_NEON 0
typedef struct { float f[4]; } scene_v4;
typedef struct { uint32_t u[4]; } scene_m4;
#endif

#if defined(__GNUC__)
#define SCENE_SIMD_INLINE static inline __attribute__((always_inline))
#else
#define SCENE_SIMD_INLINE static inline
#endif

/* ----------------------------------------------------------------------------- lanes */
#if SCENE_SIMD_NEON

SCENE_SIMD_INLINE scene_v4 v4_s(float x) { return vdupq_n_f32(x); }
SCENE_SIMD_INLINE scene_v4 v4_zero(void) { return vdupq_n_f32(0.0f); }
SCENE_SIMD_INLINE scene_v4 v4_ld(const float *p) { return vld1q_f32(p); }
SCENE_SIMD_INLINE void     v4_st(float *p, scene_v4 v) { vst1q_f32(p, v); }
SCENE_SIMD_INLINE scene_v4 v4_add(scene_v4 a, scene_v4 b) { return vaddq_f32(a, b); }
SCENE_SIMD_INLINE scene_v4 v4_sub(scene_v4 a, scene_v4 b) { return vsubq_f32(a, b); }
SCENE_SIMD_INLINE scene_v4 v4_mul(scene_v4 a, scene_v4 b) { return vmulq_f32(a, b); }
SCENE_SIMD_INLINE scene_v4 v4_neg(scene_v4 a) { return vnegq_f32(a); }
SCENE_SIMD_INLINE scene_v4 v4_abs(scene_v4 a) { return vabsq_f32(a); }
SCENE_SIMD_INLINE scene_m4 v4_lt(scene_v4 a, scene_v4 b) { return vcltq_f32(a, b); }
SCENE_SIMD_INLINE scene_m4 v4_le(scene_v4 a, scene_v4 b) { return vcleq_f32(a, b); }
SCENE_SIMD_INLINE scene_m4 v4_gt(scene_v4 a, scene_v4 b) { return vcgtq_f32(a, b); }
SCENE_SIMD_INLINE scene_m4 v4_ge(scene_v4 a, scene_v4 b) { return vcgeq_f32(a, b); }
SCENE_SIMD_INLINE scene_m4 v4_eq(scene_v4 a, scene_v4 b) { return vceqq_f32(a, b); }
SCENE_SIMD_INLINE scene_m4 m4_and(scene_m4 a, scene_m4 b) { return vandq_u32(a, b); }
SCENE_SIMD_INLINE scene_m4 m4_or(scene_m4 a, scene_m4 b) { return vorrq_u32(a, b); }
SCENE_SIMD_INLINE scene_m4 m4_not(scene_m4 a) { return vmvnq_u32(a); }
SCENE_SIMD_INLINE scene_v4 v4_sel(scene_m4 m, scene_v4 a, scene_v4 b) { return vbslq_f32(m, a, b); }   /* m ? a : b per lane */
SCENE_SIMD_INLINE int      m4_any(scene_m4 m)
{
    const uint32x2_t o = vorr_u32(vget_low_u32(m), vget_high_u32(m));
    return (vget_lane_u32(o, 0) | vget_lane_u32(o, 1)) != 0u;
}
/* all-ones where the sign bit is set (-0.0 included): the sign_bit() of scene_fast_atan2f */
SCENE_SIMD_INLINE scene_m4 v4_signbit(scene_v4 a) { return vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_f32(a), 31)); }
SCENE_SIMD_INLINE float    v4_lane0(scene_v4 a) { return vgetq_lane_f32(a, 0); }
SCENE_SIMD_INLINE float    v4_lane(scene_v4 a, int i)
{
    switch (i) {
    case 0:  return vgetq_lane_f32(a, 0);
    case 1:  return vgetq_lane_f32(a, 1);
    case 2:  return vgetq_lane_f32(a, 2);
    default: return vgetq_lane_f32(a, 3);
    }
}
SCENE_SIMD_INLINE scene_v4 v4_setlane(scene_v4 a, int i, float x)
{
    switch (i) {
    case 0:  return vsetq_lane_f32(x, a, 0);
    case 1:  return vsetq_lane_f32(x, a, 1);
    case 2:  return vsetq_lane_f32(x, a, 2);
    default: return vsetq_lane_f32(x, a, 3);
    }
}
/* max over the lanes (exact: no rounding, so the order is immaterial for finite lanes) */
SCENE_SIMD_INLINE float v4_hmax(scene_v4 a)
{
    float32x2_t m = vpmax_f32(vget_low_f32(a), vget_high_f32(a));
    m = vpmax_f32(m, m);
    return vget_lane_f32(m, 0);
}
#if defined(__aarch64__)
SCENE_SIMD_INLINE scene_v4 v4_div(scene_v4 a, scene_v4 b) { return vdivq_f32(a, b); }
SCENE_SIMD_INLINE scene_v4 v4_sqrt(scene_v4 a) { return vsqrtq_f32(a); }
#else
/* ARMv7: the VFP scalar divide / square root on the four S lanes of a pinned Q register (q0 / q1 through the
 * explicit register variables GCC documents for asm operands), in place: IEEE, exactly the scalar C result,
 * and no lane traffic through core registers (GCC 8's vgetq_lane / vsetq_lane route every lane through a
 * core register: 28 instructions per quad division, 20 per square root; this is 4 plus a register move or
 * two). The asm vsqrt also skips the errno check GCC adds to sqrtf() without -fno-math-errno. */
SCENE_SIMD_INLINE scene_v4 v4_div(scene_v4 a, scene_v4 b)
{
    register scene_v4 n __asm__("q0") = a;
    register scene_v4 d __asm__("q1") = b;
    __asm__("vdiv.f32 s0, s0, s4\n\tvdiv.f32 s1, s1, s5\n\tvdiv.f32 s2, s2, s6\n\tvdiv.f32 s3, s3, s7" : "+w"(n) : "w"(d));
    return n;
}
SCENE_SIMD_INLINE scene_v4 v4_sqrt(scene_v4 a)
{
    register scene_v4 v __asm__("q0") = a;
    __asm__("vsqrt.f32 s0, s0\n\tvsqrt.f32 s1, s1\n\tvsqrt.f32 s2, s2\n\tvsqrt.f32 s3, s3" : "+w"(v));
    return v;
}
#endif
/* interleaved (re, im) pairs <-> two lane vectors */
SCENE_SIMD_INLINE void v4_ld2(const float *p, scene_v4 *re, scene_v4 *im)
{
    const float32x4x2_t v = vld2q_f32(p);
    *re = v.val[0];
    *im = v.val[1];
}
SCENE_SIMD_INLINE void v4_st2(float *p, scene_v4 re, scene_v4 im)
{
    float32x4x2_t v;
    v.val[0] = re;
    v.val[1] = im;
    vst2q_f32(p, v);
}
/* four interleaved channels of four samples <-> four lane vectors (the overlap-add ring) */
SCENE_SIMD_INLINE void v4_ld4(const float *p, scene_v4 *a, scene_v4 *b, scene_v4 *c, scene_v4 *d)
{
    const float32x4x4_t v = vld4q_f32(p);
    *a = v.val[0]; *b = v.val[1]; *c = v.val[2]; *d = v.val[3];
}
/* lane i to p[i * stride] (the six-channel output) */
SCENE_SIMD_INLINE void v4_st_stride(float *p, scene_v4 v, int stride)
{
    vst1q_lane_f32(p, v, 0);
    vst1q_lane_f32(p + stride, v, 1);
    vst1q_lane_f32(p + 2 * stride, v, 2);
    vst1q_lane_f32(p + 3 * stride, v, 3);
}
/* lanes in reverse memory order: p[3], p[2], p[1], p[0] (the Hermitian mirror of the real-pair packing) */
SCENE_SIMD_INLINE scene_v4 v4_ldrev(const float *p)
{
    const scene_v4 v = vrev64q_f32(vld1q_f32(p));
    return vcombine_f32(vget_high_f32(v), vget_low_f32(v));
}
SCENE_SIMD_INLINE void v4_strev(float *p, scene_v4 v)
{
    const scene_v4 r = vrev64q_f32(v);
    vst1q_f32(p, vcombine_f32(vget_high_f32(r), vget_low_f32(r)));
}
/* 4x4 transpose of four lane vectors (row r of the input becomes lane r of every output) */
SCENE_SIMD_INLINE void v4_transpose(scene_v4 *a, scene_v4 *b, scene_v4 *c, scene_v4 *d)
{
    const float32x4x2_t t0 = vtrnq_f32(*a, *b), t1 = vtrnq_f32(*c, *d);
    *a = vcombine_f32(vget_low_f32(t0.val[0]), vget_low_f32(t1.val[0]));
    *b = vcombine_f32(vget_low_f32(t0.val[1]), vget_low_f32(t1.val[1]));
    *c = vcombine_f32(vget_high_f32(t0.val[0]), vget_high_f32(t1.val[0]));
    *d = vcombine_f32(vget_high_f32(t0.val[1]), vget_high_f32(t1.val[1]));
}
/* int -> float and back (truncation toward zero, like the C cast), lane shifts, int lanes */
SCENE_SIMD_INLINE int32x4_t v4_to_i(scene_v4 a) { return vcvtq_s32_f32(a); }
SCENE_SIMD_INLINE scene_v4  v4_from_i(int32x4_t a) { return vcvtq_f32_s32(a); }
SCENE_SIMD_INLINE scene_m4  i4_eq(int32x4_t a, int b) { return vceqq_s32(a, vdupq_n_s32(b)); }
SCENE_SIMD_INLINE int32x4_t i4_and(int32x4_t a, int b) { return vandq_s32(a, vdupq_n_s32(b)); }
SCENE_SIMD_INLINE scene_v4  v4_pow2i(int32x4_t n)        /* 2^n for -126 <= n <= 127, built from the exponent field */
{
    return vreinterpretq_f32_s32(vshlq_n_s32(vaddq_s32(n, vdupq_n_s32(127)), 23));
}
typedef int32x4_t scene_i4;

#else /* scalar fallback: the same operations lane by lane */

SCENE_SIMD_INLINE scene_v4 v4_s(float x) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = x; return r; }
SCENE_SIMD_INLINE scene_v4 v4_zero(void) { return v4_s(0.0f); }
SCENE_SIMD_INLINE scene_v4 v4_ld(const float *p) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = p[i]; return r; }
SCENE_SIMD_INLINE void     v4_st(float *p, scene_v4 v) { int i; for (i = 0; i < 4; i++) p[i] = v.f[i]; }
SCENE_SIMD_INLINE scene_v4 v4_add(scene_v4 a, scene_v4 b) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = a.f[i] + b.f[i]; return r; }
SCENE_SIMD_INLINE scene_v4 v4_sub(scene_v4 a, scene_v4 b) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = a.f[i] - b.f[i]; return r; }
SCENE_SIMD_INLINE scene_v4 v4_mul(scene_v4 a, scene_v4 b) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = a.f[i] * b.f[i]; return r; }
SCENE_SIMD_INLINE scene_v4 v4_div(scene_v4 a, scene_v4 b) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = a.f[i] / b.f[i]; return r; }
SCENE_SIMD_INLINE scene_v4 v4_sqrt(scene_v4 a) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = sqrtf(a.f[i]); return r; }
SCENE_SIMD_INLINE scene_v4 v4_neg(scene_v4 a) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = -a.f[i]; return r; }
SCENE_SIMD_INLINE scene_v4 v4_abs(scene_v4 a) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = fabsf(a.f[i]); return r; }
SCENE_SIMD_INLINE scene_m4 v4_lt(scene_v4 a, scene_v4 b) { scene_m4 r; int i; for (i = 0; i < 4; i++) r.u[i] = a.f[i] < b.f[i] ? 0xFFFFFFFFu : 0u; return r; }
SCENE_SIMD_INLINE scene_m4 v4_le(scene_v4 a, scene_v4 b) { scene_m4 r; int i; for (i = 0; i < 4; i++) r.u[i] = a.f[i] <= b.f[i] ? 0xFFFFFFFFu : 0u; return r; }
SCENE_SIMD_INLINE scene_m4 v4_gt(scene_v4 a, scene_v4 b) { scene_m4 r; int i; for (i = 0; i < 4; i++) r.u[i] = a.f[i] > b.f[i] ? 0xFFFFFFFFu : 0u; return r; }
SCENE_SIMD_INLINE scene_m4 v4_ge(scene_v4 a, scene_v4 b) { scene_m4 r; int i; for (i = 0; i < 4; i++) r.u[i] = a.f[i] >= b.f[i] ? 0xFFFFFFFFu : 0u; return r; }
SCENE_SIMD_INLINE scene_m4 v4_eq(scene_v4 a, scene_v4 b) { scene_m4 r; int i; for (i = 0; i < 4; i++) r.u[i] = a.f[i] == b.f[i] ? 0xFFFFFFFFu : 0u; return r; }
SCENE_SIMD_INLINE scene_m4 m4_and(scene_m4 a, scene_m4 b) { scene_m4 r; int i; for (i = 0; i < 4; i++) r.u[i] = a.u[i] & b.u[i]; return r; }
SCENE_SIMD_INLINE scene_m4 m4_or(scene_m4 a, scene_m4 b) { scene_m4 r; int i; for (i = 0; i < 4; i++) r.u[i] = a.u[i] | b.u[i]; return r; }
SCENE_SIMD_INLINE scene_m4 m4_not(scene_m4 a) { scene_m4 r; int i; for (i = 0; i < 4; i++) r.u[i] = ~a.u[i]; return r; }
SCENE_SIMD_INLINE scene_v4 v4_sel(scene_m4 m, scene_v4 a, scene_v4 b) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = m.u[i] ? a.f[i] : b.f[i]; return r; }
SCENE_SIMD_INLINE int      m4_any(scene_m4 m) { return (m.u[0] | m.u[1] | m.u[2] | m.u[3]) != 0u; }
SCENE_SIMD_INLINE scene_m4 v4_signbit(scene_v4 a)
{
    scene_m4 r; int i;
    for (i = 0; i < 4; i++) { union { float f; uint32_t u; } b; b.f = a.f[i]; r.u[i] = (b.u >> 31) ? 0xFFFFFFFFu : 0u; }
    return r;
}
SCENE_SIMD_INLINE float    v4_lane0(scene_v4 a) { return a.f[0]; }
SCENE_SIMD_INLINE float    v4_lane(scene_v4 a, int i) { return a.f[i]; }
SCENE_SIMD_INLINE scene_v4 v4_setlane(scene_v4 a, int i, float x) { a.f[i] = x; return a; }
SCENE_SIMD_INLINE float    v4_hmax(scene_v4 a)
{
    float m = a.f[0];
    if (a.f[1] > m) m = a.f[1];
    if (a.f[2] > m) m = a.f[2];
    if (a.f[3] > m) m = a.f[3];
    return m;
}
SCENE_SIMD_INLINE void v4_ld2(const float *p, scene_v4 *re, scene_v4 *im)
{
    int i;
    for (i = 0; i < 4; i++) { re->f[i] = p[2 * i]; im->f[i] = p[2 * i + 1]; }
}
SCENE_SIMD_INLINE void v4_st2(float *p, scene_v4 re, scene_v4 im)
{
    int i;
    for (i = 0; i < 4; i++) { p[2 * i] = re.f[i]; p[2 * i + 1] = im.f[i]; }
}
SCENE_SIMD_INLINE void v4_ld4(const float *p, scene_v4 *a, scene_v4 *b, scene_v4 *c, scene_v4 *d)
{
    int i;
    for (i = 0; i < 4; i++) { a->f[i] = p[4 * i]; b->f[i] = p[4 * i + 1]; c->f[i] = p[4 * i + 2]; d->f[i] = p[4 * i + 3]; }
}
SCENE_SIMD_INLINE void v4_st_stride(float *p, scene_v4 v, int stride)
{
    int i;
    for (i = 0; i < 4; i++) p[i * stride] = v.f[i];
}
SCENE_SIMD_INLINE scene_v4 v4_ldrev(const float *p) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = p[3 - i]; return r; }
SCENE_SIMD_INLINE void v4_strev(float *p, scene_v4 v) { int i; for (i = 0; i < 4; i++) p[3 - i] = v.f[i]; }
SCENE_SIMD_INLINE void v4_transpose(scene_v4 *a, scene_v4 *b, scene_v4 *c, scene_v4 *d)
{
    scene_v4 m[4]; int i, j;
    m[0] = *a; m[1] = *b; m[2] = *c; m[3] = *d;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) {
        const float x = m[j].f[i];
        if (i == 0) a->f[j] = x; else if (i == 1) b->f[j] = x; else if (i == 2) c->f[j] = x; else d->f[j] = x;
    }
}
typedef struct { int32_t i[4]; } scene_i4;
/* vcvt.s32.f32 semantics: toward zero, saturating, NaN -> 0 (a padded lane may hold anything) */
SCENE_SIMD_INLINE scene_i4 v4_to_i(scene_v4 a)
{
    scene_i4 r; int i;
    for (i = 0; i < 4; i++) {
        const float x = a.f[i];
        r.i[i] = !(x == x) ? 0 : x >= 2147483648.0f ? INT32_MAX : x <= -2147483648.0f ? INT32_MIN : (int32_t)x;
    }
    return r;
}
SCENE_SIMD_INLINE scene_v4 v4_from_i(scene_i4 a) { scene_v4 r; int i; for (i = 0; i < 4; i++) r.f[i] = (float)a.i[i]; return r; }
SCENE_SIMD_INLINE scene_m4 i4_eq(scene_i4 a, int b) { scene_m4 r; int i; for (i = 0; i < 4; i++) r.u[i] = a.i[i] == b ? 0xFFFFFFFFu : 0u; return r; }
SCENE_SIMD_INLINE scene_i4 i4_and(scene_i4 a, int b) { scene_i4 r; int i; for (i = 0; i < 4; i++) r.i[i] = a.i[i] & b; return r; }
SCENE_SIMD_INLINE scene_v4 v4_pow2i(scene_i4 n)
{
    scene_v4 r; int i;
    for (i = 0; i < 4; i++) { union { uint32_t u; float f; } b; b.u = (uint32_t)(n.i[i] + 127) << 23; r.f[i] = b.f; }
    return r;
}

#endif

/* ----------------------------------------------------------------------------- partial quads and gathers */
/* n lanes from p (1 <= n <= 4), the rest zero; n == 4 is the plain load (constant-folded at the call sites) */
SCENE_SIMD_INLINE scene_v4 v4_ld_n(const float *p, int n)
{
    if (n >= 4) return v4_ld(p);
    {
        scene_v4 r = v4_zero();
        int i;
        for (i = 0; i < n; i++) r = v4_setlane(r, i, p[i]);
        return r;
    }
}
SCENE_SIMD_INLINE void v4_st_n(float *p, scene_v4 v, int n)
{
    if (n >= 4) { v4_st(p, v); return; }
    {
        int i;
        for (i = 0; i < n; i++) p[i] = v4_lane(v, i);
    }
}
SCENE_SIMD_INLINE void v4_ld2_n(const float *p, int n, scene_v4 *re, scene_v4 *im)
{
    if (n >= 4) { v4_ld2(p, re, im); return; }
    {
        int i;
        *re = v4_zero(); *im = v4_zero();
        for (i = 0; i < n; i++) { *re = v4_setlane(*re, i, p[2 * i]); *im = v4_setlane(*im, i, p[2 * i + 1]); }
    }
}
SCENE_SIMD_INLINE void v4_st2_n(float *p, scene_v4 re, scene_v4 im, int n)
{
    if (n >= 4) { v4_st2(p, re, im); return; }
    {
        int i;
        for (i = 0; i < n; i++) { p[2 * i] = v4_lane(re, i); p[2 * i + 1] = v4_lane(im, i); }
    }
}
/* table[idx[i]] for n lanes (the mask rows and the per-group values; no vector gather on ARMv7) */
SCENE_SIMD_INLINE scene_v4 v4_gather_n(const float *table, const int *idx, int n)
{
    scene_v4 r = v4_zero();
    int i;
    for (i = 0; i < n; i++) r = v4_setlane(r, i, table[idx[i]]);
    return r;
}
/* int flags[n] != 0 as a lane mask */
SCENE_SIMD_INLINE scene_m4 m4_flags_n(const int *flags, int n)
{
    scene_v4 r = v4_zero();
    int i;
    for (i = 0; i < n; i++) r = v4_setlane(r, i, flags[i] ? 1.0f : 0.0f);
    return v4_gt(r, v4_zero());
}
/* the mask as int 0/1 lanes stored to out[n] (the valid[] output of centre_removed_pan) */
SCENE_SIMD_INLINE void m4_st_flags_n(int *out, scene_m4 m, int n)
{
    const scene_v4 r = v4_sel(m, v4_s(1.0f), v4_zero());
    int i;
    for (i = 0; i < n; i++) out[i] = v4_lane(r, i) != 0.0f;
}

/* ----------------------------------------------------------------------------- derived lane ops */
SCENE_SIMD_INLINE scene_v4 v4_min(scene_v4 a, scene_v4 b) { return v4_sel(v4_lt(a, b), a, b); }   /* a < b ? a : b */
SCENE_SIMD_INLINE scene_v4 v4_max0(scene_v4 a) { return v4_sel(v4_lt(a, v4_zero()), v4_zero(), a); }   /* a < 0 ? 0 : a */
SCENE_SIMD_INLINE scene_v4 v4_clip01(scene_v4 x)                                                     /* x < 0 ? 0 : (x > 1 ? 1 : x) */
{
    const scene_v4 one = v4_s(1.0f);
    return v4_sel(v4_lt(x, v4_zero()), v4_zero(), v4_sel(v4_gt(x, one), one, x));
}
SCENE_SIMD_INLINE scene_v4 v4_sign(scene_v4 x)                                                       /* x > 0 ? 1 : (x < 0 ? -1 : 0) */
{
    return v4_sel(v4_gt(x, v4_zero()), v4_s(1.0f), v4_sel(v4_lt(x, v4_zero()), v4_s(-1.0f), v4_zero()));
}
/* x += a * (y - x): the EMA step of every smoother */
SCENE_SIMD_INLINE scene_v4 v4_ema(scene_v4 x, scene_v4 a, scene_v4 y) { return v4_add(x, v4_mul(a, v4_sub(y, x))); }
/* scene_smoothstep: t = clip((x - lo) / max(hi - lo, 1e-9)); t * t * (3 - 2 t) */
SCENE_SIMD_INLINE scene_v4 v4_smoothstep(scene_v4 x, float lo, float hi)
{
    float d = hi - lo;
    scene_v4 t;
    if (d < 1e-9f) d = 1e-9f;
    t = v4_clip01(v4_div(v4_sub(x, v4_s(lo)), v4_s(d)));
    return v4_mul(v4_mul(t, t), v4_sub(v4_s(3.0f), v4_mul(v4_s(2.0f), t)));
}
/* sqrt(max(1 - (1 - a)^2, 0)): the energy-conserving rear gain */
SCENE_SIMD_INLINE scene_v4 v4_conserving(scene_v4 a)
{
    const scene_v4 oma = v4_sub(v4_s(1.0f), a);
    return v4_sqrt(v4_max0(v4_sub(v4_s(1.0f), v4_mul(oma, oma))));
}
/* complex lanes: (ar, ai) * (br, bi) */
SCENE_SIMD_INLINE void v4_cmul(scene_v4 ar, scene_v4 ai, scene_v4 br, scene_v4 bi, scene_v4 *rr, scene_v4 *ri)
{
    *rr = v4_sub(v4_mul(ar, br), v4_mul(ai, bi));
    *ri = v4_add(v4_mul(ar, bi), v4_mul(ai, br));
}

/* ----------------------------------------------------------------------------- transcendentals */
/* atan2 within ~2e-7 rad (scene_fast_atan2f): the quadrant selects the operands of ONE division, then
 * Cephes' atanf polynomial; quadrants and signed zeros as C99 atan2 (atan2(+-0, -0) = +-pi). */
SCENE_SIMD_INLINE scene_v4 v4_atan2(scene_v4 y, scene_v4 x)
{
    const scene_v4 ax = v4_abs(x), ay = v4_abs(y), zero = v4_zero();
    const scene_m4 c1 = v4_gt(ay, v4_mul(v4_s(2.414213562373095f), ax));            /* |y| > |x| tan(3pi/8) */
    const scene_m4 c2 = v4_gt(ay, v4_mul(v4_s(0.4142135623730950f), ax));
    const scene_m4 c3 = v4_gt(ax, zero);
    const scene_v4 base = v4_sel(c1, v4_s(1.5707963267948966f), v4_sel(c2, v4_s(0.7853981633974483f), zero));
    const scene_v4 num = v4_sel(c1, v4_neg(ax), v4_sel(c2, v4_sub(ay, ax), ay));
    const scene_v4 den = v4_sel(c1, ay, v4_sel(c2, v4_add(ay, ax), ax));
    const scene_v4 u = v4_sel(m4_or(m4_or(c1, c2), c3), v4_div(num, den), zero);     /* (+-0, +-0): u = 0 */
    const scene_v4 z = v4_mul(u, u);
    scene_v4 t = v4_mul(v4_s(8.05374449538e-2f), z);
    t = v4_sub(t, v4_s(1.38776856032e-1f));
    t = v4_mul(t, z);
    t = v4_add(t, v4_s(1.99777106478e-1f));
    t = v4_mul(t, z);
    t = v4_sub(t, v4_s(3.33329491539e-1f));
    t = v4_mul(t, z);
    t = v4_mul(t, u);
    t = v4_add(t, u);
    t = v4_add(t, base);
    t = v4_sel(v4_signbit(x), v4_sub(v4_s(3.1415927410125732f), t), t);          /* float pi, what atan2f(0, -1) returns */
    return v4_sel(v4_signbit(y), v4_neg(t), t);
}

/* sin and cos of x in [-pi, pi] within ~1.5e-7 (scene_fast_sincosf): nearest quadrant, the residual with a
 * two-part pi/2, Cephes' polynomials on [-pi/4, pi/4], the quadrant rotation by selects. */
SCENE_SIMD_INLINE void v4_sincos(scene_v4 x, scene_v4 *sn, scene_v4 *cs)
{
    const scene_v4 half = v4_sel(v4_ge(x, v4_zero()), v4_s(0.5f), v4_s(-0.5f));
    const scene_i4 k = v4_to_i(v4_add(v4_mul(x, v4_s(0.6366197723675814f)), half));
    const scene_v4 kf = v4_from_i(k);
    const scene_v4 r = v4_sub(v4_sub(x, v4_mul(kf, v4_s(1.5707963705062866f))), v4_mul(kf, v4_s(-4.3711388286737929e-8f)));
    const scene_v4 z = v4_mul(r, r);
    scene_v4 s_, c_, sgn_s, sgn_c;
    scene_m4 q1, q2, q3;
    s_ = v4_mul(v4_s(-1.9515295891e-4f), z);
    s_ = v4_add(s_, v4_s(8.3321608736e-3f));
    s_ = v4_mul(s_, z);
    s_ = v4_sub(s_, v4_s(1.6666654611e-1f));
    s_ = v4_mul(s_, z);
    s_ = v4_mul(s_, r);
    s_ = v4_add(s_, r);
    c_ = v4_mul(v4_s(2.443315711809948e-5f), z);
    c_ = v4_sub(c_, v4_s(1.388731625493765e-3f));
    c_ = v4_mul(c_, z);
    c_ = v4_add(c_, v4_s(4.166664568298827e-2f));
    c_ = v4_mul(c_, z);
    c_ = v4_mul(c_, z);
    c_ = v4_sub(c_, v4_mul(v4_s(0.5f), z));
    c_ = v4_add(c_, v4_s(1.0f));
    {
        const scene_i4 k3 = i4_and(k, 3);
        q1 = i4_eq(k3, 1); q2 = i4_eq(k3, 2); q3 = i4_eq(k3, 3);
    }
    sgn_s = v4_neg(s_); sgn_c = v4_neg(c_);
    *sn = v4_sel(q1, c_, v4_sel(q2, sgn_s, v4_sel(q3, sgn_c, s_)));      /* k&3: 0 (s, c) 1 (c, -s) 2 (-s, -c) 3 (-c, s) */
    *cs = v4_sel(q1, sgn_s, v4_sel(q2, sgn_c, v4_sel(q3, s_, c_)));
}

/* exp(-d) for 0 <= d < 3.125 (the clipped membership kernel): d = n ln2 - r with |r| <= ln2/2, Cephes' expf
 * polynomial on r, 2^-n from the exponent field. ~1e-7 relative. Lanes with d outside the range are
 * discarded by the caller's select. */
SCENE_SIMD_INLINE scene_v4 v4_exp_neg(scene_v4 d)
{
    const scene_v4 x = v4_neg(d);
    const scene_i4 n = v4_to_i(v4_sub(v4_mul(x, v4_s(1.44269504088896341f)), v4_s(0.5f)));   /* x <= 0: round half away */
    const scene_v4 nf = v4_from_i(n);
    const scene_v4 r = v4_sub(v4_sub(x, v4_mul(nf, v4_s(0.693145751953125f))), v4_mul(nf, v4_s(1.428606765330187e-6f)));
    const scene_v4 z = v4_mul(r, r);
    scene_v4 p = v4_mul(v4_s(1.9875691500e-4f), r);
    p = v4_add(p, v4_s(1.3981999507e-3f));
    p = v4_mul(p, r);
    p = v4_add(p, v4_s(8.3334519073e-3f));
    p = v4_mul(p, r);
    p = v4_add(p, v4_s(4.1665795894e-2f));
    p = v4_mul(p, r);
    p = v4_add(p, v4_s(1.6666665459e-1f));
    p = v4_mul(p, r);
    p = v4_add(p, v4_s(5.0000001201e-1f));
    p = v4_mul(p, z);
    p = v4_add(p, r);
    p = v4_add(p, v4_s(1.0f));
    return v4_mul(p, v4_pow2i(n));
}

/* ----------------------------------------------------------------------------- the per-bin kernels */
/* Closed-form eigen decomposition (spec section 3) on (PL, PR, C): P_N and the eigen spread Rt. P_N as
 * 2 (PL PR - |C|^2) / (T + Rt) (the float32-safe form, scene_engine_math.c). */
SCENE_SIMD_INLINE void v4_decompose_pn(scene_v4 pl, scene_v4 pr, scene_v4 cr, scene_v4 ci, scene_v4 *pn, scene_v4 *Rt)
{
    const scene_v4 T = v4_add(pl, pr), d = v4_sub(pl, pr);
    const scene_v4 c2 = v4_add(v4_mul(cr, cr), v4_mul(ci, ci));
    const scene_v4 q = v4_max0(v4_add(v4_mul(d, d), v4_mul(v4_s(4.0f), c2)));          /* (PL - PR)^2 + 4 |C|^2 */
    const scene_v4 R = v4_sqrt(q);
    const scene_v4 det = v4_max0(v4_sub(v4_mul(pl, pr), c2));
    const scene_v4 TR = v4_add(T, R);
    *pn = v4_sel(v4_gt(TR, v4_zero()), v4_div(v4_mul(v4_s(2.0f), det), TR), v4_zero());
    *Rt = R;
}
/* the regularised direction (a, b) of the primary */
SCENE_SIMD_INLINE void v4_decompose_ab(scene_v4 pl, scene_v4 pr, scene_v4 pn, scene_v4 *a, scene_v4 *b)
{
    const scene_v4 pa = v4_max0(v4_sub(pl, pn)), pb = v4_max0(v4_sub(pr, pn));
    const scene_v4 k = v4_add(v4_mul(v4_s(0.5f), pn), v4_s(1e-12f));
    const scene_v4 den = v4_add(v4_add(pa, pb), v4_mul(v4_s(2.0f), k));
    *a = v4_sqrt(v4_div(v4_add(pa, k), den));
    *b = v4_sqrt(v4_div(v4_add(pb, k), den));
}
/* theta = atan2(C) * Rt / (Rt + P_N + 1e-12): phase only where a primary exists */
SCENE_SIMD_INLINE scene_v4 v4_decompose_theta(scene_v4 cr, scene_v4 ci, scene_v4 Rt, scene_v4 pn)
{
    return v4_mul(v4_atan2(ci, cr), v4_div(Rt, v4_add(v4_add(Rt, pn), v4_s(1e-12f))));
}

/* Pan of the off-centre content of a cell (scene_centre_removed_pan): tol = 1.5 P_N (or 0 without PN). */
SCENE_SIMD_INLINE void v4_centre_removed_pan(scene_v4 pl, scene_v4 pr, scene_v4 cr, float eps, scene_v4 tol,
                                             scene_v4 *p, scene_v4 *e, scene_m4 *valid)
{
    const scene_v4 zero = v4_zero(), veps = v4_s(eps), ntol = v4_neg(tol);
    const scene_v4 num = v4_sub(pl, cr), den = v4_sub(cr, pr), sig = v4_add(num, den);
    const scene_m4 left = m4_and(m4_and(v4_ge(num, ntol), v4_ge(den, ntol)), v4_gt(sig, veps));
    const scene_m4 right = m4_and(m4_and(v4_le(num, tol), v4_le(den, tol)), v4_lt(sig, v4_neg(veps)));
    const scene_m4 v = m4_or(left, right);
    const scene_v4 nn = v4_max0(v4_sel(left, num, v4_neg(num)));
    const scene_v4 dd = v4_max0(v4_sel(left, den, v4_neg(den)));
    const scene_v4 n2 = v4_mul(nn, nn), d2 = v4_mul(dd, dd);
    scene_v4 pv = v4_sel(v, v4_div(v4_sub(d2, n2), v4_add(v4_add(n2, d2), v4_s(1e-30f))), zero);
    pv = v4_sel(left, v4_neg(v4_abs(pv)), v4_abs(pv));
    *p = pv;
    *e = v4_sel(v, v4_abs(sig), zero);
    *valid = v;
}

#endif
