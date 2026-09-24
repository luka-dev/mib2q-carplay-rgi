#!/bin/sh
# M.I.B. release zips (tags up to V3.7.1) run /mod/command.sh from
# GEM -> M.I.B. -> Advanced Settings -> "Run individual script"; M.I.B. main
# (since e531867, 2024-07-05) runs /mod/custom.sh from "Run Custom Script".
# The SD card is FAT, which cannot hold a symlink, so this forwards to custom.sh.
# The M.I.B. launcher also sources /mod/command.sh while installing M.I.B.
# itself; $0 is then the launcher, and this file does nothing.
case ${0##*/} in
    command.sh) exec /bin/sh "${0%/*}/custom.sh" "$@" ;;
esac
