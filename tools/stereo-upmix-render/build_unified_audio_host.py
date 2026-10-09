#!/usr/bin/env python3
"""Build a Track B audio-only C replay with no legacy learned control.

Copied host sources only. Requires a stereo eligible-audio estimate, refuses
legacy blobs/masks, and traps any legacy inference or model-file load. Scene
heads are deliberately dormant; this is not a complete unified scene port.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

from build_direct_object_host import ANCHOR, DIRECT_PATH


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Reviewed source anchor changed: ' + old[:100])
    return text.replace(old, new)


def body(text, name, replacement):
    matches = list(re.finditer(r'^(?:static\s+)?(?:int|void)\s+' + re.escape(name) + r'\s*\([^;]*?\)\s*\{', text, re.M))
    if len(matches) != 1:
        raise ValueError('Reviewed function changed: ' + name)
    start = matches[0].end() - 1
    depth = 1
    end = start + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + '{\n' + replacement + '\n}' + text[end:]


def build(out, compiler):
    cc = shutil.which(compiler)
    if not cc:
        raise ValueError('Compiler missing')
    if out.exists():
        raise FileExistsError('Use a fresh build directory; preserve prior binary')
    root = Path(__file__).resolve().parents[2]
    audio = root / 'hook/audio'
    render = root / 'tools/stereo-upmix-render/stereo_upmix_render.c'
    originals = [render] + [audio / n for n in ('stereo_upmix.c', 'hssr_dsp.c', 'hssr_neon.c', 'hssr_mask.c', 'soundcheck.c')]
    before = {str(p.relative_to(root)):sha(p) for p in originals}
    out.mkdir(parents=True)
    dsp = (audio / 'hssr_dsp.c').read_text()
    dsp = once(dsp, ANCHOR, DIRECT_PATH + ANCHOR)
    dsp = body(dsp, 'spa_hssr_run_control', '''    /* Audio-only replacement: extraction already came from the unified
     * network. No q6d, guard, mask, learned veto or object-slot control. */
    (void)model; (void)provider; (void)bands;
    if (full_frame) state->control_hop++;
    if (!full_frame || state->control_hop != SPA_HSSR_CONTROL_HOPS) return;
    state->control_hop = 0;
    state->control_sequence++;
''')
    dsp = once(dsp, 'if (!state || !model || !front_l || !front_r || !rear_l || !rear_r)',
                     'if (!state || !front_l || !front_r || !rear_l || !rear_r)')
    dsp = once(dsp, 'if (!state || !model || (!pcm2 && frames) ||',
                     'if (!state || (!pcm2 && frames) ||')
    ready = 'if (!spa_hssr_model_ready(model)) return SPA_HSSR_ERROR_NOT_READY;'
    if dsp.count(ready) != 2:
        raise ValueError('Reviewed readiness checks changed')
    dsp = dsp.replace(ready, 'if (!state->test_inject_pcm) return SPA_HSSR_ERROR_NOT_READY;')
    stereo = (audio / 'stereo_upmix.c').read_text()
    stereo = once(stereo, '''    if (config->backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL &&
        !spa_hssr_model_ready(config->hssr_model))
        return SPA_STEREO_UPMIX_ERROR_CONFIG;
''', '''    /* Copied host: CLI validates external unified eligible audio. */
''')
    cli = render.read_text()
    start = cli.index('    if (options.backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL) {\n        int model_status;')
    stop = cli.index('    if (paths_are_same_file(', start)
    cli = cli[:start] + '''    if (options.backend != SPA_STEREO_UPMIX_HSSR_UNIVERSAL ||
        !options.rear_audio_path || options.model_path ||
        options.guard_model_path || options.mask_model_path ||
        options.protect_mask_path) {
        fprintf(stderr, "error: unified audio host requires HSSR + --rear-audio; legacy blobs/masks forbidden\\n");
        return 2;
    }
    fprintf(stderr, "unified host: audio-only, no scene; legacy neural calls trapped\\n");
''' + cli[stop:]
    cli = once(cli, '    if (options.protect_mask_path && !load_protect_mask(options.protect_mask_path)) return 2;',
                     '    if (options.protect_mask_path) { fprintf(stderr, "error: legacy masks forbidden\\n"); return 2; }')
    cli = once(cli, '    uint32_t remaining = total_frames;\n    int status;\n',
                     '''    uint32_t remaining = total_frames;
    int status;
    if (g_rear_audio_frames != total_frames) {
        fprintf(stderr, "error: unified eligible estimate length differs from mixture\\n");
        return 0;
    }
''')
    cli = once(cli, '    if (state._runtime._hssr.mask_model)\n',
                     '''    fprintf(stderr, "unified host: old_neural_invocations=0, old_blobs=0, frames=%u\\n", (unsigned)total_frames);
    if (state._runtime._hssr.mask_model)
''')
    neon = (audio / 'hssr_neon.c').read_text()
    mask = (audio / 'hssr_mask.c').read_text()
    trap = '''    fprintf(stderr, "FATAL: forbidden legacy neural operation in unified host\\n");
    abort();'''
    for name in ('spa_hssr_model_load_file', 'spa_hssr_infer'):
        neon = body(neon, name, trap)
    for name in ('spa_mask_model_load_file', 'spa_mask_infer'):
        mask = body(mask, name, trap)
    copied = {}
    for name, text in [('stereo_upmix_render.c', cli), ('stereo_upmix.c', stereo),
                       ('hssr_dsp.c', dsp), ('hssr_neon.c', neon), ('hssr_mask.c', mask)]:
        p = out / name
        p.write_text(text)
        copied[name] = sha(p)
    executable = out / 'upmix_unified_audio.exe'
    command = [cc, '-std=c99', '-O2', '-ffp-contract=off', '-DSPA_HSSR_TEST_MASK',
               '-DSPA_HSSR_TEST_DIRECT_NO_BED', '-I', str(audio), '-o', str(executable)]
    command += [str(out / n) for n in copied] + [str(audio / n) for n in ('soundcheck.c', 'scene_engine_math.c', 'scene_clusters.c', 'scene_engine.c', 'scene_mask.c')] + ['-lm']
    subprocess.run(command, check=True)
    after = {str(p.relative_to(root)):sha(p) for p in originals}
    if before != after:
        raise RuntimeError('Original sources changed during build')
    manifest = dict(scope='isolated audio-only host, no scene/int8/unit port',
                    builder_sha256=sha(Path(__file__)), helper_sha256=sha(Path(__file__).with_name('build_direct_object_host.py')),
                    originals=before, copied=copied, compiler=cc, command=command,
                    binary_sha256=sha(executable), legacy_load_and_infer='abort stubs',
                    learned_controls='none; reset controls remain zero; injected eligible spectra use direct/no-bed path',
                    routing='original deterministic front/LFE DSP; original rear body/gains supplied explicitly by caller',
                    requirements='HSSR backend; eligible stereo PCM16 WAV same length; all legacy model/mask arguments refused')
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print('UNIFIED_AUDIO_HOST_BUILT', executable, manifest['binary_sha256'])


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out-dir', type=Path, required=True)
    p.add_argument('--compiler', default='x86_64-w64-mingw32-gcc')
    a = p.parse_args()
    build(a.out_dir, a.compiler)
