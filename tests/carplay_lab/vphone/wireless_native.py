"""Owned native Bluetooth API -> real QNX daemon; no host iAP2 responder."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import select
import shutil
import socket
import struct
import subprocess
import threading
import time
import ipaddress

from . import lab, usb
from tests.carplay_lab.common import qnx_receiver
from tests.carplay_lab.qnx import qemu_capture as capture

OUT = lab.OUT / "qnx-wireless-native"
RUST_IMAGE = "qnx65-armv7-toolchain:8.5-rust"


def build(*, paired_altscreen: bool = False, hook_file: Path | None = None,
          audio: bool = False, audio_peripheral: bool = False, spatial: bool = False,
          ip_address: str = '192.168.64.200') -> None:
    if audio_peripheral and not audio:raise ValueError('audio peripheral requires the explicit audio profile')
    if spatial and (not audio or not hook_file or not hook_file.with_name('libcarplay_apac.so').is_file()):
        raise ValueError('spatial profile requires audio and the matching APAC decoder')
    if spatial and b'mhi2q_lab_http_request' not in hook_file.read_bytes():
        raise ValueError('spatial software-MFi profile requires the isolated HTTP210_LAB hook')
    if ipaddress.IPv4Address(ip_address) not in ipaddress.IPv4Network('192.168.64.0/24'):
        raise ValueError('native laboratory address must remain on the owned virtual subnet')
    qnx_receiver.set_output(OUT)
    if qnx_receiver.running():
        raise RuntimeError("stop the owned native-wireless QNX receiver before building")
    OUT.mkdir(parents=True, exist_ok=True)
    container_out = "/src/" + str(OUT.relative_to(capture.AUDI))
    subprocess.run(["docker", "run", "--rm", "--platform=linux/amd64", "-v", f"{capture.AUDI}:/src",
        "-w", "/src/Patches/mhi2-carplay", "-e", f"CARGO_TARGET_DIR={container_out}/rust-target",
        RUST_IMAGE, "/opt/rust/qnx-cargo", "build", "-q", "--manifest-path",
        "wireless/daemon/Cargo.toml", "--features", "native-api-lab", "--bin", "mhi2q-wireless-daemon"], check=True)
    shutil.copy2(OUT / "rust-target/armv7-unknown-nto-qnx650/release/mhi2q-wireless-daemon", OUT / "mhi2q-wireless-daemon")
    if spatial:
        # Hook-owned cold buffered QSA calls resolve RTLD_NEXT. Keep the
        # hardware model after the hook, not inside the outer capture DSO.
        capture.docker(OUT,"arm-unknown-nto-qnx6.5.0eabi-gcc","-std=gnu99","-O2","-Wall","-Wextra","-Werror",
            "-fPIC","-shared","-o","/work/libvphone_audio_peripheral.so",
            "/src/Patches/mhi2-carplay/tests/carplay_lab/qnx/vphone_audio_peripheral.c","-lsocket")
        capture.docker(OUT,"arm-unknown-nto-qnx6.5.0eabi-gcc","-std=gnu99","-O2","-Wall","-Wextra","-Werror",
            "-o","/work/audio-peripheral-test",
            "/src/Patches/mhi2-carplay/tests/carplay_lab/qnx/vphone_audio_peripheral_test.c",
            "/src/Patches/mhi2-carplay/tests/carplay_lab/qnx/vphone_audio_peripheral.c","-lsocket")
    for source, name in (("tests/carplay_lab/qnx/native_api_relay.c", "native-api-relay"),
                         ("tests/carplay_lab/qnx/native_dio_owner.c", "native-dio-owner"),
                         ("wireless/daemon/probes/fake_uaputl.c", "fake_uaputl")):
        flags = ('-DMHI2Q_NATIVE_AUDIO_NO_DIOPROBE',) if audio and name == 'native-dio-owner' else ()
        if spatial and name=='native-dio-owner':flags+=('-DMHI2Q_NATIVE_AUDIO_SEPARATE_PERIPHERAL',)
        capture.docker(OUT, "arm-unknown-nto-qnx6.5.0eabi-gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                       *flags, "-o", f"/work/{name}", f"/src/Patches/mhi2-carplay/{source}", "-lsocket")
    original_manifest = qnx_receiver.network_manifest
    original_docker = capture.docker

    def manifest(*args, **kwargs):
        text = original_manifest(*args, **kwargs)
        audio_assets = ''
        if "procmgr_symlink /dev/ser1 /dev/shmem/carplay_hook.log" not in text:
            text = text.replace("    reopen /dev/ser1\n", "    reopen /dev/ser1\n"
                                "    procmgr_symlink /dev/ser1 /dev/shmem/carplay_hook.log\n"
                                "    procmgr_symlink /dev/null /dev/shmem/carplay_verbose\n")
        anchor = "    /proc/boot/dio_manager &\n"
        if text.count(anchor) != 1:
            raise ValueError("expected one stock DIO launch")
        text = text.replace(anchor, "    LD_PRELOAD=\n    /proc/boot/native-dio-owner &\n" + """    LD_PRELOAD=
    /proc/boot/cp -p /proc/boot/fake_uaputl /dev/shmem/fake_uaputl
    MHI2Q_CP_IE_INDEX=2
    MHI2Q_CP_IE_MONITOR_SECONDS=0
    MHI2Q_CP_UAPUTL=/dev/shmem/fake_uaputl
    MHI2Q_CP_BT_MAC=02:00:00:00:00:01
    MHI2Q_CP_MODEL=MHI2Q
    MHI2Q_CP_SERIAL=NATIVE-API-QEMU
    MHI2Q_CP_HARDWARE=QEMU
    MHI2Q_CP_SSID=MHI2-Native-Lab
    MHI2Q_CP_PASSPHRASE=testpassword
    MHI2Q_CP_CHANNEL=11
    MHI2Q_CP_IP_ADDRESS=192.168.64.200
    MHI2Q_CP_DEVICE_ID=02:00:00:00:00:01
    MHI2Q_CP_AIRPLAY_PORT=5000
    MHI2Q_CP_RECEIVER=stock210
    /proc/boot/mhi2q-wireless-daemon &
    /proc/boot/native-api-relay &
