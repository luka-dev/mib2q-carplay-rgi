//! Disposable QNX fixture: SI/DIO replies are modeled; production daemon,
//! iAP2 timers, receiver worker, DNS-SD resolver and TCP connector are real.
use super::*;
use iap2_csm::messages::car_play::{WirelessCarPlayStatus, WirelessCarPlayUpdate};
use std::os::unix::net::UnixListener;
use std::sync::{Arc, Mutex};

#[derive(Default)]
struct State {
    session: u32,
    child: u32,
    active: bool,
    flags: u8,
    activates: u32,
    starts: u32,
    cancel_at: Option<Instant>,
    failed: bool,
}
pub struct Fixture {
    state: Arc<Mutex<State>>,
    first: Option<Instant>,
    stage: u8,
    replies: u8,
}
fn word(b: &[u8], at: usize) -> u32 {
    u32::from_le_bytes(b[at..at + 4].try_into().unwrap())
}
fn put(b: &mut [u8], at: usize, v: u32) {
    b[at..at + 4].copy_from_slice(&v.to_le_bytes());
}
impl Fixture {
    pub fn new() -> io::Result<Self> {
        let path = "/dev/shmem/recovery-si.sock";
        let _ = std::fs::remove_file(path);
        let si = UnixListener::bind(path)?;
        si.set_nonblocking(true)?;
        let media = TcpListener::bind((Ipv4Addr::LOCALHOST, 57343))?;
        media.set_nonblocking(true)?;
        let control = TcpListener::bind((Ipv4Addr::LOCALHOST, 7001))?;
        control.set_nonblocking(true)?;
        let state = Arc::new(Mutex::new(State::default()));
        let shared = state.clone();
        thread::spawn(move || {
            let end = Instant::now() + Duration::from_secs(60);
            while Instant::now() < end {
                let (mut stream, _) = match si.accept() {
                    Ok(s) => s,
                    Err(_) => {
                        thread::sleep(Duration::from_millis(10));
                        continue;
                    }
                };
                let _ = stream.set_nonblocking(false);
                let _ = stream.set_read_timeout(Some(Duration::from_secs(2)));
                let mut request = [0; 192];
                if stream.read_exact(&mut request).is_err() {
                    continue;
                }
                let op = request[5];
                let mut s = shared.lock().unwrap();
                if s.cancel_at
                    .is_some_and(|t| t.elapsed() >= Duration::from_millis(800))
                {
                    s.child = 0;
                    s.cancel_at = None;
                }
                let mut result = 0;
                if op == 4 {
                    if s.child != 0 {
                        s.failed = true;
                        result = 3;
                    } else {
                        s.activates += 1;
                        s.session += 1;
                        s.child = 20000 + s.session;
                        s.active = true;
                        s.flags = 0;
                    }
                }
                if op == 5 {
                    s.active = false;
                    s.cancel_at = Some(Instant::now());
                }
                let mut response = [0; 192];
                response[..4].copy_from_slice(b"SIR2");
                response[4] = 2;
                response[5] = op;
                put(&mut response, 8, 192);
                put(&mut response, 12, word(&request, 12));
                response[16..24].copy_from_slice(&42u64.to_le_bytes());
                put(&mut response, 32, result);
                put(&mut response, 36, 101);
                put(&mut response, 40, s.session);
                put(&mut response, 44, if s.active { 0 } else { u32::MAX });
                put(&mut response, 48, 5);
                put(&mut response, 52, if s.active { 101 } else { 0 });
                put(&mut response, 56, s.child);
                let _ = stream.write_all(&response);
            }
        });
        let shared = state.clone();
        thread::spawn(move || {
            let end = Instant::now() + Duration::from_secs(60);
            while Instant::now() < end {
                let (mut stream, _) = match media.accept() {
                    Ok(s) => s,
                    Err(_) => {
                        thread::sleep(Duration::from_millis(10));
                        continue;
                    }
                };
                let _ = stream.set_nonblocking(false);
                let _ = stream.set_read_timeout(Some(Duration::from_secs(2)));
                let mut request = [0; 32];
                if stream.read_exact(&mut request).is_err() {
                    continue;
                }
                let mut s = shared.lock().unwrap();
                let op = request[5];
                let valid = &request[..5] == b"CPRX\x03"
                    && word(&request, 8) == 1
                    && word(&request, 24) == s.session
                    && u64::from_le_bytes(request[16..24].try_into().unwrap()) == 42;
                let status = if !valid {
                    2
                } else if op == 2 && s.flags == 7 {
                    3
                } else {
                    0
                };
                let _ = stream.write_all(&[b'C', b'P', b'R', b'A', 2, status, 0, 0]);
                if status != 0 {
                    continue;
                }
                if op == 1 {
                    s.starts += 1;
                    println!("[phone-qemu] receiver START=1");
                }
                if op == 5 {
                    let id = b"12345678-1234-4234-8234-123456789ABC";
                    let _ = stream.write_all(&(id.len() as u16).to_le_bytes());
                    let _ = stream.write_all(id);
                }
                if op == 6 || op == 7 {
                    let _ = stream.write_all(&[s.flags, 0, 0, 0]);
                }
            }
        });
        thread::spawn(move || {
            let end = Instant::now() + Duration::from_secs(60);
            while Instant::now() < end {
                let (mut stream, _) = match control.accept() {
                    Ok(s) => s,
                    Err(_) => {
                        thread::sleep(Duration::from_millis(10));
                        continue;
                    }
                };
                let _ = stream.set_nonblocking(false);
                let _ = stream.set_read_timeout(Some(Duration::from_secs(2)));
                let mut bytes = [0; 512];
                let mut request = Vec::new();
                while request.len() < 4096 && !request.ends_with(b"\r\n\r\n") {
                    match stream.read(&mut bytes) {
                        Ok(0) | Err(_) => break,
                        Ok(n) => request.extend_from_slice(&bytes[..n]),
                    }
                }
                if request.is_empty() {
                    println!("[recovery-qemu] same-phone TCP reachability probe");
                    continue;
                }
                if request.starts_with(b"GET /ctrl-int/1/connect HTTP/1.1\r\n") {
                    let _ = stream.write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
                    println!("[phone-qemu] CarPlay Control connect=1");
                }
            }
        });
        Ok(Self {
            state,
            first: None,
            stage: 0,
            replies: 0,
        })
    }
    pub fn started(&mut self) -> bool {
        self.replies += 1;
        self.state.lock().unwrap().flags = 7;
        if self.replies == 1 {
            self.first = Some(Instant::now());
            false
        } else {
            true
        }
    }
    pub fn tick(&mut self, engine: &mut LinkEngine, now: u64) -> io::Result<()> {
        let Some(first) = self.first else {
            return Ok(());
        };
        let elapsed = first.elapsed();
        if self.stage == 0 && elapsed >= Duration::from_secs(2) {
            engine.send(
                CONTROL_SESSION_ID,
                CarPlayAvailability {
                    wired_attributes: None,
                    wireless_attributes: Some(CarPlayAvailabilityWirelessAttributes {
                        available: Some(true),
                        bluetooth_transport_identifier: None,
                    }),
                }
                .encode(),
                now,
            );
            self.stage = 1;
        }
        if self.stage == 1 && elapsed >= Duration::from_secs(4) {
            let mut s = self.state.lock().unwrap();
            if s.activates != 1 || s.starts != 1 {
                return Err(io::Error::other("duplicate activation while streaming"));
            }
            s.flags = 4;
            drop(s);
            engine.send(
                CONTROL_SESSION_ID,
                WirelessCarPlayUpdate {
                    status: WirelessCarPlayStatus::Unavailable,
                }
                .encode(),
                now,
            );
            println!("[recovery-qemu] Wi-Fi unavailable; same RFCOMM generation retained");
            self.stage = 2;
        }
        if self.stage == 2 && elapsed >= Duration::from_secs(8) {
            let s = self.state.lock().unwrap();
            if s.activates != 1 || s.child != 0 {
                return Err(io::Error::other(
                    "rearmed early or failed old-child cleanup",
                ));
            }
            drop(s);
            engine.send(
                CONTROL_SESSION_ID,
                WirelessCarPlayUpdate {
                    status: WirelessCarPlayStatus::Available,
                }
                .encode(),
                now,
            );
            println!("[recovery-qemu] Wi-Fi available; no new 4300, pairing or BT connect");
            self.stage = 3;
        }
        Ok(())
    }
    pub fn verify(&self) -> io::Result<()> {
        let s = self.state.lock().unwrap();
        if s.failed || s.activates != 2 || s.starts != 2 || self.replies != 2 || self.stage != 3 {
            return Err(io::Error::other("same-link recovery accounting failed"));
        }
        println!(
            "[recovery-qemu] two sessions on RFCOMM generation=1; duplicate suppressed; cleanup before rearm PASS"
        );
        Ok(())
    }
}
