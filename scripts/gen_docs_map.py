"""Regenerate the generated blocks of docs/README.md: the full mindmap and the notes by topic.

Run from anywhere: python3 scripts/gen_docs_map.py
Fails if a note in a mapped folder is not placed in MAP, so a new note must be filed here first.
"""
import glob, os, re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(os.path.join(REPO, "docs"))

# domain folder -> (title, emoji, [(group, [files])])
MAP = [
 ("audio", "Audio", "🔊", [
   ("Upmix runtime", ["STEREO_TO_5_1_UPMIX", "UPMIX_SCENE_ALGORITHM_2026-10-08", "UPMIX_CALIBRATION_AND_TUNING", "UPMIX_COMPUTE_BUDGET", "UPMIX_CODE_RETIREMENT_2026-10-09"]),
   ("Upmix research and history", ["UPMIX_HISTORY_AND_RESULTS_2026-10-09", "UPMIX_REWORK_BASELINE_2026-10-07", "UPMIX_REWORK_PROPOSAL_2026-10-07", "UPMIX_RADICAL_DIRECTIONS_2026-10-07", "UPMIX_STABILITY_2026-09-25", "UPMIX_TRACKA_MATCHED40_2026-09-30", "UPMIX_TRAINING_HISTORY", "UPMIX_DATASETS_AND_TEACHERS", "UPMIX_LITERATURE", "UPMIX_DOMAIN_MODEL", "HSSR_MODEL"]),
   ("Listening and release", ["UPMIX_CARPLAY_LISTENING_PROTOCOL", "UPMIX_LISTENING_AND_DEPLOY_LOG", "UPMIX_RELEASE_AND_ACCEPTANCE"]),
   ("Amplifier and DSP", ["VIRTUAL_CABIN_AND_AMP_RE", "J525_WIRING_AND_SPEAKERS", "SHARC_B_PROCESSORS", "SHARC_DSP_BLOCKS", "INTERNAL_AMP_PATH"]),
   ("Codecs and routing", ["APAC_CODEC", "CINEMO_AAC_DECODER", "DOLBY_ATMOS", "DISCRETE_5_1_ROUTING", "BUFFERED_AUDIO_PATH", "BUFFERED_PLAYBACK_LIFECYCLE", "CALL_AND_SIRI_ROUTING", "QNX_AUDIO_TOPOLOGY", "IOS26_TYPE103_CONNECTION_TRIAGE_2026-10-03"]),
 ]),
 ("cluster-and-rgi", "Cluster and RGI", "📊", [
   ("Contexts and layout", ["CLUSTER_CONTEXTS", "KDK_GEOMETRY_AND_ANIMATION", "VC_LAYOUT_AND_FONTS", "VC_FPK_STATE_MACHINE", "PDC_SMALL_STAGE", "NAV_INITIALIZING_SCREEN"]),
   ("Route guidance", ["RGI_AND_BAP_BRIDGE", "IAP2_ROUTE_GUIDANCE_TLVS", "MANEUVER_MAPPING", "LANE_GUIDANCE", "DISTANCE_BARGRAPH", "HUD_RENDERER", "NAVSD_FCTID_MATRIX", "THIRD_PARTY_NAV_APPS"]),
   ("Phone and telephony", ["CARPLAY_TELEPHONY", "IAP2_COMMUNICATIONS", "VC_TELEPHONE_TAB", "PHONE_TAB_GATING", "SMARTPHONE_TAB_AND_KOMBI_DSI"]),
   ("Open research", ["CARPLAY_SPEED_LIMIT", "IAP2_ROAD_OBJECT_DETECTION"]),
 ]),
 ("altscreen-and-video", "AltScreen and Video", "🖥️", [
   ("Cluster video", ["ALTSCREEN_CLUSTER_VIDEO", "VIDEO_PIPELINE", "CLUSTER_LOADER", "CLUSTER_VIDEO_ENCODER", "RECONNECT_AND_LIFECYCLE"]),
   ("Pacing and input", ["ALTSCREEN_FRAME_RATE", "ALTSCREEN_PACING_VALIDATION", "ALTSCREEN_INPUT_COMMANDS"]),
   ("Features", ["COVER_ART_PIPELINE", "ENHANCED_SIRI_CONTRACT", "CARPLAY_VIDEO_PLAYBACK", "CARPLAY_FEATURE_SURVEY", "PARTIAL_360_CAMERA_PREFLIGHT"]),
   ("History", ["ALTSCREEN_RE_HISTORY"]),
 ]),
 ("connect-and-usb", "Connect and USB", "🔌", [
   ("USB session", ["USB_AND_NCM_CONNECT", "SESSION_LIFECYCLE", "IOS_VERSION_GATE", "HMI_STARTUP_AND_IDENTITY"]),
   ("Replug recovery", ["USB_REPLUG_RECOVERY", "SI_STACK_AND_REPLUG_RACE"]),
   ("Network and phone data", ["PF_FIREWALL", "VEHICLE_DATA_TO_PHONE", "NOW_PLAYING_COLD_START"]),
 ]),
 ("wireless-carplay", "Wireless CarPlay", "📶", [
   ("Receiver and ownership", ["STOCK210_ARCHITECTURE", "STOCK210_RECEIVER_LIFECYCLE", "RECEIVER_START", "RECEIVER_BRIDGE", "DIO_OWNERSHIP"]),
   ("Bluetooth graft", ["BTSTACK_GRAFT", "GRAFT_INSERTION_PLAN", "GRAFT_LAUNCHER_LIFECYCLE", "RFCOMM_CHANNEL_LAYOUT", "RFCOMM_DUPLEX", "BLUETOOTH_DISCOVERY"]),
   ("iAP2 and MFi bootstrap", ["IAP2_DAEMON", "IAP2_VENDOR_PROVENANCE", "TEST_SOFTWARE_MFI", "BOOTSTRAP_COMPARISON"]),
   ("Pairing and media", ["STOCK210_HTTP_PAIRING_ABI", "STOCK210_MEDIA_CRYPTO_ABI", "AIRPLAY280_PAIRING_RECIPE"]),
   ("iOS, radio and hardware", ["IOS_AUTH_SELECTION", "IOS_WIRELESS_SESSION_GATES", "WIFI_AND_DISCOVERY", "HARDWARE_AND_DONORS", "FIREWALL"]),
   ("Evidence", ["STOCK210_VALIDATION", "history/README", "history/MIGRATION"]),
 ]),
 ("carplay-lab", "CarPlay Laboratory", "🧪", [
   ("vphone sender", ["VPHONE_LAB", "VPHONE_TIMING", "NATIVE_SCREEN_TEST"]),
   ("Earlier labs and review", ["IOS_VM_LAB", "IOS210_CODE_REVIEW"]),
 ]),
 ("build-and-deploy", "Build and Deploy", "📦", [
   ("Build and release", ["BUILD_SYSTEM", "DEPLOY_AND_RELEASE", "SCENE_BACKEND_DEPLOY", "VIN_PROTECTION"]),
   ("Audits and recovery", ["PRODUCTION_AUDITS", "ISSUE24_RCC_MMX_RECOVERY", "RE_ARTIFACTS"]),
 ]),
 ("reference", "Reference", "📚", [
   ("System", ["ARCHITECTURE", "HOOK_TO_JAVA_BUS", "RECORD_REPLAY_TOOLING"]),
   ("Formats and ports", ["IOS_CARPLAY_SPATIAL_WIREFORMAT", "MHI2_MU1439_PORT"]),
   ("History and comparisons", ["ARCHIVE_AND_HISTORY", "REPOSITORY_COMPARISON_MHI2Q_EVO_2026-10-10"]),
 ]),
]

