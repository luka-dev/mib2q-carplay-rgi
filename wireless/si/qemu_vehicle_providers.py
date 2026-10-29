"""Disposable native-provider profile. Vehicle sources are opened read-only.

This restores services and vehicle inputs, never Coding/FEC gate results.
The MMX FEC application still needs its RCC peer; copying a FecContainer
does not establish that the native service has consumed or validated it.
"""
from __future__ import annotations

import hashlib
import json
import re
import struct
import zlib
from pathlib import Path

# Native persistence partition names (production persistence.json comments):
# 28180695 = DIAG_COD, 28442848 = DIAG_ANP. SI subscribes 28180695:1 (coding,
# 25 bytes) and 28442848:100 (adaptation blob). The RCC-side serializer
# is available as firmware-root MIBRoot_RCC, outside advanced/. The
# Car_Function_Adaptations(+Gen2_Extended) assembly below is an EXPLICITLY
# UNVERIFIED reconstruction: the vehicle owner confirms wired CarPlay worked,
# but this blob decodes CarPlay coding bit 0 via SI 0x19180C, so it is NOT the
# serialization the real RCC serves. The FEC path (FecContainer.fec, active
# FSID list incl. 0x02310026) is the better candidate for CarPlay enablement;
# its native consumption needs the MMX fec <-> RCC exchange.
ADAPTATION_SETTINGS = ("Car_Function_Adaptations",
                       "Car_Function_Adaptations_Gen2_Extended")
# SI onAdaptationChanged (0x19180C) reads up to byte 70 ([70]&4) and MMXDiag
# reads up to byte 47; a shorter blob is not the vehicle adaptation dataset.
ADAPTATION_MIN_SIZE = 71
DIAGNOSIS_COD_PARTITION = 28180695
DIAGNOSIS_ANP_PARTITION = 28442848


def _setting(raw: bytes, name: str) -> bytes:
    matches = re.findall(rb"(?<![A-Za-z_])" + name.encode() +
                         rb"=([0-9A-Fa-f]+)(?![0-9A-Za-z])", raw)
    if len(matches) != 1:
        raise ValueError(f"expected exactly one {name} in storage copy")
    return bytes.fromhex(matches[0].decode("ascii"))


def vehicle_adaptation(backup: Path) -> tuple[bytes, dict]:
    """Assemble the DIAG_ANP adaptation blob from the backed-up settings."""
    blobs = []
    sources = []
    for name in ("storage1.raw", "storage2.raw"):
        raw = (backup / name).read_bytes()
        per_copy = tuple(_setting(raw, setting) for setting in ADAPTATION_SETTINGS)
        blobs.append(per_copy)
        sources.append({"source": str(backup / name),
                        "sha256": hashlib.sha256(raw).hexdigest()})
    if any(len(values[0] + values[1]) < ADAPTATION_MIN_SIZE for values in blobs):
        raise ValueError("assembled adaptation shorter than native consumers require")
    for index, setting in enumerate(ADAPTATION_SETTINGS):
        sizes = {len(values[index]) for values in blobs}
        if len(sizes) != 1:
            raise ValueError(f"backup storage copies disagree on {setting} size")
    values = blobs[0]
    for index, setting in enumerate(ADAPTATION_SETTINGS):
        if any(values[index] != copy[index] for copy in blobs[1:]):
            raise ValueError(f"backup storage copies disagree: {setting}")
    blob = values[0] + values[1]
    record = {"sources": sources, "settings": list(ADAPTATION_SETTINGS),
              "blob_size": len(blob),
              "blob_sha256": hashlib.sha256(blob).hexdigest(),
              "usb_ports_byte_offsets": [30, 31, 32, 33],
              "carplay_coding_bit": {"byte": 51, "mask": 1, "value": blob[51] & 1},
              "native_usb_ports_from_adaptation": (blob[30] & 1) | ((blob[31] & 1) << 1) | \
                  ((blob[32] & 1) << 2) | ((blob[33] & 1) << 3)}
    return blob, record


def apply_carplay_coding(blob: bytes) -> bytes:
    """Opt-in dealer-coding simulation: set only the CarPlay bit SI reads.

    The native Coding gate is unchanged; this writes a CarPlay-coded
    adaptation value through the native persistence writer, exactly like a
    diagnostic coding session would. It is NOT this car's data and the FEC
    gate still denies admission in this fixture.
    """
    mutated = bytearray(blob)
    mutated[51] |= 1
    return bytes(mutated)


