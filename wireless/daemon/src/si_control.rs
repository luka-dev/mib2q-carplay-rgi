//! SI-owned receiver admission. SI is authoritative for device/child lifetime;
//! the existing receiver bridge is authoritative for AirPlay readiness and pi.
use crate::receiver_control::{
    LoopbackReceiverControl, PeerIdentity, ReceiverControl, ReceiverPhase,
};
use std::io::{self, Read, Write};
use std::os::unix::net::UnixStream;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

const SIZE: usize = 192;
const HELLO: u8 = 1;
const SNAPSHOT: u8 = 2;
const UPSERT: u8 = 3;
const ACTIVATE: u8 = 4;
const CANCEL: u8 = 5;
const REMOVE: u8 = 6;

#[derive(Debug, Clone, Copy)]
struct Reply {
    epoch: u64,
    result: u32,
    device: u32,
    session: u32,
    app: i32,
    state: i32,
    active_device: u32,
    child: u32,
}
fn word(bytes: &[u8], offset: usize) -> u32 {
    u32::from_le_bytes(bytes[offset..offset + 4].try_into().unwrap())
}
fn put(bytes: &mut [u8], offset: usize, value: u32) {
    bytes[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
}
fn invalid(message: &str) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message)
}
fn field(out: &mut [u8], value: &str) -> io::Result<()> {
    if value.is_empty() || value.len() >= out.len() || value.chars().any(char::is_control) {
        return Err(invalid("invalid SI text field"));
    }
    out[..value.len()].copy_from_slice(value.as_bytes());
    Ok(())
}
fn encode(
    operation: u8,
    request: u32,
    epoch: u64,
    daemon: u32,
    link: u32,
    session: u32,
    peer: &PeerIdentity,
) -> io::Result<[u8; SIZE]> {
    let mut bytes = [0; SIZE];
    bytes[..4].copy_from_slice(b"SIW2");
    bytes[4] = 2;
    bytes[5] = operation;
    put(&mut bytes, 8, SIZE as u32);
    put(&mut bytes, 12, request);
    bytes[16..24].copy_from_slice(&epoch.to_le_bytes());
    put(&mut bytes, 24, daemon);
    put(&mut bytes, 28, link);
    put(&mut bytes, 160, session);
    if !peer
        .key
        .bytes()
        .all(|b| b.is_ascii_alphanumeric() || b":-_".contains(&b))
    {
        return Err(invalid("invalid SI phone key"));
    }
    field(&mut bytes[32..64], &peer.key)?;
    field(&mut bytes[64..160], &peer.name)?;
    Ok(bytes)
}
fn decode(bytes: &[u8; SIZE], operation: u8, request: u32) -> io::Result<Reply> {
    if &bytes[..4] != b"SIR2"
        || bytes[4] != 2
        || bytes[5] != operation
        || bytes[6..8] != [0, 0]
        || word(bytes, 8) != SIZE as u32
        || word(bytes, 12) != request
        || bytes[24..32]
            .iter()
            .chain(bytes[64..].iter())
            .any(|&b| b != 0)
    {
        return Err(invalid("invalid SI response envelope"));
    }
    Ok(Reply {
        epoch: u64::from_le_bytes(bytes[16..24].try_into().unwrap()),
        result: word(bytes, 32),
        device: word(bytes, 36),
        session: word(bytes, 40),
        app: word(bytes, 44) as i32,
        state: word(bytes, 48) as i32,
        active_device: word(bytes, 52),
        child: word(bytes, 56),
    })
}

