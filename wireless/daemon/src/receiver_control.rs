//! Local contract with the in-process dio receiver bridge.
//!
//! START is acknowledged only after the stock AirPlay receiver is ready on
//! the same port sent in CarPlayStartSession. The bridge checks that port and
//! fails closed when the receiver or its QNX lifecycle is not ready.

use std::io::{self, Read, Write};
use std::net::{Ipv4Addr, SocketAddrV4, TcpStream};
use std::sync::mpsc::{self, Receiver, Sender, SyncSender, TryRecvError};
use std::sync::{Arc,atomic::{AtomicU32,Ordering}};
use std::thread::{self, JoinHandle};
use std::time::Duration;

const ADDRESS: SocketAddrV4 = SocketAddrV4::new(Ipv4Addr::LOCALHOST, 57343);
const VERSION: u8 = 2;
const OP_START: u8 = 1;
const OP_STOP: u8 = 2;
const OP_IDENTITY: u8 = 5;
const OP_STATUS: u8 = 6;
const OP_HEALTH: u8 = 7;
const OP_NOW_PLAYING: u8 = 8;
const OP_IAP2: u8 = 9;
const OP_IAP2_POLL: u8 = 10;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ReceiverPhase {
    Unknown,
    Waiting,
    Streaming,
    Draining,
    Ended,
    /// SI replaced its child within the same authenticated admission.
    Restarted,
    Blocked,
}

struct BridgeReply {
    identity: Option<String>,
    observation: Option<ReceiverPhase>,
    semantic: Option<Vec<u8>>,
    semantic_sequence: u32,
}

pub trait ReceiverControl {
    fn semantic(&mut self,_generation:u32,_kind:u8,_data:&[u8])->io::Result<()> {
        Err(io::Error::new(io::ErrorKind::Unsupported,"wireless semantic channel unavailable"))
    }
    fn semantic_outgoing(&mut self,_generation:u32)->io::Result<Option<Vec<u8>>> { Ok(None) }
    fn now_playing(&mut self, _generation: u32, _frame: &[u8]) -> io::Result<()> {
        Err(io::Error::new(
            io::ErrorKind::Unsupported,
            "native metadata unavailable",
        ))
    }
    fn set_peer(&mut self, _peer: Option<PeerIdentity>) -> io::Result<()> {
        Ok(())
    }
    fn poll(&mut self) -> io::Result<ReceiverPhase> {
        Ok(ReceiverPhase::Unknown)
    }
    fn start(&mut self, generation: u32) -> io::Result<()>;
    fn stop(&mut self, generation: u32) -> io::Result<()>;
    fn pairing_identity(&mut self, _generation: u32) -> io::Result<String> {
        Err(io::Error::new(
            io::ErrorKind::Unsupported,
            "receiver has no pairing identity contract",
        ))
    }
}

#[derive(Clone, Debug)]
pub struct PeerIdentity {
    pub key: String,
    pub name: String,
}
impl PeerIdentity {
    /// Stable SDK address bytes; no assumption about presentation byte order.
    pub fn from_sdk_address(address: [u8; 6]) -> Self {
        Self {
            key: format!(
                "bt:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                address[0], address[1], address[2], address[3], address[4], address[5]
            ),
            name: "iPhone".into(),
        }
    }
}

#[derive(Clone, Copy)]
pub struct LoopbackReceiverControl {
    airplay_port: u16,
    si: Option<(u64, u32)>,
    address: SocketAddrV4,
    semantic_input: u32,
    semantic_output: u32,
    semantic_child: u32,
}

