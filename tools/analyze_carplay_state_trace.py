#!/usr/bin/env python3
"""Summarise the compact generation-scoped CarPlay startup trace."""

from __future__ import annotations

import argparse
import collections
import json
import shlex
from pathlib import Path


# A FLUSHBUFFERED invalidates the old item's PCM immediately.  The first new
# compressed record and the first re-primed QSA write can arrive substantially
# later, so keep those intentional restart gaps separate from steady playback
# starvation.  The live 2026-09-05 sample resumed APAP after 891 ms and QSA
# after 1177 ms; two seconds leaves margin without hiding later-song failures.
FLUSH_RESTART_WINDOW_MS = 2000


def parse_event(line: str) -> dict[str, str] | None:
    marker = " ST "
    if marker not in line:
        return None
    try:
        tokens = shlex.split(line.split(marker, 1)[1])
    except ValueError:
        return None
    event: dict[str, str] = {"raw": line.rstrip("\n")}
    for token in tokens:
        if "=" in token:
            key, value = token.split("=", 1)
            event[key] = value
    return event if "name" in event else None


def load_events(paths: list[Path]) -> list[dict[str, str]]:
    events: list[dict[str, str]] = []
    for path in paths:
        with path.open("r", encoding="utf-8", errors="replace") as stream:
            for line in stream:
                event = parse_event(line)
                if event:
                    event["source"] = str(path)
                    events.append(event)
    events.sort(key=lambda item: (int(item.get("mono_ms", "0")),
                                  int(item.get("ev", "0"))))
    return events


def as_int(event: dict[str, str], key: str, default: int = 0) -> int:
    try:
        return int(event.get(key, str(default)), 0)
    except ValueError:
        return default


def yes(value: bool) -> str:
    return "yes" if value else "no"


def max_int(events: list[dict[str, str]], key: str, default: int = 0) -> int:
    return max((as_int(event, key, default) for event in events),
               default=default)


def partition_after_controls(
        events: list[dict[str, str]],
        controls: list[dict[str, str]],
        window_ms: int = FLUSH_RESTART_WINDOW_MS,
) -> tuple[list[dict[str, str]], list[dict[str, str]]]:
    """Split events into explicit-control restart and steady-state groups."""
    control_times = sorted(as_int(event, "mono_ms") for event in controls)
    restart: list[dict[str, str]] = []
    steady: list[dict[str, str]] = []
    for event in events:
        event_ms = as_int(event, "mono_ms")
        follows_control = any(
            0 <= event_ms - control_ms <= window_ms
            for control_ms in reversed(control_times)
            if control_ms <= event_ms
        )
        (restart if follows_control else steady).append(event)
    return restart, steady


def flush_restart_anchors(
        controls: list[dict[str, str]],
) -> list[dict[str, str]]:
    """Return FLUSH events plus the matching post-pause rate=1 edges.

    A seek can remain at rate=0 for longer than the correlation window.  In
    that case the first new APAP/QSA activity follows SETRATE, not the original
    FLUSH.  Only rate=1 controls with a pending FLUSH are anchors, so an
    ordinary play command cannot hide a later steady-state fault.
    """
    anchors: list[dict[str, str]] = []
    pending_flush = False
    for control in sorted(controls, key=lambda item: as_int(item, "mono_ms")):
        method = control.get("method")
        if method == "FLUSHBUFFERED":
            anchors.append(control)
            pending_flush = True
        elif (pending_flush and method in ("SETRATE", "SETRATEANCHORTIME")
              and as_int(control, "rate") != 0):
            anchors.append(control)
            pending_flush = False
    return anchors


