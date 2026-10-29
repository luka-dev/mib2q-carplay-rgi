"""Real codec roundtrip and RTP reordering under ASan/UBSan (no audio mock)."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class WirelessAudioCodec(unittest.TestCase):
    def test_direction_lifetimes(self):
        prefix=Path('/opt/homebrew')
        with tempfile.TemporaryDirectory(prefix='wireless-audio-directions-') as tmp:
            binary=Path(tmp)/'directions'
            command=[os.environ.get('CC','cc'),'-O1','-g','-pthread',
                     '-fsanitize=address,undefined','-ffunction-sections','-fdata-sections',
                     '-I'+str(prefix/'include/opus'),'-L'+str(prefix/'lib'),
                     'tests/wireless_audio_concurrency_test.c','hook/wireless/audio_codec.c',
                     'hook/wireless/audio_queue.c','hook/audio/aac_bridge.c',
                     '-lopus','-lm','-o',str(binary)]
            if os.uname().sysname=='Darwin':command.append('-Wl,-dead_strip')
            else:command+=['-Wl,--gc-sections','-ldl']
            if not prefix.exists():
                command=[arg for arg in command if not arg.startswith(('-I/opt/homebrew','-L/opt/homebrew'))]
                command.insert(1,'-I/usr/include/opus')
            subprocess.run(command,cwd=ROOT,check=True)
            subprocess.run([str(binary)],check=True,timeout=25)

    def test_opus_queue(self):
        prefix=Path('/opt/homebrew')
        with tempfile.TemporaryDirectory(prefix='wireless-audio-') as tmp:
            binary=Path(tmp)/'audio'
            command=[os.environ.get('CC','cc'),'-O1','-g','-fsanitize=address,undefined',
                     '-I'+str(prefix/'include/opus'),'-L'+str(prefix/'lib'),
                     'tests/wireless_audio_codec_test.c','hook/wireless/audio_codec.c',
                     'hook/wireless/audio_queue.c','hook/audio/aac_bridge.c',
                     '-lopus','-lpthread','-lm','-o',str(binary)]
            if not prefix.exists():
                command=[arg for arg in command if not arg.startswith(('-I/opt/homebrew','-L/opt/homebrew'))]
                command.insert(1,'-I/usr/include/opus')
                command.append('-ldl')
            subprocess.run(command,cwd=ROOT,check=True)
            subprocess.run([str(binary)],check=True,timeout=30)

if __name__=='__main__':unittest.main()
