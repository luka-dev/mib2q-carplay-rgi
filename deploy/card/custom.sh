#!/bin/sh
#
# custom.sh - M.I.B. entry point for the MHI2Q CarPlay patches.
#
# Copy this file to /mod/custom.sh on the M.I.B. SD card and the payload to
# /mod/carplay/ (the copy commands are in README.md), then on the unit:
#
#     Advanced Settings -> Run Custom Script
#
# It finds the payload on the card and runs install.sh. To uninstall instead,
# create an empty file named UNINSTALL next to install.sh on the card
# (/mod/carplay/UNINSTALL) and run the custom script again.
#
# Copyright (c) 2026 LuKa (@LuKa_dev)

set -u

echo "=== MHI2Q CarPlay - M.I.B. launcher ==="

# The card can be mounted under different names depending on the reader, so
# look for our payload rather than assuming a path.
# `dirname` is missing too, so $0's directory is derived with ${0%/*} - which
# leaves $0 alone when there is no slash in it, hence the explicit "." case.
case $0 in
    */*) SELF_DIR=${0%/*} ;;
    *)   SELF_DIR=. ;;
esac

# Globs, not `find`: this unit has no find, so a find-based search would fail
# silently and the script would report "payload not found" for the wrong reason.
# The shell expands these itself, and an unmatched pattern simply stays literal -
# the -f tests below reject it.
PAYLOAD=""
for d in \
    "$SELF_DIR/carplay" \
    /fs/*/mod/carplay \
    /mnt/*/mod/carplay \
    /net/*/fs/*/mod/carplay
do
    if [ -f "$d/install.sh" ] && [ -f "$d/bin/libcarplay_hook.so" ]; then
        PAYLOAD=$d
        break
    fi
done

if [ -z "$PAYLOAD" ]; then
    echo "!! payload not found."
    echo "!! Expected /mod/carplay/install.sh and /mod/carplay/bin/ on the card,"
    echo "!! with custom.sh itself at /mod/custom.sh."
    echo "!! Looked under: $SELF_DIR/carplay, /fs/*/mod, /mnt/*/mod, /net/*/fs/*/mod"
    exit 1
fi

echo "payload: $PAYLOAD"

if [ -f "$PAYLOAD/UNINSTALL" ]; then
    echo "UNINSTALL marker present - removing the patches"
    SRC_DIR="$PAYLOAD" sh "$PAYLOAD/uninstall.sh"
else
    SRC_DIR="$PAYLOAD" sh "$PAYLOAD/install.sh"
fi

echo ""
echo "Read the log above, then reboot the unit."
