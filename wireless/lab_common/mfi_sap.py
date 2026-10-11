#!/usr/bin/env python3
"""Loopback-only, live public-fixture MFi-SAP responder for the owned QNX VM."""
from __future__ import annotations
import argparse
import hashlib
import json
import socket
import struct
import time
from pathlib import Path
if __package__:
    from .mfi_fixture import fixture_key, sign_digest
else:
    from mfi_fixture import fixture_key, sign_digest


def respond(request: bytes, key, certificate: bytes) -> bytes:
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import x25519
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    if len(request) != 33 or request[0] != 1:
        raise ValueError('expected MFi-SAP v1 and a 32-byte public key')
    ephemeral = x25519.X25519PrivateKey.generate()
    public = ephemeral.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    shared = ephemeral.exchange(x25519.X25519PublicKey.from_public_bytes(request[1:]))
    if not any(shared):
        raise ValueError('invalid all-zero shared secret')
    aes_key = hashlib.sha1(b'AES-KEY' + shared).digest()[:16]
    aes_iv = hashlib.sha1(b'AES-IV' + shared).digest()[:16]
    signature = sign_digest(key, hashlib.sha256(public + request[1:]).digest())
    encrypt = Cipher(algorithms.AES(aes_key), modes.CTR(aes_iv)).encryptor()
    signature = encrypt.update(signature) + encrypt.finalize()
    return public + struct.pack('>I', len(certificate)) + certificate + struct.pack('>I', len(signature)) + signature


def read_exact(connection, size):
    data = bytearray()
    while len(data) < size:
        part = connection.recv(size - len(data))
        if not part:
            raise EOFError('truncated MFi-SAP request')
        data.extend(part)
    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=300)
    parser.add_argument('--qnx-dir', type=Path, help='owned receiver artifacts inside build/')
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error('seconds must be in 1..300')
    if __package__:
        from . import qnx_receiver as qnx_network
    else:
        import qnx_receiver as qnx_network
    if args.qnx_dir is not None:
        qnx_network.set_output(args.qnx_dir)
    if not qnx_network.running():
        raise RuntimeError('owned QNX VM must be running')
    record = json.loads((qnx_network.OUT / 'build-record.json').read_text())
    if not record.get('software_mfi'):
        raise RuntimeError('QNX image must be explicitly built with --software-mfi')
    key, certificate = fixture_key()
    deadline = time.monotonic() + args.seconds
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(('127.0.0.1', 32502))
        listener.listen(1)
        listener.settimeout(1)
        print(json.dumps({'mfi_sap_ready': True, 'loopback_port': 32502}), flush=True)
        while time.monotonic() < deadline:
            try:
                connection, address = listener.accept()
            except socket.timeout:
                continue
            with connection:
                connection.settimeout(5)
                try:
                    if address[0] != '127.0.0.1' or not qnx_network.running():
                        raise ValueError('owned loopback VM only')
                    response = respond(read_exact(connection, 33), key, certificate)
                    connection.sendall(struct.pack('>I', len(response)) + response)
                    print(json.dumps({'mfi_sap_signed': True, 'response_bytes': len(response)}), flush=True)
                except (ValueError, EOFError, OSError) as error:
                    print(json.dumps({'mfi_sap_error': str(error)}), flush=True)


if __name__ == '__main__':
    main()
