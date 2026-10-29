//! Accessory-to-phone CarPlay Control request after DNS-SD discovery.

use std::io::{self, Read, Write};
use std::net::{SocketAddr, TcpStream};
use std::time::Duration;

#[cfg(target_os = "nto")]
pub mod qnx_dns_sd;

const CONNECT_PATH: &str = "/ctrl-int/1/connect";
const MAX_RESPONSE_HEADER: usize = 8192;

#[derive(Debug, Clone)]
pub struct Controller {
    pub address: SocketAddr,
    pub host_name: String,
}

pub fn receiver_device_id(mac: [u8; 6]) -> u64 {
    u64::from_be_bytes([0, 0, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]])
}

/// The authenticated RFCOMM peer is mandatory: never select another phone
/// merely because its Bonjour response arrived first.
pub fn txt_matches_peer(txt: &[u8], peer: &str) -> bool {
    let Some(mut address) = peer
        .strip_prefix("bt:")
        .and_then(|s| crate::wifi_ie::parse_mac(s).ok())
    else {
        return false;
    };
    // Pinned btstack helper 0x157A8C formats SDK bytes [5]..[0]. Keep the
    // established SI key unchanged; reverse only for conventional TXT MAC.
    address.reverse();
    let mut remaining = txt;
    let mut id = None;
    while !remaining.is_empty() {
        let size = remaining[0] as usize;
        remaining = &remaining[1..];
        if size == 0 || size > remaining.len() {
            return false;
        }
        let field = &remaining[..size];
        remaining = &remaining[size..];
        if field.len() >= 3 && field[..3].eq_ignore_ascii_case(b"id=") {
            if id.is_some() {
                return false;
            }
            id = std::str::from_utf8(&field[3..])
                .ok()
                .and_then(|s| crate::wifi_ie::parse_mac(s).ok());
            if id.is_none() {
                return false;
            }
        }
    }
    id == Some(address)
}

#[cfg(target_os = "nto")]
#[derive(Clone, Copy, PartialEq, Eq)]
enum TaskMode {
    Connect,
    Probe,
}
#[cfg(target_os = "nto")]
#[derive(Clone)]
struct TaskSpec {
    config: crate::control::Config,
    peer: String,
    generation: u32,
    mode: TaskMode,
}
#[cfg(target_os = "nto")]
struct Task {
    spec: TaskSpec,
    active: std::sync::Arc<std::sync::atomic::AtomicBool>,
    success: std::sync::Arc<std::sync::atomic::AtomicBool>,
    handle: std::thread::JoinHandle<()>,
}
#[cfg(target_os = "nto")]
#[derive(Default)]
pub struct ConnectorSlot {
    task: Option<Task>,
    pending: Option<TaskSpec>,
}

