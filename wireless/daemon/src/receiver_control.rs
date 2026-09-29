//! Local contract with the in-process dio receiver bridge.
//!
//! START is acknowledged only after the stock AirPlay receiver is ready on
//! the same port sent in CarPlayStartSession. The bridge checks that port and
//! fails closed when the receiver or its QNX lifecycle is not ready.

use std::io::{self, Read, Write};
use std::net::{Ipv4Addr, SocketAddrV4, TcpStream};
use std::sync::mpsc::{self, Receiver, Sender, TryRecvError};
use std::thread::{self, JoinHandle};
use std::time::Duration;

const ADDRESS: SocketAddrV4 = SocketAddrV4::new(Ipv4Addr::LOCALHOST, 57343);
const VERSION: u8 = 2;
const OP_START: u8 = 1;
const OP_STOP: u8 = 2;
const OP_IDENTITY: u8 = 5;

pub trait ReceiverControl {
    fn start(&mut self, generation: u32) -> io::Result<()>;
    fn stop(&mut self, generation: u32) -> io::Result<()>;
    fn pairing_identity(&mut self, _generation: u32) -> io::Result<String> {
        Err(io::Error::new(io::ErrorKind::Unsupported, "receiver has no pairing identity contract"))
    }
}

#[derive(Clone, Copy)]
pub struct LoopbackReceiverControl {
    airplay_port: u16,
}

impl LoopbackReceiverControl {
    pub fn new(airplay_port: u32) -> io::Result<Self> {
        let airplay_port = u16::try_from(airplay_port)
            .ok()
            .filter(|port| *port != 0)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "invalid AirPlay port"))?;
        Ok(Self { airplay_port })
    }

    fn request(&self, operation: u8, generation: u32) -> io::Result<()> {
        Self::request_to(ADDRESS, operation, generation, self.airplay_port)
    }

    fn request_to(
        address: SocketAddrV4,
        operation: u8,
        generation: u32,
        airplay_port: u16,
    ) -> io::Result<()> {
        Self::exchange_to(address, operation, generation, airplay_port).map(|_| ())
    }

    fn exchange_to(
        address: SocketAddrV4, operation: u8, generation: u32, airplay_port: u16,
    ) -> io::Result<Option<String>> {
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
        request[4] = VERSION;
        request[5] = operation;
        request[8..12].copy_from_slice(&generation.to_le_bytes());
        request[12..14].copy_from_slice(&airplay_port.to_le_bytes());
        stream.write_all(&request)?;
        let mut response = [0u8; 8];
        stream.read_exact(&mut response)?;
        if response[..4] != *b"CPRA" || response[4] != VERSION || response[6..8] != [0, 0] {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "invalid receiver bridge response",
            ));
        }
        if response[5] != 0 {
            let kind = if response[5] == 3 {
                io::ErrorKind::WouldBlock
            } else {
                io::ErrorKind::Other
            };
            return Err(io::Error::new(kind, format!(
                "receiver bridge rejected operation {operation}: {}", response[5]
            )));
        }
        if operation == OP_IDENTITY {
            let mut size = [0u8; 2];
            stream.read_exact(&mut size)?;
            let size = u16::from_le_bytes(size) as usize;
            if size == 0 || size > 252 {
                return Err(io::Error::new(io::ErrorKind::InvalidData, "invalid receiver pairing ID length"));
            }
            let mut bytes = vec![0; size];
            stream.read_exact(&mut bytes)?;
            let id = String::from_utf8(bytes).map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;
            crate::control::validate_pairing_id(&id).map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;
            return Ok(Some(id));
        }
        Ok(None)
    }
}

impl ReceiverControl for LoopbackReceiverControl {
    fn start(&mut self, generation: u32) -> io::Result<()> {
        self.request(OP_START, generation)
    }

    fn stop(&mut self, generation: u32) -> io::Result<()> {
        self.request(OP_STOP, generation)
    }
    fn pairing_identity(&mut self, generation: u32) -> io::Result<String> {
        Self::exchange_to(ADDRESS, OP_IDENTITY, generation, self.airplay_port)?
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
            return Err(io::Error::new(io::ErrorKind::WouldBlock,
                                      "previous receiver generation did not stop"));
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
}

/// Keeps a slow QNX receiver START/STOP out of the iAP2 link timer loop.
/// Commands remain ordered, so a reset after an in-flight START releases the
/// generation even if the acknowledgement arrives after the RFCOMM disconnect.
pub struct ReceiverWorker {
    commands: Sender<Command>,
    results: Receiver<(u32, io::Result<String>)>,
    thread: Option<JoinHandle<()>>,
}

impl ReceiverWorker {
    pub fn new<R: ReceiverControl + Send + 'static>(control: R) -> Self {
        let (commands, requests) = mpsc::channel();
        let (results, replies) = mpsc::channel();
        let thread = thread::spawn(move || {
            let mut lease = ReceiverLease::new(control);
            while let Ok(command) = requests.recv() {
                match command {
                    Command::Start(generation) => {
                        let result = lease.start(generation).and_then(|()| {
                            let identity = lease.control.pairing_identity(generation);
                            if identity.is_err() { lease.reset(); }
                            identity
                        });
                        let _ = results.send((generation, result));
                    }
                    Command::Reset => lease.reset(),
                }
            }
        });
        Self {
            commands,
            results: replies,
            thread: Some(thread),
        }
    }

    pub fn start(&self, generation: u32) -> io::Result<()> {
        self.commands
            .send(Command::Start(generation))
            .map_err(|_| io::Error::new(io::ErrorKind::BrokenPipe, "receiver worker stopped"))
    }

    pub fn reset(&self) -> io::Result<()> {
        self.commands
            .send(Command::Reset)
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
    use super::*;
    use std::cell::RefCell;
    use std::net::TcpListener;
    use std::rc::Rc;
    use std::sync::mpsc;

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
        assert_eq!(state.borrow().calls,
                   [(OP_START, 11), (OP_STOP, 11), (OP_STOP, 11), (OP_STOP, 11)]);
    }

    #[test]
    fn failed_stop_blocks_new_generation() {
        let state = Rc::new(RefCell::new(State {
            busy_stops: 20,
            ..State::default()
        }));
        let mut lease = ReceiverLease::new(Mock(state.clone()));
        lease.start(11).unwrap();
        assert_eq!(lease.start(12).unwrap_err().kind(), io::ErrorKind::WouldBlock);
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
            if valid { assert_eq!(result.unwrap().as_deref(), Some("id-123")); }
            else { assert!(result.is_err()); }
            server.join().unwrap();
        }
    }

    #[test]
    fn worker_stops_receiver_when_identity_is_missing() {
        struct MissingIdentity(mpsc::Sender<(u8, u32)>);
        impl ReceiverControl for MissingIdentity {
            fn start(&mut self, g: u32) -> io::Result<()> {
                self.0.send((OP_START, g)).unwrap(); Ok(())
            }
            fn stop(&mut self, g: u32) -> io::Result<()> {
                self.0.send((OP_STOP, g)).unwrap(); Ok(())
            }
        }
        let (calls, recorded) = mpsc::channel();
        let worker = ReceiverWorker::new(MissingIdentity(calls));
        worker.start(29).unwrap();
        assert_eq!(recorded.recv_timeout(Duration::from_secs(1)).unwrap(), (OP_START, 29));
        assert_eq!(recorded.recv_timeout(Duration::from_secs(1)).unwrap(), (OP_STOP, 29));
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
}
