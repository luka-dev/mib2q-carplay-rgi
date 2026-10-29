//! Byte transport between the btstack graft's local socket and an iAP2 link.
//! This crate is an offline prototype; authentication and CarPlay control follow.

pub mod carplay_control;
pub mod control;
mod device_name;
mod factory_identity;
mod file_transfer;
pub mod mfi;
#[cfg(feature = "native-api-lab")]
#[path = "../probes/native_api_mfi.rs"]
pub mod native_api_mfi;
pub mod receiver_control;
mod recovery;
#[cfg(unix)]
pub mod si_control;
#[cfg(all(feature = "software-mfi-test", unix, not(target_os = "nto")))]
pub mod software_mfi_test;
pub mod wifi_ie;

use iap2_link::{CONTROL_SESSION_ID, Event, LinkConfig, LinkEngine, LinkState};
use std::collections::VecDeque;
use std::io::{self, Read, Write};

pub const MAX_DATA: usize = 5035;
pub const HEADER_LEN: usize = 8;
pub const TYPE_CONNECTED: u8 = 1;
pub const TYPE_DATA: u8 = 2;
pub const TYPE_DISCONNECTED: u8 = 3;

// The graft splits each bridge frame at the connected peer's RFCOMM payload
// limit and waits for SDK transmit completion between pieces.
pub const BRIDGE_TX_CHUNK: usize = MAX_DATA;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BridgeFrame {
    pub kind: u8,
    pub generation: u32,
    pub data: Vec<u8>,
}

impl BridgeFrame {
    pub fn encode(&self) -> io::Result<Vec<u8>> {
        if self.data.len() > MAX_DATA
            || (self.kind == TYPE_CONNECTED && !matches!(self.data.len(), 0 | 6))
            || (self.kind == TYPE_DISCONNECTED && !self.data.is_empty())
            || !matches!(self.kind, TYPE_CONNECTED | TYPE_DATA | TYPE_DISCONNECTED)
        {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "invalid bridge frame",
            ));
        }
        let mut out = Vec::with_capacity(HEADER_LEN + self.data.len());
        out.push(self.kind);
        out.push(0);
        out.extend_from_slice(&(self.data.len() as u16).to_le_bytes());
        out.extend_from_slice(&self.generation.to_le_bytes());
        out.extend_from_slice(&self.data);
        Ok(out)
    }
}

#[derive(Default)]
pub struct FrameDecoder {
    buffered: Vec<u8>,
}

impl FrameDecoder {
    pub fn feed(&mut self, bytes: &[u8]) -> io::Result<Vec<BridgeFrame>> {
        self.buffered.extend_from_slice(bytes);
        let mut frames = Vec::new();
        loop {
            if self.buffered.len() < HEADER_LEN {
                break;
            }
            let kind = self.buffered[0];
            let reserved = self.buffered[1];
            let length = u16::from_le_bytes([self.buffered[2], self.buffered[3]]) as usize;
            if reserved != 0
                || length > MAX_DATA
                || !matches!(kind, TYPE_CONNECTED | TYPE_DATA | TYPE_DISCONNECTED)
                || (kind == TYPE_CONNECTED && !matches!(length, 0 | 6))
                || (kind == TYPE_DISCONNECTED && length != 0)
                || (kind == TYPE_DATA && length == 0)
            {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "malformed bridge frame",
                ));
            }
            if self.buffered.len() < HEADER_LEN + length {
                break;
            }
            let generation = u32::from_le_bytes(self.buffered[4..8].try_into().unwrap());
            frames.push(BridgeFrame {
                kind,
                generation,
                data: self.buffered[HEADER_LEN..HEADER_LEN + length].to_vec(),
            });
            self.buffered.drain(..HEADER_LEN + length);
        }
        Ok(frames)
    }
}

#[derive(Default)]
pub struct BridgeLink {
    generation: Option<u32>,
    engine: Option<LinkEngine>,
    outbound: VecDeque<BridgeFrame>,
    control: VecDeque<Vec<u8>>,
    files: VecDeque<Vec<u8>>,
    authenticated: bool,
    transfers: file_transfer::Transfers,
    dead: bool,
}

