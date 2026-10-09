//! Public, bounded test vectors only. No private keys or production loader.
use std::io::Read;
use std::path::Path;

pub const MAGIC: &[u8; 8] = b"MFIQ\x01\0\0\0";
pub const MAX_CERT: usize = 1280;
pub const MAX_CASES: usize = 8;
pub const MAX_FILE: usize = 12 + MAX_CERT + MAX_CASES * 148;
const SUBJECT: &[u8] = b"NOT_A_REAL_MFI_IDENTITY";

#[derive(Clone, Debug)]
pub struct Case {
    pub digest: [u8; 20],
    pub signature: [u8; 128],
}
#[derive(Clone, Debug)]
pub struct Fixture {
    pub certificate: Vec<u8>,
    pub cases: Vec<Case>,
}
impl Fixture {
    pub fn decode(bytes: &[u8]) -> Result<Self, String> {
        if bytes.len() < 12 || bytes.len() > MAX_FILE || &bytes[..8] != MAGIC {
            return Err("invalid synthetic MFi fixture header".into());
        }
        let cert_len = u16::from_be_bytes([bytes[8], bytes[9]]) as usize;
        let count = u16::from_be_bytes([bytes[10], bytes[11]]) as usize;
        if !(1..=MAX_CERT).contains(&cert_len)
            || !(1..=MAX_CASES).contains(&count)
            || bytes.len() != 12 + cert_len + count * 148
        {
            return Err("invalid synthetic MFi fixture bounds".into());
        }
        let certificate = bytes[12..12 + cert_len].to_vec();
        if !certificate.windows(SUBJECT.len()).any(|w| w == SUBJECT) {
            return Err("fixture certificate must have the synthetic subject".into());
        }
        let mut cases: Vec<Case> = Vec::with_capacity(count);
        for part in bytes[12 + cert_len..].chunks(148) {
            let digest: [u8; 20] = part[..20].try_into().unwrap();
            if cases.iter().any(|c| c.digest == digest) {
                return Err("duplicate synthetic MFi challenge".into());
            }
            cases.push(Case {
                digest,
                signature: part[20..].try_into().unwrap(),
            });
        }
        Ok(Self { certificate, cases })
    }
    pub fn read(path: &Path) -> Result<Self, String> {
        let file = std::fs::File::open(path).map_err(|_| "cannot open synthetic MFi fixture")?;
        let mut bytes = Vec::new();
        file.take((MAX_FILE + 1) as u64)
            .read_to_end(&mut bytes)
            .map_err(|_| "cannot read synthetic MFi fixture")?;
        Self::decode(&bytes)
    }
    #[allow(dead_code)] // Generator and codec tests use this, the QNX peer only decodes.
    pub fn encode(&self) -> Result<Vec<u8>, String> {
        if self.certificate.len() > MAX_CERT || self.cases.len() > MAX_CASES {
            return Err("synthetic MFi fixture too large".into());
        }
        let mut bytes = MAGIC.to_vec();
        bytes.extend_from_slice(&(self.certificate.len() as u16).to_be_bytes());
        bytes.extend_from_slice(&(self.cases.len() as u16).to_be_bytes());
        bytes.extend_from_slice(&self.certificate);
        for case in &self.cases {
            bytes.extend_from_slice(&case.digest);
            bytes.extend_from_slice(&case.signature);
        }
        Self::decode(&bytes)?;
        Ok(bytes)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn reject_truncation_trailing_bounds_duplicate_and_unmarked_certificate() {
        let mut fixture = Fixture {
            certificate: SUBJECT.to_vec(),
            cases: vec![
                Case {
                    digest: [1; 20],
                    signature: [2; 128],
                },
                Case {
                    digest: [3; 20],
                    signature: [4; 128],
                },
            ],
        };
        let bytes = fixture.encode().unwrap();
        assert_eq!(Fixture::decode(&bytes).unwrap().cases[1].digest, [3; 20]);
        for n in 0..bytes.len() {
            assert!(Fixture::decode(&bytes[..n]).is_err());
        }
        let mut trailing = bytes.clone();
        trailing.push(0);
        assert!(Fixture::decode(&trailing).is_err());
        for offset in [0, 4, 8, 9, 10, 11] {
            let mut bad = bytes.clone();
            bad[offset] ^= 0xff;
            assert!(Fixture::decode(&bad).is_err());
        }
        fixture.cases[1].digest = fixture.cases[0].digest;
        assert!(fixture.encode().is_err());
        fixture.cases.pop();
        fixture.certificate = b"production identity".to_vec();
        assert!(fixture.encode().is_err());
    }
}