#[cfg(target_os = "nto")]
impl ConnectorSlot {
    fn request(
        &mut self,
        config: crate::control::Config,
        peer: String,
        generation: u32,
        mode: TaskMode,
    ) {
        if self.task.as_ref().is_some_and(|t| {
            t.spec.generation == generation
                && t.spec.peer == peer
                && t.spec.mode == mode
                && t.active.load(std::sync::atomic::Ordering::Acquire)
        }) {
            return;
        }
        self.reset();
        self.pending = Some(TaskSpec {
            config,
            peer,
            generation,
            mode,
        });
    }
    pub fn start(&mut self, config: crate::control::Config, peer: String, generation: u32) {
        self.request(config, peer, generation, TaskMode::Connect);
    }
    pub fn probe(&mut self, config: crate::control::Config, peer: String, generation: u32) {
        self.request(config, peer, generation, TaskMode::Probe);
    }
    pub fn reset(&mut self) {
        self.pending = None;
        if let Some(task) = &self.task {
            task.active
                .store(false, std::sync::atomic::Ordering::Release);
        }
    }
    /// Retire cancelled work before launching its successor. Never join an
    /// in-flight DNS/TCP operation on the iAP2 timer thread.
    pub fn tick(&mut self) -> Option<u32> {
        use std::sync::{
            Arc,
            atomic::{AtomicBool, Ordering},
        };
        let mut reachable = None;
        if self.task.as_ref().is_some_and(|t| t.handle.is_finished()) {
            let task = self.task.take().unwrap();
            if task.spec.mode == TaskMode::Probe
                && task.active.load(Ordering::Acquire)
                && task.success.load(Ordering::Acquire)
            {
                reachable = Some(task.spec.generation);
            }
            let _ = task.handle.join();
        }
        if self.task.is_none() {
            if let Some(spec) = self.pending.take() {
                let active = Arc::new(AtomicBool::new(true));
                let permission = active.clone();
                let success = Arc::new(AtomicBool::new(false));
                let complete = success.clone();
                let worker = spec.clone();
                let handle = std::thread::spawn(move || {
                    let device_id = match crate::wifi_ie::parse_mac(&worker.config.device_id) {
                        Ok(mac) => receiver_device_id(mac),
                        Err(_) => return,
                    };
                    let interface = if cfg!(feature = "native-api-lab") {
                        "ecm0"
                    } else {
                        "uap0"
                    };
                    while permission.load(Ordering::Acquire) {
                        match qnx_dns_sd::discover_for(
                            interface,
                            &worker.peer,
                            Duration::from_secs(15),
                            &permission,
                        ) {
                            Ok(controller) if permission.load(Ordering::Acquire) => {
                                let result = if worker.mode == TaskMode::Probe {
                                    TcpStream::connect_timeout(
                                        &controller.address,
                                        Duration::from_secs(2),
                                    )
                                    .map(|_| 200)
                                } else {
                                    connect_cancellable(
                                        &controller,
                                        device_id,
                                        &worker.config.source_version,
                                        &permission,
                                    )
                                };
                                match result {
                                    Ok(status) if permission.load(Ordering::Acquire) => {
                                        if worker.mode == TaskMode::Probe {
                                            eprintln!(
                                                "[carplay-control] same-peer reachability accepted"
                                            );
                                        } else {
                                            eprintln!(
                                                "[carplay-control] connect accepted: HTTP {status} (same RFCOMM peer)"
                                            );
                                        }
                                        complete.store(true, Ordering::Release);
                                        return;
                                    }
                                    Err(error) => eprintln!("[carplay-control] attempt: {error}"),
                                    _ => return,
                                }
                            }
                            Err(error) if permission.load(Ordering::Acquire) => {
                                eprintln!("[carplay-control] discovery: {error}")
                            }
                            _ => return,
                        }
                        for _ in 0..20 {
                            if !permission.load(Ordering::Acquire) {
                                return;
                            }
                            std::thread::sleep(Duration::from_millis(100));
                        }
                    }
                });
                self.task = Some(Task {
                    spec,
                    active,
                    success,
                    handle,
                });
            }
        }
        reachable
    }
}
#[cfg(target_os = "nto")]
impl Drop for ConnectorSlot {
    fn drop(&mut self) {
        self.reset();
    }
}

#[cfg(target_os = "nto")]
fn connect_cancellable(
    controller: &Controller,
    id: u64,
    version: &str,
    permission: &std::sync::atomic::AtomicBool,
) -> io::Result<u16> {
    use std::sync::atomic::Ordering;
    let mut stream = TcpStream::connect_timeout(&controller.address, Duration::from_secs(2))?;
    stream.set_read_timeout(Some(Duration::from_secs(2)))?;
    stream.set_write_timeout(Some(Duration::from_secs(2)))?;
    if !permission.load(Ordering::Acquire) {
        return Err(io::Error::new(
            io::ErrorKind::Interrupted,
            "cancelled CarPlay connect",
        ));
    }
    send_connect(&mut stream, &controller.host_name, id, version)
}

fn valid_header_value(value: &str) -> bool {
    !value.is_empty() && value.bytes().all(|byte| (0x20..=0x7e).contains(&byte))
}

pub fn connect(controller: &Controller, receiver_id: u64, source_version: &str) -> io::Result<u16> {
    if !valid_header_value(&controller.host_name) || !valid_header_value(source_version) {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "invalid HTTP header value",
        ));
    }
    let mut stream = TcpStream::connect_timeout(&controller.address, Duration::from_secs(5))?;
    stream.set_read_timeout(Some(Duration::from_secs(5)))?;
    stream.set_write_timeout(Some(Duration::from_secs(5)))?;
    send_connect(
        &mut stream,
        &controller.host_name,
        receiver_id,
        source_version,
    )
}