""")
        if spatial:text=text.replace('    /proc/boot/native-dio-owner &\n','    /proc/boot/audio-peripheral-test\n    /proc/boot/native-dio-owner &\n')
        text=text.replace("ifconfig ecm0 10.0.2.15 netmask", "ifconfig ecm0 192.168.64.200 netmask")
        text=text.replace('192.168.64.200',ip_address)
        if audio:
            text=text.replace('    MHI2Q_CP_WIRELESS_MEDIA=1\n',
                '    MHI2Q_CP_WIRELESS_MEDIA=1\n    MHI2Q_CP_WIRELESS_AUDIO=1\n')
            plugins=capture.AUDI/'Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/armle/usr/lib/cinemo'
            for name in ('cinemo_classes.xml','libNmeAudioAAC.so','libNmeEncoder.so'):
                audio_assets+=f'[type=file perms=0755 data=copy] /mnt/app/armle/usr/lib/cinemo/{name}=/src/{(plugins/name).relative_to(capture.AUDI)}\n'
        if spatial:
            text=text.replace('    MHI2Q_CP_WIRELESS_AUDIO=1\n',
                '    MHI2Q_CP_WIRELESS_AUDIO=1\n    AUDIO_SPATIAL=1\n    AUDIO_SPATIAL_ATMOS=0\n'
                '    SPATIAL_103_EXTRA_INDEX=23\n    AUDIO_STEREO_UPMIX=off\n'
                '    AUDIO_SPATIAL_CODEC_LIB=/proc/boot/libcarplay_apac.so\n')
            text=text.replace('    AUDIO_SPATIAL=1\n','    AUDIO_SPATIAL=1\n    MHI2Q_HTTP210_LAB=1\n')
            apac=hook_file.with_name('libcarplay_apac.so')
            audio_assets+=f'[type=file perms=0755 data=copy] /proc/boot/libcarplay_apac.so=/src/{apac.relative_to(capture.AUDI)}\n'
        text=text.replace("/proc/boot/dio_manager=/work/", "[type=file perms=0755 data=copy] /proc/boot/dio_manager=/work/")
        assets = ""
        for name in ("mhi2q-wireless-daemon", "native-api-relay", "native-dio-owner", "fake_uaputl"):
            assets += f"[type=file perms=0755 data=copy] /proc/boot/{name}={container_out}/{name}\n"
        assets += "[type=file perms=0755 data=copy] /proc/boot/cp=/opt/qnx650/target/qnx6/armle-v7/bin/cp\n"
        # Keep runtime inputs before the very large unused factory archive.
        # Files appended at ~147 MiB open/stat but read as ESRVRFAULT in this
        # native fixture; identical files near the front work in the canary.
        assets += audio_assets
        if spatial:
            assets+=f'[type=file perms=0755 data=copy] /proc/boot/libvphone_audio_peripheral.so={container_out}/libvphone_audio_peripheral.so\n'
            assets+=f'[type=file perms=0755 data=copy] /proc/boot/audio-peripheral-test={container_out}/audio-peripheral-test\n'
        index=text.index("/eso/lib/factories/")
        text=text[:index]+assets+text[index:]
        return text

    def docker(work, *args, **kwargs):
        if "/work/libdio_capture_probe.so" in args:
            args += ("-DMHI2Q_CAPTURE_BRIDGE", "-DMHI2Q_BRIDGE_STOCK210", "-DMHI2Q_BRIDGE_PRESTART",
                     "-DMHI2Q_BRIDGE_PROPERTY_HANDOFF", "-DMHI2Q_PROPERTY_HANDOFF_ALLOW_DIOPROBE_QEMU",
                     "-DMHI2Q_CAPTURE_BRIDGE_DRIVES_START",
                     "-DMHI2Q_CAPTURE_SIGSEGV_QEMU",
                     "-DMHI2Q_CAPTURE_INPUT_ONLY_OMX_QEMU",
                     '-DMHI2Q_BRIDGE_INTERFACE_NAME="ecm0"',
                     "/src/Patches/mhi2-carplay/wireless/receiver_bridge/bridge.c",
                     "/src/Patches/mhi2-carplay/wireless/receiver_bridge/property_handoff.c")
            if audio:args+=('-DMHI2Q_CAPTURE_SCREEN_AUDIO_QEMU',)
            if audio_peripheral:
                args+=('-DMHI2Q_VPHONE_AUDIO_PERIPHERAL',)
                if not spatial:args+=('/src/Patches/mhi2-carplay/tests/carplay_lab/qnx/vphone_audio_peripheral.c',)
        return original_docker(work, *args, **kwargs)

    qnx_receiver.network_manifest, capture.docker = manifest, docker
    try:
        qnx_receiver.build("qnx65-armv7-toolchain:8.5", pairing=True, media=True, software_mfi=True,
                           request_ui=True, request_ui_maps=not audio, request_ui_music=audio,
                           capture_video=True, input_control=True, clock_realtime=True,
                           paired_altscreen=paired_altscreen, capture_only_altscreen=paired_altscreen,hook_file=hook_file)
    finally:
        qnx_receiver.network_manifest, capture.docker = original_manifest, original_docker
    path = OUT / "build-record.json"
    record = json.loads(path.read_text())
    record["native_api_daemon"] = True
    record['wireless_audio']=audio
    record['audio_peripheral']=audio_peripheral
    record['spatial_audio']=spatial
    record['http210_lab_peripheral']=spatial
    if spatial:record['apac_sha256']=hashlib.sha256(hook_file.with_name('libcarplay_apac.so').read_bytes()).hexdigest()
    record['legacy_callback_trace']=not audio
    record['ip_address']=ip_address
    record["fake_decoder_output"] = "disabled; genuine AVC input independently decoded on host"
    record["native_dio_owner"] = "up to four normal exits; never restart a crashed child"
    record["native_dio_owner_sha256"] = hashlib.sha256((OUT/"native-dio-owner").read_bytes()).hexdigest()
    record["native_api_daemon_sha256"] = hashlib.sha256((OUT/"mhi2q-wireless-daemon").read_bytes()).hexdigest()
    path.write_text(json.dumps(record, indent=2) + "\n")


def frame(kind: int, data: bytes = b"") -> bytes:
    if kind not in (1, 2, 3) or len(data) > 5035 or kind != 2 and data or kind == 2 and not data:
        raise ValueError("invalid btstack bridge frame")
    return struct.pack("<BBHI", kind, 0, len(data), 1) + data


def observations(packets: list[dict]) -> dict:
    """Read-only link/CSM reassembly; the relay never synthesizes a reply."""
    from tests.carplay_lab.legacy_ios import usb_probe as wire
    streams = {direction:bytearray() for direction in ("ios-to-qnx", "qnx-to-ios")}
    controls = {direction:bytearray() for direction in streams}
    seen = {direction:[] for direction in streams}
    messages = {direction:[] for direction in streams}
    availability=[];starts=[]
    for row in packets:
        direction=row["direction"]; pending=streams[direction]
        pending.extend(bytes.fromhex(row["hex"]))
        while pending:
            if pending[:1] != b"\xff": raise ValueError("unexpected iAP2 stream prefix")
            if len(pending)<2: break
            if pending[:2]==wire.IAP2_DETECT[:2]:
                if len(pending)<len(wire.IAP2_DETECT): break
                if pending[:len(wire.IAP2_DETECT)]!=wire.IAP2_DETECT: raise ValueError("invalid iAP2 detection marker")
                del pending[:len(wire.IAP2_DETECT)];continue
            if pending[:2]!=b"\xff\x5a": raise ValueError("invalid iAP2 synchronization prefix")
            if len(pending)<4: break
            size=int.from_bytes(pending[2:4],"big")
            if not 9<=size<=8192: raise ValueError("invalid iAP2 frame length")
            if len(pending)<size: break
            raw=bytes(pending[:size]);del pending[:size]
            parsed=wire.parse_link_frame(raw)
            if parsed["control"]!=0x40 or parsed["session"]!=10 or not parsed["payload"]: continue
            if raw in seen[direction]: continue
            seen[direction]=(seen[direction]+[raw])[-64:]
            csm=controls[direction];csm.extend(parsed["payload"])
            while len(csm)>=6:
                length=int.from_bytes(csm[2:4],"big")
                if csm[:2]!=b"@@" or not 6<=length<=65535: raise ValueError("invalid iAP2 control message")
                if len(csm)<length: break
                message=bytes(csm[:length]);del csm[:length]
                mid,parameters=wire.parse_control_message(message)
                messages[direction].append(f"0x{mid:04X}")
                if direction=="ios-to-qnx" and mid==0x4300:
                    group=parameters.get(1,b"")
                    _,attributes=wire.parse_control_message(b"@@"+struct.pack(">HH",len(group)+6,0)+group)
                    availability.append({"wireless_available":attributes.get(0)==b"\x01",
                                         "bluetooth_transport_id":attributes.get(1,b"").rstrip(b"\0").decode("utf8")})
                elif direction=="qnx-to-ios" and mid==0x4301:
                    starts.append({"pairing_id":parameters.get(4,b"").rstrip(b"\0").decode("utf8"),
                                   "device_id":parameters.get(3,b"").rstrip(b"\0").decode("utf8"),
                                   "port":int.from_bytes(parameters.get(2,b""),"big"),
                                   "wireless_attributes_present":1 in parameters})
    incoming=messages["ios-to-qnx"];outgoing=messages["qnx-to-ios"]
    return {"control_messages":messages,
            "availability":availability,"start_sessions":starts,
            "native_authentication_verified":"0xAA05" in incoming,
            "native_identification_verified":"0x1D02" in incoming,
            "native_wifi_response_sent":"0x5702" in incoming and "0x5703" in outgoing,
            "native_carplay_availability_received":"0x4300" in incoming,
            "daemon_start_session_sent":"0x4301" in outgoing}


def check_mfi_service() -> None:
    from tests.carplay_lab.common.mfi_fixture import CERTIFICATE_SHA256
    from tests.carplay_lab.common.mfi_sap import read_exact
    with socket.create_connection(("127.0.0.1",32502),timeout=3) as service:
        service.settimeout(3);service.sendall(b"\x03"+bytes(32))
        length=int.from_bytes(read_exact(service,4),"big")
        if not 1<=length<=4096:raise ValueError("invalid lab MFi certificate size")
        if hashlib.sha256(read_exact(service,length)).hexdigest()!=CERTIFICATE_SHA256:
            raise ValueError("unexpected lab MFi certificate")


def receiver_ready(serial: str) -> bool:
    """Do not reuse an exited DIO epoch's Bonjour/MFi readiness markers."""
    current=serial.rsplit("[native-dio-owner] starting epoch=",1)[-1]
    return ("[native-dio-owner] ended epoch=" not in current and
            all(marker in serial for marker in ("[bridge] listening on 127.0.0.1:57342", "[native-api-relay] ready")) and
            "[dio-bridge] DNSServiceRegister status=0" in current and
            any(marker in current for marker in ("[native-mfi-sap-qemu] guarded request slot wrapped",
                "[native-mfi-sap-qemu] module router preserved; lab callbacks composed")))