def vehicle_fec_container(fw: Path) -> tuple[list[int], dict]:
    """Parse the backed-up FecContainer.fec (read-only).

    Layout recovered from the dump: u32 format, u32 payload size, 11 bytes
    header, VIN string, u32 checksum, u8 feature count, count x u32-BE FSIDs,
    128-byte signature, u32 count, count x u32-LE FSIDs (same order), 12-byte
    trailer. This records WHICH features the car has activated; consuming them
    still requires the native MMX fec <-> RCC FEC exchange, so no FEC gate
    value is derived here.
    """
    source = fw / "MU1316-special/rcc/mnt/efs-persist/FEC/FecContainer.fec"
    raw = source.read_bytes()
    if len(raw) < 0xf3 or struct.unpack_from("<I", raw, 0)[0] != 1:
        raise ValueError(f"unexpected FecContainer format: {source}")
    declared = struct.unpack_from("<I", raw, 4)[0]
    if len(raw) - 0x34 != declared:
        raise ValueError(f"FecContainer size field {declared} does not match {len(raw)}")
    vin = raw[0x14:0x25].split(b"\0")[0].decode("ascii", "replace")
    count = raw[0x2a]
    big = [struct.unpack_from(">I", raw, 0x2B + 4 * i)[0] for i in range(count)]
    little_count = struct.unpack_from("<I", raw, 0xC7)[0]
    little = [struct.unpack_from("<I", raw, 0xCB + 4 * i)[0] for i in range(little_count)]
    if count < 1 or big != little:
        raise ValueError("FecContainer FSID lists disagree or are empty")
    record = {"source": str(source), "sha256": hashlib.sha256(raw).hexdigest(),
              "vin": vin, "active_fsids": [f"{v:08x}" for v in big],
              "consumption": "not established; MMX fec requires the native RCC FEC exchange"}
    return big, record


def vehicle_coding(backup: Path) -> tuple[bytes, dict]:
    records = []
    values = []
    for name in ("storage1.raw", "storage2.raw"):
        source = backup / name
        raw = source.read_bytes()
        matches = re.findall(rb"(?<![A-Za-z_])VW_Coding=([0-9A-Fa-f]+)(?![0-9A-Za-z])", raw)
        if len(matches) != 1:
            raise ValueError(f"expected exactly one VW_Coding in {source}")
        coding = bytes.fromhex(matches[0].decode("ascii"))
        if len(coding) < 25:
            raise ValueError(f"native SI needs at least 25 coding bytes: {source}")
        values.append(coding)
        records.append({"source": str(source), "sha256": hashlib.sha256(raw).hexdigest()})
    if values[0] != values[1]:
        raise ValueError("backup storage copies disagree: select the valid generation explicitly")
    return values[0], {"sources": records, "coding_size": len(values[0]),
                       "coding_sha256": hashlib.sha256(values[0]).hexdigest(),
                       "native_usb_mode": values[0][19] >> 6}