#[derive(Clone)]
struct Client {
    path: PathBuf,
    daemon: u32,
    serial: Arc<Mutex<u32>>,
}
impl Client {
    fn new(path: PathBuf) -> io::Result<Self> {
        if !path.is_absolute() || path.as_os_str().len() > 100 {
            return Err(invalid(
                "SI endpoint must be an absolute local socket path of at most 100 bytes",
            ));
        }
        let mut random = [0; 4];
        std::fs::File::open("/dev/urandom")
            .map_err(|e| io::Error::new(e.kind(), format!("SI epoch random open: {e}")))?
            .read_exact(&mut random)
            .map_err(|e| io::Error::new(e.kind(), format!("SI epoch random read: {e}")))?;
        let daemon = u32::from_ne_bytes(random);
        if daemon == 0 {
            return Err(invalid("zero SI daemon epoch"));
        }
        Ok(Self {
            path,
            daemon,
            serial: Arc::new(Mutex::new(0)),
        })
    }
    fn call(
        &self,
        operation: u8,
        epoch: u64,
        link: u32,
        session: u32,
        peer: &PeerIdentity,
    ) -> io::Result<Reply> {
        let request = {
            let mut counter = self
                .serial
                .lock()
                .map_err(|_| invalid("SI request ledger poisoned"))?;
            *counter = counter
                .checked_add(1)
                .ok_or_else(|| invalid("SI request sequence exhausted"))?;
            *counter
        };
        let bytes = encode(operation, request, epoch, self.daemon, link, session, peer)?;
        let deadline = Instant::now() + Duration::from_secs(5);
        loop {
            let exchange = (|| -> io::Result<[u8; SIZE]> {
                let mut stream = UnixStream::connect(&self.path)?;
                let remaining = deadline.saturating_duration_since(Instant::now());
                if remaining.is_zero() {
                    return Err(io::Error::new(
                        io::ErrorKind::TimedOut,
                        "SI request deadline",
                    ));
                }
                stream.set_read_timeout(Some(remaining.min(Duration::from_secs(2))))?;
                stream.set_write_timeout(Some(remaining.min(Duration::from_secs(2))))?;
                stream.write_all(&bytes)?;
                let mut response = [0; SIZE];
                stream.read_exact(&mut response)?;
                Ok(response)
            })();
            let response = match exchange {
                Ok(response) => response,
                Err(error)
                    if Instant::now() < deadline
                        && matches!(
                            error.kind(),
                            io::ErrorKind::TimedOut
                                | io::ErrorKind::WouldBlock
                                | io::ErrorKind::UnexpectedEof
                                | io::ErrorKind::ConnectionReset
                                | io::ErrorKind::BrokenPipe
                        ) =>
                {
                    // Unknown outcome: retry the exact request ID/payload.
                    // SI's retained result prevents a second native action.
                    std::thread::sleep(Duration::from_millis(20));
                    continue;
                }
                Err(error) => return Err(error),
            };
            let reply = decode(&response, operation, request)?;
            if reply.epoch == 0 {
                return Err(invalid("empty SI epoch"));
            }
            if operation != HELLO && reply.epoch != epoch {
                return Err(io::Error::new(
                    io::ErrorKind::ConnectionAborted,
                    "SI restarted during request",
                ));
            }
            match reply.result {
                0 => return Ok(reply),
                6 if Instant::now() < deadline => std::thread::sleep(Duration::from_millis(20)),
                3 | 6 => return Err(io::Error::new(io::ErrorKind::WouldBlock, "SI busy")),
                2 => return Err(invalid("stale SI session/link/daemon epoch")),
                _ => {
                    return Err(io::Error::other(format!(
                        "SI operation {operation} rejected: {}",
                        reply.result
                    )));
                }
            }
        }
    }
}
#[derive(Clone)]
struct Binding {
    peer: PeerIdentity,
    epoch: u64,
    link: u32,
    session: u32,
    device: u32,
    child: u32,
    draining: bool,
    ready_at: Instant,
}
fn startup_wait(value:Option<&str>)->io::Result<Duration> {
    let milliseconds=match value {
        None=>12_000,
        Some(v) if !v.is_empty()&&v.bytes().all(|b|b.is_ascii_digit())=>
            v.parse::<u64>().map_err(|_|invalid("invalid SI startup wait"))?,
        _=>return Err(invalid("invalid SI startup wait")),
    };
    if !(1000..=60_000).contains(&milliseconds){return Err(invalid("SI startup wait outside 1000..60000 ms"));}
    Ok(Duration::from_millis(milliseconds))
}
#[derive(Clone)]
pub struct SiReceiverControl {
    client: Client,
    receiver: LoopbackReceiverControl,
    peer: Option<PeerIdentity>,
    binding: Option<Binding>,
    feature_link: Option<u32>,
    startup_wait: Duration,
}
impl SiReceiverControl {
    pub fn new(path: impl AsRef<Path>, airplay_port: u32) -> io::Result<Self> {
        Ok(Self {
            client: Client::new(path.as_ref().into())?,
            receiver: LoopbackReceiverControl::new(airplay_port)?,
            peer: None,
            binding: None,
            feature_link: None,
            startup_wait: startup_wait(std::env::var("MHI2Q_CP_SI_STARTUP_WAIT_MS").ok().as_deref())?,
        })
    }
    fn snapshot(&self, b: &Binding) -> io::Result<Reply> {
        let state = self
            .client
            .call(SNAPSHOT, b.epoch, b.link, b.session, &b.peer)?;
        if state.device != b.device || state.session != b.session {
            return Err(invalid("SI receiver session changed"));
        }
        Ok(state)
    }
    fn begin(&mut self, generation: u32, peer: PeerIdentity) -> io::Result<()> {
        let hello = self.client.call(HELLO, 0, generation, 0, &peer)?;
        let present = self
            .client
            .call(UPSERT, hello.epoch, generation, hello.session, &peer)?;
        let existing = if present.app == 0 && present.active_device == present.device {
            present.session
        } else {
            0
        };
        let started = self
            .client
            .call(ACTIVATE, hello.epoch, generation, existing, &peer)?;
        if started.device == 0 || started.session == 0 {
            return Err(invalid("SI returned an empty admission"));
        }
        let mut binding = Binding {
            peer,
            epoch: hello.epoch,
            link: generation,
            session: started.session,
            device: started.device,
            child: 0,
            draining: false,
            ready_at: Instant::now(),
        };
        self.binding = Some(binding.clone());
        self.feature_link=Some(generation);
        self.receiver.bind_si(binding.epoch, binding.session);
        // SI owns its native startup budget (10 s on HU, 30 s in TCG).
        // Do not cancel a valid initializing child at the old 8 s cutoff.
        let deadline = Instant::now() + self.startup_wait;
        let mut last = None;
        while Instant::now() < deadline {
            let state = self.snapshot(&binding)?;
            if state.active_device != binding.device || state.app != 0 {
                return Err(io::Error::other("SI stopped/preempted wireless activation"));
            }
            if state.child != 0 {
                if binding.child != 0 && binding.child != state.child {
                    return Err(invalid("SI changed receiver child during admission"));
                }
                binding.child = state.child;
                self.receiver.bind_child(state.child);
                self.binding = Some(binding.clone());
                match self.receiver.start(generation) {
                    Ok(()) => {
                        // The phone's pre-media grace starts at receiver
                        // READY, not at SI admission before native startup.
                        self.binding.as_mut().unwrap().ready_at=Instant::now();
                        return Ok(());
                    },
                    Err(error) => last = Some(error),
                }
            }
            std::thread::sleep(Duration::from_millis(100));
        }
        Err(last.unwrap_or_else(|| {
            io::Error::new(io::ErrorKind::TimedOut, "SI receiver startup timed out")
        }))
    }
}
impl ReceiverControl for SiReceiverControl {
    fn semantic(&mut self,generation:u32,kind:u8,data:&[u8])->io::Result<()> {
        let b=self.binding.as_ref().ok_or_else(||invalid("semantic input without admission"))?;
        if generation==0||(kind!=0&&self.feature_link.unwrap_or(b.link)!=generation)||
           b.draining||self.peer.as_ref().is_none_or(|p|p.key!=b.peer.key){return Err(invalid("stale semantic peer/link"));}
        let s=self.snapshot(b)?;
        if s.app!=0||s.active_device!=b.device||s.child!=b.child{return Err(invalid("semantic child changed"));}
        // A fresh authenticated Identify may reattach iAP2 to retained IP
        // media. Never rewrite the immutable native child's SI birth link.
        self.receiver.semantic(b.link,kind,data)?;
        if kind==0 {self.feature_link=Some(generation);}
        Ok(())
    }
    fn semantic_outgoing(&mut self,generation:u32)->io::Result<Option<Vec<u8>>> {
        let b=self.binding.as_ref().ok_or_else(||invalid("semantic output without admission"))?;
        if self.feature_link.unwrap_or(b.link)!=generation||b.draining||
           self.peer.as_ref().is_none_or(|p|p.key!=b.peer.key){return Err(invalid("stale semantic output"));}
        let s=self.snapshot(b)?;
        if s.app!=0||s.active_device!=b.device||s.child!=b.child{return Err(invalid("semantic output child changed"));}
        self.receiver.semantic_outgoing(b.link)
    }
    fn now_playing(&mut self, generation: u32, frame: &[u8]) -> io::Result<()> {
        let binding = self
            .binding
            .as_ref()
            .ok_or_else(|| invalid("metadata without admission"))?;
        if self.feature_link.unwrap_or(binding.link) != generation
            || binding.draining
            || self.peer.as_ref().is_none_or(|p| p.key != binding.peer.key)
        {
            return Err(invalid("metadata from stale peer/generation"));
        }
        let state = self.snapshot(binding)?;
        if state.app != 0 || state.active_device != binding.device || state.child != binding.child {
            return Err(invalid("metadata native receiver changed"));
        }
        self.receiver.now_playing(binding.link, frame)
    }
    fn set_peer(&mut self, peer: Option<PeerIdentity>) -> io::Result<()> {
        if let (Some(identity), Some(binding)) = (&peer, &self.binding) {
            if identity.key == binding.peer.key && identity.name != binding.peer.name {
                let reply = self.client.call(
                    UPSERT,
                    binding.epoch,
                    binding.link,
                    binding.session,
                    identity,
                )?;
                if reply.device != binding.device || reply.session != binding.session {
                    return Err(invalid("SI changed identity during phone rename"));
                }
                self.binding.as_mut().unwrap().peer = identity.clone();
            }
        }
        self.peer = peer;
        Ok(())
    }
    fn start(&mut self, generation: u32) -> io::Result<()> {
        if generation == 0 {
            return Err(invalid("zero RFCOMM generation"));
        }
        let peer = self
            .peer
            .clone()
            .ok_or_else(|| invalid("SI mode requires RFCOMM peer identity"))?;
        if peer.key == "bt:00:00:00:00:00:00" || peer.key == "bt:FF:FF:FF:FF:FF:FF" {
            return Err(invalid("invalid RFCOMM peer identity"));
        }
        let result = self.begin(generation, peer);
        if result.is_err() {
            let _ = self.stop(generation);
        }
        result
    }
    fn stop(&mut self, generation: u32) -> io::Result<()> {
        let Some(binding) = self.binding.clone() else {
            return Ok(());
        };
        if generation != binding.link {
            return Err(invalid("wrong SI receiver generation"));
        }
        let state = self.snapshot(&binding)?;
        if binding.draining {
            if state.app == -1 && state.child == 0 {
                self.binding = None;
                return Ok(());
            }
            return Err(io::Error::new(
                io::ErrorKind::WouldBlock,
                "SI native cleanup pending",
            ));
        }
        // A lost bootstrap link does not terminate established native IP media.
        // SI/DIO retain ownership; 3 is SERVICE_STARTED, 5 is INITIALIZED
        // while its native media notification may still be queued.
        if state.app == 0 && state.active_device == binding.device && matches!(state.state, 3 | 5) {
            match self.receiver.stop(generation) {
                Err(error) if error.kind() == io::ErrorKind::WouldBlock => {
                    // The receiver, rather than SI INITIALIZED alone, confirms
                    // an active native media session that must survive BT loss.
                    // Keep the correlated binding in the daemon-lifetime
                    // worker. Bootstrap ownership ends, while HEALTH/native
                    // cleanup continue even before RFCOMM comes back.
                    // A new link cannot replace this native admission until
                    // SI has reaped it; UPSERT keeps that gate authoritative.
                    return Ok(());
                }
                Err(error) => return Err(error),
                Ok(()) => (),
            }
        }
        self.client.call(
            CANCEL,
            binding.epoch,
            binding.link,
            binding.session,
            &binding.peer,
        )?;
        self.client.call(
            REMOVE,
            binding.epoch,
            binding.link,
            binding.session,
            &binding.peer,
        )?;
        self.binding.as_mut().unwrap().draining = true;
        Err(io::Error::new(
            io::ErrorKind::WouldBlock,
            "SI native cleanup pending",
        ))
    }
    fn pairing_identity(&mut self, generation: u32) -> io::Result<String> {
        let binding = self
            .binding
            .as_ref()
            .ok_or_else(|| invalid("SI receiver is not admitted"))?;
        if binding.link != generation {
            return Err(invalid("wrong receiver generation"));
        }
        let identity = self.receiver.pairing_identity(generation)?;
        let state = self.snapshot(binding)?;
        if state.app != 0 || state.active_device != binding.device || state.child != binding.child {
            return Err(invalid("SI receiver changed before StartSession"));
        }
        Ok(identity)
    }
    fn poll(&mut self) -> io::Result<ReceiverPhase> {
        let Some(binding) = self.binding.clone() else {
            return Ok(ReceiverPhase::Unknown);
        };
        let state = match self.snapshot(&binding) {
            Ok(state) => state,
            Err(error) if error.kind() == io::ErrorKind::ConnectionAborted => {
                let fresh = self.client.call(HELLO, 0, binding.link, 0, &binding.peer)?;
                if fresh.epoch == binding.epoch {
                    return Err(invalid("SI restart epoch did not change"));
                }
                if fresh.app == -1 && fresh.child == 0 {
                    self.binding = None;
                    return Ok(ReceiverPhase::Ended);
                }
                // Fresh SI may already own wired media. Never cancel it with
                // an old process's context, or infer readiness from RPC loss.
                return Ok(ReceiverPhase::Blocked);
            }
            Err(error) => return Err(error),
        };
        // SI clears its retained child only after kill(pid,0) confirms exit.
        // A socket error alone never authorizes a second receiver generation.
        if state.app == -1 && state.child == 0 {
            self.binding = None;
            return Ok(ReceiverPhase::Ended);
        }
        if state.app == -1 {
            return Ok(ReceiverPhase::Draining);
        } // Old child still alive.
        if state.app != 0 || state.active_device != binding.device {
            return Ok(ReceiverPhase::Blocked);
        }
        if binding.draining {
            return Ok(ReceiverPhase::Draining);
        }
        if state.child != binding.child {
            if state.child == 0 {
                return Ok(ReceiverPhase::Draining);
            }
            // Native SI watchdog may replace a crashed DIO while retaining
            // the same device/session/link. Its snapshot is authoritative;
            // a new Start/identity check must still validate the new CPRX
            // endpoint before the phone receives another 4301.
            let current = self.binding.as_mut().unwrap();
            current.child = state.child;
            current.ready_at = Instant::now();
            return Ok(ReceiverPhase::Restarted);
        }
        self.receiver.check_health(binding.link)?;
        let phase = self.receiver.observe(binding.link)?;
        let startup_idle =
            phase == ReceiverPhase::Waiting && binding.ready_at.elapsed() > Duration::from_secs(20);
        if phase == ReceiverPhase::Ended || startup_idle {
            // Serialized idle-stop checks the context again; a newly arriving
            // media SETUP wins over the startup timeout, never forcibly drains.
            {
                match self.receiver.stop(binding.link) {
                    Ok(()) => (),
                    // SETUP won the idle-stop race. The hook's authenticated
                    // pre-RECORD deadline remains authoritative; do not emit
                    // an error or cancel a context merely for existing.
                    Err(error) if error.kind() == io::ErrorKind::WouldBlock => {
                        return Ok(ReceiverPhase::Waiting);
                    }
                    Err(error) => return Err(error),
                }
            }
            self.client.call(
                CANCEL,
                binding.epoch,
                binding.link,
                binding.session,
                &binding.peer,
            )?;
            self.binding.as_mut().unwrap().draining = true;
            return Ok(ReceiverPhase::Draining);
        }
        Ok(phase)
    }
}

