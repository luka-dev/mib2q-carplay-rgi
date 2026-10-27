# upmix_lang_test.py - deploy/lang/upmix_lang.py on a synthetic lang_data.zip:
# slots 1000..1005 in every Sound locale, Off from slot 303, English for an
# unknown locale, other entries byte-identical, idempotent, someone else's
# text in the slots never overwritten.  python3 tests/upmix_lang_test.py
import os, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, "..", "deploy", "lang"))
import ldzip, upmix_lang

def table(off):
    strs = ["s%d" % i for i in range(897)]
    strs[303] = off
    return ldzip.d_pack(strs, bytearray(897))

def build(path, extra=None):
    names, meta, raw = [], {}, {}
    for name, body in [("resources/HMIToneEvoHigh/strings_ru_RU.data", table("vykl")),
                       ("resources/HMIToneEvoHigh/strings_xx_XX.data", table("Off")),
                       ("resources/HMICarEvoHigh/strings_ru_RU.data", table("car"))]:
        names.append(name); raw[name] = body; meta[name] = (0, ldzip.crc32(body), len(body))
    open(path, "wb").write(ldzip.zip_write(names, meta, raw))

def strings(path, name):
    names, meta, raw = ldzip.zip_read(open(path, "rb").read())
    return ldzip.d_unpack(raw[name])[0], raw

d = tempfile.mkdtemp(); z = os.path.join(d, "lang_data.zip")
build(z)
car_before = strings(z, "resources/HMICarEvoHigh/strings_ru_RU.data")[1]["resources/HMICarEvoHigh/strings_ru_RU.data"]
assert upmix_lang.patch(z) == 0
ru, raw = strings(z, "resources/HMIToneEvoHigh/strings_ru_RU.data")
assert ru[1000:1006] == ["CarPlay UpMix", "vykl"] + upmix_lang.LEVELS["ru"], ru[1000:1006]
assert ru[897:1000] == [""] * 103 and ru[:897] == strings(z, "resources/HMIToneEvoHigh/strings_ru_RU.data")[0][:897]
xx = strings(z, "resources/HMIToneEvoHigh/strings_xx_XX.data")[0]
assert xx[1000:1006] == ["CarPlay UpMix", "Off"] + upmix_lang.LEVELS["en"]
assert raw["resources/HMICarEvoHigh/strings_ru_RU.data"] == car_before
once = open(z, "rb").read()
assert upmix_lang.patch(z) == 0 and open(z, "rb").read() == once            # idempotent
# someone else's text in a slot: that locale is skipped, the run reports it
names, meta, raw = ldzip.zip_read(once)
k = "resources/HMIToneEvoHigh/strings_xx_XX.data"
s, fl = ldzip.d_unpack(raw[k]); s[1000] = "Other mod"; body = ldzip.d_pack(s, fl)
raw[k] = body; meta[k] = (0, ldzip.crc32(body), len(body))
open(z, "wb").write(ldzip.zip_write(names, meta, raw))
assert upmix_lang.patch(z) == 1 and strings(z, k)[0][1000] == "Other mod"
print("upmix_lang_test: slots 1000..1005, stock Off, English fallback, other screens untouched, idempotent, foreign slots kept PASS")
