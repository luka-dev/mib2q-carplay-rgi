//! Shared identity from the same commented JSON configuration as stock DIO.

use std::io::Read;
use std::path::PathBuf;

const MAX_CONFIG_BYTES: u64 = 1024 * 1024;

#[derive(Debug, PartialEq, Eq)]
pub(crate) struct FactoryIdentity {
    pub name: String,
    pub manufacturer: String,
    pub firmware: String,
}

pub(crate) fn config_path(get: &impl Fn(&str) -> Option<String>) -> Result<PathBuf, String> {
    let directory = get("IPL_CONFIG_DIR_DIO_MANAGER")
        .or_else(|| get("IPL_CONFIG_DIR"))
        .unwrap_or_else(|| "/etc/eso/production".into());
    if directory.trim().is_empty() {
        return Err("empty factory DIO configuration directory".into());
    }
    Ok(PathBuf::from(directory).join("dio_manager.json"))
}

impl FactoryIdentity {
    pub fn load(get: &impl Fn(&str) -> Option<String>) -> Result<Self, String> {
        let path = config_path(get)?;
        let file = std::fs::File::open(&path)
            .map_err(|e| format!("cannot read {}: {e}", path.display()))?;
        let mut bytes = Vec::new();
        file.take(MAX_CONFIG_BYTES + 1)
            .read_to_end(&mut bytes)
            .map_err(|e| format!("cannot read {}: {e}", path.display()))?;
        if bytes.len() as u64 > MAX_CONFIG_BYTES {
            return Err(format!("{} exceeds 1 MiB", path.display()));
        }
        let text =
            String::from_utf8(bytes).map_err(|_| format!("{} is not UTF-8", path.display()))?;
        Self::parse(&text).map_err(|e| format!("{}: {e}", path.display()))
    }

    fn parse(text: &str) -> Result<Self, String> {
        // Factory files contain #/## comments. Preserve newlines and never
        // treat a hash or an escaped quote inside a JSON string as a comment.
        let mut bytes = text.as_bytes().to_vec();
        let (mut quoted, mut escaped, mut comment) = (false, false, false);
        for byte in &mut bytes {
            if comment {
                if *byte == b'\n' || *byte == b'\r' {
                    comment = false;
                } else {
                    *byte = b' ';
                }
            } else if quoted {
                if escaped {
                    escaped = false;
                } else if *byte == b'\\' {
                    escaped = true;
                } else if *byte == b'"' {
                    quoted = false;
                }
            } else if *byte == b'"' {
                quoted = true;
            } else if *byte == b'#' {
                comment = true;
                *byte = b' ';
            }
        }
        let clean = std::str::from_utf8(&bytes).map_err(|_| "invalid UTF-8 configuration")?;
        let root = json::parse(clean).map_err(|e| format!("invalid DIO JSON: {e}"))?;
        let identity = &root["iap2"]["accessoryIdentification"];
        let field = |key: &str| -> Result<String, String> {
            let value = identity[key].as_str().ok_or_else(|| {
                format!("missing/string required: iap2.accessoryIdentification.{key}")
            })?;
            if value.trim().is_empty() || value.len() > 255 || value.chars().any(char::is_control) {
                return Err(format!("invalid iap2.accessoryIdentification.{key}"));
            }
            Ok(value.to_owned())
        };
        Ok(Self {
            name: field("displayName")?,
            manufacturer: field("manufacturer")?,
            firmware: field("firmwareVersion")?,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn factory_comments_strings_and_unicode() {
        let text = "# header\r\n{\n ## section\n\"iap2\":{\"accessoryIdentification\":{\
            \"displayName\":\"MMI # \\\"Audi\\\" \\u00dc\",\
            \"manufacturer\":\"HARMAN\",\"firmwareVersion\":\"MU1316\"}}} # tail";
        assert_eq!(
            FactoryIdentity::parse(text).unwrap(),
            FactoryIdentity {
                name: "MMI # \"Audi\" Ü".into(),
                manufacturer: "HARMAN".into(),
                firmware: "MU1316".into()
            }
        );
    }

    #[test]
    fn malformed_or_incomplete_identity_is_not_defaulted() {
        for text in ["", "{}", "[]", "{", "{} trailing", "{\"iap2\":null}"] {
            assert!(FactoryIdentity::parse(text).is_err(), "{text}");
        }
        for value in [
            "null",
            "42",
            "true",
            "[]",
            "{}",
            "\"\"",
            "\"  \"",
            "\"x\\u0000y\"",
            "\"x\\ny\"",
        ] {
            let text = format!(
                r#"{{"iap2":{{"accessoryIdentification":{{"displayName":{value},"manufacturer":"HARMAN","firmwareVersion":"MU1316"}}}}}}"#
            );
            assert!(FactoryIdentity::parse(&text).is_err(), "{value}");
        }
        let text = format!(
            r#"{{"iap2":{{"accessoryIdentification":{{"displayName":"{}","manufacturer":"HARMAN","firmwareVersion":"MU1316"}}}}}}"#,
            "a".repeat(256)
        );
        assert!(FactoryIdentity::parse(&text).is_err());
    }

    #[test]
    fn directory_matches_factory_precedence() {
        assert_eq!(
            config_path(&|_| None).unwrap(),
            PathBuf::from("/etc/eso/production/dio_manager.json")
        );
        assert_eq!(
            config_path(&|key| (key == "IPL_CONFIG_DIR").then(|| "/base".into())).unwrap(),
            PathBuf::from("/base/dio_manager.json")
        );
        assert_eq!(
            config_path(&|key| Some(
                if key == "IPL_CONFIG_DIR_DIO_MANAGER" {
                    "/dio"
                } else {
                    "/base"
                }
                .into()
            ))
            .unwrap(),
            PathBuf::from("/dio/dio_manager.json")
        );
        assert!(config_path(&|_| Some("".into())).is_err());
    }
}
