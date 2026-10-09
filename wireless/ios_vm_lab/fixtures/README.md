# Public iOS emulator ticket fixtures

## 📦 Provenance

`ticket.shsh2` is copied byte-for-byte from the public
[`qemu-t8030-tools` sample](https://github.com/TrungNguyen1909/qemu-t8030-tools/blob/7a2017461ee7c1864330083dc6a781b75d1d0279/bootstrap_scripts/ticket.shsh2).
`root_ticket.der` is derived using that revision's `create_apticket.py` and the
Apple iPhone 11 (`n104ap`) iOS 14 beta 5 (`18A5351d`) BuildManifest.

The [manifest](manifest.json) records source revision, target, lengths and
SHA-256 values. The exact upstream generation command is:

```sh
python3 create_apticket.py n104ap BuildManifest.plist ticket.shsh2 root_ticket.der
```

## 🔐 Material type

These are SHSH/AP-ticket fixtures for the disposable emulator. They are not
MFi private identities or the locally generated guest SSH private key.
The derived AP ticket is specific to the recorded firmware build. Reusing it
does not establish authentication or restore capability on a physical iPhone.

## 🧪 Local use

To stage the committed AP ticket in the current laboratory:

```sh
cp wireless/ios_vm_lab/fixtures/root_ticket.der \
  build/ios-vm-lab/firmware/root_ticket.der
```

The source template and generated ticket are the only binary fixtures in this
directory. Firmware images, guest disks, SSH material and runtime session keys
stay in local laboratory storage.