def summarize_generation(process_id: str, generation: str,
                         events: list[dict[str, str]]) -> dict[str, object]:
    by_name: dict[str, list[dict[str, str]]] = collections.defaultdict(list)
    for event in events:
        by_name[event["name"]].append(event)

    starts = by_name["IAP_NOW_PLAYING_START"]
    link_tx = [event for event in by_name["IAP_LINK_TX"]
               if event.get("msg") == "0x5000"]
    link_rx = by_name["IAP_LINK_RX_AFTER_5000"]
    updates = by_name["IAP_NOW_PLAYING_UPDATE"]
    setup_streams = by_name["AUDIO_SETUP_STREAM"]
    setup_types = sorted({as_int(event, "type", -1) for event in setup_streams})
    setup_types = [value for value in setup_types if value >= 0]
    formats = sorted({as_int(event, "format_index", -1) for event in setup_streams})
    formats = [value for value in formats if value >= 0]
    decoders = by_name["BUFFERED_DECODER_READY"]
    render_starts = by_name["BUFFERED_RENDER_START"]
    rate_gates = by_name["BUFFERED_RATE_GATE"]
    flush_applied = by_name["BUFFERED_FLUSH_APPLIED"]
    queue_failures = by_name["BUFFERED_QUEUE_FAILURE"]
    queue_backpressure = by_name["BUFFERED_QUEUE_BACKPRESSURE"]
    now_playing_retries = by_name["IAP_NOW_PLAYING_RETRY"]
    audio_routes = by_name["AUDIO_ROUTE"]
    qsa_opens = by_name["QSA_OPEN"]
    qsa_closes = by_name["QSA_CLOSE"]
    cold_bootstrap_failures = by_name["QSA_COLD_BOOTSTRAP"]
    qsa_writes = by_name["QSA_FIRST_WRITE"] + by_name["QSA_DETACHED_FIRST_WRITE"]
    hid_fills = by_name["HID_REMOTE_FILL"]
    hid_posts = by_name["HID_REPORT_POST"]
    buffered_health = by_name["BUFFERED_HEALTH"]
    qsa_health = by_name["QSA_HEALTH"]
    packet_anomalies = by_name["BUFFERED_PACKET_ANOMALY"]
    packet_failures = [event for event in packet_anomalies
                       if event.get("stage") != "zero-pcm"]
    sequence_gap_events = by_name["BUFFERED_SEQUENCE_GAP"]
    apap_gaps = by_name["APAP_FRAME_GAP"]
    ring_waits = by_name["QSA_RING_WAIT"]
    cadence_gaps = by_name["QSA_CADENCE_GAP"]
    buffered_silences = by_name["QSA_BUFFERED_SILENCE"]
    qsa_write_anomalies = by_name["QSA_WRITE_ANOMALY"]
    qsa_queue_prefill_resets = [
        event for event in qsa_write_anomalies
        if (as_int(event, "result", -1) == as_int(event, "expected", -2)
            and as_int(event, "auto_recovery") == 0
            and as_int(event, "clock_reset") == 1
            and as_int(event, "gap_ms") == 0)
    ]
    operational_write_anomalies = [
        event for event in qsa_write_anomalies
        if event not in qsa_queue_prefill_resets
    ]
    device_underruns = by_name["QSA_DEVICE_UNDERRUN"]
    buffered_controls = by_name["BUFFERED_CONTROL"]
    flush_controls = [event for event in buffered_controls
                      if event.get("method") == "FLUSHBUFFERED"]
    restart_anchors = flush_restart_anchors(buffered_controls)
    flush_boundaries = by_name["BUFFERED_FLUSH_BOUNDARY"]
    flush_packet_failures, steady_packet_failures = partition_after_controls(
        packet_failures, restart_anchors)
    flush_sequence_gaps, steady_sequence_gaps = partition_after_controls(
        sequence_gap_events, restart_anchors)
    flush_apap_gaps, steady_apap_gaps = partition_after_controls(
        apap_gaps, restart_anchors)
    flush_ring_waits, steady_ring_waits = partition_after_controls(
        ring_waits, restart_anchors)
    flush_cadence_gaps, steady_cadence_gaps = partition_after_controls(
        cadence_gaps, restart_anchors)
    flush_buffered_silences, steady_buffered_silences = partition_after_controls(
        buffered_silences, restart_anchors)
    flush_write_anomalies, steady_write_anomalies = partition_after_controls(
        operational_write_anomalies, restart_anchors)
    update_hashes = sorted({event.get("payload_hash", "") for event in updates})
    update_hashes = [value for value in update_hashes if value]
    ack_observed = any(as_int(event, "ack_eq_seq") or
                       as_int(event, "ack_eq_next") for event in link_rx)

    rate_one_edges = [event for event in rate_gates
                      if as_int(event, "new_playing") == 1]
    first_render_ms = min((as_int(event, "mono_ms") for event in render_starts),
                          default=-1)
    first_rate_one_ms = min((as_int(event, "mono_ms") for event in rate_one_edges),
                            default=-1)
    pre_rate_buffered_writes: list[dict[str, str]] = []
    if first_render_ms >= 0 and first_rate_one_ms >= first_render_ms:
        pre_rate_buffered_writes = [
            event for event in qsa_writes
            if first_render_ms <= as_int(event, "mono_ms") < first_rate_one_ms
            and (event.get("name") == "QSA_DETACHED_FIRST_WRITE"
                 or as_int(event, "buffered_frames") > 0)
        ]

    retained_closes = [event for event in qsa_closes
                       if event.get("action") == "retain_for_buffered"]
    detached_activity = by_name["QSA_DETACHED_FIRST_WRITE"] + [
        event for event in qsa_health if event.get("owner") == "detached"]
    missing_route_reacquires = 0
    for close in retained_closes:
        close_ms = as_int(close, "mono_ms")
        kept_rendering = any(as_int(event, "mono_ms") >= close_ms
                             for event in detached_activity)
        route_recovered = any(
            as_int(event, "mono_ms") >= close_ms and
            event.get("action") in ("request", "handoff")
            for event in audio_routes)
        stock_reopened = any(as_int(event, "mono_ms") > close_ms
                             for event in qsa_opens)
        if kept_rendering and not route_recovered and not stock_reopened:
            missing_route_reacquires += 1

    packet_anomaly_counts: dict[str, int] = {}
    for event in packet_anomalies:
        stage = event.get("stage", "?")
        packet_anomaly_counts[stage] = max(
            packet_anomaly_counts.get(stage, 0), as_int(event, "count"))

    sequence_gap_count = max(
        max_int(buffered_health, "sequence_gaps"),
        max_int(sequence_gap_events, "count"))

    if queue_failures:
        audio_health = ("The authenticated compressed queue rejected a record; the APAP "
                        "receiver closes instead of silently losing media. Inspect status.")
    elif pre_rate_buffered_writes:
        audio_health = ("Buffered PCM reached QSA before the first rate-one gate: initial "
                        "presentation ownership regressed.")
    elif device_underruns:
        audio_health = ("QSA device underrun counter advanced: loss is at the "
                        "writer/device boundary even if plugin writes were full.")
    elif steady_ring_waits:
        audio_health = ("Decoded PCM ring stayed below one 768-frame period past "
                        "the cadence budget: producer/ring starvation before QSA.")
    elif steady_cadence_gaps and steady_apap_gaps:
        audio_health = ("APAP processing and QSA write cadence both exceeded 100 ms; "
                        "correlate their timestamps to a receiver/scheduler stall.")
    elif steady_cadence_gaps:
        audio_health = ("QSA write cadence exceeded 100 ms without a recorded APAP "
                        "gap: writer/device blocking or scheduler stall.")
    elif steady_buffered_silences:
        audio_health = ("QSA stayed clocked but buffered type 103 did not own a complete "
                        "period outside a flush restart; a whole silent period prevented "
                        "cross-timeline type-100 PCM substitution.")
    elif steady_write_anomalies:
        audio_health = ("QSA write/clock anomalies occurred outside an explicit flush. "
                        "A full-byte write with clock_reset=1 is a burst/cadence "
                        "discontinuity, while auto_recovery=1 names QSA recovery.")
    elif steady_sequence_gaps:
        audio_health = ("Authenticated APAP sequence skipped before decode: the sender/media "
                        "timeline advanced without delivering every record; correlate with "
                        "track/seek control events.")
    elif steady_packet_failures:
        audio_health = ("Buffered packet/decode failures occurred before the PCM "
                        "ring; inspect per-stage counts. A zero-pcm decoder-prime event "
                        "alone is not classified as a decode failure.")
    elif steady_apap_gaps:
        audio_health = ("APAP delivery exceeded 100 ms outside an explicit flush, but no "
                        "matching QSA cadence/ring failure was recorded; the downstream "
                        "reserve absorbed it in this capture.")
    elif (flush_sequence_gaps or flush_apap_gaps or flush_ring_waits or
          flush_cadence_gaps or flush_packet_failures or
          flush_write_anomalies or flush_buffered_silences):
        audio_health = (
            "All recorded buffered/QSA anomalies followed FLUSHBUFFERED within "
            f"{FLUSH_RESTART_WINDOW_MS} ms: classify them as an explicit item/seek "
            "restart, not a steady AAC decode underrun.")
    elif buffered_health or qsa_health:
        audio_health = ("Health telemetry saw no packet, ring, cadence or device "
                        "failure in the captured interval.")
    else:
        audio_health = "Health telemetry is not present in this deployed build."

    boundary: str
    if int(generation) == 0:
        boundary = "Pre-Identify process events only; not a physical CarPlay generation."
    elif not starts:
        boundary = "No outbound 0x5000 was constructed: stock one-shot lifecycle boundary."
    elif not link_tx:
        boundary = "0x5000 was constructed but never observed at NmeTransport::Send."
    elif all(as_int(event, "result", -1) != 0 for event in link_tx):
        boundary = "Stock NmeTransport::Send rejected 0x5000."
    elif not updates:
        boundary = ("0x5000 left through stock transport but no 0x5001 returned: "
                    "inspect raw ACK values, then the iOS accessoryd/XPC boundary.")
    elif not by_name["AIRPLAY_SETUP_BEGIN"]:
        boundary = "Now Playing returned, but no AirPlay receiver SETUP reached the HU."
    elif any(event.get("action") == "request_failed" for event in audio_routes):
        boundary = ("The cold buffered media-route callback could not be resolved; QSA alone "
                    "does not activate the HU audio source.")
    elif not by_name["QSA_OPEN"]:
        boundary = "AirPlay SETUP reached the HU, but the DIO5 QSA sink was never opened."
    elif not qsa_writes:
        boundary = "QSA opened/configured, but no successful rendered write was observed."
    elif missing_route_reacquires:
        boundary = ("Stock type 100 released the stock Media route while type 103 kept "
                    "rendering on the retained QSA handle; no hook route reacquire followed, "
                    "so valid PCM can be inaudible.")
    elif any(event.get("action") == "cold_buffered" for event in qsa_opens) and not any(
            event.get("action") in ("request", "handoff") for event in audio_routes):
        boundary = ("Cold buffered PCM reached QSA without a stock media-route declaration; "
                    "successful device writes can remain inaudible until stock type 100 starts.")
    else:
        boundary = ("The traced transport/render path and stock media-route declaration "
                    "completed through QSA. Correlate with audible output and HU metadata.")

    first_update = updates[0] if updates else {}
    return {
        "process_id": int(process_id),
        "generation": int(generation),
        "events": len(events),
        "identify_accepted": yes(bool(by_name["IAP_IDENTIFY_ACCEPTED"])),
        "identify_ended": yes(bool(by_name["IAP_IDENTIFY_END"])),
        "auth_complete": yes(bool(by_name["IAP_AUTH_COMPLETE"])),
        "now_playing_5000_built": len(starts),
        "now_playing_5000_link_tx": len(link_tx),
        "post_5000_link_headers": len(link_rx),
        "raw_ack_candidate_seen": yes(ack_observed),
        "now_playing_5001_updates": len(updates),
        "now_playing_retry_actions": [event.get("action", "?")
                                      for event in now_playing_retries],
        "first_5001_media_attrs": as_int(first_update, "media_attrs", -1),
        "first_5001_playback_attrs": as_int(first_update, "playback_attrs", -1),
        "distinct_5001_payload_hashes": update_hashes,
        "rgi_correlator_seen": yes(bool(by_name["IAP_RGI_FIRST"])),
        "airplay_setup_calls": len(by_name["AIRPLAY_SETUP_BEGIN"]),
        "airplay_new_session_edges": sum(
            event.get("session_edge") == "new"
            for event in by_name["AIRPLAY_SETUP_BEGIN"]),
        "audio_stream_types": setup_types,
        "buffered_format_indices": formats,
        "buffered_controls": [event.get("method", "?")
                              for event in buffered_controls],
        "buffered_initial_rates": [as_int(event, "initial_rate", -1)
                                   for event in render_starts],
        "buffered_rate_gate_edges": [
            {
                "old": as_int(event, "old_playing", -1),
                "new": as_int(event, "new_playing", -1),
                "queued_packets": as_int(event, "queued_packets"),
                "queued_bytes": as_int(event, "queued_bytes"),
            }
            for event in rate_gates
        ],
        "buffered_pre_rate_qsa_writes": len(pre_rate_buffered_writes),
        "compressed_queue_max_packets": max(
            max_int(buffered_health, "compressed_packets"),
            max_int(rate_gates, "queued_packets"),
            max_int(queue_backpressure, "queued_packets")),
        "compressed_queue_max_bytes": max(
            max_int(buffered_health, "compressed_bytes"),
            max_int(rate_gates, "queued_bytes")),
        "compressed_queue_backpressure_events": len(queue_backpressure),
        "compressed_queue_failures": len(queue_failures),
        "flushbuffered_events": len(flush_controls),
        "flush_applied_events": len(flush_applied),
        "flush_removed_compressed_packets": sum(
            as_int(event, "removed_packets") for event in flush_applied),
        "flush_removed_pcm_frames": sum(
            as_int(event, "removed_pcm_frames") for event in flush_applied),
        "flush_restart_anchors": len(restart_anchors),
        "flush_restart_window_ms": FLUSH_RESTART_WINDOW_MS,
        "flush_boundaries_crossed": len(flush_boundaries),
        "flush_discarded_packets_total": sum(
            as_int(event, "discarded") for event in flush_boundaries),
        "flush_discarded_packets_max": max_int(flush_boundaries, "discarded"),
        "decoder_ready": [
            {
                "codec": event.get("codec", "?"),
                "index": as_int(event, "setup_index", -1),
                "channels": as_int(event, "channels", -1),
            }
            for event in decoders
        ],
        "buffered_render": [
            {
                "index": as_int(event, "setup_index", -1),
                "state": event.get("state", "?"),
                "channels": as_int(event, "output_channels", -1),
                "upmix": event.get("upmix", "?"),
                "rear_q15": as_int(event, "rear_q15", -1),
                "lfe": as_int(event, "lfe", -1),
            }
            for event in render_starts
        ],
        "audio_route_actions": [event.get("action", "?") for event in audio_routes],
        "qsa_open": len(qsa_opens),
        "qsa_open_actions": [event.get("action", "?") for event in qsa_opens],
        "qsa_retain_for_buffered": len(retained_closes),
        "route_reacquire_missing": missing_route_reacquires,
        "qsa_cold_bootstrap_failures": len(cold_bootstrap_failures),
        "qsa_first_write": len(qsa_writes),
        "buffered_health_samples": len(buffered_health),
        "buffered_packet_anomalies": packet_anomaly_counts,
        "buffered_decode_failures": max(
            max_int(buffered_health, "decode_fail"),
            packet_anomaly_counts.get("decode", 0),
            packet_anomaly_counts.get("decode-output", 0)),
        "buffered_zero_pcm": max(
            max_int(buffered_health, "zero_pcm"),
            packet_anomaly_counts.get("zero-pcm", 0)),
        "buffered_sequence_gaps": sequence_gap_count,
        "flush_restart_packet_failures": len(flush_packet_failures),
        "flush_restart_sequence_gaps": len(flush_sequence_gaps),
        "apap_frame_gaps": len(apap_gaps),
        "apap_max_gap_ms": max_int(apap_gaps, "gap_ms"),
        "flush_restart_apap_gaps": len(flush_apap_gaps),
        "steady_apap_gaps": len(steady_apap_gaps),
        "qsa_health_samples": len(qsa_health),
        "qsa_ring_waits": len(ring_waits),
        "qsa_ring_wait_max_ms": max_int(ring_waits, "gap_ms"),
        "flush_restart_ring_waits": len(flush_ring_waits),
        "steady_ring_waits": len(steady_ring_waits),
        "qsa_cadence_gaps": len(cadence_gaps),
        "qsa_cadence_max_gap_ms": max_int(cadence_gaps, "gap_ms"),
        "flush_restart_qsa_gaps": len(flush_cadence_gaps),
        "steady_qsa_gaps": len(steady_cadence_gaps),
        "qsa_buffered_silence_events": len(buffered_silences),
        "qsa_buffered_silence_periods": max_int(buffered_silences, "periods"),
        "qsa_buffered_silence_frames": max_int(buffered_silences, "frames"),
        "flush_restart_buffered_silences": len(flush_buffered_silences),
        "steady_buffered_silences": len(steady_buffered_silences),
        "qsa_write_anomalies": len(qsa_write_anomalies),
        "qsa_queue_prefill_clock_resets": len(qsa_queue_prefill_resets),
        "flush_restart_qsa_write_anomalies": len(flush_write_anomalies),
        "steady_qsa_write_anomalies": len(steady_write_anomalies),
        "qsa_device_underrun_events": len(device_underruns),
        "qsa_device_underruns": max(
            max_int(qsa_health, "device_underruns"),
            max_int(device_underruns, "device_underruns")),
        "audio_health_decision": audio_health,
        "hid_remote_fills": [
            {
                "usage": event.get("usage", "?"),
                "asserted": as_int(event, "asserted", -1),
                "result": as_int(event, "result", -1),
                "report_byte": event.get("report_byte", "?"),
            }
            for event in hid_fills
        ],
        "hid_report_posts": len(hid_posts),
        "hid_post_failures": sum(as_int(event, "result", -1) != 0
                                 for event in hid_posts),
        "decision": boundary,
    }


