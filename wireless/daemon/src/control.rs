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
        return Err(
            "invalid pairing identifier (expected 1..252 bytes without control/space)".into(),
        );
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
    pub vehicle_engine: EngineType,
}

impl Config {
    /// Configuration is explicit so a missing AP identity cannot silently
    /// advertise a test SSID or dummy MFi identity on a real unit.
    pub fn from_env() -> Result<Self, String> {
        Self::from_lookup(&|name| std::env::var(name).ok())
    }

    fn from_lookup(get: &impl Fn(&str) -> Option<String>) -> Result<Self, String> {
        let identity = crate::factory_identity::FactoryIdentity::load(get)?;
        let read = |name: &str| {
            let value = get(name).ok_or_else(|| format!("missing {name}"))?;
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
        let receiver = get("MHI2Q_CP_RECEIVER").unwrap_or_else(|| "stock210".into());
        let (expected_version, expected_port) = receiver_profile(&receiver)?;
        let source_version =
            get("MHI2Q_CP_SOURCE_VERSION").unwrap_or_else(|| expected_version.into());
        if source_version != expected_version {
            return Err(format!(
                "MHI2Q_CP_SOURCE_VERSION must match {receiver}: {expected_version}"
            ));
        }
        let config = Self {
            name: identity.name,
            manufacturer: identity.manufacturer,
            model: read("MHI2Q_CP_MODEL")?,
            serial: read("MHI2Q_CP_SERIAL")?,
            firmware: identity.firmware,
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
            pairing_id: get("MHI2Q_CP_PAIRING_ID").filter(|value| !value.is_empty()),
            source_version,
            vehicle_engine: match get("IAP2_VEHICLE_ENGINE").as_deref().unwrap_or("0") {
                "0"|"gasoline"=>EngineType::Gas,"1"|"diesel"=>EngineType::Diesel,
                "2"|"electric"=>EngineType::Electric,"3"|"cng"=>EngineType::Cng,
                _=>return Err("invalid IAP2_VEHICLE_ENGINE (expected 0..3)".into()),
            },
        };
        if config.airplay_port != expected_port {
            return Err(format!(
                "MHI2Q_CP_AIRPLAY_PORT must match {receiver}: {expected_port}"
            ));
        }
        if get("MHI2Q_CP_PUBLIC_KEY").is_some_and(|value| !value.is_empty()) {
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
    identification_complete: bool,
    authentication_complete: bool,
    pending_peer_name: Option<String>,
    wireless_available: bool,
    metadata: std::collections::VecDeque<Vec<u8>>,
    semantic: std::collections::VecDeque<Vec<u8>>,
    identification_published: bool,
}

impl<A: AuthProvider> ControlMachine<A> {
    pub fn new(config: Config, auth: A) -> Self {
        Self {
            config,
            auth,
            phase: Phase::AwaitIdentification,
            identification_complete: false,
            authentication_complete: false,
            pending_peer_name: None,
            wireless_available: false,
            metadata: std::collections::VecDeque::new(),
            semantic: std::collections::VecDeque::new(),
            identification_published: false,
        }
    }
    pub fn phase(&self) -> Phase {
        self.phase
    }
    pub fn wireless_available(&self) -> bool {
        self.phase == Phase::Authenticated && self.wireless_available
    }
    pub fn take_metadata(&mut self) -> Option<Vec<u8>> {
        self.metadata.pop_front()
    }
    pub fn take_semantic(&mut self) -> Option<Vec<u8>> { self.semantic.pop_front() }
    pub fn take_identification(&mut self) -> Option<Vec<u8>> {
        if self.phase!=Phase::Authenticated || self.identification_published { return None; }
        self.identification_published=true; Some(self.identification_frame())
    }
    pub fn metadata_subscription() -> Vec<u8> {
        // Stock requests these groups as empty attribute TLVs. Advertise
        // only fields consumed by the native DIO queue adapter.
        let mut frame = vec![0x40, 0x40, 0, 0, 0x50, 0];
        for (group, attributes) in [
            (0u16, &[1u16, 4, 6, 12, 16, 18, 26][..]),
            (1, &[0u16, 1, 2, 3, 7][..]),
        ] {
            frame.extend_from_slice(&((4 + attributes.len() * 4) as u16).to_be_bytes());
            frame.extend_from_slice(&group.to_be_bytes());
            for attribute in attributes {
                frame.extend_from_slice(&[0, 4]);
                frame.extend_from_slice(&attribute.to_be_bytes());
            }
        }
        let length = frame.len() as u16;
        frame[2..4].copy_from_slice(&length.to_be_bytes());
        frame
    }

    /// A pre-auth notification is only display text; publish it after both
    /// exchanges succeed. This machine is discarded on every link generation.
    pub fn take_peer_name(&mut self) -> Option<String> {
        if self.phase == Phase::Authenticated {
            self.pending_peer_name.take()
        } else {
            None
        }
    }

    fn update_phase(&mut self) {
        self.phase = if !self.identification_complete {
            Phase::AwaitIdentification
        } else if !self.authentication_complete {
            Phase::AwaitAuth
        } else {
            Phase::Authenticated
        };
    }

    pub fn handle(&mut self, frame: &[u8]) -> Result<Option<Vec<u8>>, String> {
        let id = message_id(frame)?;
        #[cfg(feature = "native-api-lab")]
        eprintln!(
            "[native-api-lab] control received=0x{id:04X} phase={:?}",
            self.phase
        );
        if self.phase == Phase::Failed {
            return Err("iAP2 control session failed; a new link generation is required".into());
        }
        match (self.phase, id) {
            (Phase::Authenticated, 0x5201|0x5202|0x5204|0xa100|0xa102|0xfffa|0xfffc) => {
                if frame.len()>4095 || self.semantic.len()==32 { return Err("semantic iAP2 backlog exceeded".into()); }
                self.semantic.push_back(frame.to_vec()); Ok(None)
            }
            (Phase::Authenticated, 0x5001) => {
                // Keep native wire validation authoritative, with a bounded
                // backlog so metadata cannot starve the iAP2 timer loop.
                if frame.len() <= 4096 && self.metadata.len() < 16 {
                    self.metadata.push_back(frame.to_vec());
                }
                Ok(None)
            }
            (_, 0x4E09) => {
                if let Some(name) = crate::device_name::decode(frame)? {
                    self.pending_peer_name = Some(name);
                }
                Ok(None)
            }
            (Phase::AwaitIdentification, 0x1D00) => Ok(Some(self.identification_frame())),
            (_, 0x1D02) => {
                self.identification_complete = true;
                self.update_phase();
                Ok(None)
            }
            (_, 0x1D03) => {
                self.phase = Phase::Failed;
                // Retain the field IDs from the phone's reject message. These
                // are capability diagnostics, not accessory or phone identity.
                let mut fields = Vec::new();
                let mut offset = 6;
                while offset + 4 <= frame.len() {
                    let size = u16::from_be_bytes([frame[offset], frame[offset + 1]]) as usize;
                    if size < 4 || offset + size > frame.len() { break; }
                    fields.push(u16::from_be_bytes([frame[offset + 2], frame[offset + 3]]));
                    offset += size;
                }
                Err(format!("identification rejected by phone; fields={fields:?}"))
            }
            (_, 0xAA00) if !self.authentication_complete => {
                let cert = self.auth.certificate()?;
                Ok(Some(
                    AuthenticationCertificate { certificate: cert }.encode(),
                ))
            }
            (_, 0xAA02) if !self.authentication_complete => {
                let request = RequestAuthenticationChallengeResponse::decode(frame)
                    .map_err(|e| e.to_string())?;
                let response = self.auth.sign(&request.challenge)?;
                Ok(Some(AuthenticationResponse { response }.encode()))
            }
            (_, 0xAA05) => {
                self.authentication_complete = true;
                self.update_phase();
                Ok(None)
            }
            (_, 0xAA04) => {
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
                self.wireless_available = availability
                    .wireless_attributes
                    .and_then(|wireless| wireless.available)
                    == Some(true);
                if !self.wireless_available {
                    return Ok(None);
                }
                Ok(Some(self.start_session()))
            }
            (Phase::Authenticated, 0x4E0D) => {
                self.wireless_available = WirelessCarPlayUpdate::decode(frame)
                    .map_err(|e| e.to_string())?
                    .status
                    == WirelessCarPlayStatus::Available;
                Ok(None)
            }
            (_, 0x5702 | 0x4300) => {
                Err("CarPlay request before identification and MFi authentication complete".into())
            }
            _ => Ok(None),
        }
    }

    fn start_session(&self) -> Vec<u8> {
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
        .encode()
    }

    fn identification_frame(&self)->Vec<u8> {
        let mut frame=self.identification().encode();
        let mut off=6;
        while off+4<=frame.len() {
            let size=u16::from_be_bytes([frame[off],frame[off+1]]) as usize;
            if size<4||off+size>frame.len(){break;}
            if frame[off+2..off+4]==[0,22] {
                /* R29 LocationInformationComponent VehicleSpeedData. The
                 * vendored schema models only GPS/RMC, so extend on wire. */
                frame.splice(off+size..off+size,[0,4,0,20]);
                frame[off..off+2].copy_from_slice(&((size+4)as u16).to_be_bytes());break;
            }
            off+=size;
        }
        let length=frame.len() as u16;frame[2..4].copy_from_slice(&length.to_be_bytes());frame
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
            // Native iOS 26.6.2 rejects the Identification exchange itself in
            // these capability lists (1D03 parameters 6/7 name all four IDs).
            messages_sent_by_accessory: ids(&[0xAA01, 0xAA03, 0x5703, 0x4301, 0x5000,0x5002,0x5200,0x5203,0xa101,0xfffb]),
            messages_received_from_accessory: ids(&[
                0xAA00, 0xAA02, 0xAA04, 0xAA05, 0x5702, 0x4300, 0x4E09, 0x4E0D, 0x5001,
                0x5201,0x5202,0x5204,0xa100,0xa102,0xfffa,0xfffc,
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
            vehicle_information_component: Some(VehicleInformationComponent {
                id:0x0f20,name:"Vehicle".into(),engine_type:self.config.vehicle_engine,
                display_name:Some("Audi".into()),maps_display_name:None,
            }),
            vehicle_status_component: Some(VehicleStatusComponent {
                id:0x0f21,name:"VehicleStatus".into(),range:true,outside_temperature:true,range_warning:true,
            }),
            location_information_component: Some(LocationInformationComponent {
                id:0x0f22,name:"Location".into(),global_positioning_system_fix_data:true,
                recommended_minimum_specific_gps_transit_data:true,
            }),
            wireless_car_play_transport_component: Some(WirelessCarPlayTransportComponent {
                id: 1,
                name: self.config.ssid.clone(),
                supports_iap2_connection: true,
                supports_car_play: true,
            }),
            route_guidance_display_component: vec![RouteGuidanceDisplayComponent {
                id:16,name:"RouteGuidanceDisplayComponent".into(),
                max_current_road_name_length:Some(256),max_destination_name_length:Some(256),
                max_after_maneuver_road_name_length:Some(256),max_maneuver_description_length:Some(256),
                max_guidance_maneuver_storage_capacity:Some(6),max_lane_guidance_description_length:Some(256),
                max_lane_guidance_storage_capacity:Some(6),
            }],
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

    #[test]
    fn phone_name_is_generation_local_and_gated_by_both_exchanges() {
        let mut machine = ControlMachine::new(config(), TestAuth);
        machine
            .handle(&crate::device_name::frame("iPhone Луки".as_bytes()))
            .unwrap();
        assert!(machine.take_peer_name().is_none());
        authenticate(&mut machine);
        assert!(machine.take_peer_name().is_none());
        machine.handle(&IdentificationAccepted {}.encode()).unwrap();
        assert_eq!(machine.take_peer_name().as_deref(), Some("iPhone Луки"));
        assert!(machine.take_peer_name().is_none());
        machine
            .handle(&crate::device_name::frame(b"Renamed iPhone\0"))
            .unwrap();
        assert_eq!(machine.take_peer_name().as_deref(), Some("Renamed iPhone"));
        let mut next = ControlMachine::new(config(), TestAuth);
        next.handle(&IdentificationAccepted {}.encode()).unwrap();
        authenticate(&mut next);
        assert!(next.take_peer_name().is_none());
        let mut failed = ControlMachine::new(config(), TestAuth);
        failed
            .handle(&crate::device_name::frame(b"Untrusted"))
            .unwrap();
        assert!(failed.handle(&[0x40, 0x40, 0, 6, 0xAA, 4]).is_err());
        assert!(failed.take_peer_name().is_none());
    }

    #[test]
    fn factory_identity_and_wireless_semantic_capabilities_are_used_on_wire() {
        let get = |key: &str| -> Option<String> {
            Some(match key {
                "MHI2Q_CP_NAME" | "MHI2Q_CP_MANUFACTURER" | "MHI2Q_CP_FIRMWARE" => {
                    "obsolete override".into()
                }
                "MHI2Q_CP_MODEL" | "MHI2Q_CP_SERIAL" | "MHI2Q_CP_HARDWARE" => {
                    "explicit unit value".into()
                }
                "MHI2Q_CP_BT_MAC" | "MHI2Q_CP_DEVICE_ID" => "02:00:00:00:00:01".into(),
                "MHI2Q_CP_SSID" => "Wireless Test".into(),
                "MHI2Q_CP_PASSPHRASE" => "testpassword".into(),
                "MHI2Q_CP_CHANNEL" => "11".into(),
                "MHI2Q_CP_IP_ADDRESS" => "127.0.0.1".into(),
                "MHI2Q_CP_AIRPLAY_PORT" => "5000".into(),
                _ => return None,
            })
        };
        // The fixture uses a non-production basename; put it in a disposable
        // directory so this exercises the actual file loader and Config path.
        let dir = std::env::temp_dir().join(format!("mhi2q-factory-test-{}", std::process::id()));
        std::fs::create_dir(&dir).unwrap();
        let path = dir.join("dio_manager.json");
        std::fs::write(&path, include_str!("../probes/dio_manager.fixture.json")).unwrap();
        let lookup = |key: &str| {
            if key == "IPL_CONFIG_DIR_DIO_MANAGER" {
                Some(dir.to_str().unwrap().into())
            } else {
                get(key)
            }
        };
        let config = Config::from_lookup(&lookup).unwrap();
        let encoded = ControlMachine::new(config, TestAuth)
            .identification()
            .encode();
        let identity = IdentificationInformation::decode(&encoded).unwrap();
        assert_eq!(identity.name, "QEMU MMI");
        assert_eq!(identity.manufacturer, "Test Manufacturer");
        assert_eq!(identity.firmware_version, "QEMU firmware");
        assert_eq!(identity.model_identifier, "explicit unit value");
        assert_eq!(identity.serial_number, "explicit unit value");
        assert_eq!(identity.hardware_version, "explicit unit value");
        assert_eq!(
            identity.messages_sent_by_accessory,
            vec![0xAA,1,0xAA,3,0x57,3,0x43,1,0x50,0,0x50,2,0x52,0,0x52,3,0xa1,1,0xff,0xfb]
        );
        assert_eq!(
            identity.messages_received_from_accessory,
            vec![
                0xAA, 0, 0xAA, 2, 0xAA, 4, 0xAA, 5, 0x57, 2, 0x43, 0, 0x4E, 0x09, 0x4E, 0x0D, 0x50,
                1,0x52,1,0x52,2,0x52,4,0xa1,0,0xa1,2,0xff,0xfa,0xff,0xfc
            ]
        );
        assert!(identity.usb_host_transport_component.is_empty());
        assert!(identity.usb_device_transport_component.is_empty());
        assert_eq!(
            identity.bluetooth_transport_component[0].bluetooth_transport_mac,
            [2, 0, 0, 0, 0, 1]
        );
        assert!(
            identity
                .wireless_car_play_transport_component
                .unwrap()
                .supports_car_play
        );
        std::fs::write(&path, "{}").unwrap();
        assert!(
            Config::from_lookup(&lookup)
                .unwrap_err()
                .contains("accessoryIdentification")
        );
        std::fs::write(&path, [0xFF]).unwrap();
        assert!(Config::from_lookup(&lookup).unwrap_err().contains("UTF-8"));
        std::fs::write(&path, vec![b' '; 1024 * 1024 + 1]).unwrap();
        assert!(
            Config::from_lookup(&lookup)
                .unwrap_err()
                .contains("exceeds 1 MiB")
        );
        std::fs::remove_file(&path).unwrap();
        assert!(
            Config::from_lookup(&lookup)
                .unwrap_err()
                .contains("cannot read")
        );
        std::fs::remove_dir(&dir).unwrap();
    }

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
            vehicle_engine: EngineType::Gas,
        }
    }

    fn authenticate(machine: &mut ControlMachine<TestAuth>) {
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
    }

    #[test]
    fn authentication_first_keeps_bootstrap_gated_until_identification() {
        let mut machine = ControlMachine::new(config(), TestAuth);
        authenticate(&mut machine);
        assert_eq!(machine.phase(), Phase::AwaitIdentification);
        assert!(
            machine
                .handle(&RequestAccessoryWiFiConfigurationInformation {}.encode())
                .is_err()
        );
        let availability = CarPlayAvailability {
            wired_attributes: None,
            wireless_attributes: Some(CarPlayAvailabilityWirelessAttributes {
                available: Some(true),
                bluetooth_transport_identifier: None,
            }),
        }
        .encode();
        assert!(machine.handle(&availability).is_err());
        let ident = machine
            .handle(&StartIdentification {}.encode())
            .unwrap()
            .unwrap();
        assert!(IdentificationInformation::decode(&ident).is_ok());
        machine.handle(&IdentificationAccepted {}.encode()).unwrap();
        assert_eq!(machine.phase(), Phase::Authenticated);
        assert!(machine.handle(&availability).unwrap().is_some());
        assert!(
            machine
                .handle(&RequestAccessoryWiFiConfigurationInformation {}.encode())
                .unwrap()
                .is_some()
        );
    }

    #[test]
    fn identification_can_complete_between_authentication_requests() {
        let mut machine = ControlMachine::new(config(), TestAuth);
        assert!(
            machine
                .handle(&RequestAuthenticationCertificate {}.encode())
                .unwrap()
                .is_some()
        );
        machine.handle(&StartIdentification {}.encode()).unwrap();
        machine.handle(&IdentificationAccepted {}.encode()).unwrap();
        assert_eq!(machine.phase(), Phase::AwaitAuth);
        assert!(
            machine
                .handle(&RequestAccessoryWiFiConfigurationInformation {}.encode())
                .is_err()
        );
        authenticate(&mut machine);
        assert_eq!(machine.phase(), Phase::Authenticated);
        // Duplicate completion messages cannot regress a completed session.
        machine.handle(&IdentificationAccepted {}.encode()).unwrap();
        machine
            .handle(&AuthenticationSucceeded {}.encode())
            .unwrap();
        assert_eq!(machine.phase(), Phase::Authenticated);
    }

    #[test]
    fn every_identification_authentication_interleaving_gates_credentials() {
        // Merge the two ordered exchanges: 2 identification messages and
        // 3 authentication messages have ten legal interleavings.
        let identification = [
            StartIdentification {}.encode(),
            IdentificationAccepted {}.encode(),
        ];
        let authentication = [
            RequestAuthenticationCertificate {}.encode(),
            RequestAuthenticationChallengeResponse {
                challenge: vec![4, 5, 6],
            }
            .encode(),
            AuthenticationSucceeded {}.encode(),
        ];
        let wifi = RequestAccessoryWiFiConfigurationInformation {}.encode();
        let carplay = CarPlayAvailability {
            wired_attributes: None,
            wireless_attributes: Some(CarPlayAvailabilityWirelessAttributes {
                available: Some(true),
                bluetooth_transport_identifier: None,
            }),
        }
        .encode();
        let mut permutations = 0;
        for first_ident in 0..5 {
            for second_ident in first_ident + 1..5 {
                let mut machine = ControlMachine::new(config(), TestAuth);
                let (mut ident_index, mut auth_index) = (0, 0);
                for step in 0..5 {
                    let frame = if step == first_ident || step == second_ident {
                        ident_index += 1;
                        &identification[ident_index - 1]
                    } else {
                        auth_index += 1;
                        &authentication[auth_index - 1]
                    };
                    let reply = machine.handle(frame).unwrap();
                    match message_id(frame).unwrap() {
                        0x1D00 => {
                            IdentificationInformation::decode(&reply.unwrap()).unwrap();
                        }
                        0xAA00 => {
                            assert_eq!(
                                AuthenticationCertificate::decode(&reply.unwrap())
                                    .unwrap()
                                    .certificate,
                                [1, 2, 3]
                            );
                        }
                        0xAA02 => {
                            assert_eq!(
                                AuthenticationResponse::decode(&reply.unwrap())
                                    .unwrap()
                                    .response,
                                [6, 5, 4]
                            );
                        }
                        _ => assert!(reply.is_none()),
                    }
                    let complete = ident_index == 2 && auth_index == 3;
                    assert_eq!(machine.handle(&wifi).is_ok(), complete);
                    assert_eq!(machine.handle(&carplay).is_ok(), complete);
                    assert_eq!(machine.phase() == Phase::Authenticated, complete);
                }
                permutations += 1;
            }
        }
        assert_eq!(permutations, 10);
    }

    #[test]
    fn either_failure_is_terminal_in_both_orders_and_new_session_resets() {
        for failure in [0xAA04u16, 0x1D03] {
            for completed in 0..4 {
                let mut machine = ControlMachine::new(config(), TestAuth);
                if completed == 1 || completed == 3 {
                    machine.handle(&IdentificationAccepted {}.encode()).unwrap();
                }
                if completed == 2 || completed == 3 {
                    authenticate(&mut machine);
                }
                let [hi, lo] = failure.to_be_bytes();
                assert!(machine.handle(&[0x40, 0x40, 0, 6, hi, lo]).is_err());
                assert_eq!(machine.phase(), Phase::Failed);
                for id in [0xAA00u16, 0xAA05, 0x1D00, 0x1D02, 0x5702, 0x4300] {
                    let [hi, lo] = id.to_be_bytes();
                    assert!(machine.handle(&[0x40, 0x40, 0, 6, hi, lo]).is_err());
                    assert_eq!(machine.phase(), Phase::Failed);
                }
                let mut next = ControlMachine::new(config(), TestAuth);
                assert_eq!(next.phase(), Phase::AwaitIdentification);
                authenticate(&mut next);
                assert_eq!(next.phase(), Phase::AwaitIdentification);
            }
        }
    }

    #[test]
    fn now_playing_identification_includes_start_and_stop() {
        // Physical HU capture 4040000a1d0300040006 flags parameter 6.
        // The failed capability list advertised Now Playing without its stop.
        let mut machine = ControlMachine::new(config(), TestAuth);
        let reply = machine.handle(&StartIdentification {}.encode()).unwrap().unwrap();
        let identity = IdentificationInformation::decode(&reply).unwrap();
        let sent: Vec<_> = identity.messages_sent_by_accessory.chunks_exact(2)
            .map(|id| u16::from_be_bytes([id[0], id[1]])).collect();
        assert!(sent.contains(&0x5000) && sent.contains(&0x5002));
        let reject = [0x40, 0x40, 0, 10, 0x1d, 3, 0, 4, 0, 6];
        assert_eq!(machine.handle(&reject).unwrap_err(),
                   "identification rejected by phone; fields=[6]");
        assert_eq!(machine.phase(), Phase::Failed);
    }

    #[test]
    fn identification_omits_ids_rejected_by_native_ios_bluetooth() {
        // Captured iOS 26.6.2 1D03: parameter 6 = 1D01;
        // parameter 7 = 1D00,1D02,1D03. These are implicit protocol messages.
        let mut machine = ControlMachine::new(config(), TestAuth);
        let reply = machine
            .handle(&StartIdentification {}.encode())
            .unwrap()
            .unwrap();
        let identity = IdentificationInformation::decode(&reply).unwrap();
        for list in [
            &identity.messages_sent_by_accessory,
            &identity.messages_received_from_accessory,
        ] {
            assert!(
                list.chunks_exact(2)
                    .all(|id| !(0x1D00..=0x1D03).contains(&u16::from_be_bytes([id[0], id[1]])))
            );
        }
        assert!(identity.bluetooth_transport_component[0].supports_iap2_connection);
        assert!(identity.usb_device_transport_component.is_empty());
        assert!(identity.usb_host_transport_component.is_empty());
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
    #[test]
    fn metadata_waits_for_authentication_and_has_bounded_backlog() {
        let mut machine = ControlMachine::new(config(), TestAuth);
        let frame = [0x40, 0x40, 0, 6, 0x50, 1];
        machine.handle(&frame).unwrap();
        assert!(machine.take_metadata().is_none());
        machine.handle(&IdentificationAccepted {}.encode()).unwrap();
        machine
            .handle(&AuthenticationSucceeded {}.encode())
            .unwrap();
        for _ in 0..100 {
            machine.handle(&frame).unwrap();
        }
        assert_eq!(machine.metadata.len(), 16);
        assert_eq!(machine.take_metadata(), Some(frame.to_vec()));
        let subscription =
            iap2_csm::messages::now_playing::StartNowPlayingUpdates::decode(&ControlMachine::<
                TestAuth,
            >::metadata_subscription(
            ))
            .unwrap();
        assert!(subscription.media_item_attributes.unwrap().title);
        assert!(subscription.playback_attributes.unwrap().elapsed_ms);
    }
}