def title(path):
    with open(path) as f:
        return f.readline().lstrip("# ").strip()

def label(path):
    """Short mindmap label: H1 title without dates, parentheses or a subtitle after a colon."""
    t = title(path)
    t = re.sub(r"\s*\([^)]*\)", "", t)
    t = t.split(":")[0]
    t = re.sub(r"\s*\b\d{4}-\d{2}-\d{2}\b", "", t)
    return t.replace('"', "'").strip()

# completeness check: every note in a mapped folder must be placed exactly once
placed = []
for folder, _, _, groups in MAP:
    for _, files in groups:
        placed += [f"{folder}/{f}.md" for f in files]
missing_file = [p for p in placed if not os.path.isfile(p)]
assert not missing_file, missing_file
assert len(placed) == len(set(placed)), "duplicate placement"
expected = {f for folder, *_ in MAP for f in glob.glob(f"{folder}/*.md") if not f.endswith("/README.md")}
unplaced = expected - set(placed)
assert not unplaced, sorted(unplaced)

# notes by topic: the mindmap's order as links, each domain led by its overview
topics = []
for folder, name, emoji, groups in MAP:
    head = f"**{emoji} {name}**"
    if os.path.isfile(f"{folder}/README.md"):
        head += f" · [overview]({folder}/README.md)"
    topics += [head, ""]
    for g, files in groups:
        links = " · ".join(f"[{label(f'{folder}/{f}.md')}]({folder}/{f}.md)" for f in files)
        topics.append(f"- {g}: {links}")
    topics.append("")
topics.append("Across every domain: [Landmines](LANDMINES.md).")

def splice(path, name, body):
    """Replace the text between <!-- BEGIN generated:name --> and its END marker."""
    text = open(path).read()
    begin, end = f"<!-- BEGIN generated:{name} -->", f"<!-- END generated:{name} -->"
    i, j = text.index(begin) + len(begin), text.index(end)
    open(path, "w").write(text[:i] + "\n" + body + "\n" + text[j:])

# full mindmap for docs/README.md: every note under its domain and group
full = ["```mermaid", "mindmap", "  root((MHI2Q CarPlay docs))"]
for folder, name, emoji, groups in MAP:
    full.append(f"    {name}")
    for g, files in groups:
        full.append(f"      {g}")
        full += [f"        {label(f'{folder}/{f}.md')}" for f in files]
full.append("```")

splice("README.md", "full-mindmap", "\n".join(full))
splice("README.md", "notes-by-topic", "\n".join(topics))

print(len(placed), "notes placed in", len(MAP), "domains")
