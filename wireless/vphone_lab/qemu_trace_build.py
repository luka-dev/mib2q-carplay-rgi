"""Relink an owned diagnostic QEMU using two copied sources; never edit Tools/."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shlex
import shutil
import subprocess

from . import lab
from wireless.receiver_bridge import qemu_capture
from wireless.lab_common import qnx_receiver


def installed_paths(argument):
    # Homebrew may have upgraded dependencies since the existing QEMU build.
    argument = re.sub(r'/opt/homebrew/Cellar/([^/]+)/[^/]+/',
                      lambda m: '/opt/homebrew/opt/'+m[1]+'/', argument)
    return argument


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f'QEMU instrumentation anchor changed: {old[:80]}')
    return source.replace(old, new)


def instrument(source, unit):
    source = replace_once(source, '#include "qemu/osdep.h"',
        '#include "qemu/osdep.h"\n#include "qemu/bswap.h"\n#include "qemu/timer.h"\n'
        f'#define MHT_UNIT "process"\n'+ ('#define MHT_IMPLEMENTATION\n' if unit == 'socket' else '') +
        '#include "qemu_net_trace.h"')
    if unit == 'socket':
        return replace_once(source, '    if (qemu_send_packet_async(&s->nc, rs->buf,',
            '    mht_packet("socket_rx", rs->buf, rs->packet_len);\n'
            '    if (qemu_send_packet_async(&s->nc, rs->buf,')
    source = replace_once(source, '    memcpy(in_buf, buf, size);',
        '    if (!is_rndis(s)) mht_packet("usb_accept", buf, size);\n    memcpy(in_buf, buf, size);')
    source = replace_once(source, '        /* no short packet necessary */',
        '        if (!is_rndis(s)) mht_packet("usb_complete", s->in_buf, s->in_len);\n'
        '        /* no short packet necessary */')
    source = replace_once(source, '            qemu_send_packet(qemu_get_queue(s->nic), s->out_buf, s->out_ptr);',
        '            mht_packet("usb_tx", s->out_buf, s->out_ptr);\n'
        '            qemu_send_packet(qemu_get_queue(s->nic), s->out_buf, s->out_ptr);')
    return source


def build():
    previous_output = qnx_receiver.OUT
    try:
        qnx_receiver.set_output(lab.OUT / 'qnx-network')
        if qnx_receiver.running(): raise RuntimeError('stop the owned QNX guest before relinking QEMU')
    finally:
        qnx_receiver.OUT = previous_output
    directory = qemu_capture.QEMU.parent
    out = lab.OUT / 'qemu-net-trace'
    out.mkdir(parents=True, exist_ok=True)
    header = lab.ROOT / 'wireless/lab_common/qemu_net_trace.h'
    database = json.loads((directory / 'compile_commands.json').read_text())
    replacements, fingerprints, patched, executed = {}, {}, {}, []
    for unit, suffix in [('socket', 'net/socket.c'), ('usb', 'hw/usb/dev-network.c')]:
        matches = [r for r in database if r['file'].endswith(suffix)]
        if len(matches) != 1: raise ValueError('ambiguous QEMU compile command')
        entry = matches[0]
        original = (directory / entry['file']).resolve()
        text = original.read_text()
        fingerprints[suffix] = hashlib.sha256(original.read_bytes()).hexdigest()
        source, obj = out / f'{unit}.c', out / f'{unit}.o'
        source.write_text(instrument(text, unit))
        patched[suffix] = hashlib.sha256(source.read_bytes()).hexdigest()
        args = [installed_paths(a) for a in shlex.split(entry['command'])]
        for option in ['-MQ', '-MF']:
            i = args.index(option); del args[i:i+2]
        args.remove('-MD')
        args[args.index('-o')+1] = str(obj)
        args[args.index('-c')+1] = str(source)
        args += ['-iquote', str(original.parent), '-iquote', str(header.parent)]
        executed.append(args)
        subprocess.run(args, cwd=directory, check=True)
        replacements[entry['output']] = str(obj)
    commands = subprocess.check_output([shutil.which('ninja') or 'ninja', '-t', 'commands',
                                        'qemu-system-arm'], cwd=directory, text=True).splitlines()
    links = [shlex.split(c) for c in commands if ' -o qemu-system-arm-unsigned ' in c]
    if len(links) != 1: raise ValueError('ambiguous QEMU link command')
    if any(links[0].count(obj) != 1 for obj in replacements):
        raise ValueError('expected exactly one replacement per QEMU object')
    args = [installed_paths(replacements.get(a, a)) for a in links[0]]
    binary = out / 'qemu-system-arm'
    temporary = out / 'qemu-system-arm.pending'
    args[args.index('-o')+1] = str(temporary)
    executed.append(args)
    subprocess.run(args, cwd=directory, check=True)
    subprocess.run(['codesign', '--force', '--sign', '-', str(temporary)], check=True)
    subprocess.run([str(temporary), '--version'], check=True)
    temporary.replace(binary)
    value = {'schema':1, 'binary':str(binary), 'sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
             'base_binary_sha256':hashlib.sha256(qemu_capture.QEMU.read_bytes()).hexdigest(),
             'sources':fingerprints, 'instrumented_sources':patched,
             'header_sha256':hashlib.sha256(header.read_bytes()).hexdigest()}
    (out / 'commands.json').write_text(json.dumps(executed, indent=2)+'\n')
    (out / 'build.json').write_text(json.dumps(value, indent=2)+'\n')
    print(json.dumps(value, indent=2))


if __name__ == '__main__':
    argparse.ArgumentParser(description=__doc__).parse_args()
    build()