#[derive(Clone)]
pub enum ConfiguredReceiverControl {
    Standalone(LoopbackReceiverControl),
    Si(SiReceiverControl),
}
impl ConfiguredReceiverControl {
    pub fn from_env(airplay_port: u32) -> io::Result<Self> {
        match std::env::var("MHI2Q_CP_SI_CONTROL") {
            Ok(path) if !path.is_empty() => {
                Ok(Self::Si(SiReceiverControl::new(path, airplay_port)?))
            }
            Ok(_) => Err(invalid("empty MHI2Q_CP_SI_CONTROL")),
            Err(std::env::VarError::NotPresent) => Ok(Self::Standalone(
                LoopbackReceiverControl::new(airplay_port)?,
            )),
            Err(_) => Err(invalid("invalid MHI2Q_CP_SI_CONTROL")),
        }
    }
}
impl ReceiverControl for ConfiguredReceiverControl {
    fn semantic(&mut self,g:u32,k:u8,d:&[u8])->io::Result<()> {
        match self {Self::Si(c)=>c.semantic(g,k,d),Self::Standalone(c)=>c.semantic(g,k,d)}
    }
    fn semantic_outgoing(&mut self,g:u32)->io::Result<Option<Vec<u8>>> {
        match self {Self::Si(c)=>c.semantic_outgoing(g),Self::Standalone(c)=>c.semantic_outgoing(g)}
    }
    fn now_playing(&mut self, generation: u32, frame: &[u8]) -> io::Result<()> {
        match self {
            Self::Si(control) => control.now_playing(generation, frame),
            Self::Standalone(control) => control.now_playing(generation, frame),
        }
    }
    fn set_peer(&mut self, p: Option<PeerIdentity>) -> io::Result<()> {
        match self {
            Self::Standalone(c) => c.set_peer(p),
            Self::Si(c) => c.set_peer(p),
        }
    }
    fn start(&mut self, g: u32) -> io::Result<()> {
        match self {
            Self::Standalone(c) => c.start(g),
            Self::Si(c) => c.start(g),
        }
    }
    fn stop(&mut self, g: u32) -> io::Result<()> {
        match self {
            Self::Standalone(c) => c.stop(g),
            Self::Si(c) => c.stop(g),
        }
    }
    fn pairing_identity(&mut self, g: u32) -> io::Result<String> {
        match self {
            Self::Standalone(c) => c.pairing_identity(g),
            Self::Si(c) => c.pairing_identity(g),
        }
    }
    fn poll(&mut self) -> io::Result<ReceiverPhase> {
        match self {
            Self::Standalone(c) => c.poll(),
            Self::Si(c) => c.poll(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn premedia_grace_begins_after_receiver_ready_ack() {
        use std::net::{SocketAddr,TcpListener};use std::os::unix::net::UnixListener;
        let path=std::env::temp_dir().join(format!("si-ready-clock-{}.sock",std::process::id()));
        let _=std::fs::remove_file(&path);let unix=UnixListener::bind(&path).unwrap();
        let tcp=TcpListener::bind("127.0.0.1:0").unwrap();let SocketAddr::V4(address)=tcp.local_addr().unwrap()else{unreachable!()};
        let si=std::thread::spawn(move|| {
            for op in [HELLO,UPSERT,ACTIVATE,SNAPSHOT] {
                let (mut stream,_)=unix.accept().unwrap();let mut request=[0;SIZE];stream.read_exact(&mut request).unwrap();assert_eq!(request[5],op);
                let mut reply=[0;SIZE];reply[..4].copy_from_slice(b"SIR2");reply[4]=2;reply[5]=op;
                put(&mut reply,8,SIZE as u32);put(&mut reply,12,word(&request,12));reply[16..24].copy_from_slice(&42u64.to_le_bytes());
                for (o,v) in [(36,101),(40,19),(44,0),(48,5),(52,101),(56,200)]{put(&mut reply,o,v);}
                stream.write_all(&reply).unwrap();
            }
        });
        let media=std::thread::spawn(move|| {
            let (mut stream,_)=tcp.accept().unwrap();let mut request=[0;32];stream.read_exact(&mut request).unwrap();assert_eq!(request[5],1);
            std::thread::sleep(Duration::from_millis(50));let ack=Instant::now();stream.write_all(b"CPRA\x02\0\0\0").unwrap();ack
        });
        let mut c=SiReceiverControl::new(&path,5000).unwrap();c.receiver.test_endpoint(address);
        c.set_peer(Some(PeerIdentity::from_sdk_address([6,5,4,3,2,1]))).unwrap();c.start(8).unwrap();
        assert!(c.binding.as_ref().unwrap().ready_at>=media.join().unwrap());
        si.join().unwrap();std::fs::remove_file(path).unwrap();
    }
    #[test]
    fn startup_wait_is_bounded_and_outlasts_native_si_startup() {
        assert_eq!(startup_wait(None).unwrap(),Duration::from_secs(12));
        assert_eq!(startup_wait(Some("40000")).unwrap(),Duration::from_secs(40));
        for invalid in ["","999","60001","-1000"," 12000","12s","18446744073709551616"] {
            assert!(startup_wait(Some(invalid)).is_err(),"{invalid}");
        }
    }
    #[test]
    fn authenticated_reattach_keeps_native_birth_link_and_rejects_old_feature_link() {
        use std::net::{SocketAddr,TcpListener};use std::os::unix::net::UnixListener;
        let path=std::env::temp_dir().join(format!("si-feature-reattach-{}.sock",std::process::id()));
        let _=std::fs::remove_file(&path);let unix=UnixListener::bind(&path).unwrap();
        let tcp=TcpListener::bind("127.0.0.1:0").unwrap();
        let SocketAddr::V4(address)=tcp.local_addr().unwrap()else{unreachable!()};
        let si=std::thread::spawn(move|| {
            for _ in 0..4 {
                let (mut stream,_)=unix.accept().unwrap();let mut request=[0;SIZE];stream.read_exact(&mut request).unwrap();
                assert_eq!(request[5],SNAPSHOT);assert_eq!(word(&request,28),8);
                let mut reply=[0;SIZE];reply[..6].copy_from_slice(b"SIR2\x02\x02");
                put(&mut reply,8,SIZE as u32);put(&mut reply,12,word(&request,12));reply[16..24].copy_from_slice(&42u64.to_le_bytes());
                for (o,v) in [(36,101),(40,19),(44,0),(48,3),(52,101),(56,200)] {put(&mut reply,o,v);}
                stream.write_all(&reply).unwrap();
            }
        });
        let media=std::thread::spawn(move|| {
            for operation in [9u8,9,10,8] {
                let (mut stream,_)=tcp.accept().unwrap();let mut head=[0;32];stream.read_exact(&mut head).unwrap();
                assert_eq!(head[5],operation);assert_eq!(&head[8..12],&8u32.to_le_bytes());
                if operation==10 {let mut ack=[0;4];stream.read_exact(&mut ack).unwrap();assert_eq!(ack,[0;4]);}
                else {let mut size=[0;2];stream.read_exact(&mut size).unwrap();let mut data=vec![0;u16::from_le_bytes(size)as usize];stream.read_exact(&mut data).unwrap();}
                stream.write_all(b"CPRA\x02\0\0\0").unwrap();
                if operation==10 {stream.write_all(b"\x01\0\0\0\x06\0@@\0\x06\x52\0").unwrap();}
            }
        });
        let peer=PeerIdentity::from_sdk_address([6,5,4,3,2,1]);let mut c=SiReceiverControl::new(&path,5000).unwrap();
        c.receiver.test_endpoint(address);c.receiver.bind_si(42,19);c.peer=Some(peer.clone());
        c.binding=Some(Binding{peer,epoch:42,link:8,session:19,device:101,child:200,draining:false,ready_at:Instant::now()});
        assert!(c.semantic(9,1,b"@@\0\x06\x52\x01").is_err());
        c.semantic(9,0,b"@@\0\x06\x1d\x01").unwrap();
        c.semantic(9,1,b"@@\0\x06\x52\x01").unwrap();
        assert!(c.semantic_outgoing(9).unwrap().is_some());c.now_playing(9,b"@@\0\x06\x50\x01").unwrap();
        assert!(c.semantic(8,1,b"@@\0\x06\x52\x01").is_err());
        assert!(c.now_playing(8,b"@@\0\x06\x50\x01").is_err());
        c.peer=Some(PeerIdentity::from_sdk_address([1,2,3,4,5,6]));assert!(c.semantic(10,0,b"Identify").is_err());
        assert_eq!(c.binding.as_ref().unwrap().link,8);
        si.join().unwrap();media.join().unwrap();std::fs::remove_file(path).unwrap();
    }
    #[test]
    fn native_end_does_not_rearm_until_si_idle_and_old_child_exited() {
        use std::net::{SocketAddr, TcpListener};
        use std::os::unix::net::UnixListener;
        let path = std::env::temp_dir().join(format!("si-media-end-{}.sock", std::process::id()));
        let _ = std::fs::remove_file(&path);
        let unix = UnixListener::bind(&path).unwrap();
        let tcp = TcpListener::bind("127.0.0.1:0").unwrap();
        let SocketAddr::V4(address) = tcp.local_addr().unwrap() else {
            unreachable!()
        };
        let si = std::thread::spawn(move || {
            // Active; context disappeared; cancel accepted; idle with live old
            // child; only then idle with the retained child cleared by SI.
            for (operation, app, child) in [
                (SNAPSHOT, 0, 200),
                (SNAPSHOT, 0, 200),
                (CANCEL, 0, 200),
                (SNAPSHOT, -1, 200),
                (SNAPSHOT, -1, 0),
            ] {
                let (mut stream, _) = unix.accept().unwrap();
                stream
                    .set_read_timeout(Some(Duration::from_secs(2)))
                    .unwrap();
                let mut request = [0; SIZE];
                stream.read_exact(&mut request).unwrap();
                assert_eq!(request[5], operation);
                assert_eq!(word(&request, 160), 19);
                let mut response = [0; SIZE];
                response[..4].copy_from_slice(b"SIR2");
                response[4] = 2;
                response[5] = operation;
                put(&mut response, 8, 192);
                put(&mut response, 12, word(&request, 12));
                response[16..24].copy_from_slice(&42u64.to_le_bytes());
                put(&mut response, 36, 101);
                put(&mut response, 40, 19);
                put(&mut response, 44, app as u32);
                put(&mut response, 48, 5);
                put(&mut response, 52, if app == 0 { 101 } else { 0 });
                put(&mut response, 56, child);
                stream.write_all(&response).unwrap();
            }
        });
        let media = std::thread::spawn(move || {
            for (operation, state) in [(7, 7), (6, 7), (7, 4), (6, 4), (2, 0)] {
                let (mut stream, _) = tcp.accept().unwrap();
                stream
                    .set_read_timeout(Some(Duration::from_secs(2)))
                    .unwrap();
                let mut request = [0; 32];
                stream.read_exact(&mut request).unwrap();
                assert_eq!(&request[..5], b"CPRX\x03");
                assert_eq!(request[5], operation);
                assert_eq!(word(&request, 8), 8);
                assert_eq!(word(&request, 24), 19);
                stream.write_all(b"CPRA\x02\0\0\0").unwrap();
                stream.write_all(&[state, 0, 0, 0]).unwrap();
            }
        });
        let peer = PeerIdentity::from_sdk_address([6, 5, 4, 3, 2, 1]);
        let mut c = SiReceiverControl::new(&path, 5000).unwrap();
        c.receiver.test_endpoint(address);
        c.receiver.bind_si(42, 19);
        c.binding = Some(Binding {
            peer,
            epoch: 42,
            link: 8,
            session: 19,
            device: 101,
            child: 200,
            draining: false,
            ready_at: Instant::now(),
        });
        assert_eq!(c.poll().unwrap(), ReceiverPhase::Streaming);
        assert_eq!(c.poll().unwrap(), ReceiverPhase::Draining);
        assert!(c.binding.is_some());
        assert_eq!(c.poll().unwrap(), ReceiverPhase::Draining);
        assert!(c.binding.is_some());
        assert_eq!(c.poll().unwrap(), ReceiverPhase::Ended);
        assert!(c.binding.is_none());
        si.join().unwrap();
        media.join().unwrap();
        std::fs::remove_file(path).unwrap();
    }
    #[test]
    fn si_restart_waits_for_fresh_idle_without_cancelling_wired_context() {
        use std::os::unix::net::UnixListener;
        let path = std::env::temp_dir().join(format!("si-fresh-epoch-{}.sock", std::process::id()));
        let _ = std::fs::remove_file(&path);
        let listener = UnixListener::bind(&path).unwrap();
        let server = std::thread::spawn(move || {
            for (operation, app, active) in [
                (SNAPSHOT, 0, 1),
                (HELLO, 0, 1),
                (SNAPSHOT, -1, 0),
                (HELLO, -1, 0),
            ] {
                let (mut stream, _) = listener.accept().unwrap();
                stream
                    .set_read_timeout(Some(Duration::from_secs(2)))
                    .unwrap();
                let mut request = [0; SIZE];
                stream.read_exact(&mut request).unwrap();
                assert_eq!(request[5], operation);
                let mut response = [0; SIZE];
                response[..4].copy_from_slice(b"SIR2");
                response[4] = 2;
                response[5] = operation;
                put(&mut response, 8, SIZE as u32);
                put(&mut response, 12, word(&request, 12));
                response[16..24].copy_from_slice(&99u64.to_le_bytes());
                put(&mut response, 44, app as u32);
                put(&mut response, 52, active);
                stream.write_all(&response).unwrap();
            }
        });
        let mut c = SiReceiverControl::new(&path, 5000).unwrap();
        c.binding = Some(Binding {
            peer: PeerIdentity::from_sdk_address([6, 5, 4, 3, 2, 1]),
            epoch: 42,
            link: 8,
            session: 19,
            device: 101,
            child: 200,
            draining: false,
            ready_at: Instant::now(),
        });
        assert_eq!(c.poll().unwrap(), ReceiverPhase::Blocked);
        assert!(c.binding.is_some());
        assert_eq!(c.poll().unwrap(), ReceiverPhase::Ended);
        assert!(c.binding.is_none());
        server.join().unwrap();
        std::fs::remove_file(path).unwrap();
    }
    #[test]
    fn rfcomm_reset_retains_active_binding_and_health_without_detach() {
        use std::net::{SocketAddr, TcpListener};
        use std::os::unix::net::UnixListener;
        let path =
            std::env::temp_dir().join(format!("si-detached-health-{}.sock", std::process::id()));
        let _ = std::fs::remove_file(&path);
        let unix = UnixListener::bind(&path).unwrap();
        let tcp = TcpListener::bind("127.0.0.1:0").unwrap();
        let SocketAddr::V4(address) = tcp.local_addr().unwrap() else {
            unreachable!()
        };
        let si = std::thread::spawn(move || {
            for _ in 0..2 {
                let (mut stream, _) = unix.accept().unwrap();
                stream
                    .set_read_timeout(Some(Duration::from_secs(2)))
                    .unwrap();
                let mut request = [0; SIZE];
                stream.read_exact(&mut request).unwrap();
                assert_eq!(request[5], SNAPSHOT); // No DETACH/CANCEL on lost bootstrap.
                let mut response = [0; SIZE];
                response[..4].copy_from_slice(b"SIR2");
                response[4] = 2;
                response[5] = SNAPSHOT;
                put(&mut response, 8, SIZE as u32);
                put(&mut response, 12, word(&request, 12));
                response[16..24].copy_from_slice(&42u64.to_le_bytes());
                for (offset, value) in [(36, 101), (40, 19), (44, 0), (48, 5), (52, 101), (56, 200)]
                {
                    put(&mut response, offset, value);
                }
                stream.write_all(&response).unwrap();
            }
        });
        let media = std::thread::spawn(move || {
            for (operation, busy) in [(2, true), (7, false), (6, false)] {
                let (mut stream, _) = tcp.accept().unwrap();
                stream
                    .set_read_timeout(Some(Duration::from_secs(2)))
                    .unwrap();
                let mut request = [0; 32];
                stream.read_exact(&mut request).unwrap();
                assert_eq!(request[5], operation);
                stream
                    .write_all(if busy {
                        b"CPRA\x02\x03\0\0"
                    } else {
                        b"CPRA\x02\0\0\0"
                    })
                    .unwrap();
                if !busy {
                    stream.write_all(&[7, 0, 0, 0]).unwrap();
                }
            }
        });
        let mut c = SiReceiverControl::new(&path, 5000).unwrap();
        c.receiver.test_endpoint(address);
        c.receiver.bind_si(42, 19);
        c.binding = Some(Binding {
            peer: PeerIdentity::from_sdk_address([6, 5, 4, 3, 2, 1]),
            epoch: 42,
            link: 8,
            session: 19,
            device: 101,
            child: 200,
            draining: false,
            ready_at: Instant::now(),
        });
        c.stop(8).unwrap();
        assert!(c.binding.is_some());
        c.set_peer(None).unwrap();
        assert_eq!(c.poll().unwrap(), ReceiverPhase::Streaming);
        assert_eq!(c.binding.as_ref().unwrap().session, 19);
        si.join().unwrap();
        media.join().unwrap();
        std::fs::remove_file(path).unwrap();
    }
    #[test]
    fn live_phone_rename_only_upserts_the_same_transport_and_session() {
        use std::os::unix::net::UnixListener;
        let path = std::env::temp_dir().join(format!("si-rename-{}.sock", std::process::id()));
        let _ = std::fs::remove_file(&path);
        let listener = UnixListener::bind(&path).unwrap();
        let mut control = SiReceiverControl::new(&path, 5000).unwrap();
        let mut peer = PeerIdentity::from_sdk_address([1, 2, 3, 4, 5, 6]);
        control.peer = Some(peer.clone());
        control.binding = Some(Binding {
            peer: peer.clone(),
            epoch: 42,
            link: 8,
            session: 19,
            device: 101,
            child: 200,
            draining: false,
            ready_at: Instant::now(),
        });
        let server = std::thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            stream
                .set_read_timeout(Some(Duration::from_secs(2)))
                .unwrap();
            let mut request = [0; SIZE];
            stream.read_exact(&mut request).unwrap();
            assert_eq!(request[5], UPSERT);
            assert_eq!(&request[32..53], b"bt:01:02:03:04:05:06\0");
            assert_eq!(&request[64..78], b"Actual iPhone\0");
            assert_eq!(word(&request, 28), 8);
            assert_eq!(word(&request, 160), 19);
            let mut response = [0; SIZE];
            response[..4].copy_from_slice(b"SIR2");
            response[4] = 2;
            response[5] = UPSERT;
            put(&mut response, 8, 192);
            put(&mut response, 12, word(&request, 12));
            response[16..24].copy_from_slice(&42u64.to_le_bytes());
            put(&mut response, 36, 101);
            put(&mut response, 40, 19);
            stream.write_all(&response).unwrap();
            listener.set_nonblocking(true).unwrap();
            listener
        });
        peer.name = "Actual iPhone".into();
        control.set_peer(Some(peer.clone())).unwrap();
        let listener = server.join().unwrap();
        assert_eq!(control.binding.as_ref().unwrap().peer.name, peer.name);
        assert_eq!(control.binding.as_ref().unwrap().child, 200);
        control.set_peer(Some(peer)).unwrap(); // Duplicate name does not republish.
        control
            .set_peer(Some(PeerIdentity::from_sdk_address([6, 5, 4, 3, 2, 1])))
            .unwrap();
        assert_eq!(control.binding.as_ref().unwrap().device, 101);
        assert_eq!(
            listener.accept().unwrap_err().kind(),
            io::ErrorKind::WouldBlock
        );
        std::fs::remove_file(path).unwrap();
    }
    #[test]
    fn wire_matches_c_layout_and_rejects_wrong_envelopes() {
        let p = PeerIdentity::from_sdk_address([1, 2, 3, 4, 5, 6]);
        let f = encode(UPSERT, 9, 0x1122334455667788, 7, 3, 0, &p).unwrap();
        assert_eq!(&f[16..24], &0x1122334455667788u64.to_le_bytes());
        assert_eq!(&f[32..53], b"bt:01:02:03:04:05:06\0");
        let mut r = [0; SIZE];
        r[..4].copy_from_slice(b"SIR2");
        r[4] = 2;
        r[5] = UPSERT;
        put(&mut r, 8, 192);
        put(&mut r, 12, 9);
        put(&mut r, 36, 100);
        assert_eq!(decode(&r, UPSERT, 9).unwrap().device, 100);
        r[190] = 1;
        assert!(decode(&r, UPSERT, 9).is_err());
        r[190] = 0;
        assert!(decode(&r, ACTIVATE, 9).is_err());
    }
    #[test]
    fn missing_peer_never_contacts_or_starts_si() {
        let mut c = SiReceiverControl::new("/tmp/nonexistent-si-test", 5000).unwrap();
        assert_eq!(c.start(1).unwrap_err().kind(), io::ErrorKind::InvalidData);
        assert!(c.binding.is_none());
    }
    #[test]
    fn lost_ack_and_pending_retry_exact_request_then_reject_restart() {
        use std::os::unix::net::UnixListener;
        let path = std::env::temp_dir().join(format!("si-rpc-{}.sock", std::process::id()));
        let _ = std::fs::remove_file(&path);
        let listener = UnixListener::bind(&path).unwrap();
        let client = Client::new(path.clone()).unwrap();
        let server = std::thread::spawn(move || {
            let mut original = None;
            for attempt in 0..4 {
                let (mut stream, _) = listener.accept().unwrap();
                stream
                    .set_read_timeout(Some(Duration::from_secs(2)))
                    .unwrap();
                let mut f = [0; SIZE];
                stream.read_exact(&mut f).unwrap();
                if attempt < 3 {
                    if let Some(old) = original {
                        assert_eq!(f, old);
                    } else {
                        original = Some(f);
                    }
                }
                if attempt == 0 {
                    continue;
                } // mutation may have applied; reply lost
                let mut response = [0; SIZE];
                response[..4].copy_from_slice(b"SIR2");
                response[4] = 2;
                response[5] = f[5];
                put(&mut response, 8, 192);
                put(&mut response, 12, word(&f, 12));
                response[16..24]
                    .copy_from_slice(&(if attempt == 3 { 99u64 } else { 42u64 }).to_le_bytes());
                put(&mut response, 32, if attempt == 1 { 6 } else { 0 });
                stream.write_all(&response).unwrap();
            }
        });
        let peer = PeerIdentity::from_sdk_address([1, 2, 3, 4, 5, 6]);
        assert_eq!(client.call(UPSERT, 42, 3, 0, &peer).unwrap().epoch, 42);
        assert_eq!(
            client.call(SNAPSHOT, 42, 3, 0, &peer).unwrap_err().kind(),
            io::ErrorKind::ConnectionAborted
        );
        server.join().unwrap();
        std::fs::remove_file(path).unwrap();
    }
}
