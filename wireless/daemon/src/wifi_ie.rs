//! Apple Device IE for the MHI2Q's 2.4 GHz CarPlay access point.
//! The radio configuration and custom-IE installation are separate steps.

pub const IE_MASK_REQUIRED_FRAMES: u16 = 0x12a;

#[cfg(any(test, target_os = "nto"))]
const CONTROL_STOPPED: &str = "wifi control stopped:";

// A marker is acquired before spawning, and removed only after the child
// completes. A timeout, parent crash or failed cleanup keeps subsequent
// daemon generations from issuing another ioctl into an uncertain driver.
#[cfg(any(test, target_os = "nto"))]
struct ControlLease(std::path::PathBuf);

#[cfg(any(test, target_os = "nto"))]
impl ControlLease {
    fn begin(path: &std::path::Path) -> Result<Self, String> {
        use std::io::Write;
        #[cfg(unix)]
        use std::os::unix::fs::OpenOptionsExt;
        let mut options = std::fs::OpenOptions::new();
        options.write(true).create_new(true);
        #[cfg(unix)]
        options.mode(0o600);
        let mut marker = options.open(path).map_err(|error| {
            format!("{CONTROL_STOPPED} cannot acquire {}: {error}; verify HU recovery before removing a pending marker", path.display())
        })?;
        writeln!(
            marker,
            "uaputl pending, daemon pid={}; recover HU before clearing",
            std::process::id()
        )
        .map_err(|error| format!("{CONTROL_STOPPED} cannot write pending marker: {error}"))?;
        Ok(Self(path.to_owned()))
    }

    fn complete(self) -> Result<(), String> {
        std::fs::remove_file(&self.0)
            .map_err(|error| format!("{CONTROL_STOPPED} cannot release pending marker: {error}"))
    }
    // Deliberately no Drop cleanup: unwinding is not proof of ioctl completion.
}

pub fn parse_monitor_seconds(value: &str) -> Result<u64, String> {
    let seconds = value
        .parse::<u64>()
        .map_err(|_| "invalid IE monitor interval")?;
    if seconds != 0 && !(5..=3600).contains(&seconds) {
        return Err("IE monitor interval must be 0 (disabled) or 5..3600 seconds".into());
    }
    Ok(seconds)
}

#[cfg(target_os = "nto")]
pub fn monitor_seconds_from_env() -> Result<u64, String> {
    parse_monitor_seconds(
        &std::env::var("MHI2Q_CP_IE_MONITOR_SECONDS").unwrap_or_else(|_| "5".into()),
    )
}

#[cfg(any(test, target_os = "nto"))]
#[derive(Debug, PartialEq, Eq)]
struct CustomIeSlot {
    mask: u16,
    bytes: Vec<u8>,
}

// MU1316 uaputl's sys_cfg_custom_ie query prints each slot with these
// labels; its hexdump_data routine prints two hex digits per byte.
#[cfg(any(test, target_os = "nto"))]
fn parse_custom_ie_slot(output: &str, index: u8) -> Result<CustomIeSlot, String> {
    if !output.contains("Querying custom IE successful") {
        return Err("uaputl did not confirm the custom IE query".into());
    }
    let mut lines = output.lines();
    let label = format!("Index [{index}]");
    while let Some(line) = lines.next() {
        if line.trim() != label {
            continue;
        }
        let mask_line = lines.next().ok_or("custom IE mask missing")?.trim();
        let mask_hex = mask_line
            .strip_prefix("Management Subtype Mask = 0x")
            .ok_or("custom IE mask malformed")?;
        let mask = u16::from_str_radix(mask_hex, 16).map_err(|_| "custom IE mask malformed")?;
        let buffer_line = lines.next().ok_or("custom IE length missing")?.trim();
        let length: usize = buffer_line
            .strip_prefix("IE Buffer: len=")
            .ok_or("custom IE length malformed")?
            .parse()
            .map_err(|_| "custom IE length malformed")?;
        if length > 256 {
            return Err("custom IE length out of range".into());
        }
        let mut bytes = Vec::with_capacity(length);
        while bytes.len() < length {
            let line = lines.next().ok_or("custom IE data truncated")?;
            for token in line.split_whitespace() {
                if token.len() != 2 {
                    return Err("custom IE hex byte malformed".into());
                }
                bytes.push(
                    u8::from_str_radix(token, 16).map_err(|_| "custom IE hex byte malformed")?,
                );
                if bytes.len() > length {
                    return Err("custom IE data exceeds declared length".into());
                }
            }
        }
        return Ok(CustomIeSlot { mask, bytes });
    }
    // The firmware may return only populated TLVs. An absent index in a
    // successful query is therefore an unused slot, not a query failure.
    Ok(CustomIeSlot {
        mask: 0,
        bytes: Vec::new(),
    })
}

