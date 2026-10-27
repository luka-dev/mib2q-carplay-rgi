#!/bin/sh
# smartphone_integrator usb.otgStackCleanupScript (stock: /etc/scripts/stopncm.sh).
#
# SI runs it with no arguments after a CarPlay or CarLife session, so the stock script
# destroys carplay0 by name one second later - also a carplay0 that a phone replugged
# in the meantime has already brought up.  carplay_startup.sh records the NCM generation
# each CarPlay session started on; hand it to stopncm.sh so only that one is destroyed.
# CarLife, or no recorded session, keeps the stock call.  A stock stopncm.sh ignores
# NCM_EXPECT_GENERATION, so this is safe on firmware where the NCM scripts were skipped.
[ -f /tmp/carlife_active ] && exec /etc/scripts/stopncm.sh
if [ -r /tmp/ncm_session_generation ]; then
    read NCM_EXPECT_GENERATION < /tmp/ncm_session_generation
    export NCM_EXPECT_GENERATION
fi
exec /etc/scripts/stopncm.sh
