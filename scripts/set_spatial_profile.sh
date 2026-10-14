#!/bin/sh
# Select the persistent CarPlay audio profile on the HU without replacing the
# rest of carplay_startup.sh.  The new environment is inherited by the next
# dio_manager generation (normally the next phone connection).

set -eu

DEFAULT_SSH=${AUDI_WORKSPACE:-$HOME/Desktop/AUDI_2}/audi_ssh.sh
AUDI_SSH=${AUDI_SSH:-$DEFAULT_SSH}
REMOTE_STARTUP=/mnt/app/root/hooks/carplay_startup.sh

usage()
{
    echo "usage: $0 {status|spatial-5.1|spatial-5.1.2|atmos-downmix|atmos-drop|atmos-passthrough|fallback-stereo}" >&2
    exit 2
}

[ -x "$AUDI_SSH" ] || {
    echo "Audi SSH helper is not executable: $AUDI_SSH" >&2
    echo "Override it with AUDI_SSH=/path/to/audi_ssh.sh" >&2
    exit 1
}

profile=${1:-status}
case "$profile" in
    status)
        "$AUDI_SSH" exec \
            "grep -n '^export AUDIO_SPATIAL' '$REMOTE_STARTUP'; pidin ar | grep dio_manager || true"
        exit 0
        ;;
    spatial-5.1)
        audio_spatial=1
        audio_atmos=0
        atmos_mode=downmix
        ;;
    spatial-5.1.2|atmos-downmix)
        audio_spatial=1
        audio_atmos=1
        atmos_mode=downmix
        ;;
    atmos-drop)
        audio_spatial=1
        audio_atmos=1
        atmos_mode=drop
        ;;
    atmos-passthrough)
        audio_spatial=1
        audio_atmos=1
        atmos_mode=passthrough
        ;;
    fallback-stereo)
        audio_spatial=0
        audio_atmos=0
        atmos_mode=downmix
        ;;
    *)
        usage
        ;;
esac

echo "Selecting $profile (AUDIO_SPATIAL=$audio_spatial, ATMOS=$audio_atmos, mode=$atmos_mode)"

# Values substituted into the remote command come exclusively from the case
# statement above.  Require every key exactly once before touching flash, make
# one immutable recovery copy, generate on the same filesystem, verify, then
# rename the complete file over the old generation.
"$AUDI_SSH" exec "
set -e
f='$REMOTE_STARTUP'
new='${REMOTE_STARTUP}.profile.new'
backup='${REMOTE_STARTUP}.pre_spatial_profiles'

test \"\$(grep -c '^export AUDIO_SPATIAL=' \"\$f\")\" = 1
test \"\$(grep -c '^export AUDIO_SPATIAL_ATMOS=' \"\$f\")\" = 1
test \"\$(grep -c '^export AUDIO_SPATIAL_ATMOS_MODE=' \"\$f\")\" = 1

mount -uw /mnt/app
test -e \"\$backup\" || cp \"\$f\" \"\$backup\"
/usr/bin/sed \\
  -e 's/^export AUDIO_SPATIAL=.*/export AUDIO_SPATIAL=$audio_spatial/' \\
  -e 's/^export AUDIO_SPATIAL_ATMOS=.*/export AUDIO_SPATIAL_ATMOS=$audio_atmos/' \\
  -e 's/^export AUDIO_SPATIAL_ATMOS_MODE=.*/export AUDIO_SPATIAL_ATMOS_MODE=$atmos_mode/' \\
  \"\$f\" > \"\$new\"

test \"\$(grep -c '^export AUDIO_SPATIAL=$audio_spatial$' \"\$new\")\" = 1
test \"\$(grep -c '^export AUDIO_SPATIAL_ATMOS=$audio_atmos$' \"\$new\")\" = 1
test \"\$(grep -c '^export AUDIO_SPATIAL_ATMOS_MODE=$atmos_mode$' \"\$new\")\" = 1
chmod 755 \"\$new\"
mv \"\$new\" \"\$f\"
sync
grep -n '^export AUDIO_SPATIAL' \"\$f\"
"

echo "Profile stored. It applies to the next dio_manager generation; no HU reboot is required."
echo "Current live dio_manager, if any, was deliberately left untouched."