#[cfg(any(test, target_os = "nto"))]
fn needs_install(slot: &CustomIeSlot, expected: &[u8]) -> Result<bool, String> {
    if slot.bytes.is_empty() {
        return Ok(true);
    }
    if slot.bytes != expected {
        return Err("configured custom IE slot contains another entry".into());
    }
    Ok(slot.mask != IE_MASK_REQUIRED_FRAMES)
}

pub fn parse_mac(value: &str) -> Result<[u8; 6], String> {
    let parts: Vec<_> = value.split(':').collect();
    if parts.len() != 6 || parts.iter().any(|part| part.len() != 2) {
        return Err("MAC must contain six two-digit octets".into());
    }
    let mut address = [0u8; 6];
    for (output, part) in address.iter_mut().zip(parts) {
        *output = u8::from_str_radix(part, 16).map_err(|_| "invalid MAC hex digit")?;
    }
    Ok(address)
}

/// Flags: AirPlay bit 0, Wireless CarPlay bit 10, 2.4 GHz bit 14.
/// The internet and 5 GHz bits stay clear because this AP has neither.
pub fn device_ie(bt_mac: [u8; 6], device_id: [u8; 6]) -> Vec<u8> {
    let mut bytes = vec![
        0xdd, 0, // vendor-specific IE + payload length
        0x00, 0xa0, 0x40, 0x00, // Apple OUI + Device IE subtype
        0x00, 0x02, 0x80, 0x22, // Flags sub-IE
        0x06, 0x06, // Bluetooth MAC sub-IE
    ];
    bytes.extend_from_slice(&bt_mac);
    bytes.extend_from_slice(&[0x07, 0x06]); // stable Device ID sub-IE
    bytes.extend_from_slice(&device_id);
    bytes[1] = (bytes.len() - 2) as u8;
    bytes
}

pub fn uaputl_args(index: u8, ie: &[u8]) -> Result<Vec<String>, String> {
    // The firmware's own help documents indices 0..3. The unit reported
    // additional empty slots, but those should be verified before use.
    if index > 3 {
        return Err("uaputl index must be 0..3".into());
    }
    if ie.len() < 2 || ie[0] != 0xdd || ie[1] as usize != ie.len() - 2 {
        return Err("invalid vendor IE length".into());
    }
    let hex = ie
        .iter()
        .map(|byte| format!("{byte:02x}"))
        .collect::<String>();
    Ok(vec![
        "-i".into(),
        "uap0".into(),
        "sys_cfg_custom_ie".into(),
        index.to_string(),
        format!("0x{IE_MASK_REQUIRED_FRAMES:x}"),
        format!("0x{hex}"),
    ])
}

