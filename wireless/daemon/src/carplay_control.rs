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

#[cfg(target_os = "nto")]
pub fn start_background(
    config: crate::control::Config,
) -> std::sync::Arc<std::sync::atomic::AtomicBool> {
    use std::sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    };
    let active = Arc::new(AtomicBool::new(true));
    let worker_active = Arc::clone(&active);
    std::thread::spawn(move || {
        let device_id = match crate::wifi_ie::parse_mac(&config.device_id) {
            Ok(mac) => receiver_device_id(mac),
            Err(error) => {
                eprintln!("[carplay-control] invalid device ID: {error}");
                return;
            }
        };
        while worker_active.load(Ordering::Acquire) {
            match qnx_dns_sd::discover_on("uap0", Duration::from_secs(15)) {
                Ok(controller) if worker_active.load(Ordering::Acquire) => {
                    match connect(&controller, device_id, &config.source_version) {
                        Ok(status) => {
                            eprintln!("[carplay-control] connect accepted: HTTP {status}");
                            return;
                        }
                        Err(error) => eprintln!("[carplay-control] connect failed: {error}"),
                    }
                }
                Ok(_) => return,
                Err(error) => eprintln!("[carplay-control] discovery: {error}"),
            }
            if worker_active.load(Ordering::Acquire) {
                std::thread::sleep(Duration::from_secs(2));
            }
        }
    });
    active
}

#[cfg(target_os = "nto")]
#[derive(Default)]
pub struct ConnectorSlot {
    active: Option<std::sync::Arc<std::sync::atomic::AtomicBool>>,
}

#[cfg(target_os = "nto")]
impl ConnectorSlot {
    pub fn start(&mut self, config: crate::control::Config) {
        if self.active.is_none() {
            self.active = Some(start_background(config));
        }
    }

    pub fn reset(&mut self) {
        if let Some(active) = self.active.take() {
            active.store(false, std::sync::atomic::Ordering::Release);
        }
    }
}

#[cfg(target_os = "nto")]
impl Drop for ConnectorSlot {
    fn drop(&mut self) {
        self.reset();
    }
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
