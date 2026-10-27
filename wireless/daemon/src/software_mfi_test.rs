//! Offline software authentication, generated synthetic identities only.
//!
//! Not a phone-trusted MFi credential issuer. No external key loader, runtime
//! selector or QNX build support. OpenSSL is a host test tool, not a daemon
//! dependency; production retains its real I2C/stock signers.

use crate::control::AuthProvider;
use std::ffi::{OsStr, OsString};
use std::fs::{self, DirBuilder, OpenOptions};
use std::io::{Read, Write};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt, PermissionsExt};
use std::path::PathBuf;
use std::process::{Command, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{Duration, Instant};

const MAX_OUTPUT: usize = 16 * 1024;
const PROCESS_LIMIT: Duration = Duration::from_secs(5);
const TEST_SUBJECT: &str = "/O=MHI2Q_TEST_ONLY/CN=NOT_A_REAL_MFI_IDENTITY";
static NEXT_DIRECTORY: AtomicU64 = AtomicU64::new(1);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TestProfile {
    LegacyRsaSha1,
    V3P256Sha256,
}
impl TestProfile {
    pub fn digest_len(self) -> usize {
        match self {
            Self::LegacyRsaSha1 => 20,
            Self::V3P256Sha256 => 32,
        }
    }
    pub fn signature_len(self) -> usize {
        match self {
            Self::LegacyRsaSha1 => 128,
            Self::V3P256Sha256 => 64,
        }
    }
    fn digest_option(self) -> &'static str {
        match self {
            Self::LegacyRsaSha1 => "digest:sha1",
            Self::V3P256Sha256 => "digest:sha256",
        }
    }
}

