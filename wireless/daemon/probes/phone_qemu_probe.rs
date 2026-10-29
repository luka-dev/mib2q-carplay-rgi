//! QEMU-only synthetic phone driving the ordinary QNX daemon over its bridge.
use iap2_csm::CsmMessage;
use iap2_csm::messages::authentication::{
    AuthenticationCertificate, AuthenticationFailed, AuthenticationResponse,
    AuthenticationSucceeded, RequestAuthenticationCertificate,
    RequestAuthenticationChallengeResponse,
};
use iap2_csm::messages::car_play::{
    CarPlayAvailability, CarPlayAvailabilityWirelessAttributes, CarPlayStartSession,
};
use iap2_csm::messages::identification::{
    IdentificationAccepted, IdentificationInformation, StartIdentification,
};
use iap2_csm::messages::wifi::{
    AccessoryWiFiConfigurationInformation, RequestAccessoryWiFiConfigurationInformation,
};
use iap2_link::{CONTROL_SESSION_ID, Event, LinkConfig, LinkEngine, LinkState};
use mhi2q_wireless_daemon::control::{CsmReader, message_id};
use mhi2q_wireless_daemon::{
    BridgeFrame, FrameDecoder, TYPE_CONNECTED, TYPE_DATA, TYPE_DISCONNECTED,
};
mod integrated_fixture;
mod features_fixture;
mod recovery_fixture;
use std::io::{self, Read, Write};
use std::net::{Ipv4Addr, SocketAddrV4, TcpListener, TcpStream};
use std::sync::mpsc;
use std::thread;
use std::time::{Duration, Instant};
mod software_mfi_fixture;
mod software_mfi_phone;
use software_mfi_phone::SoftwarePhoneAuth;

#[derive(Clone, Copy, PartialEq, Eq)]
enum Phase {
    Link,
    Identification,
    Certificate,
    Signature,
    Wifi,
    CarPlay,
    Recovering,
    Done,
    Rejected,
}

fn flush_link(stream: &mut TcpStream, engine: &mut LinkEngine, generation: u32) -> io::Result<()> {
    for chunk in engine.take_output().chunks(20) {
        stream.write_all(
            &BridgeFrame {
                kind: TYPE_DATA,
                generation,
                data: chunk.to_vec(),
            }
            .encode()?,
        )?;
    }
    Ok(())
}

fn fake_receiver() -> io::Result<mpsc::Receiver<()>> {
    let listener = TcpListener::bind((Ipv4Addr::LOCALHOST, 57343))?;
    listener.set_nonblocking(true)?;
    let (tx, rx) = mpsc::channel();
    let mut reject_once = std::env::var_os("MHI2Q_PHONE_QEMU_REJECT_ONCE").is_some();
    let airplay_port = airplay_port();
    thread::spawn(move || {
        let deadline = Instant::now() + Duration::from_secs(70);
        let mut started = false;
        while Instant::now() < deadline {
            match listener.accept() {
                Ok((mut stream, _)) => {
                    let _ = stream.set_nonblocking(false);
                    let _ = stream.set_read_timeout(Some(Duration::from_secs(3)));
                    let mut request = [0u8; 16];
                    let read_result = stream.read_exact(&mut request);
                    if read_result.is_err()
                        || &request[..5] != b"CPRX\x02"
                        || request[6..8] != [0, 0]
                        || request[8..12] != 1u32.to_le_bytes()
                        || request[12..14] != airplay_port.to_le_bytes()
                        || request[14..] != [0, 0]
                    {
                        eprintln!(
                            "[phone-qemu] bad receiver request read={read_result:?} bytes={request:02x?}"
                        );
                        return;
                    }
                    let operation = request[5];
                    if operation == 5 && started {
                        let id = b"12345678-1234-4234-8234-123456789ABC";
                        let _ = stream.write_all(b"CPRA\x02\0\0\0");
                        let _ = stream.write_all(&(id.len() as u16).to_le_bytes());
                        let _ = stream.write_all(id);
                        continue;
                    }
                    if operation == 1 && !started {
                        if reject_once {
                            reject_once = false;
                            println!("[phone-qemu] receiver rejected first START=1");
                            let _ = stream.write_all(b"CPRA\x02\x01\0\0");
                            continue;
                        }
                        started = true;
                        println!("[phone-qemu] receiver START=1");
                    } else if operation == 2 && started {
                        println!("[phone-qemu] receiver STOP=1");
                        let _ = stream.write_all(b"CPRA\x02\0\0\0");
                        let _ = tx.send(());
                        return;
                    } else {
                        eprintln!("[phone-qemu] unexpected receiver operation={operation}");
                        return;
                    }
                    let _ = stream.write_all(b"CPRA\x02\0\0\0");
                }
                Err(error) if error.kind() == io::ErrorKind::WouldBlock => {
                    thread::sleep(Duration::from_millis(20));
                }
                Err(error) => {
                    eprintln!("[phone-qemu] receiver listener: {error}");
                    return;
                }
            }
        }
    });
    Ok(rx)
}

