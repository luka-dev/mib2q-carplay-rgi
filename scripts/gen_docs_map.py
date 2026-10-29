"""Regenerate the generated blocks of docs/README.md: the full mindmap and the notes by topic.

Run from anywhere: python3 scripts/gen_docs_map.py
Fails if a note in a mapped folder is not placed in MAP, so a new note must be filed here first.
"""
import argparse, glob, os, re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true", help="fail if generated blocks are stale; do not write")
args = parser.parse_args()

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(os.path.join(REPO, "docs"))

# domain folder -> (title, emoji, [(group, [files])])
MAP = [
 ("stock-patches", "Stock component patches", "🔧", [
   ("Libraries", ["devnp-mrvl_wlan-sdiorm.so", "libairplay.so", "deva-ctrl-qc.so"]),
   ("Executables and configuration", ["smartphone_integrator", "mcd", "CONFIGURATION_AND_HOOKS"]),
   ("Evidence and decisions", ["PERFORMANCE", "REJECTED_CONCEPTS"]),
 ]),
 ("audio", "Audio", "🔊", [
   ("Upmix runtime", ["STEREO_TO_5_1_UPMIX", "UPMIX_SCENE_ALGORITHM_2026-10-08", "UPMIX_COMPUTE_BUDGET"]),
   ("Research references", ["UPMIX_DATASETS_AND_TEACHERS", "UPMIX_LITERATURE"]),
   ("Listening and release", []),
   ("Amplifier and DSP", ["VIRTUAL_CABIN_AND_AMP_RE", "ODIS_AMP_MAPPING", "J525_WIRING_AND_SPEAKERS", "SHARC_B_PROCESSORS", "SHARC_DSP_BLOCKS", "INTERNAL_AMP_PATH"]),
   ("Codecs and routing", ["APAC_CODEC", "CINEMO_AAC_DECODER", "DOLBY_ATMOS", "DISCRETE_5_1_ROUTING", "BUFFERED_AUDIO_PATH", "BUFFERED_PLAYBACK_LIFECYCLE", "CALL_AND_SIRI_ROUTING", "QNX_AUDIO_TOPOLOGY"]),
 ]),
 ("cluster-and-rgi", "Cluster and RGI", "📊", [
   ("Contexts and layout", ["CLUSTER_CONTEXTS", "KDK_GEOMETRY_AND_ANIMATION", "VC_LAYOUT_AND_FONTS", "VC_FPK_STATE_MACHINE", "PDC_SMALL_STAGE", "NAV_INITIALIZING_SCREEN", "STOCK_NAV_READINESS"]),
   ("Route guidance", ["RGI_AND_BAP_BRIDGE", "IAP2_ROUTE_GUIDANCE_TLVS", "MANEUVER_MAPPING", "LANE_GUIDANCE", "DISTANCE_BARGRAPH", "HUD_RENDERER", "NAVSD_FCTID_MATRIX", "THIRD_PARTY_NAV_APPS"]),
   ("Phone and telephony", ["CARPLAY_TELEPHONY", "IAP2_COMMUNICATIONS", "VC_TELEPHONE_TAB", "SMARTPHONE_TAB_AND_KOMBI_DSI"]),
   ("Open research", ["CARPLAY_SPEED_LIMIT", "IAP2_ROAD_OBJECT_DETECTION"]),
 ]),
 ("altscreen-and-video", "AltScreen and Video", "🖥️", [
   ("Cluster video", ["ALTSCREEN_CLUSTER_VIDEO", "VIDEO_PIPELINE", "CLUSTER_LOADER", "CLUSTER_VIDEO_ENCODER", "RECONNECT_AND_LIFECYCLE"]),
   ("Pacing and input", ["ALTSCREEN_FRAME_RATE", "ALTSCREEN_PACING_VALIDATION", "ALTSCREEN_INPUT_COMMANDS", "WIRED_30FPS_INVESTIGATION", "MAPS_FRAME_RATE_PROFILE_MECHANISM", "MAPS_30FPS_PROFILE", "PPID_PROFILE_COMPARISON", "DECODER_QUEUE_PATCH", "WIRED_PERFORMANCE_2026-10-21"]),
   ("Features", ["COVER_ART_PIPELINE", "ENHANCED_SIRI_CONTRACT", "CARPLAY_VIDEO_PLAYBACK", "CARPLAY_FEATURE_SURVEY", "PARTIAL_360_CAMERA_PREFLIGHT"]),
   ("History", []),
 ]),
 ("connect-and-usb", "Connect and USB", "🔌", [
   ("USB session", ["USB_AND_NCM_CONNECT", "SESSION_LIFECYCLE", "HMI_STARTUP_AND_IDENTITY"]),
   ("Replug recovery", ["USB_REPLUG_RECOVERY", "SI_STACK_AND_REPLUG_RACE"]),
   ("Network and phone data", ["PF_FIREWALL", "VEHICLE_DATA_TO_PHONE", "NOW_PLAYING_COLD_START"]),
 ]),
 ("wireless-carplay", "Wireless CarPlay", "📶", [
   ("Receiver and ownership", ["STOCK210_ARCHITECTURE", "STOCK210_RECEIVER_LIFECYCLE", "STOCK210_HU_TEST", "RECEIVER_START", "RECEIVER_BRIDGE", "DIO_OWNERSHIP", "SI_WIRELESS_EXTENSION_PLAN", "SI_MH2P_COMPARATIVE_RE", "SI_STAGE1_HU_TEST", "SI_STAGE2_OFFLINE"]),
   ("Bluetooth graft", ["BTSTACK_GRAFT", "GRAFT_INSERTION_PLAN", "GRAFT_LAUNCHER_LIFECYCLE", "RFCOMM_CHANNEL_LAYOUT", "RFCOMM_DUPLEX", "BLUETOOTH_DISCOVERY"]),
   ("iAP2 and MFi bootstrap", ["IAP2_DAEMON", "IAP2_VENDOR_PROVENANCE", "TEST_SOFTWARE_MFI", "BOOTSTRAP_COMPARISON"]),
   ("Pairing and media", ["STOCK210_HTTP_PAIRING_ABI", "STOCK210_MEDIA_CRYPTO_ABI", "AIRPLAY280_PAIRING_RECIPE", "WIRELESS_PRODUCTION_CHANNELS"]),
   ("iOS, radio and hardware", ["IOS_AUTH_SELECTION", "IOS_WIRELESS_SESSION_GATES", "WIFI_AND_DISCOVERY", "HARDWARE_AND_DONORS", "FIREWALL"]),
   ("Evidence", ["STOCK210_VALIDATION", "WCP0627_SOURCE_REVIEW", "history/README", "history/MIGRATION"]),
 ]),
 ("carplay-lab", "CarPlay Laboratory", "🧪", [
   ("vphone sender", ["VPHONE_LAB", "VPHONE_TIMING", "NATIVE_WIRELESS_QNX", "NATIVE_SCREEN_TEST", "MEDIA_STRESS_TEST", "WIRELESS_MEDIA_VALIDATION", "WIRELESS_COMPLETION_PLAN"]),
   ("Laboratory references", ["IOS_VM_LAB", "IOS210_CODE_REVIEW"]),
 ]),
 ("build-and-deploy", "Build and Deploy", "📦", [
   ("Build and release", ["REPOSITORY_WORKFLOW", "BUILD_SYSTEM", "DEPLOY_AND_RELEASE", "SCENE_BACKEND_DEPLOY",
                           ]),
   ("Audits and recovery", ["ISSUE24_RCC_MMX_RECOVERY", "RE_ARTIFACTS"]),
 ]),
 ("reference", "Reference", "📚", [
   ("System", ["ARCHITECTURE", "HOOK_TO_JAVA_BUS", "RECORD_REPLAY_TOOLING"]),
   ("Formats and ports", ["IOS_CARPLAY_SPATIAL_WIREFORMAT", "MHI2_MU1439_PORT", "CARPLAY_STATUS_CODES"]),
   ("History", []),
 ]),
]

MAP = [(folder, name, emoji, [(g, files) for g, files in groups if files])
       for folder, name, emoji, groups in MAP]

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

def splice(text, name, body):
    """Replace a generated block in memory, so --check never mutates files."""
    begin, end = f"<!-- BEGIN generated:{name} -->", f"<!-- END generated:{name} -->"
    i, j = text.index(begin) + len(begin), text.index(end)
    return text[:i] + "\n" + body + "\n" + text[j:]

# full mindmap for docs/README.md: every note under its domain and group
full = ["```mermaid", "mindmap", "  root((MHI2Q CarPlay docs))"]
for folder, name, emoji, groups in MAP:
    full.append(f"    {name}")
    for g, files in groups:
        full.append(f"      {g}")
        full += [f"        {label(f'{folder}/{f}.md')}" for f in files]
full.append("```")

original = open("README.md").read()
updated = splice(original, "full-mindmap", "\n".join(full))
updated = splice(updated, "notes-by-topic", "\n".join(topics))
if args.check and updated != original:
    raise SystemExit("docs/README.md is stale; run python3 scripts/gen_docs_map.py")
if not args.check and updated != original:
    with open("README.md", "w") as f:
        f.write(updated)

print(len(placed), "notes placed in", len(MAP), "domains")
