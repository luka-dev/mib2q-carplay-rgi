# upmix_lang.py - add the Sound effects "UpMix" row texts to lang_data.zip.
# Runs on micropython (M.I.B. apps/sbin/beta) or python3; called by the
# installer (mod/custom.sh, patch_lang) with the zip path:
#
#   micropython upmix_lang.py /mnt/app/eso/hmi/lsd/jars/lang_data.zip
#
# Puts six strings - the label "CarPlay UpMix" and the five levels - into the fixed
# slots SLOT..SLOT+5 of every HMIToneEvoHigh locale table (text ids
# 1001000..1001005, what the Java row uses: com/luka/carplay/tone/
# UpmixToneUi.java); the table is padded with empty strings up to there.  A
# locale without a translation here gets English.  The jar has no copy of
# these texts: if this step fails the row shows empty labels.  Idempotent; a slot that
# holds someone else's text is never overwritten (ours, any older wording, is) (that locale is skipped and the
# row shows empty labels).  The zip is replaced atomically.
# Copyright (c) 2026 LuKa (@LuKa_dev)
import sys, os, struct
here = sys.argv[0].rsplit("/", 1)[0] if "/" in sys.argv[0] else "."
sys.path.append(here)
import ldzip

SCREEN = "HMIToneEvoHigh"
LABEL = "CarPlay UpMix"
OUR_LABELS = (LABEL, "UpMix")   # any wording we ever wrote: rewritten, not "taken"
SLOT = 1000          # past the stock table (897 slots on US/EU/CN); text id = 1000000 + slot
# The four levels after Off, per language (locale first, then its language):
# every HMIToneEvoHigh locale of the US (MU1316), EU (MU1329) and CN (MU1003)
# trains.  "Off" is not here: each locale takes its own stock word from the
# 3D effect row (OFF_SLOT, tone text 1000303).  Only locales the unit's zip
# has are written; one missing here gets English.
LEVELS = {
    "en": ["Moderate", "Normal", "Full", "Aggressive"],
    "ar": ["\u0645\u0639\u062a\u062f\u0644", "\u0639\u0627\u062f\u064a", "\u0643\u0627\u0645\u0644", "\u0645\u0643\u062b\u0641"],
    "cs": ["M\u00edrn\u00fd", "Norm\u00e1ln\u00ed", "Pln\u00fd", "Agresivn\u00ed"],
    "da": ["Moderat", "Normal", "Fuld", "Aggressiv"],
    "de": ["Moderat", "Normal", "Voll", "Aggressiv"],
    "el": ["\u039c\u03ad\u03c4\u03c1\u03b9\u03bf", "\u039a\u03b1\u03bd\u03bf\u03bd\u03b9\u03ba\u03cc", "\u03a0\u03bb\u03ae\u03c1\u03b5\u03c2", "\u0395\u03c0\u03b9\u03b8\u03b5\u03c4\u03b9\u03ba\u03cc"],
    "es": ["Moderado", "Normal", "Completo", "Agresivo"],
    "fi": ["Maltillinen", "Normaali", "T\u00e4ysi", "Aggressiivinen"],
    "fr": ["Mod\u00e9r\u00e9", "Normal", "Complet", "Agressif"],
    "hu": ["M\u00e9rs\u00e9kelt", "Norm\u00e1l", "Teljes", "Agressz\u00edv"],
    "it": ["Moderato", "Normale", "Pieno", "Aggressivo"],
    "ms": ["Sederhana", "Biasa", "Penuh", "Agresif"],
    "nb": ["Moderat", "Normal", "Full", "Aggressiv"],
    "nl": ["Gematigd", "Normaal", "Volledig", "Agressief"],
    "pl": ["Umiarkowany", "Normalny", "Pe\u0142ny", "Agresywny"],
    "pt": ["Moderado", "Normal", "Completo", "Agressivo"],
    "ro": ["Moderat", "Normal", "Complet", "Agresiv"],
    "ru": ["\u0423\u043c\u0435\u0440\u0435\u043d\u043d\u044b\u0439", "\u041e\u0431\u044b\u0447\u043d\u044b\u0439", "\u041f\u043e\u043b\u043d\u044b\u0439", "\u0410\u0433\u0440\u0435\u0441\u0441\u0438\u0432\u043d\u044b\u0439"],
    "sl": ["Zmerno", "Normalno", "Polno", "Agresivno"],
    "sv": ["M\u00e5ttlig", "Normal", "Full", "Aggressiv"],
    "tr": ["Orta", "Normal", "Tam", "Agresif"],
    "uk": ["\u041f\u043e\u043c\u0456\u0440\u043d\u0438\u0439", "\u0417\u0432\u0438\u0447\u0430\u0439\u043d\u0438\u0439", "\u041f\u043e\u0432\u043d\u0438\u0439", "\u0410\u0433\u0440\u0435\u0441\u0438\u0432\u043d\u0438\u0439"],
    "zh": ["\u9002\u4e2d", "\u6807\u51c6", "\u5b8c\u6574", "\u5f3a\u70c8"],
    "zh_HK": ["\u9069\u4e2d", "\u6a19\u6e96", "\u5b8c\u6574", "\u5f37\u70c8"],
}
OFF_SLOT = 303

def texts(loc, strs):
    levels = LEVELS.get(loc) or LEVELS.get(loc[:2]) or LEVELS["en"]
    off = strs[OFF_SLOT] if len(strs) > OFF_SLOT and strs[OFF_SLOT] else "Off"
    return [LABEL, off] + levels

def zip_write_to(f, names, meta, raw):
    """ldzip.zip_write, streamed: the same bytes without building the archive in
    memory (the EU zip is 4.2 MB; the in-memory concat ran out of heap)."""
    central = []; off = 0
    for name in names:
        method, crc, usize = meta[name]; body = raw[name]; nm = name.encode("utf-8"); csize = len(body)
        lh = (b"PK\x03\x04" + struct.pack("<HHHHH", 20, 0, method, 0, 0)
              + struct.pack("<III", crc, csize, usize) + struct.pack("<HH", len(nm), 0) + nm)
        f.write(lh); f.write(body)
        central.append(b"PK\x01\x02" + struct.pack("<HHHHHH", 20, 20, 0, method, 0, 0)
                       + struct.pack("<III", crc, csize, usize)
                       + struct.pack("<HHHHH", len(nm), 0, 0, 0, 0)
                       + struct.pack("<II", 0, off) + nm)
        off += len(lh) + csize
    clen = 0
    for c in central:
        f.write(c); clen += len(c)
    f.write(b"PK\x05\x06" + struct.pack("<HH", 0, 0) + struct.pack("<HH", len(names), len(names))
            + struct.pack("<I", clen) + struct.pack("<I", off) + struct.pack("<H", 0))

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
        want = texts(loc, strs)
        have = strs[SLOT:SLOT + len(want)]
        if have == want:
            present += 1
            continue
        if (not have or have[0] not in OUR_LABELS) and [t for t in have if t]:   # ours (any version) is rewritten
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
        f = open(tmp, "wb"); zip_write_to(f, names, meta, raw); f.close()
        os.rename(tmp, zp)
    print("UpMix texts: added to %d locale(s), already in %d, slots taken in %d" % (added, present, taken))
    return 1 if taken else 0

if __name__ == "__main__":
    sys.exit(patch(sys.argv[1]))