#[cfg(target_os = "nto")]
pub fn install_on_qnx(bt_mac: [u8; 6], device_id: [u8; 6]) -> Result<(), String> {
    let index = std::env::var("MHI2Q_CP_IE_INDEX")
        .map_err(|_| "missing MHI2Q_CP_IE_INDEX (must name a verified free slot)")?
        .parse::<u8>()
        .map_err(|_| "invalid MHI2Q_CP_IE_INDEX")?;
    let ie = device_ie(bt_mac, device_id);
    let args = uaputl_args(index, &ie)?;
    let binary =
        std::env::var("MHI2Q_CP_UAPUTL").unwrap_or_else(|_| "/mnt/app/armle/sbin/uaputl".into());
    let mut last_error = String::new();
    for _ in 0..20 {
        match ensure_on_qnx(&binary, index, &ie, &args) {
            Ok(()) => return Ok(()),
            Err(error) if error.starts_with(CONTROL_STOPPED) => return Err(error),
            Err(error) => last_error = error,
        }
        std::thread::sleep(std::time::Duration::from_millis(500));
    }
    Err(format!(
        "custom Device IE installation failed: {last_error}"
    ))
}

/// Clear only the exact Device IE this mod installed. A different IE at the
/// configured index belongs to someone else and must never be overwritten.
#[cfg(target_os = "nto")]
pub fn remove_on_qnx(bt_mac: [u8; 6], device_id: [u8; 6]) -> Result<(), String> {
    let index = std::env::var("MHI2Q_CP_IE_INDEX")
        .map_err(|_| "missing MHI2Q_CP_IE_INDEX")?
        .parse::<u8>()
        .map_err(|_| "invalid MHI2Q_CP_IE_INDEX")?;
    if index > 3 {
        return Err("uaputl index must be 0..3".into());
    }
    let binary =
        std::env::var("MHI2Q_CP_UAPUTL").unwrap_or_else(|_| "/mnt/app/armle/sbin/uaputl".into());
    let expected = device_ie(bt_mac, device_id);
    let query = run_uaputl_command(&binary, &["-i", "uap0", "sys_cfg_custom_ie"])?;
    let slot = parse_custom_ie_slot(&query, index)?;
    if slot.bytes.is_empty() {
        return Ok(());
    }
    if slot.bytes != expected {
        return Err("configured custom IE slot contains another entry".into());
    }
    let slot_arg = index.to_string();
    let set = run_uaputl_command(
        &binary,
        &["-i", "uap0", "sys_cfg_custom_ie", &slot_arg, "0"],
    )?;
    if !set.contains("custom IE setting successful") {
        return Err("uaputl did not confirm custom IE removal".into());
    }
    let verify = run_uaputl_command(&binary, &["-i", "uap0", "sys_cfg_custom_ie"])?;
    if !parse_custom_ie_slot(&verify, index)?.bytes.is_empty() {
        return Err("custom IE remains after removal".into());
    }
    Ok(())
}

#[cfg(target_os = "nto")]
pub fn run_uaputl_command(binary: &str, args: &[&str]) -> Result<String, String> {
    use std::ffi::{CString, c_char};

    unsafe extern "C" {
        fn mhi2q_qnx_capture(
            path: *const c_char,
            argv: *const *mut c_char,
            output: *mut c_char,
            capacity: u32,
            incomplete: *mut i32,
        ) -> i32;
    }
    let path = CString::new(binary).map_err(|_| "uaputl path contains NUL")?;
    let mut values = Vec::with_capacity(args.len() + 1);
    values.push(path.clone());
    for arg in args {
        values.push(CString::new(*arg).map_err(|_| "uaputl argument contains NUL")?);
    }
    let mut argv = values
        .iter()
        .map(|arg| arg.as_ptr() as *mut c_char)
        .collect::<Vec<_>>();
    argv.push(std::ptr::null_mut());
    let mut output = vec![0u8; 16 * 1024];
    let lease = ControlLease::begin(std::path::Path::new("/tmp/mhi2q_wifi_control.pending"))?;
    let mut incomplete = 0;
    // SAFETY: all C strings and argv pointers live through this synchronous
    // call; output is writable for its full reported capacity.
    let length = unsafe {
        mhi2q_qnx_capture(
            path.as_ptr(),
            argv.as_ptr(),
            output.as_mut_ptr().cast(),
            output.len() as u32,
            &mut incomplete,
        )
    };
    if incomplete != 0 {
        return Err(format!(
            "{CONTROL_STOPPED} command did not complete (errno {}); pending marker retained; HU recovery required",
            -length
        ));
    }
    lease.complete()?;
    if length < 0 {
        return Err(format!("uaputl spawn/capture failed: errno {}", -length));
    }
    Ok(String::from_utf8_lossy(&output[..length as usize]).into_owned())
}