def print_text(summary: dict[str, object]) -> None:
    print(f"pid {summary['process_id']} generation {summary['generation']}: "
          f"{summary['decision']}")
    keys = (
        "identify_accepted", "identify_ended", "auth_complete",
        "now_playing_5000_built", "now_playing_5000_link_tx",
        "post_5000_link_headers", "raw_ack_candidate_seen",
        "now_playing_5001_updates", "first_5001_media_attrs",
        "first_5001_playback_attrs", "distinct_5001_payload_hashes",
        "now_playing_retry_actions",
        "rgi_correlator_seen", "airplay_setup_calls",
        "airplay_new_session_edges", "audio_stream_types",
        "buffered_format_indices", "buffered_controls",
        "buffered_initial_rates", "buffered_rate_gate_edges",
        "buffered_pre_rate_qsa_writes", "compressed_queue_max_packets",
        "compressed_queue_max_bytes", "compressed_queue_backpressure_events",
        "compressed_queue_failures", "flushbuffered_events",
        "flush_applied_events", "flush_removed_compressed_packets",
        "flush_removed_pcm_frames",
        "flush_restart_anchors", "flush_restart_window_ms",
        "flush_boundaries_crossed", "flush_discarded_packets_total",
        "flush_discarded_packets_max", "decoder_ready",
        "buffered_render", "audio_route_actions", "qsa_open", "qsa_open_actions",
        "qsa_retain_for_buffered", "route_reacquire_missing",
        "qsa_cold_bootstrap_failures", "qsa_first_write", "buffered_health_samples",
        "buffered_packet_anomalies", "buffered_decode_failures",
        "buffered_zero_pcm", "buffered_sequence_gaps",
        "flush_restart_packet_failures", "flush_restart_sequence_gaps",
        "apap_frame_gaps", "apap_max_gap_ms", "flush_restart_apap_gaps",
        "steady_apap_gaps", "qsa_health_samples", "qsa_ring_waits",
        "qsa_ring_wait_max_ms", "flush_restart_ring_waits",
        "steady_ring_waits", "qsa_cadence_gaps", "qsa_cadence_max_gap_ms",
        "flush_restart_qsa_gaps", "steady_qsa_gaps", "qsa_write_anomalies",
        "qsa_buffered_silence_events", "qsa_buffered_silence_periods",
        "qsa_buffered_silence_frames", "flush_restart_buffered_silences",
        "steady_buffered_silences",
        "qsa_queue_prefill_clock_resets",
        "flush_restart_qsa_write_anomalies", "steady_qsa_write_anomalies",
        "qsa_device_underrun_events", "qsa_device_underruns",
        "audio_health_decision", "hid_remote_fills",
        "hid_report_posts", "hid_post_failures",
    )
    for key in keys:
        print(f"  {key}: {summary[key]}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", nargs="+", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    events = load_events(args.trace)
    grouped: dict[tuple[str, str], list[dict[str, str]]] = \
        collections.defaultdict(list)
    for event in events:
        grouped[(event.get("pid", "0"), event.get("gen", "0"))].append(event)
    summaries = [summarize_generation(key[0], key[1], grouped[key])
                 for key in sorted(grouped,
                                   key=lambda value: (int(value[0]),
                                                      int(value[1])))]
    if args.json:
        print(json.dumps(summaries, indent=2, sort_keys=True))
    else:
        for index, summary in enumerate(summaries):
            if index:
                print()
            print_text(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
