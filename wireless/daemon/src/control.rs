//! iAP2 control-session bring-up. Host tests use a synthetic MFi provider;
//! QNX builds use the I2C8 adapter in `mfi`.

use iap2_csm::CsmMessage;
use iap2_csm::messages::authentication::*;
use iap2_csm::messages::car_play::*;
use iap2_csm::messages::identification::*;
use iap2_csm::messages::wifi::*;

// The production candidate extends the original MU1316 receiver in place.
pub const AIRPLAY_SOURCE_VERSION: &str = "210.81";

fn receiver_profile(name: &str) -> Result<(&'static str, u32), String> {
    match name {
        "stock210" => Ok((AIRPLAY_SOURCE_VERSION, 5000)),
        "experimental280" => Ok(("280.33.8", 7000)),
        _ => Err("MHI2Q_CP_RECEIVER must be stock210 or experimental280".into()),
    }
}

/// Bonjour pi and iAP2 StartSession field 4 carry an identifier, not pk.
/// 252 bytes leave room for "pi=" inside one DNS-SD TXT item.
pub fn validate_pairing_id(id: &str) -> Result<(), String> {
    if id.is_empty() || id.len() > 252 || id.bytes().any(|b| b <= 0x20 || b == 0x7f) {
        return Err("invalid pairing identifier (expected 1..252 bytes without control/space)".into());
    }
    Ok(())
}

/// Bind an outgoing StartSession to the identity returned by the active
/// receiver. An optional configured ID is an assertion, never a substitute.
pub fn bind_receiver_identity(reply: &[u8], id: &str) -> Result<Vec<u8>, String> {
    validate_pairing_id(id)?;
    let mut start = CarPlayStartSession::decode(reply).map_err(|e| e.to_string())?;
    if let Some(expected) = &start.public_key
        && expected != id
    {
        return Err("receiver pairing identifier differs from MHI2Q_CP_PAIRING_ID".into());
    }
    start.public_key = Some(id.to_owned());
    Ok(start.encode())
}

#[derive(Debug, Clone)]
pub struct Config {
    pub name: String,
    pub manufacturer: String,
    pub model: String,
    pub serial: String,
    pub firmware: String,
    pub hardware: String,
    pub bt_mac: [u8; 6],
    pub ssid: String,
    pub passphrase: String,
    pub channel: u8,
    pub ip_address: String,
    pub airplay_port: u32,
    pub device_id: String,
    pub pairing_id: Option<String>,
    pub source_version: String,
}

impl Config {
    /// Configuration is explicit so a missing AP identity cannot silently
    /// advertise a test SSID or dummy MFi identity on a real unit.
    pub fn from_env() -> Result<Self, String> {
        let read = |name: &str| {
            let value = std::env::var(name).map_err(|_| format!("missing {name}"))?;
            if value.trim().is_empty() {
                return Err(format!("empty {name}"));
            }
            Ok(value)
        };
        let mac = read("MHI2Q_CP_BT_MAC")?;
        let parts: Vec<_> = mac.split(':').collect();
        if parts.len() != 6 {
            return Err("invalid MHI2Q_CP_BT_MAC".into());
        }
        let mut bt_mac = [0u8; 6];
        for (dest, part) in bt_mac.iter_mut().zip(parts) {
            if part.len() != 2 {
                return Err("invalid MHI2Q_CP_BT_MAC".into());
            }
            *dest =
                u8::from_str_radix(part, 16).map_err(|_| "invalid MHI2Q_CP_BT_MAC".to_string())?;
        }
        let receiver = std::env::var("MHI2Q_CP_RECEIVER").unwrap_or_else(|_| "stock210".into());
        let (expected_version, expected_port) = receiver_profile(&receiver)?;
        let source_version = std::env::var("MHI2Q_CP_SOURCE_VERSION")
            .unwrap_or_else(|_| expected_version.into());
        if source_version != expected_version {
            return Err(format!("MHI2Q_CP_SOURCE_VERSION must match {receiver}: {expected_version}"));
        }
        let config = Self {
            name: read("MHI2Q_CP_NAME")?,
            manufacturer: read("MHI2Q_CP_MANUFACTURER")?,
            model: read("MHI2Q_CP_MODEL")?,
            serial: read("MHI2Q_CP_SERIAL")?,
            firmware: read("MHI2Q_CP_FIRMWARE")?,
            hardware: read("MHI2Q_CP_HARDWARE")?,
            bt_mac,
            ssid: read("MHI2Q_CP_SSID")?,
            passphrase: read("MHI2Q_CP_PASSPHRASE")?,
            channel: read("MHI2Q_CP_CHANNEL")?
                .parse()
                .map_err(|_| "invalid MHI2Q_CP_CHANNEL".to_string())?,
            ip_address: read("MHI2Q_CP_IP_ADDRESS")?,
            airplay_port: read("MHI2Q_CP_AIRPLAY_PORT")?
                .parse()
                .map_err(|_| "invalid MHI2Q_CP_AIRPLAY_PORT".to_string())?,
            device_id: read("MHI2Q_CP_DEVICE_ID")?,
            pairing_id: std::env::var("MHI2Q_CP_PAIRING_ID")
                .ok()
                .filter(|value| !value.is_empty()),
            source_version,
        };
        if config.airplay_port != expected_port {
            return Err(format!("MHI2Q_CP_AIRPLAY_PORT must match {receiver}: {expected_port}"));
        }
        if std::env::var("MHI2Q_CP_PUBLIC_KEY").is_ok_and(|value| !value.is_empty()) {
            return Err("MHI2Q_CP_PUBLIC_KEY is obsolete: use MHI2Q_CP_PAIRING_ID to pin pi, not an Ed25519 pk".into());
        }
        if let Some(id) = &config.pairing_id {
            validate_pairing_id(id)?;
        }
        if !(1..=11).contains(&config.channel) {
            return Err("MHI2Q_CP_CHANNEL must be 1..11 for this US unit".into());
        }
        if config.ssid.len() > 32 || !(8..=63).contains(&config.passphrase.len()) {
            return Err("invalid AP SSID or WPA2 passphrase length".into());
        }
        if config.airplay_port == 0 || config.airplay_port > u16::MAX as u32 {
            return Err("invalid MHI2Q_CP_AIRPLAY_PORT".into());
        }
        config
            .ip_address
            .parse::<std::net::Ipv4Addr>()
            .map_err(|_| "invalid MHI2Q_CP_IP_ADDRESS".to_string())?;
        crate::wifi_ie::parse_mac(&config.device_id)
            .map_err(|_| "invalid MHI2Q_CP_DEVICE_ID".to_string())?;
        Ok(config)
    }
}