struct Scratch(PathBuf);
impl Scratch {
    fn create() -> Result<Self, String> {
        for _ in 0..32 {
            let id = NEXT_DIRECTORY.fetch_add(1, Ordering::Relaxed);
            let path = std::env::temp_dir()
                .join(format!("mhi2q-test-only-mfi-{}-{id}", std::process::id()));
            match DirBuilder::new().mode(0o700).create(&path) {
                Ok(()) => return Ok(Self(path)),
                Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => continue,
                Err(_) => return Err("cannot create private synthetic MFi directory".into()),
            }
        }
        Err("cannot allocate synthetic MFi directory".into())
    }
    fn file(&self, name: &str) -> PathBuf {
        self.0.join(name)
    }
    fn write(&self, name: &str, bytes: &[u8]) -> Result<(), String> {
        let mut file = OpenOptions::new()
            .create(true)
            .truncate(true)
            .write(true)
            .mode(0o600)
            .open(self.file(name))
            .map_err(|_| "cannot create test input")?;
        file.write_all(bytes)
            .map_err(|_| "cannot write test input".into())
    }
}
impl Drop for Scratch {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

/// Generated, clearly marked self-signed identity for offline protocol tests.
/// All methods serialize access to one private scratch directory via &mut self.
/// Certificate bytes are public test data; private keys never leave scratch.
pub struct TestSoftwareMfi {
    scratch: Scratch,
    openssl: OsString,
    profile: TestProfile,
    certificate: Vec<u8>,
}
impl TestSoftwareMfi {
    pub fn generate(profile: TestProfile) -> Result<Self, String> {
        Self::generate_with_openssl(profile, OsStr::new("openssl"))
    }
    pub fn generate_with_openssl(profile: TestProfile, executable: &OsStr) -> Result<Self, String> {
        let mut me = Self {
            scratch: Scratch::create()?,
            openssl: executable.to_owned(),
            profile,
            certificate: Vec::new(),
        };
        let generation = match profile {
            TestProfile::LegacyRsaSha1 => vec![
                "genpkey",
                "-algorithm",
                "RSA",
                "-pkeyopt",
                "rsa_keygen_bits:1024",
                "-out",
                "private.pem",
            ],
            TestProfile::V3P256Sha256 => vec![
                "genpkey",
                "-algorithm",
                "EC",
                "-pkeyopt",
                "ec_paramgen_curve:P-256",
                "-out",
                "private.pem",
            ],
        };
        me.checked(&generation)?;
        fs::set_permissions(
            me.scratch.file("private.pem"),
            fs::Permissions::from_mode(0o600),
        )
        .map_err(|_| "cannot protect synthetic private key")?;
        me.checked(&[
            "req",
            "-new",
            "-x509",
            "-key",
            "private.pem",
            "-subj",
            TEST_SUBJECT,
            "-days",
            "1",
            "-sha256",
            "-outform",
            "DER",
            "-out",
            "certificate.der",
        ])?;
        let file = fs::File::open(me.scratch.file("certificate.der"))
            .map_err(|_| "missing test certificate")?;
        file.take((MAX_OUTPUT + 1) as u64)
            .read_to_end(&mut me.certificate)
            .map_err(|_| "cannot read test certificate")?;
        if me.certificate.is_empty() || me.certificate.len() > MAX_OUTPUT {
            return Err("invalid synthetic certificate size".into());
        }
        let public = me.checked(&[
            "x509",
            "-inform",
            "DER",
            "-in",
            "certificate.der",
            "-pubkey",
            "-noout",
        ])?;
        me.scratch.write("public.pem", &public)?;
        Ok(me)
    }
    pub fn profile(&self) -> TestProfile {
        self.profile
    }
    pub fn certificate_der(&self) -> &[u8] {
        &self.certificate
    }
    fn digest_check(&self, digest: &[u8]) -> Result<(), String> {
        if digest.len() != self.profile.digest_len() {
            return Err(format!(
                "synthetic {:?} expects {} prehashed bytes, got {}",
                self.profile,
                self.profile.digest_len(),
                digest.len()
            ));
        }
        Ok(())
    }
    /// Verify the prehashed digest against this synthetic identity's public key.
    pub fn verify(&mut self, digest: &[u8], signature: &[u8]) -> Result<bool, String> {
        self.digest_check(digest)?;
        if signature.len() != self.profile.signature_len() {
            return Ok(false);
        }
        let der = if self.profile == TestProfile::V3P256Sha256 {
            match raw_to_der(signature) {
                Ok(v) => v,
                Err(_) => return Ok(false),
            }
        } else {
            signature.to_vec()
        };
        self.scratch.write("digest.bin", digest)?;
        self.scratch.write("signature.bin", &der)?;
        let (success, _) = self.run(&[
            "pkeyutl",
            "-verify",
            "-pubin",
            "-inkey",
            "public.pem",
            "-sigfile",
            "signature.bin",
            "-in",
            "digest.bin",
            "-pkeyopt",
            self.profile.digest_option(),
        ])?;
        Ok(success)
    }
    /// Independent test oracle: recover RSA DigestInfo, which must contain the
    /// original SHA-1 digest, never SHA-1(digest). Not used by the provider.
    pub fn recovered_legacy_digest_info(&mut self, signature: &[u8]) -> Result<Vec<u8>, String> {
        if self.profile != TestProfile::LegacyRsaSha1 || signature.len() != 128 {
            return Err("DigestInfo oracle requires legacy test profile".into());
        }
        self.scratch.write("signature.bin", signature)?;
        self.checked(&[
            "pkeyutl",
            "-verifyrecover",
            "-pubin",
            "-inkey",
            "public.pem",
            "-in",
            "signature.bin",
        ])
    }
    /// Independent SHA-256/raw-message oracle for the v3 prehash path.
    pub fn verify_v3_message(&mut self, message: &[u8], signature: &[u8]) -> Result<bool, String> {
        if self.profile != TestProfile::V3P256Sha256 || message.len() > MAX_OUTPUT {
            return Err("raw-message oracle requires bounded v3 test input".into());
        }
        let der = match raw_to_der(signature) {
            Ok(v) => v,
            Err(_) => return Ok(false),
        };
        self.scratch.write("message.bin", message)?;
        self.scratch.write("signature.bin", &der)?;
        let (success, _) = self.run(&[
            "dgst",
            "-sha256",
            "-verify",
            "public.pem",
            "-signature",
            "signature.bin",
            "message.bin",
        ])?;
        Ok(success)
    }
    pub fn v3_message_digest(&mut self, message: &[u8]) -> Result<Vec<u8>, String> {
        if self.profile != TestProfile::V3P256Sha256 || message.len() > MAX_OUTPUT {
            return Err("digest oracle requires bounded v3 test input".into());
        }
        self.scratch.write("message.bin", message)?;
        self.checked(&["dgst", "-sha256", "-binary", "message.bin"])
    }
    fn checked(&self, args: &[&str]) -> Result<Vec<u8>, String> {
        let (success, bytes) = self.run(args)?;
        if success {
            Ok(bytes)
        } else {
            Err(format!("synthetic MFi OpenSSL {} failed", args[0]))
        }
    }
    fn run(&self, args: &[&str]) -> Result<(bool, Vec<u8>), String> {
        let mut child = Command::new(&self.openssl)
            .args(args)
            .current_dir(&self.scratch.0)
            .env("OPENSSL_CONF", "/dev/null")
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::null())
            .spawn()
            .map_err(|_| "cannot start host OpenSSL test tool")?;
        let stdout = child.stdout.take().ok_or("missing OpenSSL pipe")?;
        let reader = std::thread::Builder::new().spawn(move || {
            let mut bytes = Vec::new();
            stdout
                .take((MAX_OUTPUT + 1) as u64)
                .read_to_end(&mut bytes)
                .map(|_| bytes)
        });
        let reader = match reader {
            Ok(reader) => reader,
            Err(_) => {
                let _ = child.kill();
                let _ = child.wait();
                return Err("cannot start OpenSSL output reader".into());
            }
        };
        let deadline = Instant::now() + PROCESS_LIMIT;
        let status = loop {
            match child.try_wait() {
                Ok(Some(status)) => break Ok(status.success()),
                Ok(None) if Instant::now() < deadline => {
                    std::thread::sleep(Duration::from_millis(5))
                }
                _ => {
                    let _ = child.kill();
                    let _ = child.wait();
                    break Err("host OpenSSL test tool failed or timed out".to_owned());
                }
            }
        };
        let bytes = reader
            .join()
            .map_err(|_| "OpenSSL output reader failed")?
            .map_err(|_| "cannot read OpenSSL test output")?;
        if bytes.len() > MAX_OUTPUT {
            return Err("OpenSSL test output exceeds bound".into());
        }
        status.map(|ok| (ok, bytes))
    }
}
impl AuthProvider for TestSoftwareMfi {
    fn certificate(&mut self) -> Result<Vec<u8>, String> {
        Ok(self.certificate.clone())
    }
    fn sign(&mut self, digest: &[u8]) -> Result<Vec<u8>, String> {
        self.digest_check(digest)?;
        self.scratch.write("digest.bin", digest)?;
        // No -rawin/-digest: caller already supplies the digest. The digest:
        // option selects encoding/length checks without hashing again.
        let signature = self.checked(&[
            "pkeyutl",
            "-sign",
            "-inkey",
            "private.pem",
            "-in",
            "digest.bin",
            "-pkeyopt",
            self.profile.digest_option(),
        ])?;
        let wire = if self.profile == TestProfile::V3P256Sha256 {
            der_to_raw(&signature)?.to_vec()
        } else {
            signature
        };
        if wire.len() != self.profile.signature_len() {
            return Err("invalid synthetic signature length".into());
        }
        Ok(wire)
    }
}

