"""Prevent bootstrap, USB sessions and previous-run captures becoming video success."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tests.carplay_lab.vphone import wireless_video as video


def report():
    return {
        **{key: True for key in ("native_authentication_verified", "native_identification_verified",
                                "native_wifi_response_sent", "daemon_start_session_sent")},
        "availability": [{"wireless_available": True}],
        "start_sessions": [{"wireless_attributes_present": True, "pairing_id": "real-pi", "port": 5000}],
        "guest_logs": [{"process": "airplayd", "reply": {"result": {"entries": [
            {"message": value} for value in ("[Session/WiFi/USB] WiFi eligible: Y, USB eligible: N",
                "PairVerify client done", "Endpoint connected state changed: isConnected = true")
        ]}}}],
    }


class WirelessVideoEvidenceTests(unittest.TestCase):
    def test_native_wifi_pair_verify_and_endpoint_are_all_required(self):
        self.assertTrue(all(video.native_session(report()).values()))
        for missing in range(3):
            value=report();del value["guest_logs"][0]["reply"]["result"]["entries"][missing]
            self.assertFalse(all(video.native_session(value).values()))
        value=report();value["guest_logs"][0]["process"]="test-peer"
        self.assertFalse(all(video.native_session(value).values()))

    def test_usb_start_or_unsolicited_multiple_starts_are_not_wireless(self):
        for edit in (lambda r:r["start_sessions"][0].update(wireless_attributes_present=False),
                     lambda r:r["start_sessions"].append(r["start_sessions"][0]),
                     lambda r:r["availability"][0].update(wireless_available=False)):
            value=report();edit(value)
            self.assertFalse(all(video.native_session(value).values()))

    def test_successful_handshake_without_new_decoder_bytes_is_not_video(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(video.subprocess,"run") as decoder:
            value=video.verify(report(),b"no fresh capture\n",Path(directory),"run")
            self.assertFalse(value["verified"])
            self.assertIn("missing complete records",value["error"])
            decoder.assert_not_called()
            self.assertEqual(list(Path(directory).iterdir()),[])

    def test_failed_transport_never_runs_a_decoder(self):
        value=report();value["error"]="connection dropped"
        with tempfile.TemporaryDirectory() as directory, patch.object(video,"extract") as extract:
            self.assertFalse(video.verify(value,b"old AVC",Path(directory),"run")["verified"])
            extract.assert_not_called()


def capture_record(data, marker):
    lines=[f"[{marker}] begin=1 bytes={len(data)} mono_ns=1"]
    lines += [f"[{marker}] record=1 offset={offset} hex={data[offset:offset+256].hex()}"
              for offset in range(0,len(data),256)]
    return ("\n".join(lines)+f"\n[{marker}] end=1 complete=1\n").encode()


@unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "independent decoder required")
class PairedScreenDecodeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.streams=[]
        for size in ("800x480","1440x542"):
            cls.streams.append(subprocess.run(["ffmpeg","-v","error","-f","lavfi","-i",
                f"color=size={size}:rate=30","-frames:v","2","-c:v","libx264","-preset","ultrafast",
                "-f","h264","pipe:1"],capture_output=True,check=True,timeout=30).stdout)

    def verify(self, primary=True, cluster=True, swapped=False, paired=True):
        first,second=self.streams[::-1] if swapped else self.streams
        trace=(capture_record(first,"native-video-qemu") if primary else b"")
        trace+=(capture_record(second,"native-cluster-qemu") if cluster else b"")
        value=report();value["paired_altscreen"]=paired
        with tempfile.TemporaryDirectory() as directory:
            return video.verify(value,trace,Path(directory),"dual")

    def test_both_real_streams_decode_independently(self):
        value=self.verify()
        self.assertTrue(value["verified"],value)
        self.assertTrue(value["altscreen"]["verified"])
        self.assertEqual(value["stream"]["nb_read_frames"],"2")
        self.assertEqual(value["altscreen"]["stream"]["nb_read_frames"],"2")
        self.assertNotEqual(value["sha256"],value["altscreen"]["sha256"])

    def test_primary_alone_cannot_pass_paired_profile(self):
        value=self.verify(cluster=False)
        self.assertFalse(value["verified"])
        self.assertFalse(value["altscreen"]["verified"])
        self.assertIn("missing complete records",value["altscreen"]["error"])

    def test_cluster_alone_cannot_pass_paired_profile(self):
        value=self.verify(primary=False)
        self.assertFalse(value["verified"])
        self.assertTrue(value["altscreen"]["verified"])

    def test_swapped_display_dimensions_are_refused(self):
        value=self.verify(swapped=True)
        self.assertFalse(value["verified"])
        self.assertFalse(value["altscreen"]["verified"])

    def test_incomplete_cluster_record_never_passes(self):
        trace=capture_record(self.streams[0],"native-video-qemu")
        trace+=capture_record(self.streams[1],"native-cluster-qemu").rsplit(b"[native-cluster-qemu] end=",1)[0]
        value=report();value["paired_altscreen"]=True
        with tempfile.TemporaryDirectory() as directory:
            result=video.verify(value,trace,Path(directory),"truncated")
            self.assertFalse(result["verified"])
            self.assertTrue(result["primary_verified"])
            self.assertFalse(result["altscreen"]["verified"])

    def test_primary_profile_still_accepts_only_primary(self):
        self.assertTrue(self.verify(cluster=False,paired=False)["verified"])


if __name__=="__main__":unittest.main()
