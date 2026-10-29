#!/usr/bin/env python3
"""One owned VM: real Rust daemon -> SI -> DIO -> independent media phone.
Phone, I2C, radio and hardware are explicit test peripherals. No HU access.
"""
import argparse,hashlib,json,re,tarfile,subprocess
from pathlib import Path
import qemu_stage1 as q

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--workdir',type=Path,required=True)
    ap.add_argument('--timeout',type=int,default=240)
    ap.add_argument('--cycles',type=int,default=2)
    ap.add_argument('--fault',choices=['blackhole','icmp','partial-record','dio-crash','microphone','rfcomm-gap','daemon-crash','si-crash','features'],default='blackhole');args=ap.parse_args()
    q.WORK=args.workdir.resolve();q.WORK.relative_to(q.REPO/'build');q.WORK.mkdir(parents=True,exist_ok=True)
    if not 2<=args.cycles<=20:ap.error('--cycles must be 2..20')
    (q.WORK/'integrated-cycles').write_text(str(args.cycles)+'\n')
    (q.WORK/'integrated-fault').write_text(args.fault+'\n')
    roots=['wireless/si','wireless/daemon/src','wireless/daemon/probes','wireless/receiver_bridge','hook','scripts','common','altscreen_render']
    sources={str(p.relative_to(q.REPO)):hashlib.sha256(p.read_bytes()).hexdigest()
             for root in roots for p in (q.REPO/root).rglob('*')
             if p.is_file() and p.suffix in ('.c','.h','.py','.rs','.sh','.map') and 'out' not in p.relative_to(q.REPO/root).parts}
    for name in ['tests/carplay_lab/qnx/independent_srp_qemu.c','tests/carplay_lab/qnx/stock210_peer_health_qemu.h',
                 'tests/carplay_lab/qnx/paired_alt_fixture.h','tests/carplay_lab/qnx/paired_alt_forward_qemu.h']:
        sources[name]=hashlib.sha256((q.REPO/name).read_bytes()).hexdigest()
    with tarfile.open(q.WORK/'source-snapshot.tar','w') as archive:
        for name in sorted(sources):archive.add(q.REPO/name,arcname=name)
    # Standalone acceptance must not pair a current source snapshot with an
    # older prebuilt hook. The main hook is deterministic; unchanged private
    # pairing primitives keep their separately verified build cache.
    subprocess.run(['bash','scripts/build_hook_wireless.sh'],cwd=q.REPO,check=True)
    q.build_guest_and_ifs(stage2=True,service_manager=True,vehicle_providers=True,
                         carplay_coded=True,rcc_fec_fixture=True,integrated=True)
    changed=[name for name,digest in sources.items() if hashlib.sha256((q.REPO/name).read_bytes()).hexdigest()!=digest]
    if changed:raise RuntimeError(f'Sources changed during build: {changed}')
    log=q.run_qemu(args.timeout,True)
    markers=['CLIENT: NATIVE DELETE OK','slot table published',
             'SI-HMI-QEMU: native startService configured 1024x480 hid=9',
             '[integrated-media] complete native media lifecycle PASS',
             '[integrated-media] stored phone identity loaded',
             'SI-INTEGRATED: complete daemon/SI/DIO/phone lifecycle PASS']
    checks={m:m in log for m in markers}
    audio_cycles=args.cycles-1 if args.fault=='partial-record' else args.cycles
    def metadata_count(marker):
        count=log.count(marker)
        return count>=args.cycles+1 if args.fault=='rfcomm-gap' else count==args.cycles
    checks.update({
        'two_4301_one_mfi':log.count('[phone-qemu] CarPlayStartSession=1')==args.cycles and log.count('[phone-qemu] signature=1')==(2 if args.fault in ('rfcomm-gap','daemon-crash') else 1),
        'native_pcm_two_sessions':log.count('[native-audio] decrypted PCM reached native SoundIF/QSA write PASS')==audio_cycles,
        'native_audio_type_ack':log.count('[native-audio] native DSI HMI audio routing ACK PASS')==args.cycles,
        'audio_no_corruption': '[native-audio] FAIL' not in log,
        'clean_asi_terminal_delivery':('bound state delivery failed' not in log or args.fault=='si-crash') and 'reportedAppState=TIMEOUT_SHUTDOWN' not in log,
        'authenticated_pcm_only':log.count('[integrated-media] native PCM auth and playback oracle session=')==args.cycles,
        'native_hid_events':log.count('[integrated-media] native DSI HID events decrypted and acknowledged PASS')==audio_cycles,
        'native_now_playing_track':metadata_count("updateNowPlayingData : currentTrack=[title='QNX Песня',duration=123000,album='Native Album',artist='Phone Artist',genre='',composer=''], valid=1"),
        'native_now_playing_partial':metadata_count("updateNowPlayingData : currentTrack=[title='QNX Следующая',duration=123000,album='Native Album',artist='Phone Artist',genre='',composer=''], valid=1"),
        'native_now_playing_app_retained':metadata_count('STATUS: 2(PAUSED) | REPEAT: 0(OFF) | SHUFFLE: 0(OFF) | APP: Phone Music'),
        'native_now_playing_elapsed':all(metadata_count(f'SI-METADATA-QEMU: native DSI elapsed={ms} valid=1') for ms in (4200,5600)),
        'native_now_playing_no_rejection':'[receiver-control] metadata rejected:' not in log,
        'native_metadata_negative_controls':log.count('[integrated-media] native metadata stale epoch/session/link, legacy and malformed rejection PASS')==audio_cycles,
        'native_semantic_negative_controls':log.count('[integrated-media] native semantic stale epoch/session/link, legacy and malformed rejection PASS')==audio_cycles,
        'old_child_all_asi_callbacks_rejected':log.count('[integrated-media] old-child ASI state/watchdog/flush/teardown/restart rejected PASS')==audio_cycles,
        'two_native_child_exits':log.count('[integrated-media] daemon-driven native child exit PASS')==args.cycles,
        'si_orphan_self_exit':args.fault!='si-crash' or 'SI parent ended; exiting own orphan receiver' in log,
        'daemon_restart':args.fault!='daemon-crash' or '[integrated-media] phone reauthenticated with restarted daemon on new RFCOMM generation' in log,
        'rfcomm_gap_preserves_media':args.fault!='rfcomm-gap' or '[integrated-media] native IP media survived RFCOMM gap and reauthentication PASS' in log,
        'microphone_roundtrip':args.fault!='microphone' or '[integrated-media] native microphone capture encrypted to phone PASS' in log,
        'no_crash':not re.search(r'SIGSEGV|SIGABRT|SI-INTEGRATED: FAIL|SI-NATIVE: FAIL|ldd:FATAL',log)})
    if args.fault=='rfcomm-gap':
        checks['rfcomm_gap_resumes_iap2']='[integrated-media] native iAP2 features resumed on reauthenticated RFCOMM PASS' in log
    video=[]
    if args.fault=='features':
        for marker in ('production paired111 AltAV TCP forwarding PASS',
                       'real artwork file Start/Data/Success handshake PASS',
                       'native RGI reached Java bus protocol peer PASS',
                       'native lane guidance reached Java bus protocol peer PASS',
                       'artwork decoded 256x256 PNG and published over Java bus PASS',
                       'phone received native vehicle range/temperature/warning PASS',
                       'phone received vehicle GPS GGA from native DIO queue PASS',
                       'phone received vehicle GPS RMC from native DIO queue PASS',
                       'phone received subscribed wheel speed PASS'):
            checks[marker]=log.count(marker)>=args.cycles
        streams=re.findall(r'forwarded-alt-annexb=([0-9a-f]+)',log)
        checks['two_native_alt_streams']=len(streams)==args.cycles
        for index,encoded in enumerate(streams):
            path=q.WORK/f'forwarded-alt-{index}.h264';path.write_bytes(bytes.fromhex(encoded))
            try:
                subprocess.run(['ffmpeg','-v','error','-err_detect','explode','-i',str(path),'-f','null','-'],check=True,capture_output=True)
                probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-count_frames','-show_entries',
                    'stream=width,height,nb_read_frames','-of','json',str(path)]))['streams'][0]
                valid=probe.get('width')==1440 and probe.get('height')==542 and probe.get('nb_read_frames')=='1'
            except (subprocess.SubprocessError,KeyError,IndexError):valid=False;probe={}
            checks[f'independent_alt_decode_{index}']=valid
            video.append({'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'probe':probe,'decoded':valid})
    receipt={'status':'PASS' if all(checks.values()) else 'FAIL','checks':checks,
             'build':json.loads((q.WORK/'build-receipt.json').read_text()),
             'log_sha256':hashlib.sha256((q.WORK/'qemu_stage1.log').read_bytes()).hexdigest(),
             'real':['Rust daemon','SI','DIO','ASI','AirPlay pairing/control/timing'],
             'modeled':['MFi hardware','Bluetooth bridge peer','Wi-Fi radio','Bonjour phone service','HMI configuration','Screen/OMX/audio hardware'],
             'fault':args.fault,'cycles':args.cycles,'source_hashes':sources,'alt_video':video,
             'source_snapshot_sha256':hashlib.sha256((q.WORK/'source-snapshot.tar').read_bytes()).hexdigest(),
             'hu_deployed':False}
    (q.WORK/'integrated-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    for name,result in checks.items():print('PASS' if result else 'FAIL',name)
    return 0 if receipt['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