pub trait AuthProvider {
    fn certificate(&mut self) -> Result<Vec<u8>, String>;
    fn sign(&mut self, challenge: &[u8]) -> Result<Vec<u8>, String>;
}

pub struct UnavailableAuth;
impl AuthProvider for UnavailableAuth {
    fn certificate(&mut self) -> Result<Vec<u8>, String> {
        Err("QNX MFi coprocessor adapter is not implemented".into())
    }
    fn sign(&mut self, _: &[u8]) -> Result<Vec<u8>, String> {
        Err("QNX MFi coprocessor adapter is not implemented".into())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Phase {
    AwaitIdentification,
    AwaitAuth,
    Authenticated,
    Failed,
}

pub struct ControlMachine<A: AuthProvider> {
    config: Config,
    auth: A,
    phase: Phase,
}

impl<A: AuthProvider> ControlMachine<A> {
    pub fn new(config: Config, auth: A) -> Self {
        Self {
            config,
            auth,
            phase: Phase::AwaitIdentification,
        }
    }
    pub fn phase(&self) -> Phase {
        self.phase
    }

    pub fn handle(&mut self, frame: &[u8]) -> Result<Option<Vec<u8>>, String> {
        let id = message_id(frame)?;
        match (self.phase, id) {
            (Phase::AwaitIdentification, 0x1D00) => Ok(Some(self.identification().encode())),
            (Phase::AwaitIdentification, 0x1D02) => {
                self.phase = Phase::AwaitAuth;
                Ok(None)
            }
            (Phase::AwaitIdentification, 0x1D03) => {
                self.phase = Phase::Failed;
                Err("identification rejected by phone".into())
            }
            (Phase::AwaitAuth, 0xAA00) => {
                let cert = self.auth.certificate()?;
                Ok(Some(
                    AuthenticationCertificate { certificate: cert }.encode(),
                ))
            }
            (Phase::AwaitAuth, 0xAA02) => {
                let request = RequestAuthenticationChallengeResponse::decode(frame)
                    .map_err(|e| e.to_string())?;
                let response = self.auth.sign(&request.challenge)?;
                Ok(Some(AuthenticationResponse { response }.encode()))
            }
            (Phase::AwaitAuth, 0xAA05) => {
                self.phase = Phase::Authenticated;
                Ok(None)
            }
            (Phase::AwaitAuth, 0xAA04) => {
                self.phase = Phase::Failed;
                Err("MFi authentication rejected by phone".into())
            }
            (Phase::Authenticated, 0x5702) => Ok(Some(
                AccessoryWiFiConfigurationInformation {
                    ssid: Some(self.config.ssid.clone()),
                    passphrase: Some(self.config.passphrase.clone()),
                    security_type: SecurityType::WpaWpa2,
                    channel: self.config.channel,
                }
                .encode(),
            )),
            (Phase::Authenticated, 0x4300) => {
                let availability = CarPlayAvailability::decode(frame).map_err(|e| e.to_string())?;
                if availability
                    .wireless_attributes
                    .and_then(|wireless| wireless.available)
                    != Some(true)
                {
                    return Ok(None);
                }
                Ok(Some(
                    CarPlayStartSession {
                        wired_attributes: None,
                        wireless_attributes: Some(CarPlayStartSessionWirelessAttributes {
                            wifi_ssid: Some(self.config.ssid.clone()),
                            passphrase: Some(self.config.passphrase.clone()),
                            channel: Some(self.config.channel),
                            ip_address: vec![self.config.ip_address.clone()],
                            security_type: Some(SecurityType::WpaWpa2 as u8),
                        }),
                        port: Some(self.config.airplay_port),
                        device_identifier: Some(self.config.device_id.clone()),
                        public_key: self.config.pairing_id.clone(),
                        source_version: Some(self.config.source_version.clone()),
                    }
                    .encode(),
                ))
            }
            (_, 0x5702 | 0x4300) => Err("CarPlay request before MFi authentication".into()),
            _ => Ok(None),
        }
    }

    fn identification(&self) -> IdentificationInformation {
        let ids = |values: &[u16]| values.iter().flat_map(|id| id.to_be_bytes()).collect();
        IdentificationInformation {
            name: self.config.name.clone(),
            model_identifier: self.config.model.clone(),
            manufacturer: self.config.manufacturer.clone(),
            serial_number: self.config.serial.clone(),
            firmware_version: self.config.firmware.clone(),
            hardware_version: self.config.hardware.clone(),
            messages_sent_by_accessory: ids(&[0x1D01, 0xAA01, 0xAA03, 0x5703, 0x4301]),
            messages_received_from_accessory: ids(&[
                0x1D00, 0x1D02, 0x1D03, 0xAA00, 0xAA02, 0xAA04, 0xAA05, 0x5702, 0x4300, 0x4E0D,
            ]),
            power_providing_capability: PowerProvidingCapability::None,
            maximum_current_drawn_from_device: 0,
            supported_external_accessory_protocol: vec![],
            app_match_team_id: None,
            current_language: "en".into(),
            supported_language: vec!["en".into()],
            serial_transport_component: vec![],
            usb_device_transport_component: vec![],
            usb_host_transport_component: vec![],
            bluetooth_transport_component: vec![BluetoothTransportComponent {
                id: 0,
                name: "Bluetooth".into(),
                supports_iap2_connection: true,
                bluetooth_transport_mac: self.config.bt_mac.to_vec(),
            }],
            vehicle_information_component: None,
            vehicle_status_component: None,
            location_information_component: None,
            wireless_car_play_transport_component: Some(WirelessCarPlayTransportComponent {
                id: 1,
                name: self.config.ssid.clone(),
                supports_iap2_connection: true,
                supports_car_play: true,
            }),
            route_guidance_display_component: vec![],
        }
    }
}

pub fn message_id(frame: &[u8]) -> Result<u16, String> {
    if frame.len() < 6
        || frame[..2] != [0x40, 0x40]
        || u16::from_be_bytes([frame[2], frame[3]]) as usize != frame.len()
    {
        return Err("invalid iAP2 control frame".into());
    }
    Ok(u16::from_be_bytes([frame[4], frame[5]]))
}

#[derive(Default)]
pub struct CsmReader {
    bytes: Vec<u8>,
}
impl CsmReader {
    pub fn push(&mut self, data: &[u8]) -> Result<Vec<Vec<u8>>, String> {
        self.bytes.extend_from_slice(data);
        let mut frames = vec![];
        loop {
            if self.bytes.len() < 6 {
                break;
            }
            if self.bytes[..2] != [0x40, 0x40] {
                return Err("bad CSM marker".into());
            }
            let len = u16::from_be_bytes([self.bytes[2], self.bytes[3]]) as usize;
            if len < 6 {
                return Err("bad CSM length".into());
            }
            if self.bytes.len() < len {
                break;
            }
            frames.push(self.bytes.drain(..len).collect());
        }
        Ok(frames)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestAuth;
    impl AuthProvider for TestAuth {
        fn certificate(&mut self) -> Result<Vec<u8>, String> {
            Ok(vec![1, 2, 3])
        }
        fn sign(&mut self, challenge: &[u8]) -> Result<Vec<u8>, String> {
            Ok(challenge.iter().rev().copied().collect())
        }
    }

    fn config() -> Config {
        Config {
            name: "Audi Test".into(),
            manufacturer: "Audi".into(),
            model: "MHI2Q".into(),
            serial: "test".into(),
            firmware: "test".into(),
            hardware: "test".into(),
            bt_mac: [1, 2, 3, 4, 5, 6],
            ssid: "Test-AP".into(),
            passphrase: "test-passphrase".into(),
            channel: 11,
            ip_address: "fe80::1".into(),
            airplay_port: 7000,
            device_id: "01:02:03:04:05:06".into(),
            pairing_id: None,
            source_version: "test".into(),
        }
    }

    #[test]
    fn identification_auth_wifi_and_carplay_start() {
        let mut machine = ControlMachine::new(config(), TestAuth);
        assert!(
            machine
                .handle(&RequestAccessoryWiFiConfigurationInformation {}.encode())
                .is_err()
        );
        let ident = machine
            .handle(&StartIdentification {}.encode())
            .unwrap()
            .unwrap();
        let decoded = IdentificationInformation::decode(&ident).unwrap();
        assert!(
            decoded
                .wireless_car_play_transport_component
                .unwrap()
                .supports_car_play
        );
        machine.handle(&IdentificationAccepted {}.encode()).unwrap();
        let cert = machine
            .handle(&RequestAuthenticationCertificate {}.encode())
            .unwrap()
            .unwrap();
        assert_eq!(
            AuthenticationCertificate::decode(&cert)
                .unwrap()
                .certificate,
            [1, 2, 3]
        );
        let answer = machine
            .handle(
                &RequestAuthenticationChallengeResponse {
                    challenge: vec![4, 5, 6],
                }
                .encode(),
            )
            .unwrap()
            .unwrap();
        assert_eq!(
            AuthenticationResponse::decode(&answer).unwrap().response,
            [6, 5, 4]
        );
        machine
            .handle(&AuthenticationSucceeded {}.encode())
            .unwrap();
        assert_eq!(machine.phase(), Phase::Authenticated);
        let wifi = machine
            .handle(&RequestAccessoryWiFiConfigurationInformation {}.encode())
            .unwrap()
            .unwrap();
        let parsed = AccessoryWiFiConfigurationInformation::decode(&wifi).unwrap();
        assert_eq!(parsed.ssid.as_deref(), Some("Test-AP"));
        assert_eq!(parsed.channel, 11);
        let available = CarPlayAvailability {
            wired_attributes: None,
            wireless_attributes: Some(CarPlayAvailabilityWirelessAttributes {
                available: Some(true),
                bluetooth_transport_identifier: None,
            }),
        };
        let start = machine.handle(&available.encode()).unwrap().unwrap();
        let parsed = CarPlayStartSession::decode(&start).unwrap();
        assert_eq!(parsed.wireless_attributes.unwrap().channel, Some(11));
        assert_eq!(parsed.public_key, None);
        let id = "12345678-1234-4234-8234-123456789ABC";
        let bound = bind_receiver_identity(&start, id).unwrap();
        let parsed = CarPlayStartSession::decode(&bound).unwrap();
        assert_eq!(parsed.public_key.as_deref(), Some(id));
        assert_eq!(bind_receiver_identity(&bound, id).unwrap(), bound);
        assert!(bind_receiver_identity(&bound, "different-pi").is_err());
        assert!(bind_receiver_identity(&start, "").is_err());
        assert!(bind_receiver_identity(&start, "pi with spaces").is_err());
        assert!(bind_receiver_identity(&start, &"a".repeat(253)).is_err());
        assert!(bind_receiver_identity(&[0], id).is_err());

        for wireless in [None, Some(false)] {
            let unavailable = CarPlayAvailability {
                wired_attributes: None,
                wireless_attributes: Some(CarPlayAvailabilityWirelessAttributes {
                    available: wireless,
                    bluetooth_transport_identifier: None,
                }),
            };
            assert!(machine.handle(&unavailable.encode()).unwrap().is_none());
        }
        assert!(
            machine
                .handle(
                    &CarPlayAvailability {
                        wired_attributes: None,
                        wireless_attributes: None,
                    }
                    .encode()
                )
                .unwrap()
                .is_none()
        );
    }

    #[test]
    fn split_csm_reassembles() {
        let frame = StartIdentification {}.encode();
        let mut reader = CsmReader::default();
        assert!(reader.push(&frame[..3]).unwrap().is_empty());
        assert_eq!(reader.push(&frame[3..]).unwrap(), vec![frame]);
    }
}
