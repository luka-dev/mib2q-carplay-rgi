#!/bin/ksh

# change this file to run any command, script or binary from SD
# no need to touch metainfo2.txt as this is outside of its reach
# final script run time is set to 300s in metainfo2.txt
# if you need more time extend FinalScriptMaxTime = XX
# add echo to output debugging information to RCC log on UART

. /net/mmx/fs/sda0/config/BASICS

echo -ne "[stuffScript] Cool stuff found and executing...\n" | $TEE -i -a $LOG

export GEM=1
export SWDLAUTORUN=1 # allows Swdlautorun.txt in root of SD during SWDL process

# CarPlay issue #24 rescue: take the CarPlay Java patch off MMX so the HMI boots.
# First thing (MMX keeps rebooting), no checks, errors ignored; direct, then on MMX itself.
echo -ne "\n--- CarPlay rescue: removing carplay_hook jars --\n" | $TEE -i -a $LOG
CPJ=/mnt/app/eso/hmi/lsd/jars
CPF="carplay_hook.jar carplay_hook_cn.jar ._carplay_hook.jar ._carplay_hook_cn.jar"
mount -uw /net/mmx/mnt/app 2>/dev/null
for f in $CPF; do rm -rf /net/mmx$CPJ/$f 2>/dev/null; done
mount -uw /mnt/app 2>/dev/null
for f in $CPF; do rm -rf $CPJ/$f 2>/dev/null; done
on -f mmx /bin/sh -c "mount -uw /mnt/app; cd $CPJ && rm -rf $CPF; sync" 2>/dev/null
sync 2>/dev/null
ls -l /net/mmx$CPJ 2>&1 | $TEE -i -a $LOG

# The installer's USB Lua patch back to stock (when it left a .carplay-stock): if USB/CarPlay
# connect broke after an install. A copy, so the backup stays for the uninstaller.
echo -ne "\n--- CarPlay rescue: restoring stock usblauncher_otg.lua --\n" | $TEE -i -a $LOG
CPL=/mnt/system/etc/usblauncher_otg.lua
mount -uw /net/mmx/mnt/system 2>/dev/null
[ -f /net/mmx$CPL.carplay-stock ] && cp /net/mmx$CPL.carplay-stock /net/mmx$CPL 2>/dev/null
on -f mmx /bin/sh -c "mount -uw /mnt/system; [ -f $CPL.carplay-stock ] && cp $CPL.carplay-stock $CPL; sync" 2>/dev/null
sync 2>/dev/null
ls -l /net/mmx$CPL* 2>&1 | $TEE -i -a $LOG

echo -ne "\n--- Load PNG to display during finalscript --\n" | $TEE -i -a $LOG
${2}/apps/showimage -load 0 ${2}/mod/images/showimage/ finalscript.png SWDL
echo -ne "\n--- Running basic backup --------------------\n" | $TEE -i -a $LOG
${2}/apps/backup -b
echo -ne "\n--- Running Launcher script -----------------\n" | $TEE -i -a $LOG
${2}/apps/launcher -all
echo -ne "\n--- Unload PNG ------------------------------\n" | $TEE -i -a $LOG
${2}/apps/showimage -unload

echo -ne "[stuffScript] Cool stuff DONE\n" | $TEE -i -a $LOG
