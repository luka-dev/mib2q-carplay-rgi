"""Stage production daemon plus a synthetic phone in the real SI/DIO VM."""
import hashlib
import os
import shlex
import subprocess
import zlib

def extend(build, work, repo, payloads):
    audi=repo.parents[1];guest='/src/'+str(work.relative_to(repo))
    def docker(command, rust=False):
        subprocess.run(['docker','run','--rm','--platform=linux/amd64','-v',f'{repo}:/src',
                        '-w','/src','qnx65-armv7-toolchain:8.5-rust' if rust else 'qnx65-armv7-toolchain:latest',
                        'bash','-c',command],check=True)
    for extra in ['--features qemu-probe --bin phone-qemu-probe','--bin mhi2q-wireless-daemon']:
        docker('/opt/rust/qnx-cargo build -q --manifest-path /src/wireless/daemon/Cargo.toml '+extra,True)
    cc='arm-unknown-nto-qnx6.5.0eabi-gcc'
    assets=[]
    for source,name,flags in [
        ('wireless/daemon/probes/mock_mfi_qemu.c','libmock_mfi_qemu.so','-shared -fPIC'),
        ('wireless/daemon/probes/mock_dns_sd_qemu.c','libqemu_dns_sd.so','-shared -fPIC'),
        ('wireless/daemon/probes/fake_uaputl.c','fake_uaputl',''),
        ('wireless/si/si_qemu_integrated_launch.c','integrated-launch',''),
        ('wireless/si/si_qemu_integrated_observer.c','integrated-observer','-D_POSIX_C_SOURCE=200809L'),
    ]:
        docker(f'{cc} -std=gnu99 -Os -Wall -Wextra -Werror {flags} /src/{source} -o {guest}/{name} -lsocket')
        assets.append(f'/proc/boot/{name}={guest}/{name}')
    (work/'integrated-entropy').write_bytes(os.urandom(32))
    (work/'integrated-entropy-next').write_bytes(os.urandom(32))
    assets.append(f'/proc/boot/integrated-entropy-next={guest}/integrated-entropy-next')
    if not (work/'integrated-fault').exists():
        (work/'integrated-fault').write_text('blackhole\n')
    assets.append(f'/proc/boot/integrated-fault={guest}/integrated-fault')
    if not (work/'integrated-cycles').exists():(work/'integrated-cycles').write_text('2\n')
    assets.append(f'/proc/boot/integrated-cycles={guest}/integrated-cycles')
    assets += [f'/proc/boot/integrated-entropy={guest}/integrated-entropy',
               '/proc/boot/dio_manager.json=/src/wireless/daemon/probes/dio_manager.fixture.json']
    # Match production compressed delivery: mkifs cannot rewrite Rust ELFs.
    unpack=[]
    for name in ['mhi2q-wireless-daemon','phone-qemu-probe']:
        raw=(repo/'wireless/daemon/target/armv7-unknown-nto-qnx650/release'/name).read_bytes()
        packed=zlib.compress(raw,9);key='integrated_'+name
        (work/(key+'.zlib')).write_bytes(packed)
        payloads[key]={'size':len(raw),'crc32':f'{zlib.crc32(raw):08x}',
                       'sha256':hashlib.sha256(raw).hexdigest(),'packed_sha256':hashlib.sha256(packed).hexdigest()}
        assets.append(f'/etc/{key}.zlib={guest}/{key}.zlib')
        unpack.append(f'    /proc/boot/si_payload /etc/{key}.zlib /ramdisk/{name} {len(raw)} {zlib.crc32(raw):08x}')
    anchor='    /proc/boot/si_qemu_control_probe --dio\n'
    assert build.count(anchor)==1
    build=build.replace(anchor,'\n'.join(unpack)+'\n    /proc/boot/integrated-launch\n')
    return build+'\n'+'\n'.join(assets)+'\n'
