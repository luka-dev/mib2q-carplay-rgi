#![cfg(all(feature = "software-mfi-test", unix, not(target_os = "nto")))]
use iap2_csm::CsmMessage;
use iap2_csm::messages::authentication::*;
use iap2_csm::messages::car_play::*;
use iap2_csm::messages::identification::*;
use iap2_csm::messages::wifi::*;
use mhi2q_wireless_daemon::control::{AuthProvider, Config, ControlMachine, Phase};
use mhi2q_wireless_daemon::software_mfi_test::{TestProfile, TestSoftwareMfi};
use std::cell::RefCell;
use std::rc::Rc;

struct SharedAuth(Rc<RefCell<TestSoftwareMfi>>);
impl AuthProvider for SharedAuth {
    fn certificate(&mut self) -> Result<Vec<u8>, String> {
        self.0.borrow_mut().certificate()
    }
    fn sign(&mut self, digest: &[u8]) -> Result<Vec<u8>, String> {
        self.0.borrow_mut().sign(digest)
    }
}
fn config() -> Config {
    Config {
        name: "SYNTHETIC_MFI_TEST_ONLY".into(),
        manufacturer: "Test".into(),
        model: "Test".into(),
        serial: "TEST_NOT_AN_ACCESSORY_IDENTITY".into(),
        firmware: "test".into(),
        hardware: "test".into(),
        bt_mac: [1, 2, 3, 4, 5, 6],
        ssid: "TEST_ONLY_AP".into(),
        passphrase: "synthetic-test-password".into(),
        channel: 6,
        ip_address: "127.0.0.1".into(),
        airplay_port: 5000,
        device_id: "01:02:03:04:05:06".into(),
        pairing_id: Some("TEST_ONLY_PI".into()),
        source_version: "210.81".into(),
        vehicle_engine: iap2_csm::messages::identification::EngineType::Gas,
    }
}
fn availability() -> Vec<u8> {
    CarPlayAvailability {
        wired_attributes: None,
        wireless_attributes: Some(CarPlayAvailabilityWirelessAttributes {
            available: Some(true),
            bluetooth_transport_identifier: None,
        }),
    }
    .encode()
}

#[test]
fn real_signatures_prehash_and_tampering() {
    for profile in [TestProfile::LegacyRsaSha1, TestProfile::V3P256Sha256] {
        let mut signer = TestSoftwareMfi::generate(profile).unwrap();
        assert!(
            signer
                .certificate_der()
                .windows(23)
                .any(|w| w == b"NOT_A_REAL_MFI_IDENTITY")
        );
        let digest = if profile == TestProfile::V3P256Sha256 {
            signer
                .v3_message_digest(b"independent original message")
                .unwrap()
        } else {
            (0u8..20).collect()
        };
        let signature = signer.sign(&digest).unwrap();
        assert_eq!(signature.len(), profile.signature_len());
        assert!(signer.verify(&digest, &signature).unwrap());
        if profile == TestProfile::LegacyRsaSha1 {
            let recovered = signer.recovered_legacy_digest_info(&signature).unwrap();
            let mut expected = vec![
                0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04,
                0x14,
            ];
            expected.extend_from_slice(&digest);
            assert_eq!(recovered, expected); // SHA-1 DigestInfo, digest unchanged
        } else {
            assert!(
                signer
                    .verify_v3_message(b"independent original message", &signature)
                    .unwrap()
            );
            assert!(
                !signer
                    .verify_v3_message(b"different message", &signature)
                    .unwrap()
            );
        }
        let mut changed = digest.clone();
        changed[0] ^= 1;
        assert!(!signer.verify(&changed, &signature).unwrap());
        let mut changed_sig = signature.clone();
        changed_sig[0] ^= 1;
        assert!(!signer.verify(&digest, &changed_sig).unwrap());
        assert!(
            !signer
                .verify(&digest, &signature[..signature.len() - 1])
                .unwrap()
        );
        let mut other_identity = TestSoftwareMfi::generate(profile).unwrap();
        assert!(!other_identity.verify(&digest, &signature).unwrap());
        for size in [0, 1, 19, 21, 31, 33, 4096] {
            if size != profile.digest_len() {
                assert!(signer.sign(&vec![0; size]).is_err());
            }
        }
    }
}

