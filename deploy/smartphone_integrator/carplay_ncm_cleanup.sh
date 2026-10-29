#!/bin/sh
# smartphone_integrator usb.otgStackCleanupScript (stock: /etc/scripts/stopncm.sh).
#
# SI runs it with no arguments after a CarPlay or CarLife session, so the stock script
# destroys carplay0 by name one second later - also a carplay0 that a phone replugged
# in the meantime has already brought up.  carplay_startup.sh and startncm.sh record
# the NCM generation each CarPlay session started on; hand it to stopncm.sh so only
# that one is destroyed, and run the teardown in the background with NCM_STOP_DELAY=0
# so SI can immediately send CMD: start_stack none before the OTG hardware re-enumerates
# a replugged phone.  CarLife keeps the stock call.
[ -f /tmp/carlife_active ] && exec /etc/scripts/stopncm.sh
NCM_EXPECT_GENERATION=
if [ -r /tmp/ncm_session_generation ]; then
    read NCM_EXPECT_GENERATION < /tmp/ncm_session_generation
elif [ -r /tmp/ncm_generation_carplay0 ]; then
    read NCM_EXPECT_GENERATION < /tmp/ncm_generation_carplay0
fi
rm -f /tmp/ncm_session_generation
if [ -n "$NCM_EXPECT_GENERATION" ]; then
    export NCM_EXPECT_GENERATION
    NCM_STOP_DELAY=0 /etc/scripts/stopncm.sh >/dev/null 2>&1 &
    exit 0
fi
grep -q 'NCM_EXPECT_GENERATION' /etc/scripts/stopncm.sh 2>/dev/null && exit 0
exec /etc/scripts/stopncm.sh
