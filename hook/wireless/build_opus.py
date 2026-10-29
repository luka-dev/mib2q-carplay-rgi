#!/usr/bin/env python3
"""Pinned, PIC/fixed-point Opus for the QNX wireless converter only."""
import fcntl,hashlib,json,os,subprocess,tarfile,urllib.request
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/wireless-codecs'
VERSION='1.5.2'
SHA256='65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1'
URL=f'https://downloads.xiph.org/releases/opus/opus-{VERSION}.tar.gz'

def build():
    OUT.mkdir(parents=True,exist_ok=True)
    archive=OUT/f'opus-{VERSION}.tar.gz'
    if not archive.exists():
        temporary=archive.with_suffix('.download')
        with urllib.request.urlopen(URL,timeout=60) as response:temporary.write_bytes(response.read(20*1024*1024))
        temporary.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA256:raise RuntimeError('Opus source SHA-256 mismatch')
    variant=os.environ.get('QNX_TOOLCHAIN_VARIANT','latest')
    if variant not in ('latest','4.9','8.5'):raise ValueError('invalid QNX toolchain variant')
    image=f'qnx65-armv7-toolchain:{variant}'
    image_id=subprocess.check_output(['docker','image','inspect','--format','{{.Id}}',image],text=True).strip()
    record={'version':VERSION,'source_sha256':SHA256,'toolchain':variant,'image_id':image_id,
            'flags':'PIC fixed-point no-asm no-intrinsics mno-unaligned-access'}
    stamp=OUT/'opus-qnx/receipt.json';library=OUT/'opus-qnx/.libs/libopus.a'
    if stamp.exists() and library.exists():
        cached=json.loads(stamp.read_text());digest=cached.pop('library_sha256',None)
        if cached==record and digest==hashlib.sha256(library.read_bytes()).hexdigest():
            print('Pinned QNX Opus cached');return
    source=OUT/f'opus-{VERSION}'
    if not source.exists():
        with tarfile.open(archive) as t:t.extractall(OUT,filter='data')
    (OUT/'opus-qnx').mkdir(exist_ok=True)
    command='''export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
export CC=arm-unknown-nto-qnx6.5.0eabi-gcc AR=arm-unknown-nto-qnx6.5.0eabi-ar RANLIB=arm-unknown-nto-qnx6.5.0eabi-ranlib
export CFLAGS="-O2 -fPIC -mno-unaligned-access -mfpu=neon -mfloat-abi=softfp"
../opus-1.5.2/configure --host=arm-unknown-nto-qnx6.5.0eabi --enable-static --disable-shared --enable-fixed-point --disable-doc --disable-extra-programs --disable-rtcd --disable-intrinsics --disable-asm
make -j4
'''
    subprocess.run(['docker','run','--rm','--platform=linux/amd64','-v',f'{ROOT}:/src','-w','/src/build/wireless-codecs/opus-qnx',f'qnx65-armv7-toolchain:{variant}','bash','-ec',command],check=True)
    if not library.exists():raise RuntimeError('QNX Opus static library missing')
    record['library_sha256']=hashlib.sha256(library.read_bytes()).hexdigest()
    stamp.write_text(json.dumps(record,indent=2)+'\n')
    print('Built pinned QNX Opus',library)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    with (OUT/'.opus-build.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        build()

if __name__=='__main__':main()