impl BridgeLink {
    pub fn generation(&self) -> Option<u32> {
        self.generation
    }
    pub fn is_normal(&self) -> bool {
        self.engine
            .as_ref()
            .is_some_and(|e| e.state() == LinkState::Normal)
    }
    pub fn dead(&self) -> bool {
        self.dead
    }
    pub fn next_outbound(&mut self) -> Option<BridgeFrame> {
        self.outbound.pop_front()
    }
    pub fn next_control(&mut self) -> Option<Vec<u8>> {
        self.control.pop_front()
    }
    pub fn next_file(&mut self) -> Option<Vec<u8>> { self.files.pop_front() }

    pub fn send_control(&mut self, frame: Vec<u8>, now_ms: u64) -> io::Result<()> {
        let Some(engine) = &mut self.engine else {
            return Err(io::Error::new(io::ErrorKind::NotConnected, "no iAP2 link"));
        };
        if engine.state() != LinkState::Normal {
            return Err(io::Error::new(
                io::ErrorKind::NotConnected,
                "iAP2 not negotiated",
            ));
        }
        engine.send(CONTROL_SESSION_ID, frame, now_ms);
        self.drain_engine();
        Ok(())
    }

    pub fn handle(&mut self, frame: BridgeFrame, now_ms: u64) {
        match frame.kind {
            TYPE_CONNECTED => {
                if self.generation == Some(frame.generation) {
                    return;
                }
                self.generation = Some(frame.generation);
                self.outbound.clear();
                self.control.clear();
                self.files.clear();
                self.authenticated=false;self.transfers.clear();
                self.dead = false;
                let mut engine = LinkEngine::new(LinkConfig::default());
                engine.start(false, now_ms);
                self.engine = Some(engine);
            }
            TYPE_DISCONNECTED if self.generation == Some(frame.generation) => {
                self.engine = None;
                self.generation = None;
                self.outbound.clear();
                self.control.clear();
                self.files.clear();
                self.authenticated=false;self.transfers.clear();
            }
            TYPE_DATA if self.generation == Some(frame.generation) => {
                if let Some(engine) = &mut self.engine {
                    engine.feed(&frame.data, now_ms);
                }
            }
            _ => return,
        }
        self.drain_engine();
    }

    pub fn advance(&mut self, now_ms: u64) {
        if let Some(engine) = &mut self.engine {
            engine.advance_time(now_ms);
        }
        self.drain_engine();
    }

    fn drain_engine(&mut self) {
        let Some(engine) = &mut self.engine else {
            return;
        };
        let generation = self.generation.expect("engine requires generation");
        let bytes = engine.take_output();
        self.queue_outbound_bytes(generation, &bytes);
        let engine = self.engine.as_mut().expect("engine still active");
        while let Some(event) = engine.poll_event() {
            match event {
                Event::Control(data) => self.control.push_back(data),
                Event::FileTransfer(data) => {
                    if !self.authenticated {continue;}
                    if data.len()>MAX_DATA || self.files.len()==32 { self.dead=true; }
                    else { self.files.push_back(data); }
                }
                Event::Dead { .. } => self.dead = true,
                _ => {}
            }
        }
        if engine.state() == LinkState::Dead {
            self.dead = true;
        }
    }

    fn queue_outbound_bytes(&mut self, generation: u32, bytes: &[u8]) {
        for chunk in bytes.chunks(BRIDGE_TX_CHUNK) {
            self.outbound.push_back(BridgeFrame {
                kind: TYPE_DATA,
                generation,
                data: chunk.to_vec(),
            });
        }
    }
}

fn drain_control<A: control::AuthProvider>(
    link: &mut BridgeLink,
    machine: &mut Option<control::ControlMachine<A>>,
    reader: &mut control::CsmReader,
    now_ms: u64,
) -> io::Result<Vec<Vec<u8>>> {
    let mut gated_replies = Vec::new();
    while let Some(bytes) = link.next_control() {
        for frame in reader.push(&bytes).map_err(io::Error::other)? {
            let Some(machine) = machine.as_mut() else {
                continue;
            };
            if let Some(reply) = machine.handle(&frame).map_err(io::Error::other)? {
                if control::message_id(&reply).map_err(io::Error::other)? == 0x4301 {
                    gated_replies.push(reply);
                } else {
                    link.send_control(reply, now_ms)?;
                }
            }
            link.authenticated=machine.phase()==control::Phase::Authenticated;
        }
    }
    Ok(gated_replies)
}