fn fake_control_phone() -> io::Result<mpsc::Receiver<()>> {
    let listener = TcpListener::bind((Ipv4Addr::LOCALHOST, 7001))?;
    listener.set_nonblocking(true)?;
    let (tx, rx) = mpsc::channel();
    thread::spawn(move || {
        let deadline = Instant::now() + Duration::from_secs(70);
        while Instant::now() < deadline {
            match listener.accept() {
                Ok((mut stream, _)) => {
                    let _ = stream.set_nonblocking(false);
                    let _ = stream.set_read_timeout(Some(Duration::from_secs(3)));
                    let mut request = Vec::new();
                    let mut chunk = [0u8; 256];
                    while request.len() < 4096 && !request.ends_with(b"\r\n\r\n") {
                        match stream.read(&mut chunk) {
                            Ok(0) | Err(_) => return,
                            Ok(length) => request.extend_from_slice(&chunk[..length]),
                        }
                    }
                    let text = String::from_utf8_lossy(&request);
                    if !text.starts_with("GET /ctrl-int/1/connect HTTP/1.1\r\n")
                        || !text.contains("Host: iphone.local.\r\n")
                        || !text.contains("AirPlay-Receiver-Device-ID: 2199023255553\r\n")
                        || !text.contains(&format!(
                            "User-Agent: AirPlay/{}\r\n",
                            airplay_source_version()
                        ))
                    {
                        eprintln!("[phone-qemu] invalid CarPlay Control request: {text}");
                        return;
                    }
                    if stream
                        .write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n")
                        .is_ok()
                    {
                        println!("[phone-qemu] CarPlay Control connect=1");
                        let _ = tx.send(());
                    }
                    return;
                }
                Err(error) if error.kind() == io::ErrorKind::WouldBlock => {
                    thread::sleep(Duration::from_millis(20));
                }
                Err(error) => {
                    eprintln!("[phone-qemu] control listener: {error}");
                    return;
                }
            }
        }
    });
    Ok(rx)
}

fn connect_daemon() -> io::Result<TcpStream> {
    let address = SocketAddrV4::new(Ipv4Addr::LOCALHOST, 57342);
    for _ in 0..100 {
        if let Ok(stream) = TcpStream::connect(address) {
            return Ok(stream);
        }
        thread::sleep(Duration::from_millis(100));
    }
    Err(io::Error::new(
        io::ErrorKind::NotConnected,
        "daemon not listening",
    ))
}

fn airplay_port() -> u16 {
    std::env::var("MHI2Q_PHONE_QEMU_AIRPLAY_PORT")
        .ok()
        .and_then(|value| value.parse().ok())
        .unwrap_or(5000)
}

fn airplay_source_version() -> String {
    std::env::var("MHI2Q_PHONE_QEMU_SOURCE_VERSION").unwrap_or_else(|_| "210.81".to_owned())
}

