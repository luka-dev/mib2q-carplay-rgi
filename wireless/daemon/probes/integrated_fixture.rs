//! Real SI/DIO fixture. Only the phone/CarPlay-Control service is implemented
//! here: never bind SIW2 or CPRX, never issue ACTIVATE/CANCEL/HEALTH.
use super::*;
use iap2_csm::messages::car_play::{WirelessCarPlayStatus, WirelessCarPlayUpdate};
use std::process::Command;
use std::sync::{Arc, Mutex};

pub fn cycle_count() -> usize {
    std::env::var("MHI2Q_INTEGRATED_CYCLES")
        .ok()
        .and_then(|v| v.parse().ok())
        .filter(|v| (2..=20).contains(v))
        .unwrap_or(2)
}
pub fn deadline_seconds() -> u64 {
    120 + 60 * cycle_count() as u64
}
#[derive(Default)]
struct State {
    children: Vec<u32>,
    sessions: Vec<u32>,
    epochs: Vec<String>,
    completed: usize,
    error: Option<String>,
}
pub struct Fixture {
    state: Arc<Mutex<State>>,
    starts: usize,
    silent: bool,
    restore: Option<Instant>,
    duplicate: bool,
}
impl Fixture {
    pub fn new() -> io::Result<Self> {
        super::features_fixture::bus()?;
        let control = TcpListener::bind((Ipv4Addr::LOCALHOST, 7001))?;
        control.set_nonblocking(true)?;
        let state = Arc::new(Mutex::new(State::default()));
        let shared = state.clone();
        thread::spawn(move || {
            let result = (|| -> io::Result<()> {
                let end = Instant::now() + Duration::from_secs(deadline_seconds());
                while Instant::now() < end {
                    let (mut stream, _) = match control.accept() {
                        Ok(v) => v,
                        Err(e) if e.kind() == io::ErrorKind::WouldBlock => {
                            thread::sleep(Duration::from_millis(20));
                            continue;
                        }
                        Err(e) => return Err(e),
                    };
                    stream.set_nonblocking(false)?;
                    stream.set_read_timeout(Some(Duration::from_secs(2)))?;
                    let mut request = Vec::new();
                    let mut bytes = [0; 512];
                    while request.len() < 4096 && !request.ends_with(b"\r\n\r\n") {
                        match stream.read(&mut bytes) {
                            Ok(0) => break,
                            Ok(n) => request.extend_from_slice(&bytes[..n]),
                            Err(e) => return Err(e),
                        }
                    }
                    if request.is_empty() {
                        println!("[integrated-media] real same-phone reachability probe");
                        continue;
                    }
                    let text = String::from_utf8_lossy(&request);
                    if !text.starts_with("GET /ctrl-int/1/connect HTTP/1.1\r\n")
                        || !text.contains("AirPlay-Receiver-Device-ID: 2199023255553\r\n")
                    {
                        return Err(io::Error::other("unexpected CarPlay Control request"));
                    }
                    let context = std::fs::read_to_string("/dev/shmem/integrated-context")?;
                    let fields: Vec<_> = context.split_whitespace().collect();
                    if fields.len() != 5 {
                        return Err(io::Error::other("invalid observed native child context"));
                    }
                    let session = fields[1].parse::<u32>().map_err(io::Error::other)?;
                    let child = fields[3].parse::<u32>().map_err(io::Error::other)?;
                    {
                        let mut s = shared.lock().unwrap();
                        let fault = std::env::var("MHI2Q_INTEGRATED_FAULT").unwrap_or_default();
                        let replacement = (fault == "dio-crash"
                            || (fault == "si-crash"
                                && s.epochs.last().is_some_and(|epoch| epoch != fields[0])))
                            && s.completed == 1
                            && s.sessions.last() == Some(&session);
                        if s.children.contains(&child)
                            || (s.sessions.contains(&session) && !replacement)
                            || s.completed != s.children.len()
                        {
                            return Err(io::Error::other("overlapping or reused native receiver"));
                        }
                        s.children.push(child);
                        s.sessions.push(session);
                        s.epochs.push(fields[0].to_owned());
                    }
                    let observation = Command::new("/proc/boot/integrated-observer").status()?;
                    if !observation.success() {
                        return Err(io::Error::other("native observer rejected admission"));
                    }
                    stream.write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n")?;
                    drop(stream);
                    let _ = std::fs::remove_file("/dev/shmem/integrated-silent");
                    let status = Command::new("/proc/boot/independent-health-phone")
                        .env("MHI2Q_PEER_HEALTH_EPOCH", fields[0])
                        .env("MHI2Q_PEER_HEALTH_SESSION", fields[1])
                        .env("MHI2Q_PEER_HEALTH_CHILD", fields[3])
                        .env("MHI2Q_PEER_HEALTH_LINK", fields[2])
                        .env("MHI2Q_PEER_HEALTH_PARENT", fields[4])
                        .status()?;
                    if !status.success() {
                        return Err(io::Error::other(format!(
                            "independent media phone {status}"
                        )));
                    }
                    let proof = std::fs::read_to_string(format!(
                        "/mnt/persist/integrated-audio-{session}"
                    ))?;
                    let counts: Vec<u64> = proof
                        .split_whitespace()
                        .map(|v| v.parse::<u64>())
                        .collect::<Result<_, _>>()
                        .map_err(io::Error::other)?;
                    let expected = if std::env::var("MHI2Q_INTEGRATED_FAULT").as_deref()
                        == Ok("partial-record")
                        && session == 1
                    {
                        0
                    } else if std::env::var("MHI2Q_INTEGRATED_FAULT").as_deref() == Ok("microphone")
                        && session == 1
                    {
                        12800
                    } else {
                        56320
                    };
                    if counts.len() != 3 || counts[1] != expected || counts[2] != 0 {
                        return Err(io::Error::other(format!(
                            "native PCM oracle rejected {proof:?}, expected {expected}"
                        )));
                    }
                    println!(
                        "[integrated-media] native PCM auth and playback oracle session={session} samples={} bad=0 PASS",
                        counts[1]
                    );
                    let restart_expected = matches!(
                        std::env::var("MHI2Q_INTEGRATED_FAULT").as_deref(),
                        Ok("dio-crash") | Ok("si-crash")
                    ) && shared.lock().unwrap().completed == 0;
                    let cleanup_deadline = Instant::now() + Duration::from_secs(20);
                    while !restart_expected {
                        if Command::new("/proc/boot/integrated-observer")
                            .arg("--idle")
                            .status()?
                            .success()
                        {
                            break;
                        }
                        if Instant::now() >= cleanup_deadline {
                            return Err(io::Error::other("native cleanup observer failed"));
                        }
                        thread::sleep(Duration::from_millis(200));
                    }
                    let mut s = shared.lock().unwrap();
                    s.completed += 1;
                    println!(
                        "[integrated-media] actual SI/DIO media cycle={} session={session} child={child} PASS",
                        s.completed
                    );
                    if s.completed == cycle_count() {
                        return Ok(());
                    }
                }
                Err(io::Error::other("native media fixture deadline"))
            })();
            if let Err(error) = result {
                shared.lock().unwrap().error = Some(error.to_string());
            }
        });
        Ok(Self {
            state,
            starts: 0,
            silent: false,
            restore: None,
            duplicate: false,
        })
    }
    pub fn started(&mut self) {
        self.starts += 1;
    }
    pub fn tick(&mut self, engine: &mut LinkEngine, now: u64) -> io::Result<bool> {
        let s = self.state.lock().unwrap();
        if let Some(e) = &s.error {
            return Err(io::Error::other(e.clone()));
        }
        let completed = s.completed;
        drop(s);
        if completed == cycle_count() {
            if self.starts != cycle_count() {
                return Err(io::Error::other("wrong number of real 4301 replies"));
            }
            println!("[integrated-media] complete native media lifecycle PASS");
            return Ok(true);
        }
        if self.starts > 0 && !self.duplicate {
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
            self.duplicate = true;
        }
        if !self.silent && std::path::Path::new("/dev/shmem/integrated-silent").exists() {
            engine.send(
                CONTROL_SESSION_ID,
                WirelessCarPlayUpdate {
                    status: WirelessCarPlayStatus::Unavailable,
                }
                .encode(),
                now,
            );
            self.silent = true;
        }
        if completed == 1 && self.restore.is_none() {
            self.restore = Some(Instant::now());
        }
        if self
            .restore
            .is_some_and(|t| t.elapsed() > Duration::from_secs(1))
            && self.starts == 1
        {
            engine.send(
                CONTROL_SESSION_ID,
                WirelessCarPlayUpdate {
                    status: WirelessCarPlayStatus::Available,
                }
                .encode(),
                now,
            );
            self.restore = None;
        }
        Ok(false)
    }
}