fn schedule_receiver_starts(
    link: &BridgeLink,
    receiver: &receiver_control::ReceiverWorker,
    pending: &mut VecDeque<(u32, Vec<u8>)>,
    replies: Vec<Vec<u8>>,
) -> io::Result<()> {
    for reply in replies {
        let generation = link
            .generation()
            .ok_or_else(|| io::Error::new(io::ErrorKind::NotConnected, "no RFCOMM generation"))?;
        receiver.start(generation)?;
        pending.push_back((generation, reply));
    }
    Ok(())
}

fn publish_peer_name<A: control::AuthProvider>(
    machine: &mut Option<control::ControlMachine<A>>,
    peer: &mut Option<receiver_control::PeerIdentity>,
    receiver: &receiver_control::ReceiverWorker,
    generation: Option<u32>,
) -> io::Result<()> {
    if let Some(control) = machine.as_mut() {
        if let (Some(generation),Some(identify))=(generation,control.take_identification()) {
            receiver.identification(generation,identify)?;
        }
        while let Some(frame)=control.take_semantic() {
            if let Some(generation)=generation { receiver.semantic(generation,1,frame)?; }
        }
        // Metadata stays off the link timer thread. The receiver worker
        // rechecks native SI identity before forwarding the raw CSM frame.
        while let Some(frame) = control.take_metadata() {
            if let Some(generation) = generation {
                let _ = receiver.now_playing(generation, frame);
            }
        }
    }
    if let Some(name) = machine.as_mut().and_then(|m| m.take_peer_name()) {
        if let Some(identity) = peer.as_mut() {
            if identity.name != name {
                identity.name = name;
                receiver.set_peer(Some(identity.clone()))?;
            }
        }
    }
    Ok(())
}

fn complete_receiver_starts(
    link: &mut BridgeLink,
    receiver: &receiver_control::ReceiverWorker,
    pending: &mut VecDeque<(u32, Vec<u8>)>,
    now_ms: u64,
    recovery: &mut recovery::Recovery,
) -> io::Result<bool> {
    while let Some((generation,frame))=receiver.try_outgoing() {
        if link.generation()==Some(generation) && link.is_normal() { link.send_control(frame,now_ms)?; }
    }
    while let Some(frame)=link.next_file() {
        /* Only authenticated control can start a receiver. Worker also checks
         * the native SI admission before accepting any transfer payload. */
        if let Some(generation)=link.generation() {
            let (forward,replies)=link.transfers.accept(&frame);
            if forward {receiver.semantic(generation,2,frame)?;}
            for reply in replies {
                if let Some(engine)=&mut link.engine {engine.send(iap2_link::FILE_TRANSFER_SESSION_ID,reply.to_vec(),now_ms);}
            }
            link.drain_engine();
        }
    }
    let mut start_carplay = false;
    while let Some((generation, result)) = receiver.try_result()? {
        let Some(index) = pending.iter().position(|(value, _)| *value == generation) else {
            continue; // An old generation finished after a reset.
        };
        let (_, reply) = pending.remove(index).unwrap();
        if link.generation() != Some(generation) {
            continue;
        }
        let id = match result {
            Ok(id) => id,
            Err(error) => {
                recovery.started(generation, false, now_ms);
                eprintln!("[receiver-control] START generation {generation}: {error}");
                continue;
            }
        };
        let reply = match control::bind_receiver_identity(&reply, &id) {
            Ok(reply) => reply,
            Err(error) => {
                recovery.started(generation, false, now_ms);
                eprintln!("[receiver-control] identity generation {generation}: {error}");
                receiver.reset()?;
                continue;
            }
        };
        link.send_control(reply, now_ms)?;
        link.send_control(
            control::ControlMachine::<control::UnavailableAuth>::metadata_subscription(),
            now_ms,
        )?;
        recovery.started(generation, true, now_ms);
        start_carplay = true;
    }
    Ok(start_carplay)
}