fn stock_receiver_ready() -> io::Result<()> {
    let address = SocketAddrV4::new(Ipv4Addr::LOCALHOST, airplay_port());
    let deadline = Instant::now() + Duration::from_secs(60);
    let mut diagnostics = 0;
    while Instant::now() < deadline {
        if let Ok(mut stream) = TcpStream::connect(address) {
            stream.set_read_timeout(Some(Duration::from_secs(2)))?;
            if stream
                .write_all(b"GET /info HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n")
                .is_ok()
            {
                let mut status = [0u8; 12];
                match stream.read_exact(&mut status) {
                    Ok(()) if &status == b"HTTP/1.1 200" => {
                        thread::sleep(Duration::from_secs(2));
                        println!("[phone-qemu] stock receiver /info ready=1");
                        return Ok(());
                    }
                    result if diagnostics < 3 => {
                        eprintln!("[phone-qemu] /info status={status:?} read={result:?}");
                        diagnostics += 1;
                    }
                    _ => {}
                }
            }
        }
        thread::sleep(Duration::from_millis(200));
    }
    Err(io::Error::new(
        io::ErrorKind::TimedOut,
        "stock receiver not ready",
    ))
}

fn wait_stock_receiver_stopped() -> io::Result<()> {
    let address = SocketAddrV4::new(Ipv4Addr::LOCALHOST, airplay_port());
    let deadline = Instant::now() + Duration::from_secs(8);
    while Instant::now() < deadline {
        if TcpStream::connect(address).is_err() {
            println!("[phone-qemu] stock receiver STOP=1");
            return Ok(());
        }
        thread::sleep(Duration::from_millis(100));
    }
    Err(io::Error::new(
        io::ErrorKind::TimedOut,
        "stock receiver did not stop",
    ))
}

