#!/bin/sh
# Select the persistent CarPlay audio profile on the HU without replacing the
# rest of carplay_startup.sh.  The new environment is inherited by the next
# dio_manager generation (normally the next phone connection).

set -eu

DEFAULT_SSH=/Users/luka/Desktop/AUDI_2/audi_ssh.sh
AUDI_SSH=${AUDI_SSH:-$DEFAULT_SSH}
REMOTE_STARTUP=/mnt/app/root/hooks/carplay_startup.sh

usage()
{
    echo "usage: $0 {status|spatial-5.1|spatial-5.1.2|experimental-aac-5.1.2|fallback-5.1|fallback-stereo}" >&2
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
            "grep -n '^export SPATIAL_' '$REMOTE_STARTUP'; pidin ar | grep dio_manager || true"
        exit 0
        ;;
    spatial-5.1)
        spatial_5_1=1
        spatial_103=1
        format_index=65
        extra_index=
        ;;
    spatial-5.1.2)
        spatial_5_1=1
        spatial_103=1
        format_index=66
        extra_index=
        ;;
    experimental-aac-5.1.2)
        spatial_5_1=1
        spatial_103=1
        format_index=40
        extra_index=65
        ;;
    fallback-5.1)
        spatial_5_1=1
        spatial_103=0
        format_index=65
        extra_index=
        ;;
    fallback-stereo)
        spatial_5_1=0
        spatial_103=0
        format_index=65
        extra_index=
        ;;
    *)
        usage
        ;;
esac

echo "Selecting $profile (SPATIAL_103=$spatial_103, index=$format_index, extra=${extra_index:-none}, SPATIAL_5_1=$spatial_5_1)"

# Values substituted into the remote command come exclusively from the case
# statement above.  Require every key exactly once before touching flash, make
# one immutable recovery copy, generate on the same filesystem, verify, then
# rename the complete file over the old generation.
"$AUDI_SSH" exec "
set -e
f='$REMOTE_STARTUP'
new='${REMOTE_STARTUP}.profile.new'
backup='${REMOTE_STARTUP}.pre_spatial_profiles'

test \"\$(grep -c '^export SPATIAL_5_1=' \"\$f\")\" = 1
test \"\$(grep -c '^export SPATIAL_103=' \"\$f\")\" = 1
test \"\$(grep -c '^export SPATIAL_103_INDEX=' \"\$f\")\" = 1
test \"\$(grep -c '^export SPATIAL_103_EXTRA_INDEX=' \"\$f\")\" = 1

mount -uw /mnt/app
test -e \"\$backup\" || cp \"\$f\" \"\$backup\"
/usr/bin/sed \\
  -e 's/^export SPATIAL_5_1=.*/export SPATIAL_5_1=$spatial_5_1/' \\
  -e 's/^export SPATIAL_103=.*/export SPATIAL_103=$spatial_103/' \\
  -e 's/^export SPATIAL_103_INDEX=.*/export SPATIAL_103_INDEX=$format_index/' \\
  -e 's/^export SPATIAL_103_EXTRA_INDEX=.*/export SPATIAL_103_EXTRA_INDEX=$extra_index/' \\
  \"\$f\" > \"\$new\"

test \"\$(grep -c '^export SPATIAL_5_1=$spatial_5_1$' \"\$new\")\" = 1
test \"\$(grep -c '^export SPATIAL_103=$spatial_103$' \"\$new\")\" = 1
test \"\$(grep -c '^export SPATIAL_103_INDEX=$format_index$' \"\$new\")\" = 1
test \"\$(grep -c '^export SPATIAL_103_EXTRA_INDEX=$extra_index$' \"\$new\")\" = 1
chmod 755 \"\$new\"
mv \"\$new\" \"\$f\"
sync
grep -n '^export SPATIAL_' \"\$f\"
"

echo "Profile stored. It applies to the next dio_manager generation; no HU reboot is required."
echo "Current live dio_manager, if any, was deliberately left untouched."
