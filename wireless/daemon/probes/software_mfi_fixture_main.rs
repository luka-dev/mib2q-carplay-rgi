//! Host-only fixture preparation using the existing software MFi provider.
mod software_mfi_fixture;
use mhi2q_wireless_daemon::control::AuthProvider;
use mhi2q_wireless_daemon::software_mfi_test::{TestProfile, TestSoftwareMfi};
use software_mfi_fixture::{Case, Fixture};
use std::io::{Read, Write};
use std::os::unix::fs::OpenOptionsExt;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = std::env::args_os().skip(1);
    let output = args.next().ok_or("usage: software-mfi-fixture OUTPUT")?;
    if args.next().is_some() {
        return Err("usage: software-mfi-fixture OUTPUT".into());
    }
    let mut signer = TestSoftwareMfi::generate(TestProfile::LegacyRsaSha1)?;
    let mut fixture = Fixture {
        certificate: signer.certificate()?,
        cases: Vec::new(),
    };
    let mut random = std::fs::File::open("/dev/urandom")?;
    for _ in 0..2 {
        let mut digest = [0; 20];
        random.read_exact(&mut digest)?;
        let signature = signer.sign(&digest)?;
        if !signer.verify(&digest, &signature)? {
            return Err("fixture signature failed verification".into());
        }
        fixture.cases.push(Case {
            digest,
            signature: signature
                .try_into()
                .map_err(|_| "invalid RSA signature size")?,
        });
    }
    let bytes = fixture.encode()?;
    // Never overwrite an existing fixture or follow a pre-existing output link.
    let mut file = std::fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .mode(0o600)
        .open(&output)?;
    file.write_all(&bytes)?;
    let decoded = Fixture::read(std::path::Path::new(&output))?;
    println!(
        "synthetic MFi fixture: certificate={} cases={} private_key_exported=false",
        decoded.certificate.len(),
        decoded.cases.len()
    );
    Ok(())
}
