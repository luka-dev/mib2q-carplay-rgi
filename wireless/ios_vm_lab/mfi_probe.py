#!/usr/bin/env python3
"""Run the public MFi fixture against only the synthetic iOS/QEMU guest."""
from __future__ import annotations

import argparse
import json
import ipaddress
import os
from pathlib import Path
import selectors
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from wireless.lab_common.mfi_fixture import ROOT, FIXTURE, KEY_SHA256, fixture_key, sign_digest


def run(*, corrupt_signature: bool = False, corrupt_certificate: bool = False,
        identify: bool = False, start_carplay: bool = False, hold_seconds: int = 2,
        public_key: bytes | None = None, receiver_ipv6: str = "fe80::2") -> dict:
    if corrupt_signature and corrupt_certificate:
        raise ValueError("choose one negative control")
    if not 1 <= hold_seconds <= 300 or public_key is not None and len(public_key) != 32:
        raise ValueError("invalid session hold/public key")
    receiver = ipaddress.IPv6Address(receiver_ipv6)
    if not receiver.is_link_local or "%" in str(receiver):
        raise ValueError("expected a plain link-local receiver address")
    key, certificate = fixture_key()
    lab = ROOT / "build/ios-vm-lab"
    ssh = ["ssh", "-o", "BatchMode=yes", "-o", "IdentitiesOnly=yes",
           "-o", "StrictHostKeyChecking=yes", "-o", "ConnectTimeout=5",
           "-o", f"UserKnownHostsFile={lab / 'companion-arm/known-hosts'}",
           "-i", str(lab / "guest-access"), "-p", "32228", "root@127.0.0.1"]
    for remote, data in (("/tmp/mhi2-ios-vm-usb-probe.py", Path(__file__).with_name("usb_probe.py").read_bytes()),
                         ("/tmp/mhi2-ios-vm-certificate.p7b", certificate)):
        subprocess.run(ssh + [f"umask 077; cat > {remote}"], input=data, check=True,
                       capture_output=True, timeout=10)
    action = "start-carplay" if start_carplay else "identify" if identify else "authenticate"
    command = f"python3 /tmp/mhi2-ios-vm-usb-probe.py {action} --certificate /tmp/mhi2-ios-vm-certificate.p7b"
    command += f" --hold-seconds {hold_seconds}"
    command += f" --receiver-ipv6 {receiver.compressed}"
    if public_key is not None:
        command += f" --receiver-public-key {public_key.hex()}"
    certificate_offset = None
    if corrupt_certificate:
        from cryptography.hazmat.primitives.serialization import pkcs7
        signature = pkcs7.load_der_pkcs7_certificates(certificate)[0].signature
        certificate_offset = certificate.index(signature) + len(signature) - 1
        command += f" --corrupt-certificate-offset {certificate_offset}"
    process = subprocess.Popen(ssh + [command],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ, "stdout")
    selector.register(process.stderr, selectors.EVENT_READ, "stderr")
    pending = bytearray()
    output = bytearray()
    errors = bytearray()
    signed = 0
    deadline = time.monotonic() + 30 + (hold_seconds if start_carplay else 0)
    try:
        while selector.get_map():
            if time.monotonic() >= deadline:
                raise RuntimeError("virtual-phone authentication deadline expired")
            for event, _ in selector.select(min(1, deadline - time.monotonic())):
                data = os.read(event.fd, 4096)
                if not data:
                    selector.unregister(event.fileobj)
                    continue
                if event.data == "stderr":
                    errors.extend(data)
                else:
                    pending.extend(data)
                    while b"\n" in pending:
                        line, _, rest = pending.partition(b"\n")
                        pending[:] = rest
                        if line.startswith(b'{"sign_digest":'):
                            request = json.loads(line)
                            if set(request) != {"sign_digest"} or signed >= 4:
                                raise ValueError("unexpected signer request")
                            signature = sign_digest(key, bytes.fromhex(request["sign_digest"]))
                            if corrupt_signature:
                                signature = bytes((signature[0] ^ 1,)) + signature[1:]
                            process.stdin.write((json.dumps({"signature": signature.hex()}) + "\n").encode())
                            process.stdin.flush()
                            signed += 1
                        else:
                            output.extend(line + b"\n")
                if len(pending) + len(output) + len(errors) > 65536:
                    raise ValueError("remote probe output too large")
        output.extend(pending)
        code = process.wait(timeout=5)
        result = json.loads(output)
        result.update(host_signed_challenges=signed, corrupt_signature=corrupt_signature,
                      corrupt_certificate=corrupt_certificate,
                      certificate_signature_offset=certificate_offset,
                      remote_exit=code)
        if errors:
            result["remote_stderr"] = errors.decode(errors="replace")
        return result
    finally:
        selector.close()
        if process.poll() is None:
            # Closing signer input lets the guest report EOF and restore USB before SSH exits.
            process.stdin.close()
            try:
                process.wait(timeout=7)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    negatives = parser.add_mutually_exclusive_group()
    negatives.add_argument("--corrupt-signature", action="store_true")
    negatives.add_argument("--corrupt-certificate", action="store_true")
    parser.add_argument("--identify", action="store_true")
    parser.add_argument("--start-carplay", action="store_true")
    parser.add_argument("--hold-seconds", type=int, default=2)
    parser.add_argument("--receiver-public-key")
    parser.add_argument("--receiver-ipv6", default="fe80::2")
    args = parser.parse_args()
    if not 1 <= args.hold_seconds <= 300:
        parser.error("hold seconds must be between 1 and 300")
    try:
        public_key = bytes.fromhex(args.receiver_public_key) if args.receiver_public_key else None
    except ValueError:
        parser.error("receiver public key must be hexadecimal")
    if public_key is not None and len(public_key) != 32:
        parser.error("receiver public key must contain 32 bytes")
    if (args.corrupt_signature or args.corrupt_certificate) and (args.identify or args.start_carplay):
        parser.error("negative controls cannot request identification or CarPlay")
    result = run(corrupt_signature=args.corrupt_signature, corrupt_certificate=args.corrupt_certificate,
                 identify=args.identify, start_carplay=args.start_carplay,
                 hold_seconds=args.hold_seconds, public_key=public_key, receiver_ipv6=args.receiver_ipv6)
    print(json.dumps(result, indent=2))
    expected = "authentication_failed" if args.corrupt_signature or args.corrupt_certificate else "authentication_succeeded"
    signed = result.get("host_signed_challenges", 0)
    failed_cleanup = any(key in result for key in ("error", "release_error", "restore_error"))
    if not (args.corrupt_signature or args.corrupt_certificate):
        failed_cleanup |= result.get("remote_exit") != 0
    return int(not result.get(expected) or (signed != 0 if args.corrupt_certificate else signed == 0) or
               failed_cleanup or
               (args.identify and not result.get("identification_accepted")) or
               (args.start_carplay and not result.get("carplay_start_sent")))


if __name__ == "__main__":
    raise SystemExit(main())
