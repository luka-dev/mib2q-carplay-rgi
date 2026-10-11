import unittest
import sys
import tempfile
from types import SimpleNamespace
from pathlib import Path
from unittest.mock import patch

import qnx_network


class QNXNetworkTests(unittest.TestCase):
    def test_ui_request_requires_isolated_software_mfi_build(self):
        with self.assertRaisesRegex(ValueError, "requires software MFi"):
            qnx_network.build("unused", request_ui=True)

    def test_plaintext_video_capture_requires_isolated_software_mfi_build(self):
        with self.assertRaisesRegex(ValueError, "capture requires software MFi"):
            qnx_network.build("unused", capture_video=True)

    def test_host_diagnostics_are_separate_from_guest_uart(self):
        captured = {}
        def popen(command, **kwargs):
            captured["stdout"] = kwargs["stdout"].name
            captured["stderr"] = kwargs["stderr"].name
            return SimpleNamespace(pid=31415)
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out/"qemu_start_debug.bin").write_bytes(b"fixture")
            with patch.object(qnx_network, "OUT", out), patch.object(qnx_network, "running", return_value=None), \
                    patch.object(sys, "argv", ["qnx_network.py", "start", "--transport", "socket"]), \
                    patch.object(qnx_network.subprocess, "Popen", side_effect=popen):
                qnx_network.main()
            self.assertEqual(captured["stdout"], str(out/"serial.log"))
            self.assertEqual(captured["stderr"], str(out/"qemu-host.log"))

    def test_software_mfi_preserves_hook_guard_and_separate_rpc_interface(self):
        source = "    reopen /dev/ser1\n" + \
            "    /proc/boot/ifconfig lo0 alias 10.0.0.16\n" + \
            "    LD_PRELOAD=/proc/boot/libdio_capture_probe.so:/proc/boot/libdioprobe.so\n" + \
            "/eso/lib/factories/example.so=/work/example.so\n"
        with patch.object(Path, "read_bytes", return_value=b"stock fixture"), \
                patch.object(qnx_network.hashlib, "sha256") as digest:
            digest.return_value.hexdigest.return_value = "01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2"
            result = qnx_network.network_manifest(source, pairing=True, media=True, software_mfi=True)
        self.assertIn("ifconfig ecm1 10.0.3.15", result)
        self.assertIn("MHI2Q_CP_WIRELESS_MEDIA=1", result)
        self.assertIn("LD_PRELOAD=/proc/boot/libdio_capture_probe.so:/proc/boot/libcarplay_hook.so:", result)
        self.assertIn("/mnt/pairing/store", result)

    def test_manifest_orders_drivers_and_preserves_reusable_tools(self):
        source = "    /proc/boot/ifconfig lo0 alias 10.0.0.16\n" + \
            "/proc/boot/ifconfig=/work/extracted/proc/boot/ifconfig\n" + \
            "/eso/lib/factories/example.so=/work/extracted/example.so\n"
        result = qnx_network.network_manifest(source)
        self.assertLess(result.index("/mhi2_usb_enumeration"), result.index("mount -T io-pkt"))
        self.assertIn("[data=copy] /proc/boot/ifconfig=", result)
        self.assertIn("ifconfig ecm0 10.0.2.15", result)
        self.assertLess(result.index("/proc/boot/devu-ohci.so="), result.index("/eso/lib/factories/"))
        self.assertIn("data=copy] /proc/boot/io-usb=", result)

    def test_invalid_base_manifest_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "requires native media"):
            qnx_network.network_manifest("", software_mfi=True)
        with self.assertRaisesRegex(ValueError, "requires pairing"):
            qnx_network.network_manifest("", media=True)
        for source in ("", "    /proc/boot/ifconfig lo0 alias 10.0.0.16\n" * 2):
            with self.assertRaises(ValueError):
                qnx_network.network_manifest(source)

    def test_forward_is_loopback_and_does_not_attach_host_drives(self):
        command = qnx_network.command()
        self.assertIn("user,id=n0,hostfwd=tcp:127.0.0.1:32500-:5000", command)
        self.assertNotIn("-drive", command)
        self.assertIn("usb-net,netdev=n0", command)