impl LoopbackReceiverControl {
    pub fn new(airplay_port: u32) -> io::Result<Self> {
        let airplay_port = u16::try_from(airplay_port)
            .ok()
            .filter(|port| *port != 0)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "invalid AirPlay port"))?;
        Ok(Self {
            airplay_port,
            si: None,
            address: ADDRESS,
            semantic_input:0,semantic_output:0,semantic_child:0,
        })
    }

    pub fn bind_si(&mut self, epoch: u64, session: u32) {
        if self.si!=Some((epoch,session)){self.semantic_input=0;self.semantic_output=0;self.semantic_child=0;}
        self.si = Some((epoch, session));
    }
    pub fn bind_child(&mut self,child:u32) {
        if self.semantic_child!=child {self.semantic_input=0;self.semantic_output=0;self.semantic_child=child;}
    }

    fn request(&self, operation: u8, generation: u32) -> io::Result<()> {
        Self::exchange_bound(
            self.address,
            operation,
            generation,
            self.airplay_port,
            self.si,
        )
        .map(|_| ())
    }

    #[cfg(test)]
    fn request_to(
        address: SocketAddrV4,
        operation: u8,
        generation: u32,
        airplay_port: u16,
    ) -> io::Result<()> {
        Self::exchange_to(address, operation, generation, airplay_port).map(|_| ())
    }

    #[cfg(test)]
    fn exchange_to(
        address: SocketAddrV4,
        operation: u8,
        generation: u32,
        airplay_port: u16,
    ) -> io::Result<Option<String>> {
        Self::exchange_bound(address, operation, generation, airplay_port, None).map(|r| r.identity)
    }

    fn exchange_bound(
        address: SocketAddrV4,
        operation: u8,
        generation: u32,
        airplay_port: u16,
        si: Option<(u64, u32)>,
    ) -> io::Result<BridgeReply> {
        Self::exchange_payload(address, operation, generation, airplay_port, si, None)
    }
    fn exchange_payload(
        address: SocketAddrV4,
        operation: u8,
        generation: u32,
        airplay_port: u16,
        si: Option<(u64, u32)>,
        payload: Option<&[u8]>,
    ) -> io::Result<BridgeReply> {
        if let Some(frame) = payload {
            if si.is_none() || !matches!(operation,OP_NOW_PLAYING|OP_IAP2|OP_IAP2_POLL) ||
                (operation==OP_NOW_PLAYING && (frame.len()<6 || frame.len()>4096)) ||
                (operation==OP_IAP2 && (frame.len()<6 || frame.len()>8192 || frame[0]>2 || frame[1..5]==[0;4])) ||
                (operation==OP_IAP2_POLL && frame.len()!=4)
            {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "invalid metadata envelope",
                ));
            }
        }
        let timeout = if operation == OP_START {
            Duration::from_secs(8)
        } else {
            Duration::from_secs(2)
        };
        let mut stream = TcpStream::connect_timeout(&address.into(), timeout)?;
        stream.set_read_timeout(Some(timeout))?;
        stream.set_write_timeout(Some(timeout))?;
        let mut request = [0u8; 16];
        request[..4].copy_from_slice(b"CPRX");
        request[4] = if si.is_some() { 3 } else { VERSION };
        request[5] = operation;
        request[8..12].copy_from_slice(&generation.to_le_bytes());
        request[12..14].copy_from_slice(&airplay_port.to_le_bytes());
        stream.write_all(&request)?;
        if let Some((epoch, session)) = si {
            let mut context = [0; 16];
            context[..8].copy_from_slice(&epoch.to_le_bytes());
            context[8..12].copy_from_slice(&session.to_le_bytes());
            stream.write_all(&context)?;
        }
        if let Some(frame) = payload {
            if operation!=OP_IAP2_POLL {stream.write_all(&(frame.len() as u16).to_le_bytes())?;}
            stream.write_all(frame)?;
        }
        let mut response = [0u8; 8];
        stream.read_exact(&mut response)?;
        if response[..4] != *b"CPRA" || response[4] != VERSION || response[6..8] != [0, 0] {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "invalid receiver bridge response",
            ));
        }
        if response[5] != 0 {
            let kind = if matches!(response[5],1|3) {
                io::ErrorKind::WouldBlock
            } else {
                io::ErrorKind::Other
            };
            return Err(io::Error::new(
                kind,
                format!(
                    "receiver bridge rejected operation {operation}: {}",
                    response[5]
                ),
            ));
        }
        if operation == OP_IDENTITY {
            let mut size = [0u8; 2];
            stream.read_exact(&mut size)?;
            let size = u16::from_le_bytes(size) as usize;
            if size == 0 || size > 252 {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "invalid receiver pairing ID length",
                ));
            }
            let mut bytes = vec![0; size];
            stream.read_exact(&mut bytes)?;
            let id = String::from_utf8(bytes)
                .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;
            crate::control::validate_pairing_id(&id)
                .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;
            return Ok(BridgeReply {
                identity: Some(id),
                observation: None,
                semantic: None,
                semantic_sequence:0,
            });
        }
        let mut semantic_sequence=0;
        let semantic=if operation==OP_IAP2_POLL {
            if si.is_none() { return Err(io::Error::other("semantic poll requires SI binding")); }
            let mut header=[0;6];stream.read_exact(&mut header)?;
            semantic_sequence=u32::from_le_bytes(header[..4].try_into().unwrap());
            let size=u16::from_le_bytes(header[4..6].try_into().unwrap()) as usize;
            if size>512 { return Err(io::Error::other("oversized semantic output")); }
            if size==0 { None } else {
                let mut frame=vec![0;size];stream.read_exact(&mut frame)?;
                let id=crate::control::message_id(&frame).map_err(io::Error::other)?;
                if !matches!(id,0x5000|0x5200|0x5203|0xa101|0xfffb) {return Err(io::Error::other("forbidden semantic output"));}
                Some(frame)
            }
        } else {None};
        let observation = if operation == OP_STATUS || operation == OP_HEALTH {
            let mut state = [0; 4];
            stream.read_exact(&mut state)?;
            if state[1..] != [0, 0, 0] {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "invalid receiver observation",
                ));
            }
            Some(match state[0] {
                0 | 5 => ReceiverPhase::Waiting,
                7 => ReceiverPhase::Streaming,
                4 => ReceiverPhase::Ended,
                _ => {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidData,
                        "invalid receiver media flags",
                    ));
                }
            })
        } else {
            None
        };
        Ok(BridgeReply {
            identity: None,
            observation,
            semantic,
            semantic_sequence,
        })
    }

    pub fn observe(&self, generation: u32) -> io::Result<ReceiverPhase> {
        Self::exchange_bound(
            self.address,
            OP_STATUS,
            generation,
            self.airplay_port,
            self.si,
        )?
        .observation
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "missing media status"))
    }
    pub fn check_health(&self, generation: u32) -> io::Result<()> {
        Self::exchange_bound(
            self.address,
            OP_HEALTH,
            generation,
            self.airplay_port,
            self.si,
        )
        .map(|_| ())
    }
    #[cfg(test)]
    pub(crate) fn test_endpoint(&mut self, address: SocketAddrV4) {
        self.address = address;
    }
}

