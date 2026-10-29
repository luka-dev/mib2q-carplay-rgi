//! Opt-in owned-QEMU adapter to the host's public P-256 test fixture.
//! No key loading, hardware selection or production fallback.
use crate::control::AuthProvider;
use std::io::{Read, Write};
use std::net::{SocketAddr, TcpStream};
use std::time::Duration;

pub struct NativeApiMfi;
impl NativeApiMfi {
    fn exchange(operation: u8, digest: &[u8]) -> Result<Vec<u8>, String> {
        let address: SocketAddr = "10.0.3.2:32502".parse().unwrap();
        let timeout = Duration::from_secs(3);
        let mut stream =
            TcpStream::connect_timeout(&address, timeout).map_err(|e| e.to_string())?;
        stream
            .set_read_timeout(Some(timeout))
            .map_err(|e| e.to_string())?;
        stream
            .set_write_timeout(Some(timeout))
            .map_err(|e| e.to_string())?;
        let mut request = [0u8; 33];
        request[0] = operation;
        request[1..].copy_from_slice(digest);
        stream.write_all(&request).map_err(|e| e.to_string())?;
        let mut length = [0u8; 4];
        stream.read_exact(&mut length).map_err(|e| e.to_string())?;
        let length = u32::from_be_bytes(length) as usize;
        if (operation == 2 && length != 64) || length == 0 || length > 4096 {
            return Err("invalid native API lab MFi response length".into());
        }
        let mut response = vec![0; length];
        stream
            .read_exact(&mut response)
            .map_err(|e| e.to_string())?;
        eprintln!("[native-api-lab] MFi operation={operation} response_bytes={length}");
        Ok(response)
    }
}
impl AuthProvider for NativeApiMfi {
    fn certificate(&mut self) -> Result<Vec<u8>, String> {
        Self::exchange(3, &[0; 32])
    }
    fn sign(&mut self, challenge: &[u8]) -> Result<Vec<u8>, String> {
        if challenge.len() != 32 {
            return Err("native API lab requires a 32-byte challenge".into());
        }
        Self::exchange(2, challenge)
    }
}