def run(seconds: int = 90, *, guest_logs: bool = False,
        report_name: str = 'wireless-native-daemon.json') -> dict:
    if type(seconds) is not int or not 10 <= seconds <= 150:
        raise ValueError("native wireless capture must be 10..150 seconds")
    settings = lab.profile()
    lab.require_machine(settings, "running")
    qnx_receiver.set_output(OUT)
    if not qnx_receiver.running() or not json.loads((OUT / "build-record.json").read_text()).get("native_api_daemon"):
        raise RuntimeError("start the owned native-wireless QNX daemon image first")
    check_mfi_service()
    ready_deadline=time.monotonic()+60
    while True:
        serial=(OUT/"serial.log").read_text(errors="replace")
        if receiver_ready(serial):break
        if time.monotonic()>=ready_deadline or not qnx_receiver.running():
            raise RuntimeError("native QNX daemon/relay did not become ready")
        time.sleep(.25)
    nonce = secrets.token_hex(8)
    serial_path=OUT/"serial.log"
    serial_before=serial_path.stat()
    state, packets = {}, []
    def worker():
        try:
            state["native"] = usb.run("iap-stream-bluetooth", peer_nonce=nonce, peer_seconds=max(60,seconds+30),
                                       on_launch=lambda log: state.update(native_log_path=log))
        except Exception as error:
            state["error"] = str(error)
    build_record=json.loads((OUT/"build-record.json").read_text())
    if build_record.get('spatial_audio') and '[vphone-audio-peripheral] native nonblock/PCM6/stereo-transition canary PASS' not in serial:
        raise RuntimeError('native spatial hardware-model canary did not pass')
    result = {"paired_altscreen":build_record.get("paired_altscreen") is True, "daemon": "QNX/ARM Rust", "transport": "native Bluetooth API byte stream",
              "physical_bluetooth_verified": False, "video_verified": False,
              "physical_wifi_verified": False, "run_id": nonce, "started_at_ns": time.time_ns(),
              "qnx_pid": qnx_receiver.running(), "serial_start_offset": serial_before.st_size,
              "ifs_sha256":hashlib.sha256((OUT/"qemu_start_debug.bin").read_bytes()).hexdigest()}
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("192.168.64.1", 32505));listener.listen(1);listener.settimeout(.5)
        log_thread = None
        if guest_logs:
            from . import native_logs
            def collect_logs():
                result["guest_logs"] = native_logs.capture()
            log_thread=threading.Thread(target=collect_logs);log_thread.start()
        thread = threading.Thread(target=worker);thread.start()
        try:
            # Compiling/staging the helper and RPC log collection can exceed
            # the socket-connect deadline. Bound staging separately, then
            # start the connection deadline when services.load has returned.
            staging_deadline=time.monotonic()+180
            accept_deadline=None
            while True:
                if "error" in state:raise RuntimeError(state["error"])
                if accept_deadline is None and "native_log_path" in state:
                    accept_deadline=time.monotonic()+40
                try:
                    peer,address=listener.accept();break
                except socket.timeout:
                    if time.monotonic()>=(accept_deadline or staging_deadline):
                        raise TimeoutError("native helper did not connect after bounded staging")
            with peer, socket.create_connection(("127.0.0.1", build_record.get('relay_host_port',32504)), timeout=5) as daemon:
                peer.settimeout(3)
                token = bytearray()
                while len(token) < 16:
                    part = peer.recv(16-len(token))
                    if not part: raise EOFError("native peer truncated nonce")
                    token.extend(part)
                if token != nonce.encode(): raise ValueError("native peer nonce mismatch")
                result["peer_address"] = address[0]
                daemon.sendall(frame(1))
                buffers = {peer:bytearray(), daemon:bytearray()}
                deadline = time.monotonic()+seconds
                while time.monotonic() < deadline:
                    for source in select.select([peer, daemon], [], [], .25)[0]:
                        part = source.recv(16384)
                        if not part: raise EOFError("native/daemon stream disconnected")
                        pending = buffers[source];pending.extend(part)
                        while len(pending) >= (4 if source is peer else 8):
                            if source is peer:
                                size = struct.unpack_from(">I", pending)[0]; header=4
                            else:
                                kind, reserved, size, generation = struct.unpack_from("<BBHI", pending)
                                if kind != 2 or reserved or generation != 1: raise ValueError("unexpected daemon bridge generation/type")
                                header=8
                            if not 1 <= size <= (8192 if source is peer else 5035): raise ValueError("invalid native/daemon packet size")
                            if len(pending) < size+header: break
                            payload=bytes(pending[header:header+size]);del pending[:header+size]
                            packets.append({"direction":"ios-to-qnx" if source is peer else "qnx-to-ios", "hex":payload.hex(),"elapsed":round(time.monotonic()-(deadline-seconds),4)})
                            if len(packets)>2000: raise ValueError("native capture packet bound exceeded")
                            if source is peer:
                                for at in range(0,len(payload),5035): daemon.sendall(frame(2,payload[at:at+5035]))
                            else: peer.sendall(struct.pack(">I",len(payload))+payload)
                daemon.sendall(frame(3))
        except Exception as error:
            result["error"] = str(error)
        finally:
            thread.join(timeout=seconds+65)
            if thread.is_alive(): raise RuntimeError("bounded native stream helper remains active")
            if log_thread is not None:log_thread.join()
    result.update(state)
    result["packets"] = packets
    try:
        result.update(observations(packets))
    except ValueError as error:
        result["observation_error"]=str(error)
    from . import wireless_video
    if (qnx_receiver.running()!=result["qnx_pid"] or serial_path.stat().st_ino!=serial_before.st_ino or
        serial_path.stat().st_size<serial_before.st_size):
        result["video"]={"verified":False,"error":"QNX process or trace changed during capture"}
    else:
        with serial_path.open("rb") as source:
            source.seek(serial_before.st_size)
            trace=source.read(67108865)
        stem="wireless-native-"+nonce
        if len(trace)<=67108864:
            snapshot=lab.OUT/(stem+"-serial.log");snapshot.write_bytes(trace)
            result["serial_snapshot"]=str(snapshot)
        result["video"]=wireless_video.verify(result,trace,lab.OUT,stem)
    result["video_verified"]=result["video"]["verified"]
    result["altscreen_verified"]=result["video"].get("altscreen",{}).get("verified",False)
    lab.save(report_name,result)
    return result


