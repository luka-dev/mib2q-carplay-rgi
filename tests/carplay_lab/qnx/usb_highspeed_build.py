"""Build a local EHCI/High Speed experiment from copied QEMU compilation units."""
import difflib
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

from tests.carplay_lab.qnx import qemu_capture
from tests.carplay_lab.vphone.qemu_trace_build import installed_paths, replace_once, instrument

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "build/wired-altscreen-30fps-re/qemu-highspeed"


def patch(source, unit, controller="platform-ehci-usb", native_kqueue=False):
    if unit == "timer":
        if native_kqueue:
            source = replace_once(source, '#include "qemu/osdep.h"',
                '#include "qemu/osdep.h"\n#ifdef __APPLE__\n#include "darwin_poll.h"\n#endif')
            return replace_once(source, '    return g_poll(fds, nfds, qemu_timeout_ns_to_ms(timeout));',
                '#ifdef __APPLE__\n    return fps_darwin_poll_ns(fds, nfds, timeout);\n#else\n'
                '    return g_poll(fds, nfds, qemu_timeout_ns_to_ms(timeout));\n#endif')
        source = replace_once(source, '#include "qemu/osdep.h"',
                              '#include "qemu/osdep.h"\n#ifdef __APPLE__\n#include <poll.h>\n#endif')
        return replace_once(source,
            '    return g_poll(fds, nfds, qemu_timeout_ns_to_ms(timeout));',
            '''#ifdef __APPLE__
    _Static_assert(sizeof(GPollFD) == sizeof(struct pollfd), "pollfd size");
    _Static_assert(offsetof(GPollFD, fd) == offsetof(struct pollfd, fd), "pollfd fd");
    _Static_assert(offsetof(GPollFD, events) == offsetof(struct pollfd, events), "pollfd events");
    _Static_assert(offsetof(GPollFD, revents) == offsetof(struct pollfd, revents), "pollfd revents");
    return poll((struct pollfd *)fds, nfds, qemu_timeout_ns_to_ms(timeout));
#else
    return g_poll(fds, nfds, qemu_timeout_ns_to_ms(timeout));
#endif''')
    if unit == "virt":
        source = replace_once(source, 'qdev_new("sysbus-ohci")', f'qdev_new("{controller}")')
        source = replace_once(source, '    qdev_prop_set_uint32(dev, "num-ports", 3);\n', '')
        return replace_once(source, '"compatible", "generic-ohci"', '"compatible", "generic-ehci"')
    # usb_desc_init starts at Full Speed; keep a valid fallback descriptor tree.
    # Duplicate interfaces/configurations so each speed advertises valid bulk MPS.
    start = source.index('static const USBDescIface desc_iface_rndis[]')
    end = source.index('static const USBDesc desc_net =')
    high = source[start:end]
    anchor = '.wMaxPacketSize        = 0x40,'
    if high.count(anchor) != 4:
        raise ValueError("USB bulk descriptor anchors changed")
    high = high.replace(anchor, '.wMaxPacketSize        = 0x200,')
    # High Speed interrupt intervals use powers of two in 125-us microframes.
    high = high.replace('1 << LOG2_STATUS_INTERVAL_MSEC,', '9,')
    for name in ('desc_iface_rndis', 'desc_iface_cdc', 'desc_device_net'):
        high = high.replace(name, name + '_high')
    source = source[:end] + high + source[end:]
    source = replace_once(source, '    .full = &desc_device_net,',
                          '    .full = &desc_device_net,\n    .high = &desc_device_net_high,')
    source = replace_once(source, '    uc->handle_reset   = usb_net_handle_reset;',
                          '    uc->handle_attach  = usb_desc_attach;\n    uc->handle_reset   = usb_net_handle_reset;')
    source = replace_once(source, '(s->in_len & (64 - 1))',
                          '(s->in_len & ((s->dev.speed == USB_SPEED_HIGH ? 512 : 64) - 1))')
    return replace_once(source, 'p->iov.size % 64',
                        'p->iov.size % (s->dev.speed == USB_SPEED_HIGH ? 512 : 64)')


