"""SI-owned native DIO in the disposable QNX fixture.

Real SI/FEC/COMM/DIO; explicit RCC grant simulation and peripheral backends.
Never deployed on the vehicle. Exact compressed delivery for hooked ELFs.
"""
from __future__ import annotations
import hashlib
import json
import re
import shlex
import subprocess
import zlib
from pathlib import Path


def extend(buildfile: str, work: Path, fw: Path, repo: Path, payloads: dict,
           receiver_hook: Path | None = None, peer_health: bool = False,
           integrated: bool = False) -> str:
    audi = repo.parents[1]
    guest = '/src/' + str(work.relative_to(repo))
    legacy = repo / 'wireless/receiver_bridge/out/inspect_ifs/proc/boot'
    def source(path: Path) -> str:
        return '/audi/' + str(path.relative_to(audi))
    gcc = 'arm-unknown-nto-qnx6.5.0eabi-gcc'
    command = (f'{gcc} -std=c11 -fPIC -shared -Os -Wall -Wextra -Werror '
               '-DMHI2Q_CAPTURE_BRIDGE -DMHI2Q_BRIDGE_PRESTART '
               '-DMHI2Q_BRIDGE_DISPLAY_MODE=0 -DMHI2Q_BRIDGE_STOCK210 '
               '-DMHI2Q_BRIDGE_PROPERTY_HANDOFF -DMHI2Q_BRIDGE_SI_TRANSPORT -DMHI2Q_SI_QEMU_HMI '
               + ('-DMHI2Q_CAPTURE_VIDEO_QEMU -DMHI2Q_CAPTURE_INPUT_ONLY_OMX_QEMU ' if peer_health or integrated else '')
               + ('-DMHI2Q_SI_QEMU_INTEGRATED ' if integrated else '')
               + shlex.quote('-DMHI2Q_CAPTURE_INTERFACE_NAME="lo0"') + ' '
               + shlex.quote('-DMHI2Q_BRIDGE_INTERFACE_NAME="lo0"') + ' '
               f'-o {guest}/libdio_receiver_bridge.so '
               '/src/wireless/receiver_bridge/capture_probe.c '
               '/src/wireless/receiver_bridge/bridge.c '
               '/src/wireless/receiver_bridge/property_handoff.c '
               '/src/wireless/receiver_bridge/si_transport.c '
               '/src/wireless/receiver_bridge/now_playing.c '
               '/src/wireless/receiver_bridge/si_state.c '
               '/src/wireless/receiver_bridge/si_location.c '
               '/src/wireless/si/si_qemu_hmi.c '
               '/src/wireless/si/si_qemu_network.c '
               + ('/src/wireless/si/si_qemu_audio.c ' if integrated else '') + '-lsocket')
    subprocess.run(['docker', 'run', '--rm', '--platform=linux/amd64',
                    '-v', f'{audi}:/audi', '-v', f'{repo}:/src',
                    'qnx65-armv7-toolchain:latest', 'bash', '-c', command], check=True)
    inputs = {}
    health_inputs = {}
    if peer_health or integrated:
        from qemu_peer_health import stage
        entries, health_inputs = stage(work, repo, integrated=integrated)
        # Keep boot executables before the large factory-plugin tail.
        anchor = '/proc/boot/ifconfig='
        at = buildfile.index(anchor)
        buildfile = buildfile[:at] + entries + buildfile[at:]
    unpack = []
    files = {
        'dio_manager': fw / 'MU1316-appimg/eso/bin/apps/dio_manager',
        'libairplay.so': fw / 'MU1316-appimg/eso/lib/libairplay.so',
        'libcarplay_hook.so': receiver_hook or repo / 'build/wireless/libcarplay_hook.so',
        'libdio_receiver_bridge.so': work / 'libdio_receiver_bridge.so',
        'dio_owner_exec': repo / 'wireless/integration/out/dio_owner_exec.handoff.offline',
    }
    for name, path in files.items():
        raw = path.read_bytes()
        packed = zlib.compress(raw, 9)
        key = 'dio_' + name
        (work / (key + '.zlib')).write_bytes(packed)
        record = {'size': len(raw), 'crc32': f'{zlib.crc32(raw):08x}',
                  'sha256': hashlib.sha256(raw).hexdigest(),
                  'packed_sha256': hashlib.sha256(packed).hexdigest()}
        payloads[key] = record
        inputs[name] = {'source': str(path), 'sha256': record['sha256']}
        buildfile += f'\n/etc/{key}.zlib={guest}/{key}.zlib\n'
        unpack.append(f'/etc/{key}.zlib /ramdisk/{name} {len(raw)} {record["crc32"]}')
        if name != 'dio_manager':
            buildfile += f'[type=link] /proc/boot/{name}=/ramdisk/{name}\n'
    libraries = ['libNmeSDK.so', 'libNme.so', 'libNmeBaseClasses.so', 'libNmeImage.so',
                 'liblibstd.so', 'libOSAbstraction.so', 'libpps.so.1', 'libusbdi.so.2',
                 'libdns_sd.so.1', 'libOmxBase.so', 'libOmxCore.so', 'libomxctx.so.1',
                 'libscreen.so.1', 'libasound.so.2', 'libbacktrace.so.1']
    for name in libraries + ['ppsmk', 'devstub', 'mdnsd']:
        path = (fw / 'MU1316-appimg/armle/usr/sbin/mdnsd') if name=='mdnsd' else legacy / name
        if path.read_bytes()[:4] != b'\x7fELF':
            raise ValueError(f'expected real ELF: {path}')
        buildfile += f'/proc/boot/{name}={source(path)}\n'
        inputs[name] = {'source': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    # Native DIO service factories (observed startClass plus its declared
    # vehicle/position/Bluetooth interfaces), not the entire 129 MB registry.
    terms = [b'carplay', b'Carplay', b'EsoPosProviderFull',
             b'BluetoothSmartphoneIntegration', b'DSICarVehicleStates',
             b'DSIGeneralVehicleStates']
    for path in sorted((fw / 'MU1316-appimg/eso/lib/factories').glob('lib_*.so')):
        if any(term in path.read_bytes() for term in terms):
            target = '/eso/lib/factories/' + path.name
            if target + '=' not in buildfile:
                buildfile += f'{target}={source(path)}\n'
    buildfile = buildfile.replace('io-pkt-v4-hc', 'io-pkt-v6-hc')
    # Dedicated virtual ECM interface, matching the established native iOS
    # stand. mDNS needs a routed non-loopback address, even in this VM.
    buildfile += ('/proc/boot/io-usb=/opt/qnx650/target/qnx6/armle-v7/sbin/io-usb\n'
                  '/proc/boot/devu-ohci.so=/opt/qnx650/target/qnx6/armle-v7/lib/dll/devu-ohci.so\n'
                  '/proc/boot/route=/opt/qnx650/target/qnx6/armle-v7/sbin/route\n'
                  '/proc/boot/devnp-ecmplus.so=/audi/Tools/qnx-gl-passthrough/build/vm_pull/root/mntapp/armle/lib/dll/devnp-ecmplus.so\n')
    buildfile = buildfile.replace('    /proc/boot/ifconfig lo0 alias 10.0.0.15',
        '    /proc/boot/ifconfig lo0 alias 10.0.0.15\n'
        '    /proc/boot/io-usb -d ohci ioport=0x090d0000,irq=44 &\n'
        '    waitfor /dev/io-usb 5\n'
        '    sleep 1\n'
        '    /proc/boot/mount -T io-pkt /proc/boot/devnp-ecmplus.so\n'
        '    sleep 1\n'
        '    /proc/boot/ifconfig ecm0 10.0.3.15 netmask 255.255.255.0 up\n'
        '    /proc/boot/route add default 10.0.3.2\n')
    # Replace the Git LFS placeholder used by the base USB-only fixture.
    buildfile = re.sub(r'^/proc/boot/libusbdi.so=.*$',
                       '[type=link] /proc/boot/libusbdi.so=/proc/boot/libusbdi.so.2',
                       buildfile, flags=re.M)
    scripts = {
        'carplay_startup.sh': repo / 'wireless/integration/out/carplay_startup.stock210.offline.sh',
        'carplay_monitor.sh': repo / 'deploy/smartphone_integrator/carplay_monitor.sh',
        'carplay_processes.sh': repo / 'deploy/smartphone_integrator/carplay_processes.sh',
    }
    for name, path in scripts.items():
        staged = work / name
        staged.write_bytes(path.read_bytes())
        if integrated and name=='carplay_startup.sh':
            # Test baseline: real-time stereo PCM, not the optional DSP policy.
            text=staged.read_text().replace('export AUDIO_SPATIAL=1','export AUDIO_SPATIAL=0')
            text=text.replace('export AUDIO_REALTIME_UPMIX=on','export AUDIO_REALTIME_UPMIX=off')
            staged.write_text(text)
        staged.chmod(0o755)
        buildfile += f'/proc/boot/{name}={guest}/{name}\n'
    buildfile += ('[type=link] /bin/sh=/proc/boot/sh\n'
                  '[type=link] /var/run=/ramdisk/var/run\n'
                  '[type=link] /mnt/app/armle/usr/sbin/mdnsd=/proc/boot/mdnsd\n')
    for name in ['mv', 'rm']:
        buildfile += f'/proc/boot/{name}=/opt/qnx650/target/qnx6/armle-v7/bin/{name}\n'
    buildfile += '/proc/boot/expr=/opt/qnx650/target/qnx6/armle-v7/usr/bin/expr\n'
    if integrated:
        buildfile += '[type=link] /var/app=/ramdisk/var/app\n'
        buildfile=buildfile.replace('    /proc/boot/mkdir -p /mnt/persist/pairing\n',
            '    /proc/boot/mkdir -p /mnt/persist/pairing /ramdisk/var/app/icab/tmp/37\n')
    env = ('export MHI2Q_CP_WIRELESS_PAIRING=1 MHI2Q_CP_WIRELESS_MEDIA=1\n'
           'export MHI2Q_CP_WIRELESS_ALT=1\n'
           'export MHI2Q_CP_PAIRING_DIR=/mnt/persist/pairing\n'
           'export MHI2Q_CP_MDNS_BINARY=/proc/boot/mdnsd\n')
    (work / 'wireless_carplay.env').write_text(env)
    buildfile += f'/proc/boot/wireless_carplay.env={guest}/wireless_carplay.env\n'
    # ProcessTNG passes this explicit envv. Parent environment is not inherited.
    config = (fw / 'MU1316-system/etc/eso/production/smartphone_integrator.json').read_text()
    start = config.index('"carplay":{')
    end = config.index('"carlife":{', start)
    child = config[start:end].replace('"exec":"dio_manager"', '"exec":"carplay_startup.sh"')
    child = child.replace('"path":"/mnt/app/eso/bin/apps"', '"path":"/proc/boot"')
    envs = ['LD_LIBRARY_PATH=/ramdisk:/eso/lib:/proc/boot',
            'IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production', 'IPL_CONFIG_DIR=/config',
            'H=/proc/boot', 'DIODIR=/ramdisk', 'PATH=/proc/boot:/bin',
            'OWNER_FILE=/mnt/persist/.mhi2q_dio_generation',
            'MHI2Q_CP_ENV_FILE=/proc/boot/wireless_carplay.env']
    if integrated:
        envs += ['AUDIO_SPATIAL=0', 'AUDIO_SPATIAL_ATMOS=0',
                 'AUDIO_SAD_CHANNELS=0', 'AUDIO_SAD_BITS=0']
    child = re.sub(r'"envs":\[[^\]]*\]', '"envs":' + json.dumps(envs), child)
    child = child.replace('"startupTimeout":10000', '"startupTimeout":30000')
    child = child.replace('"stopTimeout":3000', '"stopTimeout":30000')
    config = config[:start] + child + config[end:]
    (work / 'smartphone_integrator.dio.json').write_text(config)
    buildfile = re.sub(r'^(/(?:etc/eso/production|config)/smartphone_integrator.json)=.*$',
                       rf'\1={guest}/smartphone_integrator.dio.json', buildfile, flags=re.M)
    buildfile = buildfile.replace('ram capacity=32768', 'ram capacity=131072')
    buildfile = buildfile.replace('    display_msg "--- mprotect-on-text probe ---"',
        '    /proc/boot/si_payload ' + ' '.join(unpack) + '\n'
        '    /proc/boot/mkdir -p /mnt/persist/pairing /ramdisk/var/app/icab/tmp/37\n'
        '    /proc/boot/chmod 700 /mnt/persist/pairing\n'
        + ('    procmgr_symlink /dev/null /tmp/carplay_verbose\n' if integrated else '') +
        '    display_msg "--- mprotect-on-text probe ---"')
    buildfile = buildfile.replace('    procmgr_symlink /pps /ramdisk/pps',
        '    procmgr_symlink /pps /ramdisk/pps\n'
        '    /proc/boot/devstub /dev/otg-cinemo /dev/i2c8 &\n'
        '    /proc/boot/ppsmk\n'
        '    /proc/boot/mkdir -p /var/run/mdnsd\n'
        '    MDNS_DIRECTLINK_IFACE=lo0\n'
        '    LD_PRELOAD=/ramdisk/libdio_receiver_bridge.so\n'
        '    /proc/boot/mdnsd -debug &\n'
        '    sleep 1\n'
        '    /proc/boot/ls /var/run/mdnsd\n'
        '    LD_PRELOAD=\n')
    buildfile = buildfile.replace('    /proc/boot/si_qemu_control_probe\n',
                                ('    MHI2Q_QEMU_PEER_HEALTH=1 /proc/boot/si_qemu_control_probe --dio\n' if peer_health else
                                 '    /proc/boot/si_qemu_control_probe --dio\n') +
                                '    /proc/boot/si_stage1_client --require-delete\n')
    buildfile = buildfile.replace('    /proc/boot/pidin ar',
        '    /proc/boot/cat /tmp/carplay_wrapper.log\n'
        '    /proc/boot/cat /tmp/carplay_hook.log\n'
        '    /proc/boot/pidin ar')
    (work / 'dio-inputs.json').write_text(json.dumps({
        'inputs': inputs, 'rcc_fec_states_simulated': True,
        'peripheral_backends': ['screen', 'omx', 'asound', 'primary_mac'],
        'native_dns_sd': True, 'native_mdnsd': True,
        'peer_health_inputs': health_inputs,
        'hmi_input_simulated': {'native_startService': True, 'resolution': [1024, 480],
                                'hid_mask': 9, 'touch_resolution': [1024, 1024],
                                'initial_resource_modes': 'empty; real audio focus not proven'},
        'tcg_child_startup_timeout_ms': 30000, 'production_startup_timeout_ms': 10000,
        'tcg_child_stop_timeout_ms': 30000, 'production_stop_timeout_ms': 3000,
        'tcg_rust_startup_wait_ms': 40000, 'production_rust_startup_wait_ms': 12000,
        'interface': 'lo0', 'media_hmi_not_proven': True}, indent=2) + '\n')
    if integrated:
        from qemu_integrated import extend
        buildfile = extend(buildfile, work, repo, payloads)
    return buildfile