def recover(seconds: int = 30) -> dict:
    """Abruptly end only our native airplayd; prove fresh same-receiver video."""
    if type(seconds) is not int or not 10<=seconds<=150:
        raise ValueError("native wireless capture must be 10..150 seconds")
    previous=json.loads((lab.OUT/"wireless-native-daemon.json").read_text())
    qnx_receiver.set_output(OUT)
    if previous.get("video_verified") is not True or previous.get("qnx_pid")!=qnx_receiver.running():
        raise RuntimeError("first verify native wireless video on this running QNX receiver")
    settings=lab.profile();lab.require_machine(settings,"running")
    check_mfi_service()
    processes=lab.rpc(settings,"processes.list",{"filter":"airplayd"})["result"]["processes"]
    owned=[row for row in processes if row.get("name")=="airplayd" and
           row.get("executable")=="/usr/libexec/airplayd" and row.get("uid")==501]
    if len(owned)!=1:raise RuntimeError("expected exactly one owned native airplayd")
    lab.save("wireless-native-before-recovery.json",previous)
    start=(OUT/"serial.log").stat().st_size
    result={"recovery_verified":False,"previous_run_id":previous["run_id"],"qnx_pid":previous["qnx_pid"],
            "airplayd_pid":owned[0]["pid"]}
    try:
        result["termination"]=lab.rpc(settings,"processes.kill",{"pid":owned[0]["pid"],"signal":15,"force":True})
        time.sleep(2)
        with (OUT/"serial.log").open("rb") as source:
            source.seek(start);trace=source.read(67108865)
        if len(trace)>67108864:raise RuntimeError("disconnect trace exceeds 64 MiB")
        stop_trace=lab.OUT/("wireless-native-"+previous["run_id"]+"-disconnect.log");stop_trace.write_bytes(trace)
        result["disconnect_trace"]=str(stop_trace)
        result["disconnect_trace_sha256"]=hashlib.sha256(trace).hexdigest()
        if b"FATAL SIG" in trace or b"[dio-crash-qemu]" in trace:
            raise RuntimeError("QNX receiver failed during native disconnect; inspect disconnect_trace")
        ready_deadline=time.monotonic()+45
        while True:
            serial=(OUT/"serial.log").read_text(errors="replace")
            if receiver_ready(serial):
                try:
                    with socket.create_connection(("127.0.0.1",32503),timeout=3) as health:
                        health.settimeout(3);health.sendall(b"MCL"+bytes(4));response=bytearray()
                        while len(response)<56:
                            part=health.recv(56-len(response))
                            if not part:raise EOFError("receiver health endpoint closed after disconnect")
                            response.extend(part)
                    break
                except (OSError,EOFError):pass
            if time.monotonic()>=ready_deadline or qnx_receiver.running()!=previous["qnx_pid"]:
                raise RuntimeError("receiver did not become ready after native disconnect")
            time.sleep(.25)
        result["receiver_ready_after_disconnect"]=True
        result["dio_restarted"]=b"[native-dio-owner] starting epoch=" in (OUT/"serial.log").read_bytes()[start:]
        lab.client("guest","unlock",*lab.machine_arguments(settings),timeout=60)
        current=run(seconds,guest_logs=True)
        result["current_run_id"]=current["run_id"]
        result["same_receiver_identity"]=previous.get("start_sessions")==current.get("start_sessions")
        result["video_verified"]=current.get("video_verified",False)
        result["altscreen_verified"]=current.get("altscreen_verified",False)
        result["same_display_profile"]=previous.get("paired_altscreen",False)==current.get("paired_altscreen",False)
        result["pair_setup_repeated"]=any("PairSetup client done" in entry.get("message","")
            for row in current.get("guest_logs",[]) if row.get("process")=="airplayd"
            for entry in row.get("reply",{}).get("result",{}).get("entries",[]))
        result["recovery_verified"]=(result["same_receiver_identity"] and result["same_display_profile"] and result["video_verified"] and
                                     current.get("qnx_pid")==previous["qnx_pid"])
    except (OSError, RuntimeError, ValueError, EOFError) as error:
        result["error"]=str(error)
    lab.save("wireless-native-recovery.json",result)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("build","start","stop","status","run","recover","prepare-link","link-status"))
    parser.add_argument("--seconds",type=int,default=90)
    parser.add_argument("--guest-logs",action="store_true")
    parser.add_argument("--require-video",action="store_true",help="fail run unless fresh native wireless H.264 independently decodes")
    parser.add_argument("--paired-altscreen",action="store_true",help="build authenticated stream111 with independent capture of both screens")
    args=parser.parse_args()
    if args.paired_altscreen and args.action!="build":
        parser.error("--paired-altscreen requires build")
    if args.require_video and (args.action!="run" or not args.guest_logs):
        parser.error("--require-video requires run --guest-logs")
    qnx_receiver.set_output(OUT)
    if args.action=="build":build(paired_altscreen=args.paired_altscreen)
    elif args.action in ("run","recover"):
        result=run(args.seconds,guest_logs=args.guest_logs) if args.action=="run" else recover(args.seconds)
        print(json.dumps(result,indent=2))
        if (result.get("error") or args.require_video and not result.get("video_verified") or
            args.action=="recover" and not result.get("recovery_verified")):
            raise SystemExit(1)
    elif args.action in ("prepare-link","link-status"):
        from . import wireless_link
        print(json.dumps(wireless_link.prepare() if args.action=="prepare-link" else wireless_link.state(),indent=2))
    else:
        from . import wireless_link
        before=None
        if args.action=="start":
            if os.geteuid()!=0:raise PermissionError("the native QNX vmnet adapter requires sudo")
            if wireless_link.state().get("status")!="prepared":raise RuntimeError("run prepare-link as the VM owner first")
            before=wireless_link.snapshot()
        elif args.action=="stop":wireless_link.leave()
        sys_args=["qnx_receiver",args.action,"--lab-dir",str(OUT),"--transport","vmnet"]
        import sys
        old=sys.argv;sys.argv=sys_args
        try:
            qnx_receiver.main()
            if before is not None:
                try:print(json.dumps(wireless_link.join(before)))
                except BaseException:
                    wireless_link.leave()
                    sys.argv=["qnx_receiver","stop","--lab-dir",str(OUT),"--transport","vmnet"]
                    qnx_receiver.main()
                    raise
        finally:sys.argv=old


if __name__=="__main__":main()