def extend(buildfile: str, work: Path, fw: Path, repo: Path,
           payloads: dict, carplay_coded: bool = False,
           rcc_fec_fixture: bool = False) -> str:
    """Add exact native executables, copied database and native startup jobs."""
    audi = repo.parents[1]
    source_dir = "/src/" + str(work.relative_to(repo))
    firmware = "/audi/" + str(fw.relative_to(audi))
    backup = fw / "MU1316-special/rcc/mnt/efs-persist"
    coding, provenance = vehicle_coding(backup)
    adaptation, adaptation_record = vehicle_adaptation(backup)
    fec_fsids, fec_record = vehicle_fec_container(fw)
    adaptation_record["backup_blob_sha256"] = adaptation_record["blob_sha256"]
    if carplay_coded:
        adaptation = apply_carplay_coding(adaptation)
        adaptation_record["carplay_coding_bit"]["value"] = adaptation[51] & 1
        adaptation_record["carplay_coding_simulated"] = (
            "testbed wrote this bit through the native persistence writer; "
            "the reconstructed backup blob bit is 0; true vehicle serialization remains unverified")
    adaptation_record["blob_sha256"] = hashlib.sha256(adaptation).hexdigest()
    adaptation_record["delivered_blob_sha256"] = adaptation_record["blob_sha256"]
    adaptation_record["verification"] = (
        "unverified reconstruction; MIBRoot_RCC exists in firmware root, "
        "but its DIAG_ANP:100 serialization has not been verified")
    provenance["adaptation"] = adaptation_record
    provenance["fec_container"] = fec_record
    additions = []
    unpack = []
    inputs = {}
    for name, source in (
        ("persistenceTNG_mibhigh", fw / "MU1316-appimg/eso/bin/apps/persistenceTNG_mibhigh"),
        ("fec", fw / "MU1316-appimg/eso/bin/apps/fec"),
        ("diagnosis", fw / "MU1316-appimg/eso/bin/apps/diagnosis"),
        ("dumb_persistence_writer", fw / "MU1316-appimg/eso/bin/dumb_persistence_writer"),
        ("dumb_persistence_reader", fw / "MU1316-appimg/eso/bin/dumb_persistence_reader"),
        ("vehicle_mmx.sqlite", fw / "MU1316-special/mmx/mnt/persist/var/fw/persistence.sqlite"),
    ):
        raw = source.read_bytes()
        packed = zlib.compress(raw, 9)
        (work / (name + ".zlib")).write_bytes(packed)
        record = {"size": len(raw), "crc32": f"{zlib.crc32(raw):08x}",
                  "sha256": hashlib.sha256(raw).hexdigest(),
                  "packed_sha256": hashlib.sha256(packed).hexdigest()}
        payloads[name] = record
        inputs[str(source.relative_to(audi))] = record["sha256"]
        additions.append(f"/etc/{name}.zlib={source_dir}/{name}.zlib")
        destination = "/ramdisk/" + name
        if name == "vehicle_mmx.sqlite":
            destination = "/mnt/persist/var/fw/persistence.sqlite"
        unpack.append(f"    /proc/boot/si_payload /etc/{name}.zlib {destination} "
                      f"{record['size']} {record['crc32']}")

    # Keep namespace routing intact. Only replace the absent RCC transport
    # backend with a writable local SQLite store in this disposable guest.
    persistence = (fw / "MU1316-system/etc/eso/production/persistence.json").read_text()
    start = persistence.index('            "rcc": [')
    end = persistence.index('            "speech-service":', start)
    persistence = (persistence[:start] +
                   '            "rcc": ["SQLite", "/mnt/persist/var/fw/persistence_rcc.sqlite", '
                   '{"filesize_max_kb": 3500}],\n' + persistence[end:])
    (work / "persistence_vehicle.json").write_text(persistence)
    original_config = f"/config/persistence.json={firmware}/MU1316-system/etc/eso/production/persistence.json"
    if buildfile.count(original_config) != 1:
        raise ValueError("native provider profile requires the Service Manager config mapping")
    buildfile = buildfile.replace(original_config,
        f"/config/persistence.json={source_dir}/persistence_vehicle.json")

    # Exclude USB and hook controls from these native provider processes.
    for name in ("persistenceTNG_mibhigh", "fec", "diagnosis"):
        script = work / (name + "_wrapper.sh")
        script.write_text('#!/proc/boot/sh\nexport LD_PRELOAD=\n'
                          'export LD_LIBRARY_PATH=/eso/lib:/proc/boot\n'
                          f'exec /ramdisk/{name} "$@"\n')
        script.chmod(0o755)
        additions.append(f"/proc/boot/{name}_wrapper.sh={source_dir}/{script.name}")
        framework_path = work / "framework.json"
        framework = framework_path.read_text()
        original = f'"exec":"./bin/apps/{name}"'
        if framework.count(original) != 1:
            raise ValueError(f"provider framework entry changed: {name}")
        framework_path.write_text(framework.replace(original,
            f'"exec":"/proc/boot/{name}_wrapper.sh"'))

    config = json.loads((work / "servicemgr.json").read_text())
    jobs = []
    for name in ("persistence", "diagnosis", "fec"):
        jobs.extend([
            {"cmd": "init", "app": name, "apprunmode": 0,
             "ps_access": "PS_ACC_READ_WRITE", "timeout": 10000, "blocking": True},
            {"cmd": "run", "app": name, "timeout": 10000, "blocking": True},
        ])
        if name == "persistence":
            jobs.append({"cmd": "sleep", "timeout": 1000, "blocking": True})
    config["startup"] = jobs + config["startup"]
    (work / "servicemgr.json").write_text(json.dumps(config, indent=2) + "\n")

    # The disposable rcc backend is a local SQLite stand-in for the absent RCC
    # transport. Its native SQLiteTNG shadow refuses writer opens for
    # partitions that no client created yet (runs 05-09: exactly one
    # new-partition write per service lifetime, everything else fails with
    # PERS_STATUS_VERSION_MISMATCH). Pre-register the two DIAG partition rows
    # — schema-identical to the vehicle database, NO values — so the unchanged
    # native writer opens and fills them through the persistence API. All
    # vehicle values still enter exclusively via dumb_persistence_writer.
    shadow = work / "persistence_rcc_seed.sqlite"
    shadow.unlink(missing_ok=True)
    try:
        import sqlite3
        connection = sqlite3.connect(shadow)
        connection.execute(
            'CREATE TABLE "persistence-partitions"( '
            "name UNIQUE ON CONFLICT REPLACE, id INTEGER PRIMARY KEY, version )")
        connection.execute(
            'CREATE TABLE "persistence-data"( partition, key, type, value, '
            "PRIMARY KEY(partition, key) ON CONFLICT REPLACE )")
        for partition in (DIAGNOSIS_ANP_PARTITION, DIAGNOSIS_COD_PARTITION):
            connection.execute(
                "INSERT INTO 'persistence-partitions'(name, version) VALUES (?, ?)",
                (partition, "DSI-unversioned"))
        connection.commit()
        connection.close()
    except OSError as error:
        raise ValueError(f"cannot seed the rcc shadow partition rows: {error}")
    raw = shadow.read_bytes()
    packed_shadow = zlib.compress(raw, 9)
    (work / "persistence_rcc_seed.sqlite.zlib").write_bytes(packed_shadow)
    payloads["persistence_rcc_seed.sqlite"] = {
        "size": len(raw), "crc32": f"{zlib.crc32(raw):08x}",
        "sha256": hashlib.sha256(raw).hexdigest(),
        "packed_sha256": hashlib.sha256(packed_shadow).hexdigest()}
    additions.append("/etc/persistence_rcc_seed.sqlite.zlib="
                     f"{source_dir}/persistence_rcc_seed.sqlite.zlib")
    unpack.append("    /proc/boot/si_payload /etc/persistence_rcc_seed.sqlite.zlib "
                  f"/mnt/persist/var/fw/persistence_rcc.sqlite {len(raw)} "
                  f"{zlib.crc32(raw):08x}")
    inputs["fixture/persistence_rcc_seed.sqlite"] = payloads[
        "persistence_rcc_seed.sqlite"]["sha256"]
    provenance["rcc_shadow_seed"] = {
        "rows": [{"partition": DIAGNOSIS_ANP_PARTITION, "version": "DSI-unversioned"},
                 {"partition": DIAGNOSIS_COD_PARTITION, "version": "DSI-unversioned"}],
        "values": "none; written only by the native dumb_persistence_writer"}

    # Write through the original firmware tool/API (partition registration
    # and subscription notification), never by writing SI's memory. Both DIAG
    # partition rows are pre-registered in the disposable SQLite shadow (no
    # values), so the unchanged writer opens them; the completion marker is
    # gated on native readbacks.
    (work / "seed_vehicle.sh").write_text(
        '#!/proc/boot/sh\nexport LD_PRELOAD=\nexport LD_LIBRARY_PATH=/eso/lib:/proc/boot\n'
        'waitfor /dev/eso/persistence 15\n'
        'sleep 2\n'
        'echo "SI-VEHICLE: write backed-up coding via native persistence API"\n'
        f'/ramdisk/dumb_persistence_writer --flush {DIAGNOSIS_COD_PARTITION} 1 {coding.hex()}\n'
        'echo "SI-VEHICLE: write backed-up adaptation via native persistence API"\n'
        '/ramdisk/dumb_persistence_writer --flush '
        f'{DIAGNOSIS_ANP_PARTITION} 100 {adaptation.hex()} || '
        '/ramdisk/dumb_persistence_writer --version-tag DSI-unversioned --flush '
        f'{DIAGNOSIS_ANP_PARTITION} 100 {adaptation.hex()}\n'
        'echo "SI-VEHICLE: readback coding and adaptation"\n'
        'ok=1\n'
        f'/ramdisk/dumb_persistence_reader {DIAGNOSIS_COD_PARTITION} 1 || ok=0\n'
        f'/ramdisk/dumb_persistence_reader {DIAGNOSIS_ANP_PARTITION} 100 || ok=0\n'
        'if [ "$ok" -eq 1 ]; then\n'
        '    echo "SI-VEHICLE: writer returned"\n'
        '    echo done > /tmp/si_vehicle_seeded\n'
        'else\n'
        '    echo "SI-VEHICLE: readback failed; delivery incomplete"\n'
        'fi\n')
    additions.append(f"/etc/seed_vehicle.sh={source_dir}/seed_vehicle.sh")

    # Required native service factories, including SI's previously absent FEC
    # and OOC/time proxies, and the diagnosis peers observed in run 04
    # (RCCDiagService, MMX2AppDtcTestService, NavDBUtil). Further missing
    # peers must be observed, not faked.
    factories = {
        "17b8d9e8-c125-4a29-9049-8e3cb4bfef59",
        "cc574f3e-189d-5109-b9a6-fe3b6220212d",
        "374197ca-caef-403f-a993-7e171899db9d",
        "c7f869df-1be1-5090-ba2d-aed8a69db8ea",
        "3a7a576a-0b38-45fc-9ad4-1eab302e4f5b",
        "ff733e4d-6e39-49b8-b354-ee9c22f6cfc9",
        "11968057-7d03-5bf2-a680-ea43e050b90a",
        "b82b0691-ac00-5dac-8503-d963791ab441",
        "4ab484de-3b10-4a6a-aa60-18bfc5c3235b",
        "888d84b0-425a-11df-9879-0800200c9a66",
        "888d84b1-425a-11df-9879-0800200c9a66",
        "73dfd96c-440b-4796-94bd-53b91acd30fc",
        "1875f7e5-0255-4283-8265-93dd91cd564d",
        "f669c772-a80d-457f-b9af-0eb44581d9e1",
        "2ffff5fe-3ecc-4e5e-a640-76dd6851b878",
        "9e79c740-f61e-42bd-a575-c0c06d5f623b",
        "8a9ad25b-5b32-5f75-b76b-b9b9dd2a5f3c",
    }
    for uuid in sorted(factories):
        name = f"lib_{uuid}.so"
        additions.append(f"/eso/lib/factories/{name}={firmware}/MU1316-appimg/eso/lib/factories/{name}")
    additions.append("/proc/boot/libusbdi.so.2=/opt/qnx650/target/qnx6/armle-v7/lib/libusbdi.so.2")

    anchor = '    display_msg "--- native Service Manager owns SI startup ---"'
    if buildfile.count(anchor) != 1:
        raise ValueError("provider fixture must use native Service Manager")
    buildfile = buildfile.replace(anchor,
        '    /proc/boot/mkdir -p /mnt/persist/var/fw\n' + "\n".join(unpack) +
        '\n    /proc/boot/sh /etc/seed_vehicle.sh &\n' + anchor)
    buildfile = buildfile.replace('    /proc/boot/si_qemu_control_probe',
        '    waitfor /tmp/si_vehicle_seeded 75\n    sleep 3\n'
        '    /proc/boot/si_qemu_control_probe')
    if rcc_fec_fixture:
        from qemu_stage1 import docker
        docker(f"arm-unknown-nto-qnx6.5.0eabi-gcc -std=c11 -Os -Wall -Wextra -Werror "
               f"/src/wireless/si/si_qemu_fec_peer.c -o {source_dir}/si_qemu_fec_peer "
               "-lsocket")
        additions.append(f"/proc/boot/si_qemu_fec_peer={source_dir}/si_qemu_fec_peer")
        # The reader tool has exited before this independent COMM agent starts.
        buildfile = buildfile.replace('    /proc/boot/si_qemu_control_probe',
            '    /proc/boot/si_qemu_fec_peer grant ' +
            ' '.join(f'{key:08x}' for key in fec_fsids) +
            '\n    sleep 2\n    /proc/boot/si_qemu_control_probe')
        provenance['rcc_fec_fixture'] = {
            'simulated': True, 'transport': 'native COMM FecClient proxy',
            'states': 'explicit grant test response; not signature/license validation',
            'fsids': [f'{key:08x}' for key in fec_fsids]}
    buildfile += "\n" + "\n".join(additions) + "\n"
    provenance.update({"firmware_inputs": inputs,
                       "rcc_backend": "local SQLite through native persistenceTNG",
                       "gate_bypass": False,
                       "coding_partition": {"id": DIAGNOSIS_COD_PARTITION, "key": 1,
                                            "name": "DIAG_COD"},
                       "adaptation_partition": {"id": DIAGNOSIS_ANP_PARTITION, "key": 100,
                                                "name": "DIAG_ANP"},
                       "fec_container_consumption": "not established; MMX fec requires RCC service"})
    (work / "vehicle-inputs.json").write_text(json.dumps(provenance, indent=2) + "\n")
    return buildfile
