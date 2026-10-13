# upmix_lang.py - add the Sound effects "UpMix" row texts to lang_data.zip.
# Runs on micropython (M.I.B. apps/sbin/beta) or python3; called by the
# installer (mod/custom.sh, patch_lang) with the zip path:
#
#   micropython upmix_lang.py /mnt/app/eso/hmi/lsd/jars/lang_data.zip
#
# Puts six strings - the label "UpMix" and the five levels - into the fixed
# slots SLOT..SLOT+5 of every HMIToneEvoHigh locale table (text ids
# 1009900..1009905, what the Java row uses: com/luka/carplay/tone/
# UpmixToneUi.java); the table is padded with empty strings up to there.  A
# locale without a translation here gets English.  Idempotent; a slot that
# holds someone else's text is never overwritten (ours, any older wording, is) (that locale is skipped and the
# row shows the texts built into the jar).  The zip is replaced atomically.
# Copyright (c) 2026 LuKa (@LuKa_dev)
import sys, os
here = sys.argv[0].rsplit("/", 1)[0] if "/" in sys.argv[0] else "."
sys.path.append(here)
import ldzip

SCREEN = "HMIToneEvoHigh"
LABEL = "UpMix"
SLOT = 9900          # far past the stock table (~900 slots); text id = 1000000 + slot
# "Off" is the stock wording of the 3D effect row in all three (tone text 1000303)
TEXTS = {
    "en": [LABEL, "Off", "Moderate", "Normal", "Full", "Aggressive"],
    "es": [LABEL, "Off", "Moderado", "Normal", "Completo", "Agresivo"],
    "fr": [LABEL, "Off", "Modéré", "Normal", "Complet", "Agressif"],
}

def patch(zp):
    names, meta, raw = ldzip.zip_read(open(zp, "rb").read())
    pre = "resources/%s/strings_" % SCREEN
    added = present = taken = 0
    for n in names:
        if not (n.startswith(pre) and n.endswith(".data")):
            continue
        loc = n[len(pre):-len(".data")]
        m = meta[n][0]
        strs, flags = ldzip.d_unpack(ldzip.inflate(raw[n]) if m == 8 else raw[n])
        want = TEXTS.get(loc[:2], TEXTS["en"])
        have = strs[SLOT:SLOT + len(want)]
        if have == want:
            present += 1
            continue
        if (not have or have[0] != LABEL) and [t for t in have if t]:   # ours (any version) is rewritten
            print("UpMix texts: %s slots %d.. hold other text %r - skipped" % (loc, SLOT, have))
            taken += 1
            continue
        flags = bytearray(flags)
        while len(strs) < SLOT + len(want):
            strs.append(""); flags.append(0)
        for k in range(len(want)):
            strs[SLOT + k] = want[k]; flags[SLOT + k] = 8   # 8 = the flag ldzip "add" gives a new string
        d = ldzip.d_pack(strs, flags)
        raw[n] = d; meta[n] = (0, ldzip.crc32(d), len(d))   # edited entry is STORED
        added += 1
    if added + present + taken == 0:
        print("UpMix texts: no %s tables in %s" % (SCREEN, zp)); return 1
    if added:
        tmp = zp + ".carplay-new"
        f = open(tmp, "wb"); f.write(ldzip.zip_write(names, meta, raw)); f.close()
        os.rename(tmp, zp)
    print("UpMix texts: added to %d locale(s), already in %d, slots taken in %d" % (added, present, taken))
    return 1 if taken else 0

if __name__ == "__main__":
    sys.exit(patch(sys.argv[1]))
