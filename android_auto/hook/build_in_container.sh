#!/bin/sh
set -eu
SRC=/src/android_auto/hook
OUT=/src/build/android_auto
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
RE=arm-unknown-nto-qnx6.5.0eabi-readelf
B=$(mktemp -d /tmp/sq5-live.XXXXXX)
trap 'rm -rf "$B"' EXIT HUP INT TERM
cp "$SRC"/vendor/*.[ch] "$SRC"/src/*.[ch] "$B/"
cd "$B"
# The verified receiver hook is snapshotted, not edited in the probe project.
sed -e 's@/tmp/sq5_cluster_probe/probe-%ld.log@/tmp/sq5_cluster_live-hook-%ld.log@g' \
    -e 's/sq5-cluster-probe-v1/sq5-cluster-live-v1/g' \
    -e 's/capture_only=1/capture_only=0/g' probe.c > live_hook.c
# Wide band (owner 2026-09-29): 1280x720 (codec_resolution 2) with height_margin 315, so the phone draws
# its UI in a centred 1280x405 band = the cockpit's 1440x455 shape (player.c crops it, x1.125).
# SD flag sq5_cluster_800 = the proven 800x480 request (logs 42/45).
[ "$(grep -c 'real_config(secondary,1,30,0,0,160,3,10000);' live_hook.c)" = 1 ]
sed -i \
    -e 's@real_config(secondary,1,30,0,0,160,3,10000);@if(access("/fs/sda0/sq5_cluster_800",F_OK)==0) real_config(secondary,1,30,0,0,160,3,10000); else real_config(secondary,2,30,0,315,160,3,10000);@' \
    -e 's#codec=800x480@30 callbacks#codec=%s callbacks#' \
    -e 's#registered,video_service,input_service);#registered,video_service,input_service,access("/fs/sda0/sq5_cluster_800",F_OK)==0?"800x480@30":"1280x720@30 hmargin=315 band=1280x405");#' \
    live_hook.c
[ "$(grep -c 'real_config(secondary,2,30,0,315,160,3,10000)' live_hook.c)" = 1 ]
# Protocol version test (2026-09-29): with 1.7 advertised, every hooked session ends with gal replying
# "Sending unexpected message on channel 12" (stock service) < 1 s before the phone resets USB (logs
# run 19, run3). Live run 5: at the stock 1.2 it is stable BUT the phone treats the second sink as another MAIN
# display (owner photos: the AA main UI, not the cluster map). 1.7 stays the DEFAULT (with src/unexpected.c);
# SD flag sq5_cluster_aap12 = the stock 1.2 request.
[ "$(grep -c 'if(!enabled || !registered) { if(real_version) real_version(s); return; }' live_hook.c)" = 1 ]
sed -i 's#if(!enabled || !registered) { if(real_version) real_version(s); return; }#if(!enabled || !registered || access("/fs/sda0/sq5_cluster_aap12",F_OK)==0) { if(enabled \&\& registered) probe_log("version.stock minor=2 (sq5_cluster_aap12)"); if(real_version) real_version(s); return; }#' live_hook.c
[ "$(grep -c 'sq5_cluster_aap12' live_hook.c)" = 1 ]
[ "$(grep -c 'band=1280x405' live_hook.c)" = 1 ]
# Log the first 24 messages on every stock channel except main video (1) too, to see what the phone
# sends on channel 12 at AAP 1.7 (see src/unexpected.c).
[ "$(grep -c 'if(registered && (ch==input_channel || ch==u8(secondary,5))) {' live_hook.c)" = 1 ]
sed -i 's#if(registered \&\& (ch==input_channel || ch==u8(secondary,5))) {#if(registered \&\& (ch==input_channel || ch==u8(secondary,5) || ch>=2)) {#' live_hook.c
[ "$(grep -c 'ch==u8(secondary,5) || ch>=2)' live_hook.c)" = 1 ]
# UiConfig (dark theme + content insets, src/uiconfig.c): on the sink before registration and on the
# discovery-response copy of the cluster VideoConfiguration.
[ "$(grep -c '    registered=real_register(receiver,secondary)!=0;' live_hook.c)" = 1 ]
sed -i 's#^    registered=real_register(receiver,secondary)!=0;#    { void live_uiconfig_sink(void *); live_uiconfig_sink(secondary); }\n    registered=real_register(receiver,secondary)!=0;#' live_hook.c
[ "$(grep -c 'probe_log("discovery.video service=%u cluster=%d metadata_ok=%d",u8(sink,12),sink==secondary,unknown(media,meta,4));' live_hook.c)" = 1 ]
sed -i 's#^\(    probe_log("discovery.video service=%u cluster=%d metadata_ok=%d",u8(sink,12),sink==secondary,unknown(media,meta,4));\)#\1\n    if(sink==secondary) { void live_uiconfig_media(void *); live_uiconfig_media(media); }#' live_hook.c
[ "$(grep -c 'live_uiconfig_' live_hook.c)" = 2 ]
# Density: 120 dpi (run 8) made the driving view too zoomed in (owner); back to 160 by default.
# SD flag sq5_cluster_dpi120 = 120 dpi (labels/cards ~1.33x larger, less map).
[ "$(grep -c 'else real_config(secondary,2,30,0,315,160,3,10000);' live_hook.c)" = 1 ]
sed -i 's#else real_config(secondary,2,30,0,315,160,3,10000);#else real_config(secondary,2,30,0,315,access("/fs/sda0/sq5_cluster_dpi120",F_OK)==0?120:160,3,10000);#' live_hook.c
[ "$(grep -c 'sq5_cluster_dpi120' live_hook.c)" = 1 ]
# 1080p band (owner, run 9: "too zoomed, blurry"): SD flag sq5_cluster_1080 = 1920x1080 (codec 3) with
# height_margin 473 -> centred 1920x607 band, scaled DOWN x0.75 to 1440x455 (sharper, 1.5x more map at the
# same dpi). Costs ~2.25x the decode of 720p; the player log shows whether it keeps up.
# Run 10 (owner: "as if this was made for 1080p"): 1080p is the DEFAULT; SD flag sq5_cluster_720 = 720p band.
sed -i 's#else real_config(secondary,2,30,0,315,access#else if(access("/fs/sda0/sq5_cluster_720",F_OK)!=0) real_config(secondary,3,30,0,473,access("/fs/sda0/sq5_cluster_dpi120",F_OK)==0?120:160,3,10000); else real_config(secondary,2,30,0,315,access#' live_hook.c
sed -i 's#"1280x720@30 hmargin=315 band=1280x405"#(access("/fs/sda0/sq5_cluster_720",F_OK)!=0?"1920x1080@30 hmargin=473 band=1920x607":"1280x720@30 hmargin=315 band=1280x405")#' live_hook.c
[ "$(grep -c 'real_config(secondary,3,30,0,473' live_hook.c)" = 1 ]
# Density from SD file sq5_cluster_dpi (run 11: 160 at 1080p "a lil too zoomed out"); src/uiconfig.c live_dpi():
# default 190 for 1080p, 120 for 720p; read when the phone connects.
sed -i 's#real_config(secondary,3,30,0,473,access("/fs/sda0/sq5_cluster_dpi120",F_OK)==0?120:160,#real_config(secondary,3,30,0,473,live_dpi(3),#; s#real_config(secondary,2,30,0,315,access("/fs/sda0/sq5_cluster_dpi120",F_OK)==0?120:160,#real_config(secondary,2,30,0,315,live_dpi(2),#' live_hook.c
sed -i '0,/^#include/s//int live_dpi(int res);\n#include/' live_hook.c
[ "$(grep -o 'live_dpi(' live_hook.c | wc -l)" = 3 ]
[ "$(grep -c 'sq5_cluster_dpi120' live_hook.c)" = 0 ]
[ "$(grep -c 'band=1920x607' live_hook.c)" = 1 ]
# Navigation 0x8006/0x8007 -> legacy 0x8004/0x8005 (src/navxlate.c), in place, before gal routes it.
[ "$(grep -c '^    if(real_route) real_route(r,ch,shared);' live_hook.c)" = 1 ]
sed -i 's#^    if(real_route) real_route(r,ch,shared);#    { void live_nav_translate(unsigned char,const void *); live_nav_translate(ch,shared); }\n    if(real_route) real_route(r,ch,shared);#' live_hook.c
[ "$(grep -c 'live_nav_translate(ch,shared)' live_hook.c)" = 1 ]
# Per-view layout (src/uiconfig.c live_relayout_tick): runs in the cockpit sink's frame callback.
sed -i 's#^    ++frame_count;#    { void live_relayout_tick(void *); live_relayout_tick(sink); }\n    ++frame_count;#' live_hook.c
[ "$(grep -c 'live_relayout_tick(sink)' live_hook.c)" = 1 ]
# Roller -> cockpit map input channel (src/rotary.c): bind after the phone's KeyBindingRequest on the cluster
# input channel, unbind in both sink destructors (the endpoint goes with them).
sed -i 's#^        probe_log("input.binding response=ok");#        probe_log("input.binding response=ok");\n        { void live_rotary_bind(void *,unsigned char); live_rotary_bind(endpoint,channel); }#' live_hook.c
sed -i 's#registered=0; secondary=NULL; input_channel=256;#registered=0; secondary=NULL; input_channel=256; { void live_rotary_bind(void *,unsigned char); live_rotary_bind(0,0); }#' live_hook.c
[ "$(grep -c 'live_rotary_bind(endpoint,channel)' live_hook.c)" = 1 ]
[ "$(grep -c 'live_rotary_bind(0,0)' live_hook.c)" = 2 ]
# GAL 4.3 request (validated from run 102 onward). sq5_cluster_aap17 is the diagnostic fallback;
# the phone's 6.1 response is rewritten to 1.7 for gal. Every routed message also goes to live_msg_watch.
[ "$(grep -c 'unsigned char msg\[6\]={0,1,0,1,0,7};' live_hook.c)" = 1 ]
sed -i 's#unsigned char msg\[6\]={0,1,0,1,0,7};#unsigned char msg[6]={0,1,0,1,0,7}; if(access("/fs/sda0/sq5_cluster_aap17",F_OK)!=0) { msg[3]=4; msg[5]=3; }#' live_hook.c
sed -i 's#probe_log("version.sent major=1 minor=7");#probe_log("version.sent major=%u minor=%u",msg[3],msg[5]);#' live_hook.c
[ "$(grep -c 'version.sent major=%u minor=%u' live_hook.c)" = 1 ]
[ "$(grep -c '^    if(real_route) real_route(r,ch,shared);' live_hook.c)" = 1 ]
sed -i 's#^    if(real_route) real_route(r,ch,shared);#    if(registered) { void live_msg_watch(unsigned char,const unsigned char *,unsigned); unsigned wn=0; const unsigned char *wp=payload(shared,0,\&wn); live_msg_watch(ch,wp,wn); }\n    if(real_route) real_route(r,ch,shared);#' live_hook.c
[ "$(grep -c 'live_msg_watch(ch,wp,wn)' live_hook.c)" = 1 ]
# 1080p viewport (2026-09-30, MIBSI geometry): margins 480 x 540 -> the phone lays out a centred 1440x540 viewport
# = the cockpit terminal, shown 1:1 by the player (src/uiconfig.c vp_preset). SD flag sq5_cluster_band = old band.
[ "$(grep -c 'real_config(secondary,3,30,0,473,live_dpi(3),' live_hook.c)" = 1 ]
sed -i 's#real_config(secondary,3,30,0,473,live_dpi(3),#real_config(secondary,3,30,access("/fs/sda0/sq5_cluster_band",F_OK)==0?0:480,access("/fs/sda0/sq5_cluster_band",F_OK)==0?473:540,live_dpi(3),#' live_hook.c
[ "$(grep -c '?0:480,' live_hook.c)" = 1 ]
sed -i 's#"1920x1080@30 hmargin=473 band=1920x607"#"1920x1080@30 margins=480x540 viewport=1440x540 (band 473 with sq5_cluster_band)"#' live_hook.c
[ "$(grep -c 'viewport=1440x540 (band 473' live_hook.c)" = 1 ]
# Resolution from the cockpit menu (src/uiconfig.c live_res: menu "res=2" or SD flag sq5_cluster_720 = 720p band).
[ "$(grep -c 'access("/fs/sda0/sq5_cluster_720",F_OK)!=0' live_hook.c)" = 2 ]
sed -i 's#access("/fs/sda0/sq5_cluster_720",F_OK)!=0#live_res()==3#g' live_hook.c
sed -i '0,/^#include/s//int live_res(void);\n#include/' live_hook.c
[ "$(grep -c 'live_res()==3' live_hook.c)" = 2 ]
$CC -O2 -std=gnu99 -Wall -Wextra -Werror -fPIC -shared live_hook.c transport.c unexpected.c uiconfig.c navxlate.c rotary.c versionfix.c -o libsq5_cluster_live.so -lc
sed -e 's/probe_startup.sh/gal_startup.sh/g' -e 's@sq5_cluster_probe@sq5_android_auto@g' config_tool.c > live_config.c
$CC -O2 -std=gnu99 -Wall -Wextra -Werror live_config.c -o config_tool -lc
$CC -O2 -std=gnu99 -Wall -Wextra jarpatch.c -o jarpatch -lc
grep -hoE 'screen_[a-z_]+' player.c cluster_surface.c | sort -u | sed 's/.*/int &(){return 0;}/' > screen_stub.c
$CC -shared -fPIC -Wl,-soname,libscreen.so.1 screen_stub.c -o libscreen.so.1
# Run 127 (lag): same CPU flags as the NEON FFmpeg build (the player was built for the generic ARM default).
$CC -O2 -ftree-vectorize -march=armv7-a -mfloat-abi=softfp -mfpu=neon -std=gnu99 -Wall -Wextra -D__QNX__ \
    -I/src/toolchain/qnx65-abi/include -I/cache/ffmpeg-neon \
    -I"$SRC/vendor/openmax" player.c cluster_surface.c omxdec.c /cache/ffmpeg-neon/libavcodec/libavcodec.a /cache/ffmpeg-neon/libavutil/libavutil.a \
    -Wl,--allow-shlib-undefined ./libscreen.so.1 -lm -lsocket -o cluster-player
# Hardware decode probe (src/omx_probe.c, 2026-09-30): standalone, dlopens the unit's libOmxCore.so.
$CC -O2 -std=gnu99 -Wall -Wextra -I"$SRC/vendor/openmax" omx_probe.c -o omx_probe -lc
mkdir -p "$OUT"
cp libsq5_cluster_live.so cluster-player config_tool jarpatch omx_probe "$OUT/"
cp live_hook.c "$OUT/"
$RE -h cluster-player | grep -E 'Class:|Machine:|Type:'
$RE -d cluster-player libsq5_cluster_live.so | grep NEEDED
echo 'Native live cluster build complete; no SD files changed.'
