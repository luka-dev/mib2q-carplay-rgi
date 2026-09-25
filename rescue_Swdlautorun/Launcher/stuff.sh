#!/bin/ksh
# CarPlay rescue (issue #24): remove the Java patch from a unit whose HMI no longer boots.
# SD autorun: Swdlautorun.txt -> SWDL -> metainfo2.txt FinalScript -> Launcher/final/finalScript.sh
# (checksummed, M.I.B. original) -> this file, on RCC; $2 = the card (/net/mmx/fs/sda0).
# Only the jars go: lsd.sh puts every jar in lsd/jars on j9's boot classpath.
# The SWDL environment is unknown: set PATH like the M.I.B. launcher does.
export PATH=/proc/boot:/sbin:/bin:/usr/bin:/usr/sbin:/net/mmx/bin:/net/mmx/usr/bin:/net/mmx/usr/sbin:/net/mmx/sbin
SD=${2:-/net/mmx/fs/sda0}
JARS=/net/mmx/mnt/app/eso/hmi/lsd/jars

# The removal first and outside the log: a failed log redirect skips its whole block.
BEFORE=$(ls -l $JARS 2>&1)
mount -uw /net/mmx/mnt/app
rm -f $JARS/carplay_hook.jar $JARS/carplay_hook_cn.jar $JARS/._carplay_hook*.jar
on -f mmx sync          # MMX owns the fs; a sync on RCC flushes nothing there

mount -uw "$SD"
{
    date
    echo "[rescue] HW $1, card $SD"
    echo "$BEFORE"
    ls -l $JARS
    # one shot: no autorun on the next insert/boot
    mv -f "$SD/Swdlautorun.txt" "$SD/_Swdlautorun.txt"
    echo "[rescue] done - restart the HU"
} >> "$SD/carplay_rescue_log.txt" 2>&1
on -f mmx sync          # the card is served by MMX too