impl ReceiverControl for LoopbackReceiverControl {
    fn semantic(&mut self,generation:u32,kind:u8,data:&[u8])->io::Result<()> {
        if self.si.is_none()||kind>2||data.is_empty()||data.len()>8187{return Err(io::Error::new(io::ErrorKind::Unsupported,"invalid semantic input/binding"));}
        self.semantic_input=self.semantic_input.checked_add(1).ok_or_else(||io::Error::other("semantic sequence exhausted"))?;
        let mut frame=Vec::with_capacity(data.len()+5);frame.push(kind);frame.extend(self.semantic_input.to_le_bytes());frame.extend_from_slice(data);
        for attempt in 0..3 {
            match Self::exchange_payload(self.address,OP_IAP2,generation,self.airplay_port,self.si,Some(&frame)) {
                Ok(_)=>return Ok(()),
                Err(error) if attempt<2&&matches!(error.kind(),io::ErrorKind::TimedOut|io::ErrorKind::WouldBlock|
                    io::ErrorKind::BrokenPipe|io::ErrorKind::ConnectionReset|io::ErrorKind::ConnectionAborted|
                    io::ErrorKind::ConnectionRefused|io::ErrorKind::UnexpectedEof)=>{thread::sleep(Duration::from_millis(50));},
                Err(error)=>return Err(error),
            }
        }
        unreachable!()
    }
    fn semantic_outgoing(&mut self,generation:u32)->io::Result<Option<Vec<u8>>> {
        if self.si.is_none(){return Ok(None);}
        let reply=Self::exchange_payload(self.address,OP_IAP2_POLL,generation,self.airplay_port,self.si,Some(&self.semantic_output.to_le_bytes()))?;
        if reply.semantic.is_some() {
            if reply.semantic_sequence<=self.semantic_output{return Err(io::Error::other("stale semantic output sequence"));}
            self.semantic_output=reply.semantic_sequence;
        } else if reply.semantic_sequence!=self.semantic_output{return Err(io::Error::other("empty semantic output changed sequence"));}
        Ok(reply.semantic)
    }
    fn now_playing(&mut self, generation: u32, frame: &[u8]) -> io::Result<()> {
        if self.si.is_none() {
            return Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "metadata requires SI context",
            ));
        }
        // Native metadata updates are state assignments, so a repeated
        // frame after a lost acknowledgement is idempotent. Keep partial
        // updates ordered and bound to the same native admission.
        for attempt in 0..3 {
            match Self::exchange_payload(self.address,OP_NOW_PLAYING,generation,self.airplay_port,self.si,Some(frame)) {
                Ok(_)=>return Ok(()),
                Err(error) if attempt<2&&matches!(error.kind(),io::ErrorKind::TimedOut|io::ErrorKind::WouldBlock|
                    io::ErrorKind::BrokenPipe|io::ErrorKind::ConnectionReset|io::ErrorKind::ConnectionAborted|
                    io::ErrorKind::ConnectionRefused|io::ErrorKind::UnexpectedEof)=>thread::sleep(Duration::from_millis(50)),
                Err(error)=>return Err(error),
            }
        }
        unreachable!()
    }
    fn start(&mut self, generation: u32) -> io::Result<()> {
        self.request(OP_START, generation)
    }

    fn stop(&mut self, generation: u32) -> io::Result<()> {
        self.request(OP_STOP, generation)
    }
    fn pairing_identity(&mut self, generation: u32) -> io::Result<String> {
        Self::exchange_bound(
            self.address,
            OP_IDENTITY,
            generation,
            self.airplay_port,
            self.si,
        )?
        .identity
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "missing receiver pairing ID"))
    }
}

/// Owns the single receiver generation associated with one RFCOMM stream.
pub struct ReceiverLease<R: ReceiverControl> {
    control: R,
    active: Option<u32>,
}

impl<R: ReceiverControl> ReceiverLease<R> {
    pub fn new(control: R) -> Self {
        Self {
            control,
            active: None,
        }
    }

    pub fn start(&mut self, generation: u32) -> io::Result<()> {
        if self.active == Some(generation) {
            return Ok(());
        }
        self.reset();
        if self.active.is_some() {
            return Err(io::Error::new(
                io::ErrorKind::WouldBlock,
                "previous receiver generation did not stop",
            ));
        }
        self.control.start(generation)?;
        self.active = Some(generation);
        Ok(())
    }

    pub fn reset(&mut self) {
        if let Some(generation) = self.active {
            for attempt in 0..6 {
                match self.control.stop(generation) {
                    Ok(()) => {
                        self.active = None;
                        return;
                    }
                    Err(error) if error.kind() == io::ErrorKind::WouldBlock && attempt < 5 => {
                        thread::sleep(Duration::from_millis(100));
                    }
                    Err(error) => {
                        eprintln!("[receiver-control] stop generation {generation}: {error}");
                        return;
                    }
                }
            }
        }
    }
}

impl<R: ReceiverControl> Drop for ReceiverLease<R> {
    fn drop(&mut self) {
        self.reset();
    }
}

enum Command {
    Start(u32),
    Reset,
    Peer(Option<PeerIdentity>),
    Identification(u32,Vec<u8>),
}

/// Keeps a slow QNX receiver START/STOP out of the iAP2 link timer loop.
/// Commands remain ordered, so a reset after an in-flight START releases the
/// generation even if the acknowledgement arrives after the RFCOMM disconnect.
pub struct ReceiverWorker {
    commands: Sender<Command>,
    metadata: SyncSender<(u32, Vec<u8>)>,
    semantic: SyncSender<(u32,u8,Vec<u8>)>,
    outgoing: Receiver<(u32,u32,Vec<u8>)>,
    delivery_epoch: Arc<AtomicU32>,
    results: Receiver<(u32, io::Result<String>)>,
    thread: Option<JoinHandle<()>>,
    phases: Receiver<(u32, ReceiverPhase)>,
}

