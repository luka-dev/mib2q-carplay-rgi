#!/bin/sh
# Compatibility entrypoint for M.I.B. releases that invoke mod/command.sh.
# When sourced during M.I.B. installation, this deliberately does nothing.
case ${0##*/} in
    command.sh) exec /bin/sh "${0%/*}/custom.sh" "$@" ;;
esac
main