def asynchronous_rx(source):
    source = replace_once(source, '    uint8_t in_buf[2048];',
                          '    uint8_t in_buf[2048];\n    USBPacket *pending_in;')
    source = replace_once(source, '''static void usb_net_handle_reset(USBDevice *dev)
{
}''', '''static void usb_net_handle_reset(USBDevice *dev)
{
    USBNetState *s = USB_NET(dev);
    USBPacket *p = s->pending_in;
    s->pending_in = NULL;
    if (p) {
        p->status = USB_RET_STALL;
        usb_packet_complete(dev, p);
    }
    s->in_ptr = s->in_len = s->out_ptr = 0;
}

static void usb_net_cancel_packet(USBDevice *dev, USBPacket *p)
{
    USBNetState *s = USB_NET(dev);
    assert(s->pending_in == p);
    s->pending_in = NULL;
}''')
    source = replace_once(source, '''    if (!s->in_len) {
        p->status = USB_RET_NAK;
        return;
    }''', '''    if (!s->in_len) {
        assert(!s->pending_in);
        s->pending_in = p;
        p->status = USB_RET_ASYNC;
        return;
    }''')
    source = replace_once(source, '    usb_wakeup(s->bulk_in, 0);', '''    if (s->pending_in) {
        USBPacket *p = s->pending_in;
        s->pending_in = NULL;
        p->status = USB_RET_SUCCESS;
        usb_net_handle_datain(s, p);
        usb_packet_complete(&s->dev, p);
    } else {
        usb_wakeup(s->bulk_in, 0);
    }''')
    return replace_once(source, '    uc->handle_attach  = usb_desc_attach;',
                        '    uc->cancel_packet  = usb_net_cancel_packet;\n    uc->handle_attach  = usb_desc_attach;')