impl ReceiverWorker {
    pub fn new<R: ReceiverControl + Send + 'static>(control: R) -> Self {
        let (commands, requests) = mpsc::channel();
        let (metadata, metadata_rx) = mpsc::sync_channel::<(u32, Vec<u8>)>(16);
        let (semantic,semantic_rx)=mpsc::sync_channel::<(u32,u8,Vec<u8>)>(32);
        let (outgoing_tx,outgoing)=mpsc::sync_channel::<(u32,u32,Vec<u8>)>(16);
        let delivery_epoch=Arc::new(AtomicU32::new(1));let worker_epoch=delivery_epoch.clone();
        let (results, replies) = mpsc::channel();
        let (phase_tx, phases) = mpsc::channel();
        let thread = thread::spawn(move || {
            let mut lease = ReceiverLease::new(control);
            let mut current = None;
            let mut previous = None;
            let mut polled = std::time::Instant::now();
            let mut identification:Option<(u32,Vec<u8>)>=None;
            let mut semantic_ready=false;
            let mut semantic_polled=std::time::Instant::now();
            let mut semantic_retry=std::time::Instant::now();
            loop {
                /* Authenticated bootstrap and native media have independent
                 * lifetimes. SI can retain IP media across an RFCOMM reset;
                 * the control implementation revalidates that admission. */
                if let Some((generation,frame))=&identification {
                    let generation=*generation;
                    if !semantic_ready && std::time::Instant::now()>=semantic_retry {
                        semantic_retry=std::time::Instant::now()+Duration::from_millis(250);
                        match lease.control.semantic(generation,0,frame) {
                            Ok(())=>semantic_ready=true,
                            Err(error) if error.kind()==io::ErrorKind::Unsupported=>{},
                            Err(error)=>eprintln!("[receiver-control] semantic begin rejected: {error}"),
                        }
                    }
                    if semantic_ready {
                        if let Ok((origin,kind,data))=semantic_rx.try_recv() {
                            if origin==generation {
                                if let Err(error)=lease.control.semantic(generation,kind,&data) {
                                    eprintln!("[receiver-control] semantic input rejected: {error}");
                                    while semantic_rx.try_recv().is_ok(){}semantic_ready=false;
                                }
                            }
                        }
                        if semantic_polled.elapsed()>=Duration::from_millis(100) {
                            semantic_polled=std::time::Instant::now();
                            match lease.control.semantic_outgoing(generation) {
                                Ok(Some(frame))=> {if outgoing_tx.try_send((generation,worker_epoch.load(Ordering::Acquire),frame)).is_err(){semantic_ready=false;}},
                                Ok(None)=>{},Err(error)=>eprintln!("[receiver-control] semantic poll rejected: {error}"),
                            }
                        }
                    }
                } else { while semantic_rx.try_recv().is_ok(){} }
                if semantic_ready { if let Ok((generation, frame)) = metadata_rx.try_recv() {
                    if identification.as_ref().is_some_and(|(g,_)|*g==generation) {
                        if let Err(error) = lease.control.now_playing(generation, &frame) {
                            eprintln!("[receiver-control] metadata rejected: {error}");
                        }
                    }
                } }
                if polled.elapsed() >= Duration::from_secs(1) {
                    match lease.control.poll() {
                        Ok(phase) => {
                            if matches!(phase, ReceiverPhase::Ended | ReceiverPhase::Restarted) {
                                lease.active = None;
                                semantic_ready=false;
                                if worker_epoch.fetch_add(1,Ordering::AcqRel)==u32::MAX {break;}
                            }
                            if let Some(generation) = current {
                                if previous != Some(phase) {
                                    let _ = phase_tx.send((generation, phase));
                                    previous = Some(phase);
                                }
                            }
                        }
                        Err(error) => eprintln!("[receiver-control] heartbeat: {error}"),
                    }
                    polled = std::time::Instant::now();
                }
                let command = match requests.recv_timeout(
                    Duration::from_millis(10)
                        .min(Duration::from_secs(1).saturating_sub(polled.elapsed())),
                ) {
                    Ok(command) => command,
                    Err(mpsc::RecvTimeoutError::Disconnected) => break,
                    Err(mpsc::RecvTimeoutError::Timeout) => continue,
                };
                match command {
                    Command::Identification(generation,frame)=> {
                        identification=Some((generation,frame));semantic_ready=false;
                        semantic_retry=std::time::Instant::now();
                    }
                    Command::Peer(peer) => {
                        if peer.is_none() {
                            while metadata_rx.try_recv().is_ok() {}
                        }
                        if let Err(error) = lease.control.set_peer(peer) {
                            eprintln!("[receiver-control] peer identity rejected: {error}");
                        }
                    }
                    Command::Start(generation) => {
                        if worker_epoch.fetch_add(1,Ordering::AcqRel)==u32::MAX {break;}
                        while semantic_rx.try_recv().is_ok(){}semantic_ready=false;
                        semantic_retry=std::time::Instant::now();
                        while metadata_rx.try_recv().is_ok() {}
                        current = Some(generation);
                        previous = None;
                        let result = lease.start(generation).and_then(|()| {
                            let identity = lease.control.pairing_identity(generation);
                            if identity.is_err() {
                                lease.reset();
                            }
                            identity
                        });
                        let _ = results.send((generation, result));
                    }
                    Command::Reset => {
                        if worker_epoch.fetch_add(1,Ordering::AcqRel)==u32::MAX {break;}
                        while semantic_rx.try_recv().is_ok(){}semantic_ready=false;identification=None;
                        while metadata_rx.try_recv().is_ok() {}
                        current = None;
                        previous = None;
                        lease.reset();
                    }
                }
            }
        });
        Self {
            commands,
            metadata,
            semantic,
            outgoing,
            delivery_epoch,
            results: replies,
            thread: Some(thread),
            phases,
        }
    }

    pub fn start(&self, generation: u32) -> io::Result<()> {
        self.commands
            .send(Command::Start(generation))
            .map_err(|_| io::Error::new(io::ErrorKind::BrokenPipe, "receiver worker stopped"))
    }
    pub fn now_playing(&self, generation: u32, frame: Vec<u8>) -> io::Result<()> {
        self.metadata
            .try_send((generation, frame))
            .map_err(|e| io::Error::new(io::ErrorKind::WouldBlock, e.to_string()))
    }
    pub fn identification(&self,generation:u32,frame:Vec<u8>)->io::Result<()> {
        if generation==0||frame.len()>4096{return Err(io::Error::other("invalid identification"));}
        self.commands.send(Command::Identification(generation,frame)).map_err(|_|io::Error::other("receiver worker stopped"))
    }
    pub fn semantic(&self,generation:u32,kind:u8,data:Vec<u8>)->io::Result<()> {
        if data.len()>8187||kind<1||kind>2{return Err(io::Error::other("invalid semantic input"));}
        self.semantic.try_send((generation,kind,data)).map_err(|_|io::Error::new(io::ErrorKind::WouldBlock,"semantic backlog full"))
    }
    pub fn try_outgoing(&self)->Option<(u32,Vec<u8>)> {
        while let Ok((generation,epoch,frame))=self.outgoing.try_recv() {
            if epoch==self.delivery_epoch.load(Ordering::Acquire){return Some((generation,frame));}
        }
        None
    }

    pub fn reset(&self) -> io::Result<()> {
        self.commands
            .send(Command::Reset)
            .map_err(|_| io::Error::new(io::ErrorKind::BrokenPipe, "receiver worker stopped"))
    }

    pub fn set_peer(&self, peer: Option<PeerIdentity>) -> io::Result<()> {
        self.commands
            .send(Command::Peer(peer))
            .map_err(|_| io::Error::new(io::ErrorKind::BrokenPipe, "receiver worker stopped"))
    }

    pub fn try_result(&self) -> io::Result<Option<(u32, io::Result<String>)>> {
        match self.results.try_recv() {
            Ok(result) => Ok(Some(result)),
            Err(TryRecvError::Empty) => Ok(None),
            Err(TryRecvError::Disconnected) => Err(io::Error::new(
                io::ErrorKind::BrokenPipe,
                "receiver worker stopped",
            )),
        }
    }
    pub fn try_phase(&self) -> Option<(u32, ReceiverPhase)> {
        self.phases.try_recv().ok()
    }
}

