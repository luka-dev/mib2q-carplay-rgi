"""Prepare reversible Maps-rate controls for an isolated owned VM.

These functions do no I/O. The caller must journal the original preference,
read back the write, and notify CARSession after applying and restoring it.
The profile override is a phone-side causal control. The PPID helper emits
the accessory input that selects the installed version-40 Maps-rate profile;
it is not wired into the production hook.
"""
from copy import deepcopy
import struct

from tests.carplay_lab.legacy_ios.usb_probe import parameter, parse_control_message

USER_INFO = "CRCapabilitiesUserInfoKey"
MAPS_RATE = "mapsRefreshRate"
# Installed CarExperienceAssets version 40; this is an asset match, not a
# protocol-level FPS setting. Only use on an isolated, owned lab connection.
MAPS30_PROFILE_PPID = "bf889dfdaea54096"


def with_maps30_ppid(identification):
    """Append the tested profile selector without changing other raw TLVs.

    The caller must journal and restore the phone's automatically persisted
    capabilities. Existing accessory identities are never replaced silently.
    """
    if not isinstance(identification, bytes) or len(identification) > 4096:
        raise ValueError("bounded IdentificationInfo bytes required")
    message, fields = parse_control_message(identification)
    if message != 0x1D01:
        raise ValueError("IdentificationInfo required")
    ppid = MAPS30_PROFILE_PPID.encode("ascii") + b"\0"
    if 34 in fields:
        if fields[34] != ppid:
            raise ValueError("preserve the existing accessory PPID")
        return identification
    params = identification[6:] + parameter(34, ppid)
    if len(params) + 6 > 4096:
        raise ValueError("IdentificationInfo exceeds the lab bound")
    return struct.pack(">HHH", 0x4040, len(params) + 6, message) + params


def _profile(capabilities, identifier):
    if not isinstance(capabilities, dict):
        raise ValueError("CarCapabilities must be a dictionary")
    profile = capabilities.get(identifier)
    if not isinstance(profile, dict) or not isinstance(profile.get(USER_INFO), dict):
        raise ValueError("the selected existing vehicle profile must have a user-info dictionary")
    return profile


def with_maps_rate(capabilities, identifier, rate=30):
    if type(rate) is not int or rate != 30:
        raise ValueError("this diagnostic only supports a target of 30")
    _profile(capabilities, identifier)
    result = deepcopy(capabilities)
    result[identifier][USER_INFO][MAPS_RATE] = rate
    return result


def restore_maps_rate(current, original, identifier, expected_rate=30):
    """Restore only our key; reject conflicts and preserve unrelated writes."""
    old_info = _profile(original, identifier)[USER_INFO]
    now_info = _profile(current, identifier)[USER_INFO]
    if ((MAPS_RATE in now_info) != (MAPS_RATE in old_info) or
            now_info.get(MAPS_RATE) != old_info.get(MAPS_RATE)):
        if now_info.get(MAPS_RATE) != expected_rate:
            raise ValueError("Maps rate changed outside this diagnostic; retain restore journal")
    result = deepcopy(current)
    info = result[identifier][USER_INFO]
    if MAPS_RATE in old_info:
        info[MAPS_RATE] = deepcopy(old_info[MAPS_RATE])
    else:
        info.pop(MAPS_RATE, None)
    return result
