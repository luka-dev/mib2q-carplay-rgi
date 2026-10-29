"""Independent tone oracle rejects silence, swapped channels and dropouts."""
import unittest
import numpy as np
from .audio_pcm import verify

class AudioPCMTests(unittest.TestCase):
    def test_discrete_51_checks_every_channel(self):
        from .audio_playback import FREQUENCIES,waveform
        import io,wave
        raw=waveform(6)
        import struct
        self.assertEqual(struct.unpack_from('<I',raw,40)[0],0x60f)
        with wave.open(io.BytesIO(raw)) as source:
            self.assertEqual((source.getnchannels(),source.getframerate()),(6,48000))
        t=np.arange(5461)/48000
        pcm=np.column_stack([1200*np.sin(2*np.pi*f*t) for f in FREQUENCIES]).astype('<i2')
        self.assertTrue(verify(pcm.tobytes(),48000,6,6)['passed'])
        for channel in range(6):
            broken=pcm.copy();broken[:,channel]=0
            self.assertFalse(verify(broken.tobytes(),48000,6,6)['passed'])
        self.assertFalse(verify(pcm[:,[0,1,3,2,4,5]].tobytes(),48000,6,6)['passed'])
        broken=pcm.copy();broken[3000:3500,4]=0
        self.assertFalse(verify(broken.tobytes(),48000,6,6)['passed'])
        with self.assertRaises(ValueError):verify(pcm[:,:2].tobytes(),48000,2,6)

    def test_tones_and_negative_controls(self):
        for rate in (44100,48000):
            t=np.arange(16384)/rate
            pcm=np.column_stack((1200*np.sin(2*np.pi*997*t),1200*np.sin(2*np.pi*701*t))).astype('<i2')
            self.assertTrue(verify(pcm.tobytes(),rate,2)['passed'])
            self.assertFalse(verify(pcm[:,::-1].tobytes(),rate,2)['passed'])
            self.assertFalse(verify(bytes(pcm.nbytes),rate,2)['passed'])
            damaged=pcm.copy();damaged[8000:8500]=0
            self.assertFalse(verify(damaged.tobytes(),rate,2)['passed'])

if __name__=='__main__':unittest.main()