impl Drop for ReceiverWorker {
    fn drop(&mut self) {
        // Closing the command channel lets the worker drop its lease, which
        // sends STOP for any accepted generation before this session returns.
        let (replacement, _) = mpsc::channel();
        self.commands = replacement;
        if let Some(thread) = self.thread.take() {
            let _ = thread.join();
        }
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn metadata_retries_not_ready_and_lost_ack_with_the_same_bound_frame() {
        let listener=TcpListener::bind("127.0.0.1:0").unwrap();
        let std::net::SocketAddr::V4(address)=listener.local_addr().unwrap()else{unreachable!()};
        let server=std::thread::spawn(move|| {
            let mut original=vec![];
            for attempt in 0..3 {
                let (mut stream,_)=listener.accept().unwrap();let mut packet=vec![0;40];stream.read_exact(&mut packet).unwrap();
                assert_eq!(&packet[..6],b"CPRX\x03\x08");assert_eq!(&packet[16..24],&42u64.to_le_bytes());
                if attempt==0{original=packet.clone();}else{assert_eq!(original,packet);}
                stream.write_all(match attempt{0=>&b"CPRA\x02\x01\0\0"[..],1=>&b"CPRA\x02"[..],_=>&b"CPRA\x02\0\0\0"[..]}).unwrap();
            }
        });
        let mut client=LoopbackReceiverControl::new(5000).unwrap();client.test_endpoint(address);client.bind_si(42,7);
        client.now_playing(3,b"@@\0\x06\x50\x01").unwrap();server.join().unwrap();
    }
    #[test]
    fn authenticated_feature_channel_survives_a_busy_new_media_start() {
        struct Retained(Sender<(u32,u8)>);
        impl ReceiverControl for Retained {
            fn start(&mut self,_:u32)->io::Result<()> {Err(io::Error::from(io::ErrorKind::WouldBlock))}
            fn stop(&mut self,_:u32)->io::Result<()> {panic!("no accepted media lease")}
            fn semantic(&mut self,g:u32,k:u8,_:&[u8])->io::Result<()> {self.0.send((g,k)).unwrap();Ok(())}
            fn now_playing(&mut self,g:u32,_:&[u8])->io::Result<()> {self.0.send((g,8)).unwrap();Ok(())}
        }
        let (tx,rx)=mpsc::channel();let worker=ReceiverWorker::new(Retained(tx));
        worker.identification(9,vec![1]).unwrap();
        assert_eq!(rx.recv_timeout(Duration::from_secs(1)).unwrap(),(9,0));
        worker.start(9).unwrap();
        assert_eq!(rx.recv_timeout(Duration::from_secs(1)).unwrap(),(9,0));
        worker.semantic(9,1,vec![2]).unwrap();worker.now_playing(9,vec![3]).unwrap();
        let mut kinds=vec![rx.recv_timeout(Duration::from_secs(1)).unwrap(),rx.recv_timeout(Duration::from_secs(1)).unwrap()];
        kinds.sort();assert_eq!(kinds,vec![(9,1),(9,8)]);
        worker.semantic(8,1,vec![4]).unwrap();worker.reset().unwrap();drop(worker);
        assert!(rx.try_recv().is_err());
    }
    #[test]
    fn semantic_outgoing_discards_old_child_even_on_same_link_generation() {
        let (commands,_)=mpsc::channel();let (metadata,_)=mpsc::sync_channel(1);
        let (semantic,_)=mpsc::sync_channel(1);let (tx,outgoing)=mpsc::sync_channel(4);
        let (_,results)=mpsc::channel();let (_,phases)=mpsc::channel();
        let worker=ReceiverWorker{commands,metadata,semantic,outgoing,
            delivery_epoch:Arc::new(AtomicU32::new(3)),results,phases,thread:None};
        tx.send((7,2,vec![1])).unwrap();tx.send((7,3,vec![2])).unwrap();
        assert_eq!(worker.try_outgoing(),Some((7,vec![2])));
        tx.send((7,3,vec![3])).unwrap();worker.delivery_epoch.store(4,Ordering::Release);
        assert!(worker.try_outgoing().is_none());
    }
    #[test]
    fn semantic_tcp_retry_reuses_sequence_and_new_child_resets_it() {
        use super::*;
        let listener=TcpListener::bind("127.0.0.1:0").unwrap();
        let std::net::SocketAddr::V4(address)=listener.local_addr().unwrap()else{unreachable!()};
        let server=std::thread::spawn(move|| {
            let mut first=Vec::new();
            for (index,sequence) in [1u32,1,2,1].into_iter().enumerate() {
                let (mut stream,_)=listener.accept().unwrap();let mut head=[0;34];stream.read_exact(&mut head).unwrap();
                assert_eq!(&head[..6],b"CPRX\x03\x09");
                assert_eq!(&head[16..24],&42u64.to_le_bytes());
                let size=u16::from_le_bytes(head[32..34].try_into().unwrap())as usize;
                let mut data=vec![0;size];stream.read_exact(&mut data).unwrap();
                assert_eq!(data[0],2);assert_eq!(u32::from_le_bytes(data[1..5].try_into().unwrap()),sequence);
                if index==0 {first=data;continue;}
                if index==1 {assert_eq!(data,first);}
                stream.write_all(b"CPRA\x02\0\0\0").unwrap();
            }
        });
        let mut client=LoopbackReceiverControl::new(5000).unwrap();client.test_endpoint(address);client.bind_si(42,7);client.bind_child(1234);
        client.semantic(3,2,&[129,0x80]).unwrap();client.bind_child(1234);
        client.semantic(3,2,&[129,0x40]).unwrap();client.bind_child(1235);
        client.semantic(3,2,&[129,0x80]).unwrap();server.join().unwrap();
    }
    #[test]
    fn semantic_output_ack_waits_for_a_complete_tcp_reply() {
        use super::*;
        let listener=TcpListener::bind("127.0.0.1:0").unwrap();
        let std::net::SocketAddr::V4(address)=listener.local_addr().unwrap()else{unreachable!()};
        let server=std::thread::spawn(move|| {
            for (index,ack) in [0u32,0,1].into_iter().enumerate() {
                let (mut stream,_)=listener.accept().unwrap();let mut request=[0;36];stream.read_exact(&mut request).unwrap();
                assert_eq!(&request[..6],b"CPRX\x03\x0a");assert_eq!(&request[32..36],&ack.to_le_bytes());
                stream.write_all(b"CPRA\x02\0\0\0").unwrap();
                stream.write_all(&1u32.to_le_bytes()).unwrap();
                stream.write_all(&(if index==2{0u16}else{6}).to_le_bytes()).unwrap();
                if index<2 {stream.write_all(if index==0{b"@"}else{b"@@\0\x06\x52\0"}).unwrap();}
            }
        });
        let mut client=LoopbackReceiverControl::new(5000).unwrap();client.test_endpoint(address);client.bind_si(42,7);client.bind_child(1234);
        assert_eq!(client.semantic_outgoing(3).unwrap_err().kind(),io::ErrorKind::UnexpectedEof);
        assert_eq!(client.semantic_outgoing(3).unwrap(),Some(b"@@\0\x06\x52\0".to_vec()));
        assert!(client.semantic_outgoing(3).unwrap().is_none());server.join().unwrap();
    }
    use super::*;
    use std::cell::RefCell;
    use std::net::TcpListener;
    use std::rc::Rc;
    use std::sync::mpsc;

    #[test]
    fn metadata_envelope_is_bounded_and_bound_to_native_session() {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let std::net::SocketAddr::V4(address) = listener.local_addr().unwrap() else {
            unreachable!()
        };
        let frame = b"@@\0\x06\x50\x01";
        for (op, si, payload) in [
            (OP_NOW_PLAYING, None, &frame[..]),
            (OP_START, Some((42, 7)), &frame[..]),
            (OP_NOW_PLAYING, Some((42, 7)), &[][..]),
            (OP_NOW_PLAYING, Some((42, 7)), &[0u8; 4097][..]),
        ] {
            assert_eq!(
                LoopbackReceiverControl::exchange_payload(address, op, 3, 5000, si, Some(payload))
                    .err()
                    .unwrap()
                    .kind(),
                io::ErrorKind::InvalidInput
            );
        }
        let server = std::thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            let mut request = [0u8; 40];
            stream.read_exact(&mut request).unwrap();
            assert_eq!(&request[..8], b"CPRX\x03\x08\0\0");
            assert_eq!(&request[8..12], &3u32.to_le_bytes());
            assert_eq!(&request[16..24], &42u64.to_le_bytes());
            assert_eq!(&request[24..28], &7u32.to_le_bytes());
            assert_eq!(&request[28..32], &[0; 4]);
            assert_eq!(&request[32..34], &6u16.to_le_bytes());
            assert_eq!(&request[34..], b"@@\0\x06\x50\x01");
            for byte in b"CPRA\x02\0\0\0" {
                stream.write_all(&[*byte]).unwrap();
            }
        });
        LoopbackReceiverControl::exchange_payload(
            address,
            OP_NOW_PLAYING,
            3,
            5000,
            Some((42, 7)),
            Some(frame),
        )
        .unwrap();
        server.join().unwrap();
    }

    #[test]
    fn status_flags_are_strict_and_generation_bound() {
        for (flags, expected) in [
            (0, Some(ReceiverPhase::Waiting)),
            (5, Some(ReceiverPhase::Waiting)),
            (7, Some(ReceiverPhase::Streaming)),
            (4, Some(ReceiverPhase::Ended)),
            (1, None),
            (6, None),
            (8, None),
        ] {
            let listener = TcpListener::bind("127.0.0.1:0").unwrap();
            let std::net::SocketAddr::V4(address) = listener.local_addr().unwrap() else {
                unreachable!()
            };
            let server = std::thread::spawn(move || {
                let (mut stream, _) = listener.accept().unwrap();
                let mut request = [0; 32];
                stream.read_exact(&mut request).unwrap();
                assert_eq!(&request[..6], b"CPRX\x03\x06");
                assert_eq!(&request[8..12], &3u32.to_le_bytes());
                assert_eq!(&request[24..28], &7u32.to_le_bytes());
                stream
                    .write_all(&[b'C', b'P', b'R', b'A', 2, 0, 0, 0, flags, 0, 0, 0])
                    .unwrap();
            });
            let result =
                LoopbackReceiverControl::exchange_bound(address, OP_STATUS, 3, 5000, Some((42, 7)));
            match expected {
                Some(p) => assert_eq!(result.unwrap().observation, Some(p)),
                None => assert!(result.is_err()),
            }
            server.join().unwrap();
        }
    }

    #[derive(Default)]
    struct State {
        calls: Vec<(u8, u32)>,
        reject_start: bool,
        busy_stops: usize,
    }

    struct Mock(Rc<RefCell<State>>);
    impl ReceiverControl for Mock {
        fn start(&mut self, generation: u32) -> io::Result<()> {
            let mut state = self.0.borrow_mut();
            state.calls.push((OP_START, generation));
            if state.reject_start {
                return Err(io::Error::other("not ready"));
            }
            Ok(())
        }
        fn stop(&mut self, generation: u32) -> io::Result<()> {
            let mut state = self.0.borrow_mut();
            state.calls.push((OP_STOP, generation));
            if state.busy_stops > 0 {
                state.busy_stops -= 1;
                return Err(io::Error::from(io::ErrorKind::WouldBlock));
            }
            Ok(())
        }
    }

    #[test]
    fn lease_stops_only_the_accepted_generation() {
        let state = Rc::new(RefCell::new(State::default()));
        {
            let mut lease = ReceiverLease::new(Mock(state.clone()));
            lease.start(7).unwrap();
            lease.start(7).unwrap();
            lease.start(8).unwrap();
        }
        assert_eq!(
            state.borrow().calls,
            [(OP_START, 7), (OP_STOP, 7), (OP_START, 8), (OP_STOP, 8)]
        );
    }

    #[test]
    fn failed_start_can_retry_and_stops_only_accepted_generation() {
        let state = Rc::new(RefCell::new(State {
            reject_start: true,
            ..State::default()
        }));
        {
            let mut lease = ReceiverLease::new(Mock(state.clone()));
            assert!(lease.start(9).is_err());
            state.borrow_mut().reject_start = false;
            lease.start(9).unwrap();
        }
        assert_eq!(
            state.borrow().calls,
            [(OP_START, 9), (OP_START, 9), (OP_STOP, 9)]
        );
    }

    #[test]
    fn busy_stop_retries_before_releasing_generation() {
        let state = Rc::new(RefCell::new(State {
            busy_stops: 2,
            ..State::default()
        }));
        let mut lease = ReceiverLease::new(Mock(state.clone()));
        lease.start(11).unwrap();
        lease.reset();
        assert!(lease.active.is_none());
        assert_eq!(
            state.borrow().calls,
            [(OP_START, 11), (OP_STOP, 11), (OP_STOP, 11), (OP_STOP, 11)]
        );
    }

    #[test]
    fn failed_stop_blocks_new_generation() {
        let state = Rc::new(RefCell::new(State {
            busy_stops: 20,
            ..State::default()
        }));
        let mut lease = ReceiverLease::new(Mock(state.clone()));
        lease.start(11).unwrap();
        assert_eq!(
            lease.start(12).unwrap_err().kind(),
            io::ErrorKind::WouldBlock
        );
        assert_eq!(lease.active, Some(11));
        assert!(!state.borrow().calls.contains(&(OP_START, 12)));
        state.borrow_mut().busy_stops = 0;
        lease.reset();
        assert!(lease.active.is_none());
    }

    #[test]
    fn loopback_wire_contract_carries_generation_and_requires_ack() {
        let listener = TcpListener::bind((Ipv4Addr::LOCALHOST, 0)).unwrap();
        let address = match listener.local_addr().unwrap() {
            std::net::SocketAddr::V4(address) => address,
            _ => unreachable!(),
        };
        let server = std::thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            let mut request = [0u8; 16];
            stream.read_exact(&mut request).unwrap();
            assert_eq!(&request[..8], b"CPRX\x02\x01\0\0");
            assert_eq!(&request[8..12], &0x1234_5678u32.to_le_bytes());
            assert_eq!(&request[12..14], &5000u16.to_le_bytes());
            assert_eq!(&request[14..], &[0, 0]);
            stream.write_all(b"CPRA\x02\0\0\0").unwrap();
        });
        LoopbackReceiverControl::request_to(address, OP_START, 0x1234_5678, 5000).unwrap();
        server.join().unwrap();
    }

    #[test]
    fn invalid_advertised_port_is_rejected_before_connect() {
        assert!(LoopbackReceiverControl::new(0).is_err());
        assert!(LoopbackReceiverControl::new(65_536).is_err());
    }

    #[test]
    fn identity_response_is_bounded_validated_and_reassembled() {
        for (payload, valid) in [
            (b"\x06\0id-123".to_vec(), true),
            (b"\0\0".to_vec(), false),
            (b"\xfd\0".to_vec(), false),
            (b"\x06\0short".to_vec(), false),
            (b"\x03\0a\0b".to_vec(), false),
            (b"\x01\0\xff".to_vec(), false),
        ] {
            let listener = TcpListener::bind((Ipv4Addr::LOCALHOST, 0)).unwrap();
            let address = match listener.local_addr().unwrap() {
                std::net::SocketAddr::V4(address) => address,
                _ => unreachable!(),
            };
            let server = std::thread::spawn(move || {
                let (mut stream, _) = listener.accept().unwrap();
                let mut request = [0u8; 16];
                stream.read_exact(&mut request).unwrap();
                assert_eq!(&request[..8], b"CPRX\x02\x05\0\0");
                assert_eq!(&request[8..12], &37u32.to_le_bytes());
                for byte in b"CPRA\x02\0\0\0".iter().chain(payload.iter()) {
                    stream.write_all(&[*byte]).unwrap();
                }
            });
            let result = LoopbackReceiverControl::exchange_to(address, OP_IDENTITY, 37, 5000);
            if valid {
                assert_eq!(result.unwrap().as_deref(), Some("id-123"));
            } else {
                assert!(result.is_err());
            }
            server.join().unwrap();
        }
    }

    #[test]
    fn worker_stops_receiver_when_identity_is_missing() {
        struct MissingIdentity(mpsc::Sender<(u8, u32)>);
        impl ReceiverControl for MissingIdentity {
            fn start(&mut self, g: u32) -> io::Result<()> {
                self.0.send((OP_START, g)).unwrap();
                Ok(())
            }
            fn stop(&mut self, g: u32) -> io::Result<()> {
                self.0.send((OP_STOP, g)).unwrap();
                Ok(())
            }
        }
        let (calls, recorded) = mpsc::channel();
        let worker = ReceiverWorker::new(MissingIdentity(calls));
        worker.start(29).unwrap();
        assert_eq!(
            recorded.recv_timeout(Duration::from_secs(1)).unwrap(),
            (OP_START, 29)
        );
        assert_eq!(
            recorded.recv_timeout(Duration::from_secs(1)).unwrap(),
            (OP_STOP, 29)
        );
        drop(worker);
    }

    #[test]
    fn worker_does_not_block_link_and_resets_after_late_start() {
        struct SlowReceiver {
            entered: mpsc::Sender<()>,
            release: mpsc::Receiver<()>,
            calls: mpsc::Sender<(u8, u32)>,
        }
        impl ReceiverControl for SlowReceiver {
            fn start(&mut self, generation: u32) -> io::Result<()> {
                self.entered.send(()).unwrap();
                self.release.recv().unwrap();
                self.calls.send((OP_START, generation)).unwrap();
                Ok(())
            }
            fn stop(&mut self, generation: u32) -> io::Result<()> {
                self.calls.send((OP_STOP, generation)).unwrap();
                Ok(())
            }
        }
        let (entered, observed) = mpsc::channel();
        let (release, held) = mpsc::channel();
        let (calls, recorded) = mpsc::channel();
        let worker = ReceiverWorker::new(SlowReceiver {
            entered,
            release: held,
            calls,
        });
        worker.start(17).unwrap();
        observed.recv_timeout(Duration::from_secs(1)).unwrap();
        assert!(worker.try_result().unwrap().is_none());
        worker.reset().unwrap();
        release.send(()).unwrap();
        drop(worker);
        assert_eq!(recorded.recv().unwrap(), (OP_START, 17));
        assert_eq!(recorded.recv().unwrap(), (OP_STOP, 17));
    }
    #[test]
    fn si_wire_contract_binds_full_epoch_and_session() {
        let listener = TcpListener::bind((Ipv4Addr::LOCALHOST, 0)).unwrap();
        let address = match listener.local_addr().unwrap() {
            std::net::SocketAddr::V4(a) => a,
            _ => unreachable!(),
        };
        let server = std::thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            stream
                .set_read_timeout(Some(Duration::from_secs(2)))
                .unwrap();
            let mut f = [0; 32];
            stream.read_exact(&mut f).unwrap();
            assert_eq!(&f[..8], b"CPRX\x03\x01\0\0");
            assert_eq!(&f[16..24], &0x123456789abcdef0u64.to_le_bytes());
            assert_eq!(&f[24..28], &7u32.to_le_bytes());
            assert_eq!(&f[28..], [0; 4]);
            stream.write_all(b"CPRA\x02\0\0\0").unwrap();
        });
        LoopbackReceiverControl::exchange_bound(
            address,
            OP_START,
            3,
            5000,
            Some((0x123456789abcdef0, 7)),
        )
        .unwrap();
        server.join().unwrap();
    }
}