fn handle_csm(
    engine: &mut LinkEngine,
    phase: &mut Phase,
    frame: &[u8],
    now: u64,
    software_auth: &mut Option<SoftwarePhoneAuth>,
) -> io::Result<()> {
    let id = message_id(frame).map_err(io::Error::other)?;
    if features_fixture::control(engine,id,frame,now)? {return Ok(());}
    match (*phase, id) {
        (_, 0x5000) => {
            use iap2_csm::messages::now_playing::*;
            let subscription = StartNowPlayingUpdates::decode(frame).map_err(io::Error::other)?;
            if !subscription
                .media_item_attributes
                .is_some_and(|v| v.title && v.artist)
                || !subscription
                    .playback_attributes
                    .is_some_and(|v| v.status && v.elapsed_ms && v.app_name)
            {
                return Err(io::Error::other(
                    "missing native metadata subscription fields",
                ));
            }
            if std::env::var_os("MHI2Q_PHONE_QEMU_INTEGRATED").is_some() {
                let update = NowPlayingUpdate {
                    media_item_attributes: Some(MediaItemAttributes {
                        title: Some("QNX Песня".into()),
                        duration_ms: Some(123000),
                        album: Some("Native Album".into()),
                        artist: Some("Phone Artist".into()),
                        persistent_id: None,
                        album_artist: None,
                        genre: None,
                        artwork_ftid: None,
                    }),
                    playback_attributes: Some(PlaybackAttributes {
                        status: Some(PlaybackStatus::Playing),
                        elapsed_ms: Some(4200),
                        app_name: Some("Phone Music".into()),
                        app_bundle_id: None,
                    }),
                };
                engine.send(CONTROL_SESSION_ID, update.encode(), now);
                let partial = NowPlayingUpdate {
                    media_item_attributes: Some(MediaItemAttributes {
                        title: Some("QNX Следующая".into()),
                        duration_ms: None,
                        album: None,
                        artist: None,
                        persistent_id: None,
                        album_artist: None,
                        genre: None,
                        artwork_ftid: None,
                    }),
                    playback_attributes: Some(PlaybackAttributes {
                        status: Some(PlaybackStatus::Paused),
                        elapsed_ms: Some(5600),
                        app_name: None,
                        app_bundle_id: None,
                    }),
                };
                engine.send(CONTROL_SESSION_ID, partial.encode(), now);
                println!("[integrated-media] real iAP2 Now Playing update sent");
                if features_fixture::enabled(){features_fixture::artwork(engine,now);}
            }
        }
        (Phase::Identification, 0x1d01) => {
            let info = IdentificationInformation::decode(frame).map_err(io::Error::other)?;
            let has_sent = |wanted| info.messages_sent_by_accessory.chunks_exact(2)
                .any(|id| u16::from_be_bytes([id[0], id[1]]) == wanted);
            // Real iPhone rejects parameter 6 for an incomplete Now Playing
            // start/stop capability. Do not let the lab accept that payload.
            if has_sent(0x5000) != has_sent(0x5002) {
                return Err(io::Error::other("incomplete Now Playing start/stop capability"));
            }
            if std::env::var("MHI2Q_PHONE_QEMU_FACTORY_FIXTURE").as_deref() == Ok("1") {
                if info.name != "QEMU MMI"
                    || info.manufacturer != "Test Manufacturer"
                    || info.firmware_version != "QEMU firmware"
                {
                    return Err(io::Error::other("factory JSON identity differs on wire"));
                }
                println!("[phone-qemu] factory JSON identity verified on wire");
            }
            if !info
                .wireless_car_play_transport_component
                .is_some_and(|component| component.supports_car_play)
            {
                return Err(io::Error::other(
                    "CarPlay transport absent in identification",
                ));
            }
            println!("[phone-qemu] identification=1");
            engine.send(CONTROL_SESSION_ID, IdentificationAccepted {}.encode(), now);
            if std::env::var("MHI2Q_PHONE_QEMU_AUTH_FIRST").as_deref() == Ok("1") {
                engine.send(
                    CONTROL_SESSION_ID,
                    RequestAccessoryWiFiConfigurationInformation {}.encode(),
                    now,
                );
                *phase = Phase::Wifi;
            } else {
                engine.send(
                    CONTROL_SESSION_ID,
                    RequestAuthenticationCertificate {}.encode(),
                    now,
                );
                *phase = Phase::Certificate;
            }
        }
        (Phase::Certificate, 0xaa01) => {
            let cert = AuthenticationCertificate::decode(frame).map_err(io::Error::other)?;
            if let Some(auth) = software_auth.as_mut() {
                auth.accept_certificate(&cert.certificate)?;
            } else if cert.certificate != b"CERT" {
                return Err(io::Error::other("wrong mock certificate"));
            }
            println!("[phone-qemu] certificate=1");
            engine.send(
                CONTROL_SESSION_ID,
                RequestAuthenticationChallengeResponse {
                    challenge: software_auth
                        .as_ref()
                        .map_or_else(|| vec![0x12; 20], |auth| auth.challenge()),
                }
                .encode(),
                now,
            );
            *phase = Phase::Signature;
        }
        (Phase::Signature, 0xaa03) => {
            let answer = AuthenticationResponse::decode(frame).map_err(io::Error::other)?;
            if let Some(auth) = software_auth.as_mut() {
                match auth.verify_response(&answer.response)? {
                    None if std::env::var_os("MHI2Q_MFI_QEMU_TAMPER").is_some() => {
                        println!("[phone-qemu] software signature rejected=1");
                        engine.send(CONTROL_SESSION_ID, AuthenticationFailed {}.encode(), now);
                        *phase = Phase::Rejected;
                        return Ok(());
                    }
                    None => return Err(io::Error::other("invalid software MFi AA03 signature")),
                    Some(true) => {
                        engine.send(
                            CONTROL_SESSION_ID,
                            RequestAuthenticationChallengeResponse {
                                challenge: auth.challenge(),
                            }
                            .encode(),
                            now,
                        );
                        return Ok(());
                    }
                    Some(false) => {}
                }
            } else if answer.response != [0x5a; 20] {
                return Err(io::Error::other("wrong mock signature"));
            }
            println!("[phone-qemu] signature=1");
            engine.send(CONTROL_SESSION_ID, AuthenticationSucceeded {}.encode(), now);
            if std::env::var("MHI2Q_PHONE_QEMU_AUTH_FIRST").as_deref() == Ok("1") {
                println!("[phone-qemu] authentication before identification=1");
                engine.send(CONTROL_SESSION_ID, StartIdentification {}.encode(), now);
                *phase = Phase::Identification;
            } else {
                engine.send(
                    CONTROL_SESSION_ID,
                    RequestAccessoryWiFiConfigurationInformation {}.encode(),
                    now,
                );
                *phase = Phase::Wifi;
            }
        }
        (Phase::Wifi, 0x5703) => {
            let wifi =
                AccessoryWiFiConfigurationInformation::decode(frame).map_err(io::Error::other)?;
            if wifi.ssid.as_deref() != Some("QemuCarPlay") || wifi.channel != 11 {
                return Err(io::Error::other("wrong Wi-Fi configuration"));
            }
            println!("[phone-qemu] wifi=1");
            *phase = Phase::CarPlay;
        }
        (Phase::CarPlay | Phase::Recovering, 0x4301) => {
            let start = CarPlayStartSession::decode(frame).map_err(io::Error::other)?;
            if start.port != Some(u32::from(airplay_port()))
                || start.wireless_attributes.and_then(|attrs| attrs.channel) != Some(11)
                || start.device_identifier.as_deref() != Some("02:00:00:00:00:01")
                || start.public_key.as_ref().is_none_or(|id| {
                    mhi2q_wireless_daemon::control::validate_pairing_id(id).is_err()
                })
                || start.source_version.as_deref() != Some(airplay_source_version().as_str())
            {
                return Err(io::Error::other("wrong CarPlayStartSession"));
            }
            println!("[phone-qemu] CarPlayStartSession=1");
            if std::env::var_os("MHI2Q_PHONE_QEMU_HOLD_MEDIA").is_some() {
                std::fs::write("/dev/shmem/stock210_start_pi", start.public_key.unwrap())?;
            }
            *phase = Phase::Done;
        }
        _ => return Err(io::Error::other(format!("unexpected CSM {id:04x}"))),
    }
    Ok(())
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut integrated = if std::env::var_os("MHI2Q_PHONE_QEMU_INTEGRATED").is_some() {
        Some(integrated_fixture::Fixture::new()?)
    } else {
        None
    };
    let mut recovery = if std::env::var_os("MHI2Q_PHONE_QEMU_RECOVERY").is_some() {
        Some(recovery_fixture::Fixture::new()?)
    } else {
        None
    };
    let mut software_auth = SoftwarePhoneAuth::from_environment()?;
    let control = if integrated.is_none()
        && recovery.is_none()
        && std::env::var_os("MHI2Q_PHONE_QEMU_CONTROL").is_some()
    {
        Some(fake_control_phone()?)
    } else {
        None
    };
    let real_receiver = std::env::var_os("MHI2Q_PHONE_QEMU_REAL_RECEIVER").is_some();
    let stop = if real_receiver {
        stock_receiver_ready()?;
        None
    } else if recovery.is_some() || integrated.is_some() {
        None
    } else {
        Some(fake_receiver()?)
    };
    let mut stream = connect_daemon()?;
    stream.set_read_timeout(Some(Duration::from_millis(50)))?;
    stream.write_all(
        &BridgeFrame {
            kind: TYPE_CONNECTED,
            generation: 1,
            data: vec![6, 5, 4, 3, 2, 1],
        }
        .encode()?,
    )?;
    let mut generation = 1;
    let mut bootstrap_gap = None;
    let mut gap_done = false;
    let start = Instant::now();
    let mut engine = LinkEngine::new(LinkConfig::default());
    engine.start(true, 0);
    flush_link(&mut stream, &mut engine, generation)?;
    let mut bridge = FrameDecoder::default();
    let mut csm = CsmReader::default();
    let mut phase = Phase::Link;
    let mut last_availability_ms: Option<u64> = None;
    let mut input = [0u8; 4096];
    while start.elapsed()
        < Duration::from_secs(if integrated.is_some() {
            integrated_fixture::deadline_seconds()
        } else {
            65
        })
        && phase != Phase::Done
    {
        let now = start.elapsed().as_millis() as u64;
        if !gap_done
            && std::env::var("MHI2Q_INTEGRATED_FAULT").as_deref() == Ok("rfcomm-gap")
            && std::path::Path::new("/dev/shmem/integrated-rfcomm-gap").exists()
        {
            if bootstrap_gap.is_none() {
                stream.write_all(
                    &BridgeFrame {
                        kind: TYPE_DISCONNECTED,
                        generation,
                        data: vec![],
                    }
                    .encode()?,
                )?;
                bootstrap_gap = Some(Instant::now());
                println!(
                    "[integrated-media] RFCOMM disconnected while native IP media remains live"
                );
            }
            if bootstrap_gap.unwrap().elapsed() < Duration::from_secs(3) {
                thread::sleep(Duration::from_millis(20));
                continue;
            }
            generation += 1;
            stream.write_all(
                &BridgeFrame {
                    kind: TYPE_CONNECTED,
                    generation,
                    data: vec![6, 5, 4, 3, 2, 1],
                }
                .encode()?,
            )?;
            engine = LinkEngine::new(LinkConfig::default());
            engine.start(true, now);
            csm = CsmReader::default();
            phase = Phase::Link;
            last_availability_ms = None;
            gap_done = true;
            println!("[integrated-media] RFCOMM reopened without radio or Bluetooth bond reset");
        }
        match stream.read(&mut input) {
            Ok(0) => {
                if std::env::var("MHI2Q_INTEGRATED_FAULT").as_deref() == Ok("daemon-crash")
                    && generation == 1
                {
                    stream = connect_daemon()?;
                    stream.set_read_timeout(Some(Duration::from_millis(50)))?;
                    generation = 2;
                    stream.write_all(
                        &BridgeFrame {
                            kind: TYPE_CONNECTED,
                            generation,
                            data: vec![6, 5, 4, 3, 2, 1],
                        }
                        .encode()?,
                    )?;
                    bridge = FrameDecoder::default();
                    engine = LinkEngine::new(LinkConfig::default());
                    engine.start(true, now);
                    csm = CsmReader::default();
                    phase = Phase::Link;
                    last_availability_ms = None;
                    println!(
                        "[integrated-media] phone reauthenticated with restarted daemon on new RFCOMM generation"
                    );
                    flush_link(&mut stream, &mut engine, generation)?;
                    continue;
                }
                if phase == Phase::Rejected && std::env::var_os("MHI2Q_MFI_QEMU_TAMPER").is_some() {
                    println!("[phone-qemu] software AA04 closed bridge=1");
                    return Ok(());
                }
                if std::env::var("MHI2Q_PHONE_QEMU_EXPECT_MFI_TIMEOUT").as_deref() == Ok("1")
                    && phase == Phase::Certificate
                {
                    println!("[phone-qemu] MFi timeout closed bridge=1");
                    return Ok(());
                }
                return Err("daemon closed bridge".into());
            }
            Ok(n) => {
                for frame in bridge.feed(&input[..n])? {
                    if frame.generation < generation {
                        continue;
                    }
                    if frame.kind != TYPE_DATA || frame.generation != generation {
                        return Err("unexpected bridge frame".into());
                    }
                    engine.feed(&frame.data, now);
                }
            }
            Err(error)
                if matches!(
                    error.kind(),
                    io::ErrorKind::TimedOut | io::ErrorKind::WouldBlock
                ) => {}
            Err(error) => return Err(error.into()),
        }
        engine.advance_time(now);
        if engine.state() == LinkState::Dead {
            return Err("iAP2 link died".into());
        }
        while let Some(event) = engine.poll_event() {
            if let Event::FileTransfer(data)=&event {features_fixture::file(&mut engine,data,now)?;}
            if let Event::Control(data) = event {
                for frame in csm.push(&data).map_err(io::Error::other)? {
                    handle_csm(&mut engine, &mut phase, &frame, now, &mut software_auth)?;
                    if phase == Phase::Done {
                        if let Some(fixture) = integrated.as_mut() {
                            fixture.started();
                            phase = Phase::Recovering;
                        }
                        if let Some(fixture) = recovery.as_mut() {
                            if !fixture.started() {
                                phase = Phase::Recovering;
                            }
                        }
                    }
                }
            }
        }
        if engine.state() == LinkState::Normal && phase == Phase::Link {
            println!("[phone-qemu] iap2 normal=1");
            if std::env::var("MHI2Q_PHONE_QEMU_AUTH_FIRST").as_deref() == Ok("1") {
                engine.send(
                    CONTROL_SESSION_ID,
                    RequestAuthenticationCertificate {}.encode(),
                    now,
                );
                phase = Phase::Certificate;
            } else {
                engine.send(CONTROL_SESSION_ID, StartIdentification {}.encode(), now);
                phase = Phase::Identification;
            }
        }
        if phase == Phase::CarPlay
            && last_availability_ms.is_none_or(|last| now.saturating_sub(last) >= 9000)
        {
            engine.send(
                CONTROL_SESSION_ID,
                CarPlayAvailability {
                    wired_attributes: None,
                    wireless_attributes: Some(CarPlayAvailabilityWirelessAttributes {
                        available: Some(true),
                        bluetooth_transport_identifier: None,
                    }),
                }
                .encode(),
                now,
            );
            last_availability_ms = Some(now);
            println!("[phone-qemu] availability sent=1");
        }
        if let Some(fixture) = recovery.as_mut() {
            fixture.tick(&mut engine, now)?;
        }
        if let Some(fixture) = integrated.as_mut() {
            if fixture.tick(&mut engine, now)? {
                phase = Phase::Done;
            }
        }
        if let Err(error) = flush_link(&mut stream, &mut engine, generation) {
            if std::env::var("MHI2Q_INTEGRATED_FAULT").as_deref() != Ok("daemon-crash")
                || generation != 1
            {
                return Err(error.into());
            }
        }
    }
    if phase != Phase::Done {
        return Err("CarPlayStartSession timeout".into());
    }
    if let Some(fixture) = recovery.as_ref() {
        fixture.verify()?;
    }
    if let Some(control) = control {
        control.recv_timeout(Duration::from_secs(12))?;
    }
    if std::env::var_os("MHI2Q_PHONE_QEMU_HOLD_MEDIA").is_some() {
        // Retain the synthetic RFCOMM link while the separate independent
        // SRP/media client runs. EOF must come from the real supervisor drain.
        while start.elapsed() < Duration::from_secs(180) {
            let now = start.elapsed().as_millis() as u64;
            match stream.read(&mut input) {
                Ok(0) => {
                    println!("[phone-qemu] media owner ended RFCOMM generation=1");
                    println!("[phone-qemu] full daemon path=1");
                    return Ok(());
                }
                Ok(n) => {
                    for frame in bridge.feed(&input[..n])? {
                        if frame.kind == TYPE_DATA {
                            engine.feed(&frame.data, now);
                        }
                    }
                }
                Err(error)
                    if matches!(
                        error.kind(),
                        io::ErrorKind::TimedOut | io::ErrorKind::WouldBlock
                    ) => {}
                Err(error) => return Err(error.into()),
            }
            engine.advance_time(now);
            while engine.poll_event().is_some() {}
            flush_link(&mut stream, &mut engine, generation)?;
        }
        return Err("media owner failed to close RFCOMM generation".into());
    }
    drop(stream);
    if real_receiver {
        wait_stock_receiver_stopped()?;
    } else if let Some(stop) = stop {
        stop.recv_timeout(Duration::from_secs(8))?;
    }
    println!("[phone-qemu] full daemon path=1");
    if std::env::var_os("MHI2Q_OWNER_SUPERVISOR_QEMU").is_some() {
        std::fs::write("/dev/shmem/phone_done", b"1")?;
    }
    Ok(())
}