#[cfg(target_os = "nto")]
fn ensure_on_qnx(binary: &str, index: u8, ie: &[u8], args: &[String]) -> Result<(), String> {
    let query = run_uaputl_command(binary, &["-i", "uap0", "sys_cfg_custom_ie"])?;
    let slot = parse_custom_ie_slot(&query, index)?;
    if !needs_install(&slot, ie)? {
        return Ok(());
    }
    let refs = args.iter().map(String::as_str).collect::<Vec<_>>();
    let set = run_uaputl_command(binary, &refs)?;
    if !set.contains("custom IE setting successful") {
        return Err("uaputl did not confirm the custom IE set".into());
    }
    let verify = run_uaputl_command(binary, &["-i", "uap0", "sys_cfg_custom_ie"])?;
    let slot = parse_custom_ie_slot(&verify, index)?;
    if slot.bytes != ie || slot.mask != IE_MASK_REQUIRED_FRAMES {
        return Err("custom IE readback does not match requested value".into());
    }
    Ok(())
}

#[cfg(target_os = "nto")]
pub fn monitor_on_qnx(
    bt_mac: [u8; 6],
    device_id: [u8; 6],
    active: &std::sync::atomic::AtomicBool,
) -> Result<(), String> {
    use std::sync::atomic::Ordering;
    let interval = monitor_seconds_from_env()?;
    if interval == 0 {
        return Ok(());
    }
    let index = std::env::var("MHI2Q_CP_IE_INDEX")
        .map_err(|_| "missing MHI2Q_CP_IE_INDEX")?
        .parse::<u8>()
        .map_err(|_| "invalid MHI2Q_CP_IE_INDEX")?;
    let ie = device_ie(bt_mac, device_id);
    let args = uaputl_args(index, &ie)?;
    let binary =
        std::env::var("MHI2Q_CP_UAPUTL").unwrap_or_else(|_| "/mnt/app/armle/sbin/uaputl".into());
    let mut last_error = String::new();
    while active.load(Ordering::Acquire) {
        for _ in 0..(interval * 2) {
            if !active.load(Ordering::Acquire) {
                return Ok(());
            }
            std::thread::sleep(std::time::Duration::from_millis(500));
        }
        if !active.load(Ordering::Acquire) {
            break;
        }
        match ensure_on_qnx(&binary, index, &ie, &args) {
            Ok(()) => last_error.clear(),
            Err(error) if error.starts_with(CONTROL_STOPPED) => return Err(error),
            Err(error) if error != last_error => {
                eprintln!("[wifi-ie] {error}");
                last_error = error;
            }
            Err(_) => {}
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn control_lease_survives_abandonment_and_excludes_other_generations() {
        let root = std::env::temp_dir().join(format!("mhi2q-ie-lease-{}", std::process::id()));
        std::fs::create_dir(&root).unwrap();
        let marker = root.join("pending");
        let lease = ControlLease::begin(&marker).unwrap();
        assert!(ControlLease::begin(&marker).is_err());
        lease.complete().unwrap();
        assert!(!marker.exists());
        let interrupted = ControlLease::begin(&marker).unwrap();
        drop(interrupted);
        assert!(ControlLease::begin(&marker).is_err());
        std::fs::remove_file(&marker).unwrap();
        assert!(ControlLease::begin(&root.join("missing-parent/pending")).is_err());
        assert!(!root.join("missing-parent").exists());
        std::fs::remove_dir(root).unwrap();
    }

    #[test]
    fn first_test_can_disable_periodic_radio_queries() {
        assert_eq!(parse_monitor_seconds("0").unwrap(), 0);
        assert_eq!(parse_monitor_seconds("5").unwrap(), 5);
        assert_eq!(parse_monitor_seconds("3600").unwrap(), 3600);
        for invalid in ["", "-1", "1", "4", "3601", "no"] {
            assert!(parse_monitor_seconds(invalid).is_err());
        }
    }

    #[test]
    fn carplay_ie_and_marvell_frame_mask() {
        let bt = parse_mac("00:11:22:33:44:55").unwrap();
        let id = parse_mac("02:00:22:44:66:88").unwrap();
        let ie = device_ie(bt, id);
        assert_eq!(
            ie,
            [
                0xdd, 0x18, 0x00, 0xa0, 0x40, 0x00, 0x00, 0x02, 0x80, 0x22, 0x06, 0x06, 0x00, 0x11,
                0x22, 0x33, 0x44, 0x55, 0x07, 0x06, 0x02, 0x00, 0x22, 0x44, 0x66, 0x88,
            ]
        );
        let args = uaputl_args(3, &ie).unwrap();
        assert_eq!(args[4], "0x12a");
        assert_eq!(
            args[5],
            "0xdd1800a040000002802206060011223344550706020022446688"
        );
        // Association/reassociation response, probe response, beacon.
        assert_eq!(
            IE_MASK_REQUIRED_FRAMES & (2 | 8 | 32 | 256),
            2 | 8 | 32 | 256
        );
        assert_eq!(IE_MASK_REQUIRED_FRAMES & 1, 0);
    }

    #[test]
    fn reject_malformed_addresses_and_elements() {
        assert!(parse_mac("1:02:03:04:05:06").is_err());
        assert!(parse_mac("GG:02:03:04:05:06").is_err());
        assert!(uaputl_args(4, &[0xdd, 0]).is_err());
        assert!(uaputl_args(0, &[0xdd, 1]).is_err());
    }

    #[test]
    fn query_readback_and_slot_ownership() {
        let ie = device_ie([0, 1, 2, 3, 4, 5], [6, 7, 8, 9, 10, 11]);
        let hex = ie
            .chunks(16)
            .map(|line| {
                line.iter()
                    .map(|byte| format!("{byte:02x}"))
                    .collect::<Vec<_>>()
                    .join(" ")
            })
            .collect::<Vec<_>>()
            .join("\n");
        let output = format!(
            "Querying custom IE successful\nIndex [1]\nManagement Subtype Mask = 0x00\nIE Buffer: len=0\n\nIndex [2]\nManagement Subtype Mask = 0x12a\nIE Buffer: len={}\n{}\n",
            ie.len(),
            hex
        );
        let empty = parse_custom_ie_slot(&output, 1).unwrap();
        assert!(needs_install(&empty, &ie).unwrap());
        let owned = parse_custom_ie_slot(&output, 2).unwrap();
        assert_eq!(owned.bytes, ie);
        assert!(!needs_install(&owned, &ie).unwrap());
        assert!(
            needs_install(
                &CustomIeSlot {
                    mask: 0,
                    bytes: ie.clone()
                },
                &ie
            )
            .unwrap()
        );
        assert!(
            needs_install(
                &CustomIeSlot {
                    mask: 0x12a,
                    bytes: vec![0xdd, 0]
                },
                &ie
            )
            .is_err()
        );
        assert!(parse_custom_ie_slot("Index [2]\n", 2).is_err());
        assert!(
            needs_install(
                &parse_custom_ie_slot("Querying custom IE successful\n", 2).unwrap(),
                &ie
            )
            .unwrap()
        );
    }
}
