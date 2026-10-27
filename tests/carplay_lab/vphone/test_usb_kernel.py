import struct
import unittest
from unittest.mock import patch

from tests.carplay_lab.vphone import usb_kernel as kernel


def branch_target(word, pc, bits, shift):
    immediate = (word >> shift) & ((1 << bits) - 1)
    if immediate & (1 << (bits - 1)):
        immediate -= 1 << bits
    return pc + immediate * 4


class USBKernelTests(unittest.TestCase):
    def test_partial_live_adapter_is_rejected(self):
        from unittest.mock import Mock
        original = [o for _, o, _ in kernel.VENDOR_BULK]
        replacement = [n for _, _, n in kernel.VENDOR_BULK]
        for values, expected in ((original, False), (replacement, True)):
            remote = Mock(); remote.read.side_effect = values
            self.assertEqual(kernel.verify_sites(remote, kernel.VENDOR_BULK, 0x4000), expected)
        remote = Mock(); remote.read.side_effect = replacement[:2] + original[2:]
        with self.assertRaises(RuntimeError):
            kernel.verify_sites(remote, kernel.VENDOR_BULK, 0x4000)
        remote.write.assert_not_called()

    def test_debug_listener_on_external_interface_is_rejected(self):
        from types import SimpleNamespace
        with patch.object(kernel.subprocess, 'run', return_value=SimpleNamespace(stdout='p123\nn*:60321\n')) as process:
            with self.assertRaises(RuntimeError):
                kernel.endpoint_owner({'libraryRoot':'/tmp', 'name':'mhi2-carplay'})
            process.assert_called_once()

    def test_vendor_adapter_changes_interface_and_pipe_only(self):
        expected_registers = (1, 2, 3, 1)
        expected_immediates = (255, 255, 255, 2)
        for (_, original, replacement), register, immediate in zip(kernel.VENDOR_BULK, expected_registers, expected_immediates):
            before = struct.unpack('<I', original)[0]
            after = struct.unpack('<I', replacement)[0]
            self.assertEqual(after & 31, register)
            self.assertEqual((after >> 5) & 0xffff, immediate)
            self.assertEqual(before & ~0x1fffe0, after & ~0x1fffe0)

    def test_reorder_preserves_native_call_and_failure_targets(self):
        result = kernel.patch(kernel.REGION)
        self.assertEqual(len(result), len(kernel.REGION))
        # The complete DRIVER_OK setter (11 instructions) is preserved.
        self.assertEqual(result[:44], kernel.REGION[100:144])
        old_failure = branch_target(struct.unpack_from('<I', kernel.REGION, 144)[0], kernel.ADDRESS + 144, 14, 5)
        new_failure = branch_target(struct.unpack_from('<I', result, 44)[0], kernel.ADDRESS + 44, 14, 5)
        self.assertEqual(old_failure, new_failure)
        old_call = branch_target(struct.unpack_from('<I', kernel.REGION, 8)[0], kernel.ADDRESS + 8, 26, 0)
        new_call = branch_target(struct.unpack_from('<I', result, 56)[0], kernel.ADDRESS + 56, 26, 0)
        self.assertEqual(old_call, new_call)
        # The original loop branch still returns to the moved buffer loop.
        loop = struct.unpack_from('<I', result, 144)[0]
        self.assertEqual(branch_target(loop, kernel.ADDRESS + 144, 19, 5), kernel.ADDRESS + 52)

    def test_changed_kernel_bytes_are_never_patched(self):
        for index in range(len(kernel.REGION)):
            region = bytearray(kernel.REGION); region[index] ^= 1
            with self.assertRaises(ValueError):
                kernel.patch(bytes(region))

    def test_external_relocation_bounds_are_checked(self):
        with self.assertRaises(ValueError):
            kernel.relocated(0x14000001, 0, 1 << 40, 0x1000, 0x2000)

    def test_binary_gdb_command_escapes_metacharacters(self):
        remote = object.__new__(kernel.Remote)
        from unittest.mock import Mock
        remote.socket = Mock()
        with patch.object(remote, 'receive', return_value=b'OK'):
            remote.command(b'X12,4:$#}*')
        packet = remote.socket.sendall.call_args.args[0]
        wire, checksum = packet[1:].rsplit(b'#', 1)
        self.assertTrue(wire.endswith(bytes((125, 4, 125, 3, 125, 93, 125, 10))))
        self.assertEqual(int(checksum, 16), sum(wire) % 256)

    def test_classic_hex_write_is_used_only_for_unsupported_binary_write(self):
        remote = object.__new__(kernel.Remote)
        with patch.object(remote, 'command', side_effect=[b'', b'OK']) as command:
            self.assertEqual(remote.write(0x1234, b'\x80\x20'), b'OK')
            self.assertEqual(command.call_args.args[0], b'M1234,2:8020')
        with patch.object(remote, 'command', return_value=b'E01') as command:
            self.assertEqual(remote.write(0x1234, b'\x80'), b'E01')
            command.assert_called_once()

    def test_fragmented_checksum_and_binary_payload(self):
        remote = object.__new__(kernel.Remote)
        from unittest.mock import Mock
        remote.socket = Mock()
        wire = b'$A}\x03#c1'
        remote.socket.recv.side_effect = [bytes((byte,)) for byte in wire]
        self.assertEqual(remote.receive(), b'A#')


if __name__ == '__main__':
    unittest.main()