pub fn send_connect<S: Read + Write>(
    stream: &mut S,
    host_name: &str,
    receiver_id: u64,
    source_version: &str,
) -> io::Result<u16> {
    if !valid_header_value(host_name) || !valid_header_value(source_version) {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "invalid HTTP header value",
        ));
    }
    let request = format!(
        "GET {CONNECT_PATH} HTTP/1.1\r\nHost: {host_name}\r\nUser-Agent: AirPlay/{source_version}\r\nAirPlay-Receiver-Device-ID: {receiver_id}\r\nConnection: close\r\n\r\n"
    );
    stream.write_all(request.as_bytes())?;
    let mut response = Vec::new();
    let mut buffer = [0u8; 1024];
    loop {
        let count = stream.read(&mut buffer)?;
        if count == 0 {
            return Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "no HTTP response header",
            ));
        }
        response.extend_from_slice(&buffer[..count]);
        if response.windows(4).any(|part| part == b"\r\n\r\n") {
            break;
        }
        if response.len() > MAX_RESPONSE_HEADER {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "HTTP header too large",
            ));
        }
    }
    if response.len() > MAX_RESPONSE_HEADER {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "HTTP header too large",
        ));
    }
    let line_end = response
        .windows(2)
        .position(|part| part == b"\r\n")
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "missing HTTP status line"))?;
    let status_line = std::str::from_utf8(&response[..line_end])
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "invalid HTTP status line"))?;
    let mut parts = status_line.split_whitespace();
    let version = parts.next().unwrap_or_default();
    if !matches!(version, "HTTP/1.0" | "HTTP/1.1") {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "invalid HTTP version",
        ));
    }
    let status = parts
        .next()
        .and_then(|value| value.parse::<u16>().ok())
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "invalid HTTP status"))?;
    if !(200..300).contains(&status) {
        return Err(io::Error::other(format!("CarPlay Control HTTP {status}")));
    }
    Ok(status)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::TcpListener;

    #[test]
    fn discovery_correlates_sdk_order_and_rejects_ambiguous_or_foreign_txt() {
        let text = b"id=01:02:03:04:05:06";
        let mut txt = vec![text.len() as u8];
        txt.extend_from_slice(text);
        assert!(txt_matches_peer(&txt, "bt:06:05:04:03:02:01"));
        assert!(!txt_matches_peer(&txt, "bt:01:02:03:04:05:06"));
        assert!(!txt_matches_peer(&txt, "bt:00:00:00:00:00:00"));
        assert!(!txt_matches_peer(&[], "bt:06:05:04:03:02:01"));
        for n in 1..txt.len() {
            assert!(!txt_matches_peer(&txt[..n], "bt:06:05:04:03:02:01"));
        }
        txt.extend_from_slice(&txt.clone());
        assert!(!txt_matches_peer(&txt, "bt:06:05:04:03:02:01"));
    }

    #[test]
    fn sends_control_connect_to_discovered_phone() {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        let server = std::thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            let mut request = Vec::new();
            let mut chunk = [0u8; 256];
            while !request.ends_with(b"\r\n\r\n") {
                let count = stream.read(&mut chunk).unwrap();
                assert_ne!(count, 0);
                request.extend_from_slice(&chunk[..count]);
            }
            let request = String::from_utf8(request).unwrap();
            assert!(request.starts_with("GET /ctrl-int/1/connect HTTP/1.1\r\n"));
            assert!(request.contains("Host: iphone.local.\r\n"));
            assert!(request.contains("AirPlay-Receiver-Device-ID: 73588229205\r\n"));
            stream
                .write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n")
                .unwrap();
        });
        let controller = Controller {
            address,
            host_name: "iphone.local.".into(),
        };
        let id = receiver_device_id([0x00, 0x11, 0x22, 0x33, 0x44, 0x55]);
        assert_eq!(connect(&controller, id, "280.33.8").unwrap(), 200);
        server.join().unwrap();
    }

    #[test]
    fn rejects_non_success_and_header_injection() {
        struct Duplex {
            response: std::io::Cursor<Vec<u8>>,
            request: Vec<u8>,
        }
        impl Read for Duplex {
            fn read(&mut self, output: &mut [u8]) -> io::Result<usize> {
                self.response.read(output)
            }
        }
        impl Write for Duplex {
            fn write(&mut self, input: &[u8]) -> io::Result<usize> {
                self.request.write(input)
            }
            fn flush(&mut self) -> io::Result<()> {
                Ok(())
            }
        }
        let mut socket = Duplex {
            response: std::io::Cursor::new(b"HTTP/1.1 403 Forbidden\r\n\r\n".to_vec()),
            request: Vec::new(),
        };
        assert!(send_connect(&mut socket, "phone.local.", 1, "280.33.8").is_err());
        assert!(socket.request.starts_with(b"GET /ctrl-int/1/connect"));
        assert!(send_connect(&mut socket, "phone\r\nEvil: 1", 1, "280.33.8").is_err());
    }
}
