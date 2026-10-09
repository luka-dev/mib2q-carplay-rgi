//! QEMU test peer verifies actual AA03 using factory QNX libcrypto.
use crate::software_mfi_fixture::Fixture;
use std::io;
use std::path::Path;

pub struct SoftwarePhoneAuth {
    fixture: Fixture,
    next: usize,
    certificate_accepted: bool,
}
impl SoftwarePhoneAuth {
    pub fn from_environment() -> io::Result<Option<Self>> {
        let Some(path) = std::env::var_os("MHI2Q_MFI_QEMU_FIXTURE") else {
            return Ok(None);
        };
        let fixture = Fixture::read(Path::new(&path)).map_err(io::Error::other)?;
        Ok(Some(Self {
            fixture,
            next: 0,
            certificate_accepted: false,
        }))
    }
    pub fn challenge(&self) -> Vec<u8> {
        self.fixture.cases[self.next].digest.to_vec()
    }
    pub fn accept_certificate(&mut self, actual: &[u8]) -> io::Result<()> {
        if actual != self.fixture.certificate {
            return Err(io::Error::other("wrong synthetic certificate in AA01"));
        }
        let first = &self.fixture.cases[0];
        if !verify(actual, &first.digest, &first.signature)? {
            return Err(io::Error::other(
                "QNX verifier failed its certificate/key positive control",
            ));
        }
        self.certificate_accepted = true;
        println!("[phone-qemu] software certificate verified=1");
        Ok(())
    }
    /// None means a rejected signature; Some(true) requests another challenge,
    /// Some(false) means every actual response was cryptographically verified.
    pub fn verify_response(&mut self, signature: &[u8]) -> io::Result<Option<bool>> {
        if !self.certificate_accepted {
            return Err(io::Error::other("signature before certificate"));
        }
        let digest = self.fixture.cases[self.next].digest;
        let certificate = &self.fixture.certificate;
        if !verify(certificate, &digest, signature)? {
            if !verify(
                certificate,
                &digest,
                &self.fixture.cases[self.next].signature,
            )? {
                return Err(io::Error::other(
                    "QNX RSA verifier failed its positive control",
                ));
            }
            return Ok(None);
        }
        let mut bad_digest = digest;
        bad_digest[0] ^= 1;
        let mut bad_signature = signature.to_vec();
        bad_signature[0] ^= 1;
        if verify(certificate, &bad_digest, signature)?
            || verify(certificate, &digest, &bad_signature)?
        {
            return Err(io::Error::other(
                "QNX RSA verifier admitted tampered test input",
            ));
        }
        println!(
            "[phone-qemu] software RSA signature verified case={} bytes=128",
            self.next
        );
        println!("[phone-qemu] software tamper checks rejected=1");
        self.next += 1;
        Ok(Some(self.next < self.fixture.cases.len()))
    }
}

fn verify(certificate: &[u8], digest: &[u8], signature: &[u8]) -> io::Result<bool> {
    if digest.len() != 20 || signature.len() != 128 {
        return Ok(false);
    }
    #[cfg(target_os = "nto")]
    {
        unsafe extern "C" {
            fn mfiq_verify(
                cert: *const u8,
                cert_len: u32,
                digest: *const u8,
                digest_len: u32,
                signature: *const u8,
                signature_len: u32,
            ) -> i32;
        }
        // SAFETY: slices remain live during this call. The C adapter bounds all
        // lengths, parses the entire DER certificate and holds no input pointers.
        match unsafe {
            mfiq_verify(
                certificate.as_ptr(),
                certificate.len() as u32,
                digest.as_ptr(),
                digest.len() as u32,
                signature.as_ptr(),
                signature.len() as u32,
            )
        } {
            1 => Ok(true),
            0 => Ok(false),
            _ => Err(io::Error::other(
                "factory QNX libcrypto verifier unavailable or invalid certificate",
            )),
        }
    }
    #[cfg(not(target_os = "nto"))]
    {
        // Host builds compile the probe, but the native adapter is QNX-only.
        let _ = certificate;
        Err(io::Error::other(
            "software MFi phone verification requires the QNX test adapter",
        ))
    }
}
