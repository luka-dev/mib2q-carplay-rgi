//! MFi authentication coprocessor register protocol. The QNX transport is
//! isolated in `qnx_i2c` so the register sequence can be checked offline.

use crate::control::AuthProvider;

#[cfg(target_os = "nto")]
pub mod qnx;

const REG_STATUS: u8 = 0x10;
const REG_SIGNATURE_LENGTH: u8 = 0x11;
const REG_SIGNATURE_DATA: u8 = 0x12;
const REG_CHALLENGE_LENGTH: u8 = 0x20;
const REG_CERT_LENGTH: u8 = 0x30;
const REG_CERT_DATA: u8 = 0x31;
const STATUS_ERROR: u8 = 0x80;
const SIGN_COMMAND: u8 = 0x01;
const DIGEST_SIZE: usize = 20;
const BLOCK_SIZE: usize = 128;
const MAX_CERTIFICATE: usize = 1280;
const MAX_SIGNATURE: usize = 128;

pub trait RegisterBus {
    fn lock(&mut self) -> Result<(), String>;
    fn unlock(&mut self) -> Result<(), String>;
    fn read(&mut self, register: u8, output: &mut [u8]) -> Result<(), String>;
    fn write(&mut self, register: u8, data: &[u8]) -> Result<(), String>;
}

pub struct MfiChip<B: RegisterBus> {
    bus: B,
    certificate_cache: Option<Vec<u8>>,
}

impl<B: RegisterBus> MfiChip<B> {
    pub fn new(bus: B) -> Self {
        Self {
            bus,
            certificate_cache: None,
        }
    }

    fn locked<T>(
        &mut self,
        operation: impl FnOnce(&mut B) -> Result<T, String>,
    ) -> Result<T, String> {
        self.bus.lock()?;
        let result = operation(&mut self.bus);
        let unlock = self.bus.unlock();
        match (result, unlock) {
            (Err(error), _) => Err(error),
            (Ok(_), Err(error)) => Err(error),
            (Ok(value), Ok(())) => Ok(value),
        }
    }

    pub fn certificate(&mut self) -> Result<Vec<u8>, String> {
        if let Some(cached) = &self.certificate_cache {
            return Ok(cached.clone());
        }
        let certificate = self.locked(|bus| {
            let mut length_bytes = [0; 2];
            bus.read(REG_CERT_LENGTH, &mut length_bytes)?;
            let length = u16::from_be_bytes(length_bytes) as usize;
            if !(1..=MAX_CERTIFICATE).contains(&length) {
                return Err(format!("invalid MFi certificate length {length}"));
            }
            read_blocks(bus, REG_CERT_DATA, length)
        })?;
        self.certificate_cache = Some(certificate.clone());
        Ok(certificate)
    }

    pub fn sign(&mut self, challenge: &[u8]) -> Result<Vec<u8>, String> {
        if challenge.len() != DIGEST_SIZE {
            return Err(format!("invalid MFi challenge length {}", challenge.len()));
        }
        self.locked(|bus| {
            let mut input = Vec::with_capacity(challenge.len() + 2);
            input.extend_from_slice(&(challenge.len() as u16).to_be_bytes());
            input.extend_from_slice(challenge);
            bus.write(REG_CHALLENGE_LENGTH, &input)?;
            bus.write(REG_STATUS, &[SIGN_COMMAND])?;
            let mut status = [0];
            bus.read(REG_STATUS, &mut status)?;
            if status[0] & STATUS_ERROR != 0 {
                return Err(format!("MFi chip reported status 0x{:02x}", status[0]));
            }
            let mut length_bytes = [0; 2];
            bus.read(REG_SIGNATURE_LENGTH, &mut length_bytes)?;
            let length = u16::from_be_bytes(length_bytes) as usize;
            if !(1..=MAX_SIGNATURE).contains(&length) {
                return Err(format!("invalid MFi signature length {length}"));
            }
            read_blocks(bus, REG_SIGNATURE_DATA, length)
        })
    }
}

fn read_blocks<B: RegisterBus>(
    bus: &mut B,
    first_register: u8,
    length: usize,
) -> Result<Vec<u8>, String> {
    let mut output = vec![0; length];
    for (index, block) in output.chunks_mut(BLOCK_SIZE).enumerate() {
        let register = first_register
            .checked_add(index as u8)
            .ok_or("MFi register overflow")?;
        bus.read(register, block)?;
    }
    Ok(output)
}

impl<B: RegisterBus> AuthProvider for MfiChip<B> {
    fn certificate(&mut self) -> Result<Vec<u8>, String> {
        Self::certificate(self)
    }
    fn sign(&mut self, challenge: &[u8]) -> Result<Vec<u8>, String> {
        Self::sign(self, challenge)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Bus {
        calls: Vec<(u8, Vec<u8>)>,
        locked: bool,
    }
    impl RegisterBus for Bus {
        fn lock(&mut self) -> Result<(), String> {
            assert!(!self.locked);
            self.locked = true;
            Ok(())
        }
        fn unlock(&mut self) -> Result<(), String> {
            assert!(self.locked);
            self.locked = false;
            Ok(())
        }
        fn read(&mut self, register: u8, output: &mut [u8]) -> Result<(), String> {
            assert!(self.locked);
            self.calls.push((register, vec![]));
            match register {
                REG_CERT_LENGTH => output.copy_from_slice(&[0, 129]),
                REG_CERT_DATA => output.fill(0xa1),
                0x32 => output.fill(0xa2),
                REG_STATUS => output.copy_from_slice(&[0]),
                REG_SIGNATURE_LENGTH => output.copy_from_slice(&[0, 3]),
                REG_SIGNATURE_DATA => output.copy_from_slice(&[1, 2, 3]),
                _ => return Err(format!("unexpected register {register:02x}")),
            }
            Ok(())
        }
        fn write(&mut self, register: u8, data: &[u8]) -> Result<(), String> {
            assert!(self.locked);
            self.calls.push((register, data.to_vec()));
            Ok(())
        }
    }

    #[test]
    fn certificate_blocks_cache_and_sign_sequence() {
        let mut chip = MfiChip::new(Bus::default());
        let cert = chip.certificate().unwrap();
        assert_eq!(cert.len(), 129);
        assert_eq!(cert[0], 0xa1);
        assert_eq!(cert[128], 0xa2);
        assert_eq!(chip.certificate().unwrap(), cert);
        let digest = [0x12; DIGEST_SIZE];
        assert_eq!(chip.sign(&digest).unwrap(), [1, 2, 3]);
        assert_eq!(
            chip.bus.calls,
            vec![
                (REG_CERT_LENGTH, vec![]),
                (REG_CERT_DATA, vec![]),
                (0x32, vec![]),
                (
                    REG_CHALLENGE_LENGTH,
                    [&[0, DIGEST_SIZE as u8][..], &digest].concat()
                ),
                (REG_STATUS, vec![SIGN_COMMAND]),
                (REG_STATUS, vec![]),
                (REG_SIGNATURE_LENGTH, vec![]),
                (REG_SIGNATURE_DATA, vec![]),
            ]
        );
        assert!(!chip.bus.locked);
        assert!(chip.sign(&[0x12, 0x34]).is_err());
    }
}