def build(controller="platform-ehci-usb", async_usb=False, native_poll=False, native_kqueue=False):
    OUT.mkdir(parents=True, exist_ok=True)
    directory = qemu_capture.QEMU.parent
    baseline = hashlib.sha256(qemu_capture.QEMU.read_bytes()).hexdigest()
    database = json.loads((directory / "compile_commands.json").read_text())
    replacements, executed, fingerprints = {}, [], {}
    units = [("virt", "hw/arm/virt.c"), ("usb", "hw/usb/dev-network.c")]
    if native_poll or native_kqueue:
        units.append(("timer", "util/qemu-timer.c"))
    for unit, suffix in units:
        matches = [r for r in database if r["file"].endswith(suffix)]
        if len(matches) != 1:
            raise ValueError(f"ambiguous compilation unit: {suffix}")
        entry = matches[0]
        original = (directory / entry["file"]).resolve()
        old = original.read_text()
        new = patch(old, unit, controller, native_kqueue)
        if unit == "usb":
            if async_usb:
                new = asynchronous_rx(new)
            new = instrument(new, "usb").replace('#define MHT_UNIT "process"',
                  '#define MHT_UNIT "process"\n#define MHT_IMPLEMENTATION')
        source, obj = OUT / f"{unit}.c", OUT / f"{unit}.o"
        source.write_text(new)
        (OUT / f"{unit}.diff").write_text("".join(difflib.unified_diff(
            old.splitlines(True), new.splitlines(True), fromfile=suffix, tofile=str(source))))
        fingerprints[suffix] = {"original_sha256": hashlib.sha256(original.read_bytes()).hexdigest(),
                                "patched_sha256": hashlib.sha256(source.read_bytes()).hexdigest()}
        arguments = [installed_paths(a) for a in shlex.split(entry["command"])]
        for option in ("-MQ", "-MF"):
            at = arguments.index(option)
            del arguments[at:at+2]
        arguments.remove("-MD")
        arguments[arguments.index("-o")+1] = str(obj)
        arguments[arguments.index("-c")+1] = str(source)
        arguments += ["-iquote", str(original.parent)]
        if unit == "timer" and native_kqueue:
            header = Path(__file__).with_name("darwin_poll.h")
            shutil.copyfile(header, OUT / header.name)
            fingerprints[header.name] = hashlib.sha256(header.read_bytes()).hexdigest()
            arguments += ["-iquote", str(OUT)]
        if unit == "usb":
            header = ROOT / "tests/carplay_lab/common/qemu_net_trace.h"
            shutil.copyfile(header, OUT / header.name)
            fingerprints[header.name] = hashlib.sha256(header.read_bytes()).hexdigest()
            arguments += ["-iquote", str(OUT)]
        executed.append(arguments)
        subprocess.run(arguments, cwd=directory, check=True)
        if unit != "timer":
            replacements[entry["output"]] = str(obj)
    commands = subprocess.check_output([shutil.which("ninja") or "ninja", "-t", "commands", "qemu-system-arm"],
                                       cwd=directory, text=True).splitlines()
    links = [shlex.split(c) for c in commands if " -o qemu-system-arm-unsigned " in c]
    if len(links) != 1 or any(links[0].count(obj) != 1 for obj in replacements):
        raise ValueError("QEMU link command changed")
    arguments = [installed_paths(replacements.get(a, a)) for a in links[0]]
    if native_poll or native_kqueue:
        # The original timer object is a member of this archive. Supplying all
        # its definitions first prevents the linker from extracting that member.
        if arguments.count("libqemuutil.a") != 1:
            raise ValueError("QEMU utility archive link changed")
        arguments.insert(arguments.index("libqemuutil.a"), str(OUT / "timer.o"))
    binary = OUT / "qemu-system-arm"
    temporary = OUT / "qemu-system-arm.pending"
    arguments[arguments.index("-o")+1] = str(temporary)
    executed.append(arguments)
    subprocess.run(arguments, cwd=directory, check=True)
    subprocess.run(["codesign", "--force", "--sign", "-", str(temporary)], check=True)
    subprocess.run([str(temporary), "--version"], check=True)
    if hashlib.sha256(qemu_capture.QEMU.read_bytes()).hexdigest() != baseline:
        raise RuntimeError("shared QEMU changed during experiment")
    temporary.replace(binary)
    result = {"binary": str(binary), "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
              "controller": controller,
              "async_usb": async_usb,
              "native_poll": native_poll,
              "native_kqueue": native_kqueue,
              "base_sha256": baseline, "sources": fingerprints, "commands": executed,
              "scope": "local experiment only; shared QEMU sources, objects and binary untouched"}
    (OUT / "build.json").write_text(json.dumps(result, indent=2)+"\n")
    print(json.dumps({k:v for k,v in result.items() if k != "commands"}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exynos", action="store_true", help="test the EHCI layout with operational registers at 0x10")
    parser.add_argument("--async-usb", action="store_true", help="experiment with pending bulk-IN requests instead of NAK polling")
    backend = parser.add_mutually_exclusive_group()
    backend.add_argument("--native-poll", action="store_true", help="use macOS poll directly in qemu_poll_ns instead of GLib")
    backend.add_argument("--native-kqueue", action="store_true", help="preserve sub-millisecond macOS waits with kqueue")
    args = parser.parse_args()
    if args.exynos:
        OUT = ROOT / "build/wired-altscreen-30fps-re/qemu-highspeed-exynos"
    if args.async_usb:
        OUT = OUT.with_name(OUT.name + "-async")
    if args.native_poll:
        OUT = OUT.with_name(OUT.name + "-poll")
    if args.native_kqueue:
        OUT = OUT.with_name(OUT.name + "-kqueue")
    build("exynos4210-ehci-usb" if args.exynos else "platform-ehci-usb", args.async_usb, args.native_poll, args.native_kqueue)
