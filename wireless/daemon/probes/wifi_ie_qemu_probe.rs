//! Disposable QEMU probe for the QNX custom-IE install and restore path.
#[cfg(target_os = "nto")]
fn main() {
    use mhi2q_wireless_daemon::wifi_ie;
    use std::sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    };
    use std::time::Duration;

    let bt_mac = [0, 1, 2, 3, 4, 5];
    let ap_mac = [6, 7, 8, 9, 10, 11];
    wifi_ie::install_on_qnx(bt_mac, ap_mac).expect("initial custom IE install");
    eprintln!("[wifi-ie-qemu] initial=ok");
    let active = Arc::new(AtomicBool::new(true));
    let monitor_active = Arc::clone(&active);
    let monitor = std::thread::spawn(move || {
        wifi_ie::monitor_on_qnx(bt_mac, ap_mac, &monitor_active).expect("custom IE monitor");
    });
    std::thread::sleep(Duration::from_secs(12));
    let binary = std::env::var("MHI2Q_CP_UAPUTL").expect("fake uaputl path");
    let text = wifi_ie::run_uaputl_command(&binary, &["--stats"]).expect("fake uaputl stats");
    eprintln!("[wifi-ie-qemu] {}", text.trim());
    assert!(text.contains("sets=2 clears=0 queries="));
    eprintln!("[wifi-ie-qemu] restore=ok");
    active.store(false, Ordering::Release);
    monitor.join().expect("custom IE monitor join");
    wifi_ie::remove_on_qnx(bt_mac, ap_mac).expect("remove owned custom IE");
    let text = wifi_ie::run_uaputl_command(&binary, &["--stats"]).expect("fake uaputl stats");
    eprintln!("[wifi-ie-qemu] after removal {}", text.trim());
    assert!(text.contains("sets=2 clears=1 queries="));
    assert!(text.contains("present=0"));
    eprintln!("[wifi-ie-qemu] removal=ok");
}

#[cfg(not(target_os = "nto"))]
fn main() {
    panic!("the Wi-Fi IE QEMU probe requires QNX");
}