pub fn serve_stream<
    S: Read + Write,
    A: control::AuthProvider,
    R: receiver_control::ReceiverControl + Send + 'static,
>(
    stream: &mut S,
    time_ms: impl Fn() -> u64,
    config: &control::Config,
    auth_factory: impl FnMut() -> A,
    receiver_control: R,
) -> io::Result<()> {
    let receiver = receiver_control::ReceiverWorker::new(receiver_control);
    serve_stream_with_receiver(stream, time_ms, config, auth_factory, &receiver)
}

/// Daemon-lifetime worker: RFCOMM/TCP loss must not drop an active IP media
/// heartbeat. The caller resets bootstrap ownership when a stream ends.
pub fn serve_stream_with_receiver<S: Read + Write, A: control::AuthProvider>(
    stream: &mut S,
    time_ms: impl Fn() -> u64,
    config: &control::Config,
    mut auth_factory: impl FnMut() -> A,
    receiver: &receiver_control::ReceiverWorker,
) -> io::Result<()> {
    let mut decoder = FrameDecoder::default();
    let mut link = BridgeLink::default();
    let mut machine: Option<control::ControlMachine<A>> = None;
    let mut current_peer = None;
    let mut csm_reader = control::CsmReader::default();

    let mut pending_starts = VecDeque::new();
    let mut recovery = recovery::Recovery::default();
    #[cfg(target_os = "nto")]
    let mut connector = carplay_control::ConnectorSlot::default();
    let mut buf = [0u8; 4096];
    loop {
        match stream.read(&mut buf) {
            Ok(0) => return Ok(()),
            Ok(n) => {
                for frame in decoder.feed(&buf[..n])? {
                    let old_generation = link.generation();
                    let peer = if frame.kind == TYPE_CONNECTED && frame.data.len() == 6 {
                        Some(receiver_control::PeerIdentity::from_sdk_address(
                            frame.data.as_slice().try_into().unwrap(),
                        ))
                    } else {
                        None
                    };
                    #[cfg(target_os = "nto")]
                    if frame.kind == TYPE_DATA && frame.data == iap2_link::IAP2_MARKER {
                        eprintln!("[bridge] iAP2 marker received from RFCOMM");
                    }
                    link.handle(frame, time_ms());
                    if link.generation() != old_generation {
                        recovery.reset(link.generation());
                        receiver.reset()?;
                        current_peer = peer;
                        receiver.set_peer(current_peer.clone())?;
                        pending_starts.clear();
                        #[cfg(target_os = "nto")]
                        connector.reset();
                        machine = link
                            .generation()
                            .map(|_| control::ControlMachine::new(config.clone(), auth_factory()));
                        csm_reader = control::CsmReader::default();
                    }
                    let replies =
                        drain_control(&mut link, &mut machine, &mut csm_reader, time_ms())?;
                    publish_peer_name(
                        &mut machine,
                        &mut current_peer,
                        &receiver,
                        link.generation(),
                    )?;
                    let replies = replies
                        .into_iter()
                        .filter(|reply| {
                            link.generation()
                                .is_some_and(|g| recovery.offer(g, reply.clone()))
                        })
                        .collect();
                    schedule_receiver_starts(&link, &receiver, &mut pending_starts, replies)?;
                    #[cfg(target_os = "nto")]
                    if complete_receiver_starts(
                        &mut link,
                        &receiver,
                        &mut pending_starts,
                        time_ms(),
                        &mut recovery,
                    )? {
                        if let (Some(peer), Some(generation)) = (&current_peer, link.generation()) {
                            connector.start(config.clone(), peer.key.clone(), generation);
                        }
                    }
                    #[cfg(not(target_os = "nto"))]
                    let _ = complete_receiver_starts(
                        &mut link,
                        &receiver,
                        &mut pending_starts,
                        time_ms(),
                        &mut recovery,
                    )?;
                }
            }
            Err(e)
                if e.kind() == io::ErrorKind::WouldBlock || e.kind() == io::ErrorKind::TimedOut => {
            }
            Err(e) => return Err(e),
        }
        link.advance(time_ms());
        let replies = drain_control(&mut link, &mut machine, &mut csm_reader, time_ms())?;
        publish_peer_name(
            &mut machine,
            &mut current_peer,
            &receiver,
            link.generation(),
        )?;
        let replies = replies
            .into_iter()
            .filter(|reply| {
                link.generation()
                    .is_some_and(|g| recovery.offer(g, reply.clone()))
            })
            .collect();
        schedule_receiver_starts(&link, &receiver, &mut pending_starts, replies)?;
        #[cfg(target_os = "nto")]
        if complete_receiver_starts(
            &mut link,
            &receiver,
            &mut pending_starts,
            time_ms(),
            &mut recovery,
        )? {
            if let (Some(peer), Some(generation)) = (&current_peer, link.generation()) {
                connector.start(config.clone(), peer.key.clone(), generation);
            }
        }
        #[cfg(not(target_os = "nto"))]
        let _ = complete_receiver_starts(
            &mut link,
            &receiver,
            &mut pending_starts,
            time_ms(),
            &mut recovery,
        )?;
        while let Some((generation, phase)) = receiver.try_phase() {
            if recovery.observe(generation, phase, time_ms()) {
                eprintln!("[wireless-recovery] generation {generation}: {phase:?}");
                #[cfg(target_os = "nto")]
                connector.reset();
            }
        }
        #[cfg(target_os = "nto")]
        {
            let available = machine.as_ref().is_some_and(|m| m.wireless_available());
            if !available {
                connector.reset();
                recovery.probe_cancelled();
            }
            if let Some(generation) = connector.tick() {
                if let Some(reply) = recovery.reachable(generation, available) {
                    receiver.start(generation)?;
                    pending_starts.push_back((generation, reply));
                }
            }
            if let (Some(generation), Some(peer)) =
                (recovery.probe_due(time_ms(), available), &current_peer)
            {
                connector.probe(config.clone(), peer.key.clone(), generation);
            }
        }
        while let Some(frame) = link.next_outbound() {
            stream.write_all(&frame.encode()?)?;
        }
        if link.dead() {
            return Err(io::Error::new(
                io::ErrorKind::ConnectionAborted,
                "iAP2 link dead",
            ));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use iap2_link::{
        IAP2_MARKER, LinkPacketHeader, LinkSynchronizationPayload, LspSession, gen_checksum,
    };
    use std::time::{Duration, Instant};

    struct RejectReceiver;
    impl receiver_control::ReceiverControl for RejectReceiver {
        fn start(&mut self, _: u32) -> io::Result<()> {
            Err(io::Error::other("receiver not ready"))
        }
        fn stop(&mut self, _: u32) -> io::Result<()> {
            panic!("a failed START must not be followed by STOP")
        }
    }

    #[test]
    fn rejected_receiver_never_sends_carplay_start_reply() {
        let mut link = BridgeLink {
            generation: Some(27),
            ..BridgeLink::default()
        };
        let receiver = receiver_control::ReceiverWorker::new(RejectReceiver);
        let mut pending = VecDeque::new();
        schedule_receiver_starts(&link, &receiver, &mut pending, vec![vec![1]]).unwrap();
        let deadline = Instant::now() + Duration::from_secs(1);
        let mut recovery = recovery::Recovery::default();
        loop {
            match complete_receiver_starts(&mut link, &receiver, &mut pending, 0, &mut recovery) {
                Ok(false) if pending.is_empty() => break,
                Ok(false) if Instant::now() < deadline => {
                    std::thread::sleep(Duration::from_millis(1))
                }
                other => panic!("unexpected receiver gate result: {other:?}"),
            }
        }
        assert!(link.outbound.is_empty());
        assert_eq!(link.generation(), Some(27));
    }

    fn link_frame(
        control: u8,
        seq: u8,
        ack: u8,
        session_id: u8,
        payload: Option<&[u8]>,
    ) -> Vec<u8> {
        let len = payload.map_or(9, |p| p.len() + 10) as u16;
        let mut bytes = LinkPacketHeader {
            length: len,
            control,
            seq,
            ack,
            session_id,
        }
        .pack()
        .to_vec();
        if let Some(p) = payload {
            bytes.extend_from_slice(p);
            bytes.push(gen_checksum(p));
        }
        bytes
    }

    #[test]
    fn fragmented_bridge_frames_and_validation() {
        let frame = BridgeFrame {
            kind: TYPE_DATA,
            generation: 7,
            data: vec![1, 2, 3],
        }
        .encode()
        .unwrap();
        let mut decoder = FrameDecoder::default();
        assert!(decoder.feed(&frame[..5]).unwrap().is_empty());
        assert!(decoder.feed(&frame[5..9]).unwrap().is_empty());
        assert_eq!(
            decoder.feed(&frame[9..]).unwrap(),
            vec![BridgeFrame {
                kind: TYPE_DATA,
                generation: 7,
                data: vec![1, 2, 3]
            }]
        );
        assert!(decoder.feed(&[2, 1, 0, 0, 0, 0, 0, 0]).is_err());
    }

    #[test]
    fn outbound_frames_preserve_large_link_byte_stream() {
        let bytes: Vec<u8> = (0..(MAX_DATA * 2 + 17)).map(|i| i as u8).collect();
        let mut link = BridgeLink::default();
        link.queue_outbound_bytes(42, &bytes);
        let mut recovered = Vec::new();
        let mut lengths = Vec::new();
        while let Some(frame) = link.next_outbound() {
            lengths.push(frame.data.len());
            let encoded = frame.encode().unwrap();
            let mut decoder = FrameDecoder::default();
            let decoded = decoder.feed(&encoded).unwrap();
            assert_eq!(decoded, vec![frame.clone()]);
            assert_eq!(frame.generation, 42);
            recovered.extend(frame.data);
        }
        assert_eq!(lengths, vec![MAX_DATA, MAX_DATA, 17]);
        assert_eq!(recovered, bytes);
    }

    #[test]
    fn generation_resets_link_and_ignores_stale_bytes() {
        let mut link = BridgeLink::default();
        link.handle(
            BridgeFrame {
                kind: TYPE_CONNECTED,
                generation: 7,
                data: vec![],
            },
            0,
        );
        let mut marker = Vec::new();
        while let Some(f) = link.next_outbound() {
            marker.extend(f.data);
        }
        assert_eq!(marker, IAP2_MARKER);
        link.handle(
            BridgeFrame {
                kind: TYPE_DATA,
                generation: 6,
                data: vec![0],
            },
            1,
        );
        assert!(link.next_outbound().is_none());
        link.handle(
            BridgeFrame {
                kind: TYPE_CONNECTED,
                generation: 8,
                data: vec![],
            },
            2,
        );
        assert_eq!(link.generation(), Some(8));
        link.handle(
            BridgeFrame {
                kind: TYPE_DISCONNECTED,
                generation: 7,
                data: vec![],
            },
            3,
        );
        assert_eq!(link.generation(), Some(8));
    }

    #[test]
    fn synthetic_phone_completes_passive_link_handshake() {
        let mut link = BridgeLink::default();
        let connected = BridgeFrame {
            kind: TYPE_CONNECTED,
            generation: 1,
            data: vec![],
        };
        link.handle(connected, 0);
        link.next_outbound();
        link.handle(
            BridgeFrame {
                kind: TYPE_DATA,
                generation: 1,
                data: IAP2_MARKER.to_vec(),
            },
            0,
        );
        let mut syn = Vec::new();
        while let Some(f) = link.next_outbound() {
            syn.extend(f.data);
        }
        assert_eq!(&syn[..2], &[0xFF, 0x5A]);
        let lsp = LinkSynchronizationPayload {
            max_outgoing: 8,
            max_len: 1024,
            retransmission_timeout: 1000,
            ack_timeout: 100,
            max_retransmissions: 3,
            max_ack: 2,
            sessions: vec![LspSession {
                id: 10,
                kind: 0,
                version: 1,
            }],
        };
        link.handle(
            BridgeFrame {
                kind: TYPE_DATA,
                generation: 1,
                data: link_frame(0x80, 0x2A, 0, 0, Some(&lsp.pack())),
            },
            1,
        );
        while link.next_outbound().is_some() {}
        link.handle(
            BridgeFrame {
                kind: TYPE_DATA,
                generation: 1,
                data: link_frame(0x40, 0, 99, 0, None),
            },
            2,
        );
        assert!(link.is_normal());
    }

    #[test]
    fn phone_control_request_produces_identification_on_bridge() {
        use control::{AuthProvider, Config, ControlMachine, CsmReader};
        use iap2_csm::{
            CsmMessage,
            messages::identification::{IdentificationInformation, StartIdentification},
        };
        struct TestAuth;
        impl AuthProvider for TestAuth {
            fn certificate(&mut self) -> Result<Vec<u8>, String> {
                Ok(vec![1])
            }
            fn sign(&mut self, _: &[u8]) -> Result<Vec<u8>, String> {
                Ok(vec![2])
            }
        }
        let config = Config {
            name: "Audi Test".into(),
            manufacturer: "Audi".into(),
            model: "MHI2Q".into(),
            serial: "test".into(),
            firmware: "test".into(),
            hardware: "test".into(),
            bt_mac: [1, 2, 3, 4, 5, 6],
            ssid: "Test-AP".into(),
            passphrase: "testpass".into(),
            channel: 11,
            ip_address: "fe80::1".into(),
            airplay_port: 7000,
            device_id: "01:02:03:04:05:06".into(),
            pairing_id: None,
            source_version: "test".into(),
            vehicle_engine: iap2_csm::messages::identification::EngineType::Gas,
        };
        let mut link = BridgeLink::default();
        link.handle(
            BridgeFrame {
                kind: TYPE_CONNECTED,
                generation: 12,
                data: vec![],
            },
            0,
        );
        link.next_outbound();
        link.handle(
            BridgeFrame {
                kind: TYPE_DATA,
                generation: 12,
                data: IAP2_MARKER.to_vec(),
            },
            0,
        );
        while link.next_outbound().is_some() {}
        let lsp = LinkSynchronizationPayload {
            max_outgoing: 8,
            max_len: 1024,
            retransmission_timeout: 1000,
            ack_timeout: 100,
            max_retransmissions: 3,
            max_ack: 2,
            sessions: vec![LspSession {
                id: 10,
                kind: 0,
                version: 1,
            }],
        };
        link.handle(
            BridgeFrame {
                kind: TYPE_DATA,
                generation: 12,
                data: link_frame(0x80, 0x2A, 0, 0, Some(&lsp.pack())),
            },
            1,
        );
        while link.next_outbound().is_some() {}
        link.handle(
            BridgeFrame {
                kind: TYPE_DATA,
                generation: 12,
                data: link_frame(0x40, 0, 99, 0, None),
            },
            2,
        );
        assert!(link.is_normal());

        let request = StartIdentification {}.encode();
        link.handle(
            BridgeFrame {
                kind: TYPE_DATA,
                generation: 12,
                data: link_frame(0x40, 0x2B, 99, 10, Some(&request)),
            },
            3,
        );
        let mut machine = Some(ControlMachine::new(config, TestAuth));
        let mut reader = CsmReader::default();
        assert!(
            drain_control(&mut link, &mut machine, &mut reader, 3)
                .unwrap()
                .is_empty()
        );
        let mut out = Vec::new();
        while let Some(frame) = link.next_outbound() {
            out.extend(frame.data);
        }
        let marker = out.windows(2).position(|w| w == [0x40, 0x40]).unwrap();
        let csm_len = u16::from_be_bytes([out[marker + 2], out[marker + 3]]) as usize;
        let ident = IdentificationInformation::decode(&out[marker..marker + csm_len]).unwrap();
        assert_eq!(ident.name, "Audi Test");
        assert!(
            ident
                .wireless_car_play_transport_component
                .unwrap()
                .supports_car_play
        );
    }
}
