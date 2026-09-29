#[cfg(not(target_os = "nto"))]
use mhi2q_wireless_daemon::control::UnavailableAuth;
use mhi2q_wireless_daemon::{
    control::Config, receiver_control::LoopbackReceiverControl, serve_stream,
};
#[cfg(target_os = "nto")]
use mhi2q_wireless_daemon::{
    mfi::{MfiChip, qnx::QnxBus},
    wifi_ie,
};
use std::net::TcpListener;
#[cfg(target_os = "nto")]
use std::sync::{
    Arc,
    atomic::{AtomicBool, Ordering},
};
use std::time::{Duration, Instant};

fn main() -> std::io::Result<()> {
    let argument = std::env::args().nth(1);
    #[cfg(target_os = "nto")]
    if argument.as_deref() == Some("--clear-ie") {
        let bt = std::env::var("MHI2Q_CP_BT_MAC").map_err(std::io::Error::other)?;
        let device_id = std::env::var("MHI2Q_CP_DEVICE_ID").map_err(std::io::Error::other)?;
        return wifi_ie::remove_on_qnx(
            wifi_ie::parse_mac(&bt).map_err(std::io::Error::other)?,
            wifi_ie::parse_mac(&device_id).map_err(std::io::Error::other)?,
        )
        .map_err(std::io::Error::other);
    }
    if argument.as_deref() == Some("--check-config") {
        let config = Config::from_env().map_err(std::io::Error::other)?;
        #[cfg(target_os = "nto")]
        {
            QnxBus::from_env().map_err(std::io::Error::other)?;
            wifi_ie::parse_mac(&config.device_id).map_err(std::io::Error::other)?;
            let index = std::env::var("MHI2Q_CP_IE_INDEX").map_err(std::io::Error::other)?;
            let index = index.parse::<u8>().map_err(std::io::Error::other)?;
            if index > 3 {
                return Err(std::io::Error::other("custom IE index must be 0..3"));
            }
        }
        #[cfg(not(target_os = "nto"))]
        let _ = config;
        return Ok(());
    }
    if argument.is_some() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "unknown daemon argument",
        ));
    }
    let config = Config::from_env().map_err(std::io::Error::other)?;
    let receiver_control = LoopbackReceiverControl::new(config.airplay_port)?;
    #[cfg(target_os = "nto")]
    let mfi_bus = QnxBus::from_env().map_err(std::io::Error::other)?;
    // Reserve the transport before advertising wireless CarPlay. If another
    // daemon owns the port, startup fails without leaving a fresh Wi-Fi IE.
    let listener = TcpListener::bind("127.0.0.1:57342")?;
    eprintln!("[bridge] listening on 127.0.0.1:57342");
    for accepted in listener.incoming() {
        let mut stream = accepted?;
        stream.set_read_timeout(Some(Duration::from_millis(20)))?;
        stream.set_write_timeout(Some(Duration::from_secs(2)))?;
        #[cfg(target_os = "nto")]
        let (ie_active, ie_monitor) = {
            let bt_mac = config.bt_mac;
            let device_id = wifi_ie::parse_mac(&config.device_id).map_err(std::io::Error::other)?;
            if let Err(error) = wifi_ie::install_on_qnx(bt_mac, device_id) {
                let _ = wifi_ie::remove_on_qnx(bt_mac, device_id);
                return Err(std::io::Error::other(error));
            }
            let active = Arc::new(AtomicBool::new(true));
            let monitor_active = Arc::clone(&active);
            let monitor = match std::thread::Builder::new()
                .name("mhi2q-wifi-ie".into())
                .spawn(move || {
                    if let Err(error) = wifi_ie::monitor_on_qnx(bt_mac, device_id, &monitor_active)
                    {
                        eprintln!("[wifi-ie] monitor stopped: {error}");
                    }
                }) {
                Ok(monitor) => monitor,
                Err(error) => {
                    let _ = wifi_ie::remove_on_qnx(bt_mac, device_id);
                    return Err(error);
                }
            };
            (active, monitor)
        };
        let start = Instant::now();
        #[cfg(target_os = "nto")]
        let result = serve_stream(
            &mut stream,
            || start.elapsed().as_millis() as u64,
            &config,
            || MfiChip::new(mfi_bus.clone()),
            receiver_control,
        );
        #[cfg(not(target_os = "nto"))]
        let result = serve_stream(
            &mut stream,
            || start.elapsed().as_millis() as u64,
            &config,
            || UnavailableAuth,
            receiver_control,
        );
        #[cfg(target_os = "nto")]
        {
            ie_active.store(false, Ordering::Release);
            if ie_monitor.join().is_err() {
                eprintln!("[wifi-ie] monitor panicked");
            }
            wifi_ie::remove_on_qnx(
                config.bt_mac,
                wifi_ie::parse_mac(&config.device_id).map_err(std::io::Error::other)?,
            )
            .map_err(std::io::Error::other)?;
        }
        if let Err(error) = result {
            eprintln!("[bridge] session ended: {error}");
        }
    }
    Ok(())
}
