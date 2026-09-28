#!/usr/bin/env python3
"""The rapid-loop leaderboard (astra review 15): one row per job folder under RAPID_DIR, rebuilt from its files -
train.log (SI-SDR / vocal leak at 4k), gap.txt (attribution: eligible / vocal transmission, air), transitions.txt
(entry ducking med / p10, 0-2 s vocal peak, exit release) and screen.log (the frozen 20-song screen at G 1.0).
Context rows (ctx_*) have the screen only.  Writes LEADERBOARD.md.      rapid_board.py RAPID_DIR"""
import os, re, sys


def grab(text, pat, g=1, cast=float):
    m = re.search(pat, text, re.M)
    return cast(m.group(g)) if m else None


def fmt(v, f="{:.2f}"):
    return "—" if v is None else f.format(v)


def row(d):
    r = {"tag": os.path.basename(d)}
    def read(name):                                            # PowerShell redirection writes UTF-16 LE with a BOM
        p = os.path.join(d, name)
        if not os.path.exists(p): return ""
        raw = open(p, "rb").read()
        return raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8", errors="replace")
    job, tl, gap, tr, sc = read("job.txt"), read("train.log"), read("gap.txt"), read("transitions.txt"), read("screen.log")
    r["args"] = grab(job, r"args \[(.*?)\]", cast=str) or grab(job, r"inject \[(.*?)\]", cast=str) or ""
    r["sdr"] = grab(tl, r"^step 4000 .*?si_sdr (-?[\d.]+)"); r["leak_eval"] = grab(tl, r"^step 4000 .*?vocal_leak ([\d.]+)")
    m = re.search(r"^MEDIAN\s+([-+]?[\d.]+)\s+([-+]?[\d.]+)\s+([-+]?[\d.]+)\s+([-+]?[\d.]+)\s*\|\s*([\d.]+)\s+([\d.]+)\s*\|\s*([\d.]+)\s+([\d.]+)\s*\|\s*([\d.]+)", gap, re.M)
    if m:
        r["rec"], r["rec_d"], r["vl"], r["pl"] = float(m.group(5)), float(m.group(6)), float(m.group(7)), float(m.group(9))
    r["air"] = grab(gap, r"8-14k (-?[\d.]+)/"); r["air2"] = grab(gap, r">14k (-?[\d.]+)/")
    ent = grab(tr, r"^(entry .*)", cast=str) or ""; ex = grab(tr, r"^(exit .*)", cast=str) or ""
    r["duck"] = grab(ent, r"post/pre med ([\d.]+)"); r["duck10"] = grab(ent, r"p10 ([\d.]+)"); r["peak"] = grab(ent, r"peak0-2 ([\d.]+)/")
    r["release"] = grab(ex, r"post/pre med ([\d.]+)")
    r["level"] = grab(sc, r"median (-?[\d.]+) dBFS"); r["err"] = grab(sc, r"broadband ([\d.]+)")
    for k, name in (("voice", "vocals"), ("guitar", "guitar"), ("keys", "other_keys")):
        r[k] = grab(sc, r"^%s\s+\d+\s+([\d.]+)" % name)
    r["swing"] = grab(sc, r"^guitar .*?(\d+)/\d+ songs swing", cast=int)
    r["done"] = "done" in job
    r["_screen"] = sc
    return r


def main():
    rd = sys.argv[1]
    dirs = sorted(d for d in (os.path.join(rd, x) for x in os.listdir(rd)) if os.path.isdir(d))
    rows = [row(d) for d in dirs]
    order = lambda r: (0 if r["tag"].startswith("ctx_") else 1 if r["tag"].startswith("anchor") else 2, r["tag"])
    rows.sort(key=order)
    head = ("| tag | args | SI-SDR@4k | elig tx (all/edom) | vocal tx | prot tx | air 8-14k / >14k dB | entry duck med/p10 | 0-2 s peak | exit release "
            "| voice | guitars | keys | swing/20 | level | err |")
    out = ["# Rapid board (4k-step mechanism screen; the 20-song CPU screen at G 1.0)", "", head, "|" + "---|" * 16]
    for r in rows:
        out.append("| %s%s | `%s` | %s | %s / %s | %s | %s | %s / %s | %s / %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["tag"], "" if r["done"] else " (running)", r["args"], fmt(r["sdr"]), fmt(r.get("rec")), fmt(r.get("rec_d")), fmt(r.get("vl"), "{:.3f}"), fmt(r.get("pl"), "{:.3f}"),
            fmt(r["air"], "{:.1f}"), fmt(r["air2"], "{:.1f}"), fmt(r["duck"]), fmt(r["duck10"]), fmt(r["peak"]), fmt(r["release"]),
            fmt(r["voice"]), fmt(r["guitar"]), fmt(r["keys"]), fmt(r["swing"], "{}"), fmt(r["level"], "{:.1f}"), fmt(r["err"])))
    # every stem (28.09: "not only the voice"): rear share, and how much depth follows level (last column of the screen)
    stems = ("vocals", "guitar", "piano", "other_keys", "bowed_strings", "wind", "other", "bass", "drums", "percussion")
    out += ["", "## Every stem: rear share / depth-follows-level", "", "| tag | " + " | ".join(stems) + " | swinging (all stems) |", "|" + "---|" * (len(stems) + 2)]
    for r in rows:
        sc = r["_screen"]; cells = []; swing = 0
        for st in stems:
            m = re.search(r"^%s\s+\d+\s+([\d.]+)\s.*?([-+][\d.]+)\s+([-+][\d.]+)" % st, sc, re.M)
            cells.append("—" if not m else f"{float(m.group(1)):.2f} / {float(m.group(3)):+.2f}")
        swing = sum(int(x) for x in re.findall(r"(\d+)/\d+ songs swing", sc))
        out.append(f"| {r['tag']} | " + " | ".join(cells) + f" | {swing if sc else '—'} |")
    open(os.path.join(rd, "LEADERBOARD.md"), "w", encoding="utf-8").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