#[test]
fn production_control_machine_uses_verified_synthetic_peer_in_both_orders() {
    for profile in [TestProfile::LegacyRsaSha1, TestProfile::V3P256Sha256] {
        for auth_first in [false, true] {
            let mut provider = TestSoftwareMfi::generate(profile).unwrap();
            let digest = vec![0x42; profile.digest_len()];
            let expected_signature = provider.sign(&digest).unwrap();
            assert!(provider.verify(&digest, &expected_signature).unwrap());
            let certificate = provider.certificate().unwrap();
            // The test peer holds a public verification view of the generated identity.
            // The real control machine uses the same provider through AuthProvider.
            let provider = Rc::new(RefCell::new(provider));
            let mut machine = ControlMachine::new(config(), SharedAuth(provider.clone()));
            if !auth_first {
                machine.handle(&StartIdentification {}.encode()).unwrap();
                machine.handle(&IdentificationAccepted {}.encode()).unwrap();
            }
            assert!(
                machine
                    .handle(&RequestAccessoryWiFiConfigurationInformation {}.encode())
                    .is_err()
            );
            assert!(machine.handle(&availability()).is_err());
            let cert_frame = machine
                .handle(&RequestAuthenticationCertificate {}.encode())
                .unwrap()
                .unwrap();
            assert_eq!(
                AuthenticationCertificate::decode(&cert_frame)
                    .unwrap()
                    .certificate,
                certificate
            );
            let answer = machine
                .handle(
                    &RequestAuthenticationChallengeResponse {
                        challenge: digest.clone(),
                    }
                    .encode(),
                )
                .unwrap()
                .unwrap();
            let signature = AuthenticationResponse::decode(&answer).unwrap().response;
            // Verify the actual CSM reply before the synthetic peer sends AA05.
            if profile == TestProfile::LegacyRsaSha1 {
                assert_eq!(signature, expected_signature);
            }
            assert_eq!(signature.len(), profile.signature_len());
            assert!(provider.borrow_mut().verify(&digest, &signature).unwrap());
            assert!(
                machine
                    .handle(&RequestAccessoryWiFiConfigurationInformation {}.encode())
                    .is_err()
            );
            machine
                .handle(&AuthenticationSucceeded {}.encode())
                .unwrap();
            if auth_first {
                assert_eq!(machine.phase(), Phase::AwaitIdentification);
                assert!(machine.handle(&availability()).is_err());
                machine.handle(&StartIdentification {}.encode()).unwrap();
                machine.handle(&IdentificationAccepted {}.encode()).unwrap();
            }
            assert_eq!(machine.phase(), Phase::Authenticated);
            let wifi = machine
                .handle(&RequestAccessoryWiFiConfigurationInformation {}.encode())
                .unwrap()
                .unwrap();
            assert_eq!(
                AccessoryWiFiConfigurationInformation::decode(&wifi)
                    .unwrap()
                    .channel,
                6
            );
            let start = machine.handle(&availability()).unwrap().unwrap();
            assert_eq!(
                CarPlayStartSession::decode(&start)
                    .unwrap()
                    .source_version
                    .as_deref(),
                Some("210.81")
            );
        }
    }
}

#[test]
fn rejection_is_terminal_and_a_new_session_is_fresh() {
    let mut machine = ControlMachine::new(
        config(),
        TestSoftwareMfi::generate(TestProfile::LegacyRsaSha1).unwrap(),
    );
    machine
        .handle(&RequestAuthenticationCertificate {}.encode())
        .unwrap();
    assert!(machine.handle(&AuthenticationFailed {}.encode()).is_err());
    assert_eq!(machine.phase(), Phase::Failed);
    assert!(
        machine
            .handle(&AuthenticationSucceeded {}.encode())
            .is_err()
    );
    assert!(machine.handle(&availability()).is_err());
    let mut fresh = ControlMachine::new(
        config(),
        TestSoftwareMfi::generate(TestProfile::LegacyRsaSha1).unwrap(),
    );
    assert!(
        fresh
            .handle(&RequestAuthenticationCertificate {}.encode())
            .unwrap()
            .is_some()
    );
    assert_eq!(fresh.phase(), Phase::AwaitIdentification);
}

#[test]
fn invalid_digest_cannot_open_wifi_and_a_valid_retry_can_authenticate() {
    for profile in [TestProfile::LegacyRsaSha1, TestProfile::V3P256Sha256] {
        let provider = Rc::new(RefCell::new(TestSoftwareMfi::generate(profile).unwrap()));
        let mut machine = ControlMachine::new(config(), SharedAuth(provider.clone()));
        machine.handle(&StartIdentification {}.encode()).unwrap();
        machine.handle(&IdentificationAccepted {}.encode()).unwrap();
        for len in [0, 1, 20, 32, 4096] {
            if len == profile.digest_len() {
                continue;
            }
            assert!(
                machine
                    .handle(
                        &RequestAuthenticationChallengeResponse {
                            challenge: vec![0x42; len],
                        }
                        .encode()
                    )
                    .is_err()
            );
            assert_eq!(machine.phase(), Phase::AwaitAuth);
            assert!(
                machine
                    .handle(&RequestAccessoryWiFiConfigurationInformation {}.encode())
                    .is_err()
            );
            assert!(machine.handle(&availability()).is_err());
        }
        let digest = vec![0x42; profile.digest_len()];
        let answer = machine
            .handle(
                &RequestAuthenticationChallengeResponse {
                    challenge: digest.clone(),
                }
                .encode(),
            )
            .unwrap()
            .unwrap();
        let signature = AuthenticationResponse::decode(&answer).unwrap().response;
        assert!(provider.borrow_mut().verify(&digest, &signature).unwrap());
        machine
            .handle(&AuthenticationSucceeded {}.encode())
            .unwrap();
        assert_eq!(machine.phase(), Phase::Authenticated);
    }
}
