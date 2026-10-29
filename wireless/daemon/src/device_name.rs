//! Phone-supplied iAP2 DeviceInformationUpdate (0x4E09), parameter 0.
//! A name is display text, never the persistent identity of a transport.

pub fn normalize(name: &str) -> Result<String, String> {
    if name.chars().any(char::is_control) {
        return Err("control character in phone name".into());
    }
    let name = name.trim();
    if name.is_empty() {
        return Err("empty phone name".into());
    }
    // SIW2 reserves 96 bytes including the NUL terminator. Keep UTF-8 whole.
    let mut end = name.len().min(95);
    while !name.is_char_boundary(end) { end -= 1; }
    Ok(name[..end].to_owned())
}

pub fn decode(frame: &[u8]) -> Result<Option<String>, String> {
    if crate::control::message_id(frame)? != 0x4E09 {
        return Err("not DeviceInformationUpdate".into());
    }
    let mut remaining = &frame[6..];
    let mut name = None;
    while !remaining.is_empty() {
        if remaining.len() < 4 { return Err("short device information parameter".into()); }
        let size = u16::from_be_bytes([remaining[0], remaining[1]]) as usize;
        let id = u16::from_be_bytes([remaining[2], remaining[3]]);
        if size < 4 || size > remaining.len() {
            return Err("invalid device information parameter length".into());
        }
        if id == 0 {
            if name.is_some() { return Err("duplicate phone name".into()); }
            let value = &remaining[4..size];
            let value = value.strip_suffix(&[0]).unwrap_or(value);
            name = Some(normalize(std::str::from_utf8(value)
                .map_err(|_| "invalid UTF-8 phone name")?)?);
        }
        remaining = &remaining[size..];
    }
    Ok(name)
}

#[cfg(test)]
pub(crate) fn frame(name: &[u8]) -> Vec<u8> {
    let mut out = vec![0x40, 0x40];
    out.extend_from_slice(&((10 + name.len()) as u16).to_be_bytes());
    out.extend_from_slice(&[0x4E, 0x09]);
    out.extend_from_slice(&((4 + name.len()) as u16).to_be_bytes());
    out.extend_from_slice(&[0, 0]);
    out.extend_from_slice(name);
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strict_name_tlv_and_unicode_boundaries() {
        assert_eq!(decode(&frame("  iPhone Луки 📱\0".as_bytes())).unwrap().as_deref(), Some("iPhone Луки 📱"));
        let long = "📱".repeat(40);
        let value = decode(&frame(long.as_bytes())).unwrap().unwrap();
        assert_eq!(value.len(), 92);
        for invalid in [b"".as_slice(), b"  ", b"a\0b", b"a\n", b"\xff", b"\xc0\xaf"] {
            assert!(decode(&frame(invalid)).is_err(), "{invalid:?}");
        }
        let valid = frame(b"Phone");
        for length in 0..valid.len() { assert!(decode(&valid[..length]).is_err()); }
        let mut duplicate = valid.clone();
        duplicate.extend_from_slice(&valid[6..]);
        let size = duplicate.len() as u16;
        duplicate[2..4].copy_from_slice(&size.to_be_bytes());
        assert!(decode(&duplicate).is_err());
        let mut short = valid.clone(); short[6..8].copy_from_slice(&3u16.to_be_bytes());
        assert!(decode(&short).is_err());
        let mut unknown = valid; unknown[8..10].copy_from_slice(&12u16.to_be_bytes());
        assert_eq!(decode(&unknown).unwrap(), None);
    }

    #[test]
    fn mutated_names_never_escape_the_si_text_contract() {
        let original = frame("Téléphone 📱".as_bytes());
        let mut seed = 0x4e09u32;
        for _ in 0..10000 {
            let mut bytes = original.clone();
            for _ in 0..3 {
                seed = seed.wrapping_mul(1664525).wrapping_add(1013904223);
                let index = (seed as usize) % bytes.len();
                bytes[index] ^= (seed >> 24) as u8;
            }
            if let Ok(Some(name)) = decode(&bytes) {
                assert!(!name.is_empty() && name.len() <= 95);
                assert!(!name.chars().any(char::is_control));
                assert_eq!(normalize(&name).unwrap(), name);
            }
        }
    }
}