/// Strict DER/raw conversion for P-256, not a general ASN.1 parser.
fn der_to_raw(der: &[u8]) -> Result<[u8; 64], String> {
    if !(8..=72).contains(&der.len()) || der[0] != 0x30 || der[1] as usize + 2 != der.len() {
        return Err("invalid test ECDSA sequence".into());
    }
    let mut raw = [0; 64];
    let mut at = 2;
    for part in 0..2 {
        if at + 2 > der.len() || der[at] != 2 {
            return Err("invalid ECDSA integer tag".into());
        }
        let n = der[at + 1] as usize;
        at += 2;
        if n == 0 || n > 33 || at + n > der.len() {
            return Err("invalid ECDSA integer length".into());
        }
        let mut value = &der[at..at + n];
        at += n;
        if value[0] == 0 {
            if value.len() < 2 || value[1] & 0x80 == 0 {
                return Err("noncanonical ECDSA integer".into());
            }
            value = &value[1..];
        } else if value[0] & 0x80 != 0 {
            return Err("negative ECDSA integer".into());
        }
        if value.len() > 32 || value.iter().all(|b| *b == 0) {
            return Err("invalid ECDSA scalar".into());
        }
        raw[(part + 1) * 32 - value.len()..(part + 1) * 32].copy_from_slice(value);
    }
    if at != der.len() {
        return Err("trailing ECDSA signature bytes".into());
    }
    Ok(raw)
}
fn raw_to_der(raw: &[u8]) -> Result<Vec<u8>, String> {
    if raw.len() != 64 {
        return Err("test ECDSA signature must be 64 bytes".into());
    }
    let mut body = Vec::new();
    for value in [&raw[..32], &raw[32..]] {
        let start = value
            .iter()
            .position(|b| *b != 0)
            .ok_or("zero ECDSA scalar")?;
        let value = &value[start..];
        let pad = value[0] & 0x80 != 0;
        body.extend_from_slice(&[2, (value.len() + usize::from(pad)) as u8]);
        if pad {
            body.push(0);
        }
        body.extend_from_slice(value);
    }
    let mut der = vec![0x30, body.len() as u8];
    der.extend_from_slice(&body);
    Ok(der)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn der_boundaries_and_noncanonical_inputs() {
        let mut raw = [0; 64];
        raw[0] = 0x80;
        raw[63] = 1;
        let der = raw_to_der(&raw).unwrap();
        assert_eq!(der_to_raw(&der).unwrap(), raw);
        for input in [
            vec![],
            vec![0x30, 0],
            vec![0x30, 6, 2, 1, 0x80, 2, 1, 1],
            vec![0x30, 7, 2, 2, 0, 1, 2, 1, 1],
            vec![0x30, 6, 2, 1, 0, 2, 1, 1],
        ] {
            assert!(der_to_raw(&input).is_err());
        }
        assert!(raw_to_der(&[0; 64]).is_err());
        assert!(raw_to_der(&[1; 63]).is_err());
        assert!(raw_to_der(&[1; 65]).is_err());
    }
    #[test]
    fn random_der_never_panics_and_accepted_values_roundtrip() {
        let mut state = 17u32;
        for n in 0..100_000 {
            let mut bytes = vec![0; n % 100];
            for b in &mut bytes {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                *b = state as u8;
            }
            if let Ok(raw) = der_to_raw(&bytes) {
                assert_eq!(der_to_raw(&raw_to_der(&raw).unwrap()).unwrap(), raw);
            }
            let mut raw = [0; 64];
            for b in &mut raw {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                *b = state as u8;
            }
            raw[31] |= 1;
            raw[63] |= 1;
            let der = raw_to_der(&raw).unwrap();
            assert_eq!(der_to_raw(&der).unwrap(), raw);
            let mut trailing = der.clone();
            trailing.push(0);
            assert!(der_to_raw(&trailing).is_err());
            assert!(der_to_raw(&der[..der.len() - 1]).is_err());
        }
    }
    #[test]
    fn scratch_directory_and_key_are_private_and_removed() {
        let path;
        {
            let me = TestSoftwareMfi::generate(TestProfile::LegacyRsaSha1).unwrap();
            path = me.scratch.0.clone();
            assert_eq!(
                fs::metadata(&path).unwrap().permissions().mode() & 0o777,
                0o700
            );
            assert_eq!(
                fs::metadata(me.scratch.file("private.pem"))
                    .unwrap()
                    .permissions()
                    .mode()
                    & 0o777,
                0o600
            );
        }
        assert!(!path.exists());
    }
    #[test]
    fn missing_crypto_tool_fails_closed() {
        assert!(
            TestSoftwareMfi::generate_with_openssl(
                TestProfile::LegacyRsaSha1,
                OsStr::new("/no/such/openssl-test-tool")
            )
            .is_err()
        );
    }

    #[test]
    fn missing_key_and_oversized_output_fail_closed() {
        let mut me = TestSoftwareMfi::generate(TestProfile::LegacyRsaSha1).unwrap();
        fs::remove_file(me.scratch.file("private.pem")).unwrap();
        assert!(me.sign(&[0; 20]).is_err());
        me.scratch
            .write(
                "oversize.sh",
                b"#!/bin/sh\nexec dd if=/dev/zero bs=1024 count=20\n",
            )
            .unwrap();
        fs::set_permissions(
            me.scratch.file("oversize.sh"),
            fs::Permissions::from_mode(0o700),
        )
        .unwrap();
        me.openssl = me.scratch.file("oversize.sh").into_os_string();
        assert!(me.run(&[]).is_err());
    }

    #[test]
    fn stalled_crypto_tool_is_killed_and_reaped() {
        let mut me = TestSoftwareMfi::generate(TestProfile::LegacyRsaSha1).unwrap();
        me.scratch
            .write("stall.sh", b"#!/bin/sh\nexec sleep 30\n")
            .unwrap();
        fs::set_permissions(
            me.scratch.file("stall.sh"),
            fs::Permissions::from_mode(0o700),
        )
        .unwrap();
        me.openssl = me.scratch.file("stall.sh").into_os_string();
        let start = Instant::now();
        assert!(me.run(&[]).unwrap_err().contains("timed out"));
        assert!(start.elapsed() < Duration::from_secs(10));
    }
}
