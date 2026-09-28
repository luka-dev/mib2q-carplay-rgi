#!/usr/bin/env python3
"""Build isolated Windows host renderers for the oracle-audio object-path test.

The build reads Track A's HSSR C source but never writes under hook/audio.
It inserts a test-only direct object path in a copy and produces three named
host executables: an unmodified-source control, direct object with the old
bed, and direct object without the bed. No binary is suitable for the unit.
"""

import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path


ANCHOR = "            mask = spa_hssr_dsp_bound(mask, 0.0f, 1.45f);\n"
DIRECT_PATH = r"""#if defined(SPA_HSSR_TEST_DIRECT_OBJECT) || defined(SPA_HSSR_TEST_DIRECT_NO_BED)
            /* Host-only oracle-audio experiment.  Bypass the dynamic pan
             * projection and steer the isolated stereo source itself.  Keep
             * the source used for front transfer identical to that object.
             * The NO_BED arm also removes ambience/air at this bin. */
            if (state->test_inject_pcm) {
                float direct_mask = spa_hssr_dsp_bound(mask, 0.0f, 1.45f);
                float transfer_mask = direct_mask > 1.0f ? 1.0f : direct_mask;
                float keep = state->band_front_keep[band];
                state->source_fft_re[0][bin] = transfer_mask * keep * obj_l_re;
                state->source_fft_im[0][bin] = transfer_mask * keep * obj_l_im;
                state->source_fft_re[1][bin] = transfer_mask * keep * obj_r_re;
                state->source_fft_im[1][bin] = transfer_mask * keep * obj_r_im;
                state->fft_re[SPA_HSSR_LEFT][bin] = direct_mask * obj_l_re;
                state->fft_im[SPA_HSSR_LEFT][bin] = direct_mask * obj_l_im;
                state->fft_re[SPA_HSSR_RIGHT][bin] = direct_mask * obj_r_re;
                state->fft_im[SPA_HSSR_RIGHT][bin] = direct_mask * obj_r_im;
                if (split) {
#ifdef SPA_HSSR_TEST_DIRECT_OBJECT
                    state->bed_re[0][bin] = ambience * side_re + band_air[band] * left_re;
                    state->bed_im[0][bin] = ambience * side_im + band_air[band] * left_im;
                    state->bed_re[1][bin] = -ambience * side_re + band_air[band] * right_re;
                    state->bed_im[1][bin] = -ambience * side_im + band_air[band] * right_im;
#else
                    state->bed_re[0][bin] = state->bed_im[0][bin] = 0.0f;
                    state->bed_re[1][bin] = state->bed_im[1][bin] = 0.0f;
#endif
                } else {
#ifdef SPA_HSSR_TEST_DIRECT_OBJECT
                    state->fft_re[SPA_HSSR_LEFT][bin] += ambience * side_re;
                    state->fft_im[SPA_HSSR_LEFT][bin] += ambience * side_im;
                    state->fft_re[SPA_HSSR_RIGHT][bin] -= ambience * side_re;
                    state->fft_im[SPA_HSSR_RIGHT][bin] -= ambience * side_im;
#endif
                }
                continue;
            }
#endif
"""


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out-dir", type=Path, required=True)
    p.add_argument("--compiler", default="x86_64-w64-mingw32-gcc")
    a = p.parse_args()
    cc = shutil.which(a.compiler)
    if not cc:
        p.error(f"compiler not found: {a.compiler}")
    root = Path(__file__).resolve().parents[2]
    audio = root / "hook" / "audio"
    source = audio / "hssr_dsp.c"
    original = source.read_text(encoding="utf-8")
    if original.count(ANCHOR) != 1:
        raise ValueError("HSSR insertion point changed; review the source before rebuilding")
    if "#ifdef SPA_HSSR_TEST_MASK\nmasked:\n#endif" not in original:
        raise ValueError("oracle injection control flow changed")
    modified = original.replace(ANCHOR, DIRECT_PATH + ANCHOR)
    a.out_dir.mkdir(parents=True, exist_ok=True)
    modified_path = a.out_dir / "hssr_dsp_direct_object.c"
    modified_path.write_text(modified, encoding="utf-8")
    common = [cc, "-std=c99", "-O2", "-ffp-contract=off", "-DSPA_HSSR_TEST_MASK",
              "-I", str(audio), "-o"]
    sources = [root / "tools/stereo-upmix-render/stereo_upmix_render.c",
               audio / "soundcheck.c", audio / "stereo_upmix.c", None,
               audio / "hssr_neon.c", audio / "hssr_mask.c"]
    builds = {}
    for name, macro, dsp in (
        ("control", None, source),
        ("direct_object", "SPA_HSSR_TEST_DIRECT_OBJECT", modified_path),
        ("direct_no_bed", "SPA_HSSR_TEST_DIRECT_NO_BED", modified_path),
    ):
        output = a.out_dir / f"upmix_{name}.exe"
        selected = [dsp if item is None else item for item in sources]
        command = common + [str(output)] + ([f"-D{macro}"] if macro else []) + [str(s) for s in selected] + ["-lm"]
        subprocess.run(command, check=True)
        builds[name] = {"binary": str(output), "sha256": sha256(output), "macro": macro}
        print(f"{name}: {output} sha256 {builds[name]['sha256']}")
    manifest = dict(source=str(source), source_sha256=sha256(source),
                    modified_source_sha256=sha256(modified_path), builds=builds)
    (a.out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
