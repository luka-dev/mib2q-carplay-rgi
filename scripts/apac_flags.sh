# The one definition of how the APAC decoder is compiled for the head unit.
#
# Sourced by scripts/build_hook_common.sh, the compile core behind build_hook.sh and
# build_hook_protected.sh that produces the shipping .so, and by
# apac_decoder/bench/build_qnx_bench.sh, which measures how much it costs. Those
# two had copies of the same flag list, and a copy that drifts does not announce
# itself: the benchmark would go on reporting a headroom figure for a binary
# that is no longer the one being shipped.
#
# The paths are the ones inside the build container. A caller that compiles
# outside it passes its own -I and appends them.
#
# -fno-threadsafe-statics keeps a function-local static from ever needing
# __cxa_guard_acquire, which this libstdc++ configuration lacks.
# -D_NO_CPP_INLINES keeps the QNX headers from pulling C++ inline definitions in.

APAC_CXX_STD="-std=c++11"
APAC_CXX_OPT="-O2 -g"
APAC_CXX_WARN="-Wall -Wextra -Werror"
APAC_CXX_NORUNTIME="-fno-exceptions -fno-rtti -fno-threadsafe-statics"
APAC_CXX_CODEGEN="-D_NO_CPP_INLINES -fPIC -fdata-sections -ffunction-sections"

# Everything except the include paths, so a caller outside the container can
# supply its own.
APAC_CXXFLAGS_NOINC="$APAC_CXX_STD $APAC_CXX_OPT $APAC_CXX_WARN \
$APAC_CXX_NORUNTIME $APAC_CXX_CODEGEN"

# The C side: the hook itself, apap_crypto.c and spatial_downmix.c.
APAC_CFLAGS_NOINC="-O2 -g -std=gnu99 -Wall -Wextra -fdata-sections -ffunction-sections"

# The stereo upmix DSP (the per-period realtime work).  -O3 and -fno-math-errno
# change no result: no -ffast-math, so nothing is reassociated, and without
# errno sqrtf is a bare vsqrt.  Measured bit-identical and -6.9% instructions
# in tools/stereo-upmix-render/bench.  No -mfpu=neon-vfpv4: fused multiply-add
# changes the arithmetic.  The hook and the bench both compile these files so.
UPMIX_DSP_SRCS="audio/stereo_upmix.c audio/hssr_dsp.c audio/hssr_neon.c audio/hssr_mask.c \
audio/hssr_control_worker.c audio/soundcheck.c"
UPMIX_DSP_CFLAGS="-O3 -fno-math-errno"
