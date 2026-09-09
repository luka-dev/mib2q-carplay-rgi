#!/usr/bin/env python3
"""Host-side Audi J525 logical-bus model and measured-BRIR renderer."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator, Sequence

import numpy as np


SAMPLE_RATE = 48_000
PCM_FORMAT = 0x0001
EXTENSIBLE_FORMAT = 0xFFFE
PCM_SUBFORMAT_GUID = bytes.fromhex("0100000000001000800000aa00389b71")
DEFAULT_BLOCK_FRAMES = 16_384
SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_CONFIG = SCRIPT_DIR / "audi_a5_f5_9vs_3d_off.json"


class CabinError(RuntimeError):
    """A user-facing virtual-cabin input or configuration error."""


@dataclass(frozen=True)
class WavInfo:
    path: Path
    sample_rate: int
    channels: int
    bits_per_sample: int
    valid_bits_per_sample: int
    channel_mask: int
    block_align: int
    data_offset: int
    data_bytes: int

    @property
    def frames(self) -> int:
        return self.data_bytes // self.block_align


class Pcm16WavReader:
    def __init__(self, path: Path):
        self.path = Path(path)
        self._file = self.path.open("rb")
        try:
            self.info = self._parse()
            self._file.seek(self.info.data_offset)
        except Exception:
            self._file.close()
            raise

    def _parse(self) -> WavInfo:
        header = self._file.read(12)
        if len(header) != 12 or header[0:4] != b"RIFF" or header[8:12] != b"WAVE":
            raise CabinError(f"{self.path}: expected little-endian RIFF/WAVE")

        fmt: tuple[int, int, int, int, int, int] | None = None
        valid_bits = 0
        channel_mask = 0
        data_offset: int | None = None
        data_bytes: int | None = None

        while True:
            chunk = self._file.read(8)
            if not chunk:
                break
            if len(chunk) != 8:
                raise CabinError(f"{self.path}: truncated WAV chunk header")
            chunk_id, chunk_size = struct.unpack("<4sI", chunk)
            payload_offset = self._file.tell()
            if chunk_id == b"fmt ":
                payload = self._file.read(chunk_size)
                if len(payload) != chunk_size or chunk_size < 16:
                    raise CabinError(f"{self.path}: invalid fmt chunk")
                fmt = struct.unpack_from("<HHIIHH", payload, 0)
                tag, channels, rate, byte_rate, block_align, bits = fmt
                valid_bits = bits
                if tag == EXTENSIBLE_FORMAT:
                    if chunk_size < 40 or struct.unpack_from("<H", payload, 16)[0] < 22:
                        raise CabinError(f"{self.path}: truncated WAVE_FORMAT_EXTENSIBLE fmt")
                    valid_bits, channel_mask = struct.unpack_from("<HI", payload, 18)
                    if payload[24:40] != PCM_SUBFORMAT_GUID:
                        raise CabinError(f"{self.path}: extensible WAV is not integer PCM")
                elif tag != PCM_FORMAT:
                    raise CabinError(f"{self.path}: WAV format {tag:#06x} is not integer PCM")
                expected_rate = rate * block_align
                if byte_rate != expected_rate:
                    raise CabinError(
                        f"{self.path}: byte rate {byte_rate} != {expected_rate}"
                    )
            elif chunk_id == b"data" and data_offset is None:
                data_offset = payload_offset
                data_bytes = chunk_size
                self._file.seek(chunk_size, 1)
            else:
                self._file.seek(chunk_size, 1)
            if chunk_size & 1:
                self._file.seek(1, 1)

        if fmt is None or data_offset is None or data_bytes is None:
            raise CabinError(f"{self.path}: missing fmt or data chunk")
        _tag, channels, rate, _byte_rate, block_align, bits = fmt
        if bits != 16 or valid_bits != 16:
            raise CabinError(
                f"{self.path}: only 16-bit PCM is supported, got {bits}/{valid_bits} bits"
            )
        if rate != SAMPLE_RATE:
            raise CabinError(f"{self.path}: expected 48000 Hz, got {rate} Hz")
        if block_align != channels * 2 or data_bytes % block_align:
            raise CabinError(f"{self.path}: malformed PCM block alignment")
        return WavInfo(
            path=self.path,
            sample_rate=rate,
            channels=channels,
            bits_per_sample=bits,
            valid_bits_per_sample=valid_bits,
            channel_mask=channel_mask,
            block_align=block_align,
            data_offset=data_offset,
            data_bytes=data_bytes,
        )

    def blocks(self, block_frames: int = DEFAULT_BLOCK_FRAMES) -> Iterator[np.ndarray]:
        remaining = self.info.frames
        while remaining:
            frames = min(remaining, block_frames)
            raw = self._file.read(frames * self.info.block_align)
            if len(raw) != frames * self.info.block_align:
                raise CabinError(f"{self.path}: truncated PCM data")
            pcm = np.frombuffer(raw, dtype="<i2").reshape(frames, self.info.channels)
            yield pcm.astype(np.float64) / 32768.0
            remaining -= frames

    def close(self) -> None:
        self._file.close()

    def __enter__(self) -> "Pcm16WavReader":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()


class Pcm16WavWriter:
    def __init__(
        self,
        path: Path,
        channels: int,
        frames: int,
        channel_mask: int = 0,
    ):
        self.path = Path(path)
        self.channels = channels
        self.expected_frames = frames
        self.written_frames = 0
        self._file = self.path.open("wb")
        self._write_header(channel_mask)

    def _write_header(self, channel_mask: int) -> None:
        data_bytes = self.expected_frames * self.channels * 2
        if data_bytes > 0xFFFFFFFF - 60:
            raise CabinError(f"{self.path}: output is too large for RIFF/WAVE")
        block_align = self.channels * 2
        if self.channels <= 2:
            riff_size = 36 + data_bytes
            self._file.write(
                struct.pack(
                    "<4sI4s4sIHHIIHH4sI",
                    b"RIFF",
                    riff_size,
                    b"WAVE",
                    b"fmt ",
                    16,
                    PCM_FORMAT,
                    self.channels,
                    SAMPLE_RATE,
                    SAMPLE_RATE * block_align,
                    block_align,
                    16,
                    b"data",
                    data_bytes,
                )
            )
            return

        riff_size = 60 + data_bytes
        self._file.write(
            struct.pack(
                "<4sI4s4sIHHIIHHHHI16s4sI",
                b"RIFF",
                riff_size,
                b"WAVE",
                b"fmt ",
                40,
                EXTENSIBLE_FORMAT,
                self.channels,
                SAMPLE_RATE,
                SAMPLE_RATE * block_align,
                block_align,
                16,
                22,
                16,
                channel_mask,
                PCM_SUBFORMAT_GUID,
                b"data",
                data_bytes,
            )
        )

    def write(self, samples: np.ndarray) -> None:
        samples = np.asarray(samples, dtype=np.float64)
        if samples.ndim != 2 or samples.shape[1] != self.channels:
            raise CabinError(
                f"{self.path}: expected Nx{self.channels} output block, got {samples.shape}"
            )
        if not np.all(np.isfinite(samples)):
            raise CabinError(f"{self.path}: output contains NaN or infinity")
        pcm = np.rint(samples * 32768.0)
        pcm = np.clip(pcm, -32768, 32767).astype("<i2")
        self._file.write(pcm.tobytes(order="C"))
        self.written_frames += samples.shape[0]

    def close(self) -> None:
        if self._file.closed:
            return
        self._file.close()
        if self.written_frames != self.expected_frames:
            raise CabinError(
                f"{self.path}: wrote {self.written_frames} frames, expected "
                f"{self.expected_frames}"
            )

    def __enter__(self) -> "Pcm16WavWriter":
        return self

    def __exit__(self, exc_type: object, *_exc: object) -> None:
        if exc_type is None:
            self.close()
        else:
            self._file.close()


class ChannelMetrics:
    def __init__(self, names: Sequence[str]):
        self.names = tuple(names)
        self.frames = 0
        self.sum_squares = np.zeros(len(names), dtype=np.float64)
        self.peaks = np.zeros(len(names), dtype=np.float64)
        self.clipped_samples = np.zeros(len(names), dtype=np.int64)

    def update(self, samples: np.ndarray) -> None:
        samples = np.asarray(samples, dtype=np.float64)
        if samples.ndim != 2 or samples.shape[1] != len(self.names):
            raise CabinError(f"metrics expected Nx{len(self.names)}, got {samples.shape}")
        if not np.all(np.isfinite(samples)):
            raise CabinError("audio contains NaN or infinity")
        self.frames += samples.shape[0]
        self.sum_squares += np.sum(samples * samples, axis=0)
        self.peaks = np.maximum(self.peaks, np.max(np.abs(samples), axis=0))
        self.clipped_samples += np.sum(np.abs(samples) > 1.0, axis=0)

    @staticmethod
    def _db(value: float) -> float | None:
        return round(20.0 * math.log10(value), 6) if value > 0.0 else None

    def power(self) -> np.ndarray:
        if not self.frames:
            return np.zeros(len(self.names), dtype=np.float64)
        return self.sum_squares / self.frames

    def report(self) -> dict[str, object]:
        powers = self.power()
        channels: dict[str, object] = {}
        for index, name in enumerate(self.names):
            rms = math.sqrt(float(powers[index]))
            peak = float(self.peaks[index])
            channels[name] = {
                "rms": round(rms, 9),
                "rms_dbfs": self._db(rms),
                "peak": round(peak, 9),
                "peak_dbfs": self._db(peak),
                "preclip_samples": int(self.clipped_samples[index]),
            }
        return {"frames": self.frames, "channels": channels}


def load_config(path: Path) -> dict[str, object]:
    try:
        config = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CabinError(f"cannot load config {path}: {exc}") from exc
    inputs = config.get("input_channels")
    outputs = config.get("logical_buses")
    matrix = config.get("matrix")
    speakers = config.get("speakers")
    if not isinstance(inputs, list) or len(inputs) != 8:
        raise CabinError("config input_channels must contain eight entries")
    if not isinstance(outputs, list) or len(outputs) != 8:
        raise CabinError("config logical_buses must contain eight entries")
    if not isinstance(matrix, dict):
        raise CabinError("config matrix object is missing")
    gains = matrix.get("gain_db")
    if not isinstance(gains, list) or len(gains) != 8 or any(
        not isinstance(row, list) or len(row) != 8 for row in gains
    ):
        raise CabinError("config matrix.gain_db must be 8x8")
    if not isinstance(speakers, list) or len(speakers) != 19:
        raise CabinError("9VS config must enumerate 19 service-manual speakers")
    physical = config.get("physical_output_graph")
    if not isinstance(physical, dict):
        raise CabinError("config physical_output_graph object is missing")
    circuits = physical.get("output_circuits")
    if not isinstance(circuits, list) or len(circuits) != 16:
        raise CabinError("9VS config must enumerate 16 documented T38 output circuits")
    circuit_ids: list[str] = []
    audio_pins: list[int] = []
    inferred_channels: list[int] = []
    for circuit in circuits:
        if not isinstance(circuit, dict):
            raise CabinError("physical output circuit entry must be an object")
        circuit_id = circuit.get("id")
        pins = circuit.get("pins")
        speaker_ids = circuit.get("speaker_ids")
        if not isinstance(circuit_id, str) or not circuit_id:
            raise CabinError("physical output circuit needs a stable id")
        if not isinstance(pins, list) or len(pins) != 2 or not all(
            isinstance(pin, int) for pin in pins
        ):
            raise CabinError(f"circuit {circuit_id} must have two integer T38 pins")
        if not isinstance(speaker_ids, list) or not speaker_ids:
            raise CabinError(f"circuit {circuit_id} must name at least one speaker")
        circuit_ids.append(circuit_id)
        audio_pins.extend(pins)
        inferred_channel = circuit.get("inferred_amplifier_channel")
        if not isinstance(inferred_channel, int):
            raise CabinError(
                f"circuit {circuit_id} needs an explicitly labelled inferred channel"
            )
        inferred_channels.append(inferred_channel)
    if len(set(circuit_ids)) != 16:
        raise CabinError("physical output circuit ids must be unique")
    if len(set(audio_pins)) != 32:
        raise CabinError("the 16 T38 audio output pairs must use 32 unique pins")
    if sorted(inferred_channels) != list(range(1, 17)):
        raise CabinError("inferred amplifier channel ids must cover 1..16 exactly once")
    speaker_ids = {str(speaker["id"]) for speaker in speakers if isinstance(speaker, dict)}
    wired_speaker_ids = {
        str(speaker_id)
        for circuit in circuits
        for speaker_id in circuit["speaker_ids"]
    }
    if wired_speaker_ids != speaker_ids:
        raise CabinError("T38 circuit inventory must cover exactly the 19 speakers")

    fanout = config.get("documentary_bus_fanout")
    if not isinstance(fanout, dict) or fanout.get("normalization") != "equal_power_per_logical_bus":
        raise CabinError("documentary_bus_fanout must use equal_power_per_logical_bus")
    routes = fanout.get("routes")
    if not isinstance(routes, dict) or set(routes) != set(outputs):
        raise CabinError("documentary fan-out must enumerate all eight logical buses")
    routed_ids: list[str] = []
    for bus_name in outputs:
        destinations = routes[bus_name]
        if not isinstance(destinations, list) or not all(
            isinstance(destination, str) for destination in destinations
        ):
            raise CabinError(f"documentary route {bus_name} must be a list of circuit ids")
        routed_ids.extend(destinations)
    if sorted(routed_ids) != sorted(circuit_ids):
        raise CabinError("documentary fan-out must route every T38 circuit exactly once")
    state = config.get("operating_state")
    if not isinstance(state, dict) or state.get("symphoria_3d_enabled") is not False:
        raise CabinError("this model requires operating_state.symphoria_3d_enabled=false")
    return config


def linear_matrix(config: dict[str, object]) -> np.ndarray:
    matrix = config["matrix"]
    assert isinstance(matrix, dict)
    gain_db = np.asarray(matrix["gain_db"], dtype=np.float64)
    floor_db = float(matrix["effective_floor_db"])
    return np.power(10.0, np.maximum(gain_db, floor_db) / 20.0)


def apply_matrix(samples: np.ndarray, gains: np.ndarray) -> np.ndarray:
    samples = np.asarray(samples, dtype=np.float64)
    if samples.ndim != 2 or samples.shape[1] not in (6, 8):
        raise CabinError(f"matrix input must be Nx6 or Nx8, got {samples.shape}")
    if samples.shape[1] == 6:
        padded = np.zeros((samples.shape[0], 8), dtype=np.float64)
        padded[:, :6] = samples
        samples = padded
    return samples @ gains


def documentary_fanout_matrix(config: dict[str, object]) -> tuple[np.ndarray, tuple[str, ...]]:
    """Return the explicitly non-OEM equal-power logical-bus to T38 test mapping."""
    physical = config["physical_output_graph"]
    fanout = config["documentary_bus_fanout"]
    assert isinstance(physical, dict) and isinstance(fanout, dict)
    circuits = physical["output_circuits"]
    routes = fanout["routes"]
    assert isinstance(circuits, list) and isinstance(routes, dict)
    bus_names = tuple(str(name) for name in config["logical_buses"])
    circuit_names = tuple(str(circuit["id"]) for circuit in circuits)
    circuit_index = {name: index for index, name in enumerate(circuit_names)}
    matrix = np.zeros((len(bus_names), len(circuit_names)), dtype=np.float64)
    for bus_index, bus_name in enumerate(bus_names):
        destinations = routes[bus_name]
        assert isinstance(destinations, list)
        if not destinations:
            continue
        gain = 1.0 / math.sqrt(len(destinations))
        for destination in destinations:
            matrix[bus_index, circuit_index[str(destination)]] = gain
    return matrix, circuit_names


def apply_documentary_fanout(
    buses: np.ndarray, fanout_matrix: np.ndarray
) -> np.ndarray:
    buses = np.asarray(buses, dtype=np.float64)
    if buses.ndim != 2 or buses.shape[1] != 8:
        raise CabinError(f"documentary fan-out input must be Nx8, got {buses.shape}")
    if fanout_matrix.shape != (8, 16):
        raise CabinError(f"documentary fan-out matrix must be 8x16, got {fanout_matrix.shape}")
    return buses @ fanout_matrix


def default_report_path(output: Path) -> Path:
    return output.with_name(output.name + ".report.json")


def write_report(report: dict[str, object], path: Path) -> None:
    Path(path).write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def matrix_aggregates(metrics: ChannelMetrics) -> dict[str, object]:
    power = metrics.power()
    non_lfe = float(np.sum(power[[0, 1, 2, 4, 5, 6, 7]]))
    rear = float(np.sum(power[[4, 5, 6, 7]]))
    front = float(np.sum(power[[0, 1, 2]]))
    return {
        "maximum_preclip_peak": round(float(np.max(metrics.peaks)), 9),
        "maximum_preclip_peak_dbfs": ChannelMetrics._db(float(np.max(metrics.peaks))),
        "rear_bus_energy_fraction_excluding_lfe": (
            round(rear / non_lfe, 9) if non_lfe > 0.0 else None
        ),
        "rear_to_front_energy_db": (
            round(10.0 * math.log10(rear / front), 6)
            if rear > 0.0 and front > 0.0
            else None
        ),
        "total_preclip_samples": int(np.sum(metrics.clipped_samples)),
    }


def circuit_aggregates(
    metrics: ChannelMetrics, circuits: Sequence[dict[str, object]]
) -> dict[str, object]:
    power = metrics.power()
    zone_power: dict[str, float] = {}
    for index, circuit in enumerate(circuits):
        zone = str(circuit["zone"])
        zone_power[zone] = zone_power.get(zone, 0.0) + float(power[index])
    front = sum(value for name, value in zone_power.items() if name.startswith("front"))
    rear = sum(value for name, value in zone_power.items() if name.startswith("rear"))
    non_sub = front + rear
    return {
        "maximum_preclip_peak": round(float(np.max(metrics.peaks)), 9),
        "maximum_preclip_peak_dbfs": ChannelMetrics._db(float(np.max(metrics.peaks))),
        "rear_circuit_energy_fraction_excluding_subwoofer": (
            round(rear / non_sub, 9) if non_sub > 0.0 else None
        ),
        "rear_to_front_circuit_energy_db": (
            round(10.0 * math.log10(rear / front), 6)
            if rear > 0.0 and front > 0.0
            else None
        ),
        "zone_power": {name: round(value, 12) for name, value in sorted(zone_power.items())},
        "total_preclip_samples": int(np.sum(metrics.clipped_samples)),
    }


def render_matrix(
    input_path: Path,
    output_path: Path,
    config_path: Path,
    report_path: Path | None,
    block_frames: int = DEFAULT_BLOCK_FRAMES,
) -> dict[str, object]:
    config = load_config(config_path)
    gains = linear_matrix(config)
    input_names = tuple(config["input_channels"])
    bus_names = tuple(config["logical_buses"])

    with Pcm16WavReader(input_path) as reader:
        if reader.info.channels not in (6, 8):
            raise CabinError(
                f"{input_path}: matrix mode requires 6 or 8 channels, got "
                f"{reader.info.channels}"
            )
        input_metrics = ChannelMetrics(input_names[: reader.info.channels])
        bus_metrics = ChannelMetrics(bus_names)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with Pcm16WavWriter(output_path, 8, reader.info.frames, channel_mask=0) as writer:
            for block in reader.blocks(block_frames):
                input_metrics.update(block)
                buses = apply_matrix(block, gains)
                bus_metrics.update(buses)
                writer.write(buses)

        matrix = config["matrix"]
        state = config["operating_state"]
        physical = config["physical_output_graph"]
        assert isinstance(matrix, dict) and isinstance(state, dict) and isinstance(physical, dict)
        report: dict[str, object] = {
            "schema_version": 1,
            "mode": "exact-logical-bus-matrix",
            "input": str(input_path),
            "output": str(output_path),
            "sample_rate": SAMPLE_RATE,
            "duration_seconds": round(reader.info.frames / SAMPLE_RATE, 6),
            "input_channel_order": list(input_names[: reader.info.channels]),
            "output_channel_order": list(bus_names),
            "input_metrics": input_metrics.report(),
            "logical_bus_metrics_preclip": bus_metrics.report(),
            "aggregates": matrix_aggregates(bus_metrics),
            "model": {
                "id": config["model_id"],
                "matrix_profile": matrix["profile"],
                "matrix_bank": matrix["bank"],
                "sink": matrix["sink"],
                "symphoria_3d_enabled": state["symphoria_3d_enabled"],
                "operating_state": state,
                "physical_output_graph_status": physical["status"],
            },
            "limits": [
                "This output ends at the eight logical SHARC buses.",
                "Audi wiring proves the 16 T38 output circuits, but not their internal SHARC bus indices or factory calibration.",
                "PCM output is clipped to int16; metrics above are measured before clipping."
            ],
        }

    if report_path is None:
        report_path = default_report_path(output_path)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    write_report(report, report_path)
    return report


def render_documentary_circuits(
    input_path: Path,
    output_path: Path,
    config_path: Path,
    report_path: Path | None,
    block_frames: int = DEFAULT_BLOCK_FRAMES,
) -> dict[str, object]:
    """Render exact input matrix plus explicitly inferred, equal-power T38 fan-out."""
    config = load_config(config_path)
    input_matrix = linear_matrix(config)
    fanout_matrix, circuit_names = documentary_fanout_matrix(config)
    input_names = tuple(config["input_channels"])
    bus_names = tuple(config["logical_buses"])
    physical = config["physical_output_graph"]
    fanout = config["documentary_bus_fanout"]
    assert isinstance(physical, dict) and isinstance(fanout, dict)
    circuits = physical["output_circuits"]
    assert isinstance(circuits, list)

    with Pcm16WavReader(input_path) as reader:
        if reader.info.channels not in (6, 8):
            raise CabinError(
                f"{input_path}: circuits mode requires 6 or 8 channels, got "
                f"{reader.info.channels}"
            )
        input_metrics = ChannelMetrics(input_names[: reader.info.channels])
        bus_metrics = ChannelMetrics(bus_names)
        circuit_metrics = ChannelMetrics(circuit_names)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with Pcm16WavWriter(output_path, 16, reader.info.frames, channel_mask=0) as writer:
            for block in reader.blocks(block_frames):
                input_metrics.update(block)
                buses = apply_matrix(block, input_matrix)
                circuit_samples = apply_documentary_fanout(buses, fanout_matrix)
                bus_metrics.update(buses)
                circuit_metrics.update(circuit_samples)
                writer.write(circuit_samples)

        report: dict[str, object] = {
            "schema_version": 1,
            "mode": "documentary-t38-circuit-model",
            "input": str(input_path),
            "output": str(output_path),
            "sample_rate": SAMPLE_RATE,
            "duration_seconds": round(reader.info.frames / SAMPLE_RATE, 6),
            "input_channel_order": list(input_names[: reader.info.channels]),
            "logical_bus_order": list(bus_names),
            "output_circuit_order": list(circuit_names),
            "output_circuits": circuits,
            "input_metrics": input_metrics.report(),
            "logical_bus_metrics_preclip": bus_metrics.report(),
            "circuit_metrics_preclip": circuit_metrics.report(),
            "logical_bus_aggregates": matrix_aggregates(bus_metrics),
            "aggregates": circuit_aggregates(circuit_metrics, circuits),
            "model": {
                "id": config["model_id"],
                "physical_output_graph_status": physical["status"],
                "fanout_status": fanout["status"],
                "fanout_normalization": fanout["normalization"],
                "operating_state": config["operating_state"],
            },
            "limits": [
                "The exact V850-recovered 8x8 input matrix is applied first.",
                "T38 pin pairs and connected speakers are proven by the Audi current-flow diagram.",
                "Logical-bus to T38 assignment is an architectural inference, not a recovered SHARC binding.",
                "Equal-power fan-out is a neutral test normalization, not Audi gain/delay/EQ calibration.",
                "The 16-channel PCM is an electrical-circuit audit artifact, not an acoustic cabin rendering.",
                "PCM output is clipped to int16; metrics above are measured before clipping.",
            ],
        }

    if report_path is None:
        report_path = default_report_path(output_path)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    write_report(report, report_path)
    return report


def read_all_wav(path: Path) -> tuple[WavInfo, np.ndarray]:
    with Pcm16WavReader(path) as reader:
        blocks = list(reader.blocks())
        samples = (
            np.concatenate(blocks, axis=0)
            if blocks
            else np.zeros((0, reader.info.channels), dtype=np.float64)
        )
        return reader.info, samples


def load_brir_manifest(
    path: Path, expected_channels: Sequence[str]
) -> tuple[str, list[np.ndarray], dict[str, object]]:
    try:
        manifest = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CabinError(f"cannot load BRIR manifest {path}: {exc}") from exc
    if manifest.get("sample_rate") != SAMPLE_RATE:
        raise CabinError("BRIR manifest sample_rate must be 48000")
    domain = manifest.get("domain")
    if domain not in ("most_inputs", "logical_buses"):
        raise CabinError("BRIR manifest domain must be most_inputs or logical_buses")
    names = manifest.get("channels")
    if names != list(expected_channels):
        raise CabinError(
            f"BRIR manifest channels must be exactly {list(expected_channels)}, got {names}"
        )
    responses = manifest.get("responses")
    if not isinstance(responses, dict):
        raise CabinError("BRIR manifest responses object is missing")

    global_gain = 10.0 ** (float(manifest.get("global_ir_gain_db", 0.0)) / 20.0)
    filters: list[np.ndarray] = []
    base = Path(path).resolve().parent
    for name in expected_channels:
        entry = responses.get(name)
        if not isinstance(entry, dict):
            raise CabinError(f"BRIR response {name} is missing")
        gain = 10.0 ** (float(entry.get("gain_db", 0.0)) / 20.0)
        if "file" in entry:
            info, ir = read_all_wav(base / str(entry["file"]))
            if info.channels != 2:
                raise CabinError(f"BRIR {name} file must be stereo, got {info.channels} ch")
        else:
            if "left" not in entry or "right" not in entry:
                raise CabinError(f"BRIR {name} needs file or left/right paths")
            left_info, left = read_all_wav(base / str(entry["left"]))
            right_info, right = read_all_wav(base / str(entry["right"]))
            if left_info.channels != 1 or right_info.channels != 1:
                raise CabinError(f"BRIR {name} left/right files must be mono")
            length = max(left.shape[0], right.shape[0])
            ir = np.zeros((length, 2), dtype=np.float64)
            ir[: left.shape[0], 0] = left[:, 0]
            ir[: right.shape[0], 1] = right[:, 0]
        if ir.shape[0] == 0:
            raise CabinError(f"BRIR {name} is empty")
        filters.append(ir * gain * global_gain)
    return str(domain), filters, manifest


def next_power_of_two(value: int) -> int:
    return 1 << (value - 1).bit_length()


def faded_log_sweep(
    frames: int,
    start_hz: float,
    end_hz: float,
    level_db: float,
    fade_frames: int,
) -> np.ndarray:
    if frames < 2 or start_hz <= 0.0 or end_hz <= start_hz:
        raise CabinError("invalid logarithmic sweep parameters")
    time = np.arange(frames, dtype=np.float64) / SAMPLE_RATE
    duration = frames / SAMPLE_RATE
    ratio_log = math.log(end_hz / start_hz)
    phase = (
        2.0
        * math.pi
        * start_hz
        * duration
        / ratio_log
        * (np.exp(time * ratio_log / duration) - 1.0)
    )
    sweep = np.sin(phase) * (10.0 ** (level_db / 20.0))
    fade_frames = min(fade_frames, frames // 2)
    if fade_frames:
        angle = np.linspace(0.0, math.pi, fade_frames, endpoint=False)
        fade = 0.5 - 0.5 * np.cos(angle)
        sweep[:fade_frames] *= fade
        sweep[-fade_frames:] *= fade[::-1]
    return sweep


def make_probe(
    output_path: Path,
    manifest_path: Path | None,
    level_db: float,
    sweep_seconds: float,
    tail_seconds: float,
    guard_seconds: float,
) -> dict[str, object]:
    if level_db > -18.0:
        raise CabinError("probe level above -18 dBFS is intentionally refused")
    if sweep_seconds < 0.25 or tail_seconds < 0.1 or guard_seconds < 0.1:
        raise CabinError("probe requires sweep>=0.25 s, tail>=0.1 s and guard>=0.1 s")
    channels = ("L", "R", "C", "LFE", "Ls", "Rs")
    pre_roll = SAMPLE_RATE
    sync_frames = SAMPLE_RATE // 2
    sync_start = pre_roll
    sync_tail = SAMPLE_RATE // 2
    guard_frames = round(guard_seconds * SAMPLE_RATE)
    sweep_frames = round(sweep_seconds * SAMPLE_RATE)
    tail_frames = round(tail_seconds * SAMPLE_RATE)
    cursor = sync_start + sync_frames + sync_tail
    schedule: list[dict[str, object]] = []
    for index, name in enumerate(channels):
        cursor += guard_frames
        schedule.append(
            {
                "channel_index": index,
                "channel_name": name,
                "sweep_start_frame": cursor,
                "sweep_frames": sweep_frames,
                "tail_frames": tail_frames,
            }
        )
        cursor += sweep_frames + tail_frames
    total_frames = cursor + SAMPLE_RATE

    sync = faded_log_sweep(
        sync_frames, 500.0, 8_000.0, level_db, SAMPLE_RATE // 100
    )
    sweep = faded_log_sweep(
        sweep_frames, 30.0, 22_000.0, level_db, SAMPLE_RATE // 20
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with Pcm16WavWriter(output_path, 6, total_frames, channel_mask=0x60F) as writer:
        position = 0
        for entry in schedule:
            start = int(entry["sweep_start_frame"])
            while position < start:
                count = min(DEFAULT_BLOCK_FRAMES, start - position)
                block = np.zeros((count, 6), dtype=np.float64)
                sync_begin = max(position, sync_start)
                sync_end = min(position + count, sync_start + sync_frames)
                if sync_end > sync_begin:
                    dst = sync_begin - position
                    src = sync_begin - sync_start
                    block[dst : dst + sync_end - sync_begin, 0] = sync[
                        src : src + sync_end - sync_begin
                    ]
                writer.write(block)
                position += count
            block = np.zeros((sweep_frames, 6), dtype=np.float64)
            block[:, int(entry["channel_index"])] = sweep
            writer.write(block)
            position += sweep_frames
        while position < total_frames:
            count = min(DEFAULT_BLOCK_FRAMES, total_frames - position)
            writer.write(np.zeros((count, 6), dtype=np.float64))
            position += count

    digest = hashlib.sha256(output_path.read_bytes()).hexdigest()
    manifest: dict[str, object] = {
        "schema_version": 1,
        "kind": "soundamp-virtual-cabin-sequential-sweep",
        "probe_wav": str(output_path.resolve()),
        "probe_sha256": digest,
        "sample_rate": SAMPLE_RATE,
        "channel_order": list(channels),
        "level_dbfs": level_db,
        "sync": {
            "channel_index": 0,
            "start_frame": sync_start,
            "frames": sync_frames,
            "start_hz": 500.0,
            "end_hz": 8_000.0,
        },
        "sweep": {
            "start_hz": 30.0,
            "end_hz": 22_000.0,
            "frames": sweep_frames,
            "fade_frames": SAMPLE_RATE // 20,
        },
        "schedule": schedule,
        "total_frames": total_frames,
        "duration_seconds": round(total_frames / SAMPLE_RATE, 6),
        "safety": [
            "Play only while parked, with the system volume starting low.",
            "Disable Symphoria/3D and keep every tone/focus/surround control fixed.",
            "Do not normalize, compress or noise-reduce the recorded stereo capture."
        ],
    }
    if manifest_path is None:
        manifest_path = output_path.with_name(output_path.name + ".probe.json")
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    write_report(manifest, manifest_path)
    return manifest


def fft_correlation_lag(recording: np.ndarray, reference: np.ndarray) -> tuple[int, float]:
    if recording.ndim != 1 or reference.ndim != 1 or reference.size > recording.size:
        raise CabinError("invalid sync correlation geometry")
    full_size = recording.size + reference.size - 1
    nfft = next_power_of_two(full_size)
    correlation = np.fft.irfft(
        np.fft.rfft(recording, n=nfft)
        * np.fft.rfft(reference[::-1], n=nfft),
        n=nfft,
    )[:full_size]
    full_index = int(np.argmax(np.abs(correlation)))
    lag = full_index - (reference.size - 1)
    if lag < 0 or lag + reference.size > recording.size:
        return lag, 0.0
    observed = recording[lag : lag + reference.size]
    denominator = math.sqrt(float(np.dot(observed, observed) * np.dot(reference, reference)))
    score = abs(float(np.dot(observed, reference))) / denominator if denominator > 0.0 else 0.0
    return lag, score


def slice_with_zeros(samples: np.ndarray, start: int, frames: int) -> np.ndarray:
    result = np.zeros((frames, samples.shape[1]), dtype=np.float64)
    source_start = max(start, 0)
    source_end = min(start + frames, samples.shape[0])
    if source_end > source_start:
        result_start = source_start - start
        result[result_start : result_start + source_end - source_start] = samples[
            source_start:source_end
        ]
    return result


def deconvolve_probe(
    capture_path: Path,
    output_directory: Path,
    probe_manifest_path: Path,
    config_path: Path,
    ir_seconds: float,
    regularization: float,
) -> dict[str, object]:
    config = load_config(config_path)
    if ir_seconds < 0.05 or regularization <= 0.0:
        raise CabinError("deconvolution requires ir_seconds>=0.05 and regularization>0")
    try:
        probe_manifest = json.loads(probe_manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CabinError(f"cannot load probe manifest {probe_manifest_path}: {exc}") from exc
    if probe_manifest.get("kind") != "soundamp-virtual-cabin-sequential-sweep":
        raise CabinError("not a SoundAmp virtual-cabin probe manifest")
    probe_path = Path(str(probe_manifest["probe_wav"]))
    if hashlib.sha256(probe_path.read_bytes()).hexdigest() != probe_manifest.get("probe_sha256"):
        raise CabinError("probe WAV hash does not match its manifest")
    probe_info, probe = read_all_wav(probe_path)
    capture_info, capture = read_all_wav(capture_path)
    if probe_info.channels != 6 or capture_info.channels != 2:
        raise CabinError("probe must be six-channel and cabin capture must be stereo")

    sync_info = probe_manifest["sync"]
    assert isinstance(sync_info, dict)
    sync_start = int(sync_info["start_frame"])
    sync_frames = int(sync_info["frames"])
    sync_channel = int(sync_info["channel_index"])
    sync = probe[sync_start : sync_start + sync_frames, sync_channel]
    capture_mono = np.mean(capture, axis=1)
    sync_lag, sync_score = fft_correlation_lag(capture_mono, sync)
    capture_origin = sync_lag - sync_start
    if sync_score < 0.02:
        raise CabinError(
            f"sync correlation is too weak ({sync_score:.4f}); check capture level and editing"
        )

    schedule = probe_manifest.get("schedule")
    if not isinstance(schedule, list) or len(schedule) != 6:
        raise CabinError("probe schedule must contain six entries")
    ir_frames = round(ir_seconds * SAMPLE_RATE)
    impulse_responses: list[tuple[str, np.ndarray]] = []
    for entry in schedule:
        if not isinstance(entry, dict):
            raise CabinError("malformed probe schedule entry")
        channel = int(entry["channel_index"])
        name = str(entry["channel_name"])
        sweep_start = int(entry["sweep_start_frame"])
        sweep_frames = int(entry["sweep_frames"])
        tail_frames = int(entry["tail_frames"])
        excitation = probe[sweep_start : sweep_start + sweep_frames, channel]
        observed = slice_with_zeros(
            capture, capture_origin + sweep_start, sweep_frames + tail_frames
        )
        nfft = next_power_of_two(observed.shape[0] + excitation.size - 1)
        excitation_fft = np.fft.rfft(excitation, n=nfft)
        denominator = np.abs(excitation_fft) ** 2
        denominator += regularization * float(np.max(denominator))
        ir = np.zeros((ir_frames, 2), dtype=np.float64)
        for ear in range(2):
            observed_fft = np.fft.rfft(observed[:, ear], n=nfft)
            transfer = observed_fft * np.conj(excitation_fft) / denominator
            decoded = np.fft.irfft(transfer, n=nfft)
            ir[:, ear] = decoded[:ir_frames]
        impulse_responses.append((name, ir))

    largest = max(float(np.max(np.abs(ir))) for _name, ir in impulse_responses)
    if largest <= 1e-12:
        raise CabinError("deconvolution produced silent impulse responses")
    storage_scale = 0.8 / largest
    global_ir_gain_db = -20.0 * math.log10(storage_scale)
    output_directory.mkdir(parents=True, exist_ok=True)
    responses: dict[str, object] = {}
    for name, ir in impulse_responses:
        path = output_directory / f"{name}.wav"
        with Pcm16WavWriter(path, 2, ir_frames) as writer:
            writer.write(ir * storage_scale)
        responses[name] = {"file": path.name, "gain_db": 0.0}

    state = config["operating_state"]
    assert isinstance(state, dict)
    manifest = {
        "schema_version": 1,
        "sample_rate": SAMPLE_RATE,
        "domain": "most_inputs",
        "description": "End-to-end J525/cabin responses decoded from the sequential six-channel probe.",
        "channels": list(config["input_channels"][:6]),
        "responses": responses,
        "global_ir_gain_db": round(global_ir_gain_db, 9),
        "capture": {
            "source": str(capture_path.resolve()),
            "probe_manifest": str(probe_manifest_path.resolve()),
            "seat": "fill-before-use",
            "symphoria_3d_enabled": state["symphoria_3d_enabled"],
            "state_evidence": "capture settings must be confirmed; model target is not measurement metadata",
            "surround_control": state["surround_control"],
            "sound_focus": state["sound_focus"],
            "bass_mid_treble": "neutral-unless-capture-notes-say-otherwise",
            "absolute_spl_calibrated": False,
        },
        "deconvolution": {
            "sync_lag_frame": sync_lag,
            "capture_origin_frame": capture_origin,
            "sync_normalized_correlation": round(sync_score, 9),
            "ir_frames": ir_frames,
            "regularization": regularization,
            "relative_channel_gain_preserved": True,
            "individual_peak_normalization": False,
        },
    }
    manifest_path = output_directory / "brir.json"
    write_report(manifest, manifest_path)
    return manifest


def render_binaural(
    input_path: Path,
    output_path: Path,
    config_path: Path,
    brir_manifest_path: Path,
    report_path: Path | None,
    gain_db: float,
    block_frames: int,
) -> dict[str, object]:
    config = load_config(config_path)
    all_inputs = tuple(config["input_channels"])
    bus_names = tuple(config["logical_buses"])
    with Pcm16WavReader(input_path) as reader:
        if reader.info.channels != 6:
            raise CabinError(
                f"{input_path}: binaural mode expects HSSR/APAC 5.1 in L,R,C,LFE,Ls,Rs "
                f"order; got {reader.info.channels} channels"
            )
        manifest_data = json.loads(Path(brir_manifest_path).read_text(encoding="utf-8"))
        domain_hint = manifest_data.get("domain")
        expected = all_inputs[:6] if domain_hint == "most_inputs" else bus_names
        domain, filters, manifest = load_brir_manifest(brir_manifest_path, expected)
        max_ir_frames = max(ir.shape[0] for ir in filters)
        nfft = next_power_of_two(block_frames + max_ir_frames - 1)
        filter_fft = np.zeros((len(filters), 2, nfft // 2 + 1), dtype=np.complex128)
        for channel, ir in enumerate(filters):
            for ear in range(2):
                filter_fft[channel, ear] = np.fft.rfft(ir[:, ear], n=nfft)

        output_frames = reader.info.frames + max_ir_frames - 1
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_metrics = ChannelMetrics(("left_ear", "right_ear"))
        overlap = np.zeros((max_ir_frames - 1, 2), dtype=np.float64)
        output_gain = 10.0 ** (gain_db / 20.0)
        matrix = linear_matrix(config)

        with Pcm16WavWriter(output_path, 2, output_frames) as writer:
            for input_block in reader.blocks(block_frames):
                source = (
                    input_block
                    if domain == "most_inputs"
                    else apply_matrix(input_block, matrix)
                )
                frames = source.shape[0]
                spectrum = np.zeros((2, nfft // 2 + 1), dtype=np.complex128)
                for channel in range(source.shape[1]):
                    source_fft = np.fft.rfft(source[:, channel], n=nfft)
                    spectrum += filter_fft[channel] * source_fft[np.newaxis, :]
                rendered = np.fft.irfft(spectrum, n=nfft, axis=1).T
                rendered = rendered[: frames + max_ir_frames - 1]
                if overlap.shape[0]:
                    rendered[: overlap.shape[0]] += overlap
                block_output = rendered[:frames] * output_gain
                overlap = rendered[frames : frames + max_ir_frames - 1]
                output_metrics.update(block_output)
                writer.write(block_output)
            if overlap.shape[0]:
                tail = overlap * output_gain
                output_metrics.update(tail)
                writer.write(tail)

        state = config["operating_state"]
        assert isinstance(state, dict)
        report = {
            "schema_version": 1,
            "mode": "measured-brir-binaural",
            "input": str(input_path),
            "output": str(output_path),
            "brir_manifest": str(brir_manifest_path),
            "brir_domain": domain,
            "sample_rate": SAMPLE_RATE,
            "input_frames": reader.info.frames,
            "ir_frames": max_ir_frames,
            "output_frames": output_frames,
            "fft_frames": nfft,
            "gain_db": gain_db,
            "output_metrics_preclip": output_metrics.report(),
            "model": {
                "id": config["model_id"],
                "symphoria_3d_enabled": state["symphoria_3d_enabled"],
                "capture": manifest.get("capture"),
                "operating_state": state,
            },
            "limits": [
                "Acoustic validity is determined by the supplied measured responses.",
                "A most_inputs BRIR already includes the OEM matrix/DSP/speakers/cabin; the matrix is not applied twice.",
                "PCM output is clipped to int16; metrics above are measured before clipping."
            ],
        }

    if report_path is None:
        report_path = default_report_path(output_path)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    write_report(report, report_path)
    return report


def print_matrix(config: dict[str, object]) -> None:
    matrix = config["matrix"]
    assert isinstance(matrix, dict)
    inputs = list(config["input_channels"])
    outputs = list(config["logical_buses"])
    gains = matrix["gain_db"]
    width = 9
    print(f"model: {config['model_id']}")
    print("Symphoria/3D: OFF")
    print(f"matrix: profile {matrix['profile']}, bank {matrix['bank']} ({matrix['sink']})")
    print("input ->" + "".join(f"{name:>{width}}" for name in outputs))
    for name, row in zip(inputs, gains):
        print(f"{name:<8}" + "".join(f"{value:>{width}.0f}" for value in row))


def inspect_config(
    config_path: Path, show_speakers: bool, show_circuits: bool
) -> None:
    config = load_config(config_path)
    print_matrix(config)
    physical = config["physical_output_graph"]
    assert isinstance(physical, dict)
    print(
        f"physical graph: {physical['amplified_channel_count']} amplifier channels, "
        f"{physical['speaker_count']} speakers, binding={physical['status']}"
    )
    if show_speakers:
        print("speakers (service-manual location; coordinates are schematic only):")
        for speaker in config["speakers"]:
            assert isinstance(speaker, dict)
            print(f"  {speaker['id']:>4}  {speaker['name']:<27} {speaker['location']}")
    if show_circuits:
        circuits = physical["output_circuits"]
        assert isinstance(circuits, list)
        print("T38 output circuits (Audi current-flow diagram):")
        for index, circuit in enumerate(circuits, 1):
            assert isinstance(circuit, dict)
            pin_text = "/".join(str(pin) for pin in circuit["pins"])
            speaker_text = "+".join(str(name) for name in circuit["speaker_ids"])
            print(
                f"  row {index:>2}  inferred ch {circuit['inferred_amplifier_channel']:>2}  "
                f"T38/{pin_text:<5} {speaker_text:<11} "
                f"{circuit['zone']:<13} {circuit['topology']}"
            )
        fanout = config["documentary_bus_fanout"]
        assert isinstance(fanout, dict)
        print(
            "documentary fan-out: "
            f"{fanout['status']}; normalization={fanout['normalization']}"
        )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Audi J525 9VS 3D-Off logical-bus and measured-cabin simulator"
    )
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    sub = parser.add_subparsers(dest="command", required=True)

    inspect_parser = sub.add_parser("inspect", help="show the verified model state")
    inspect_parser.add_argument("--speakers", action="store_true")
    inspect_parser.add_argument("--circuits", action="store_true")

    probe_parser = sub.add_parser(
        "probe", help="generate a safe sequential six-channel cabin-measurement sweep"
    )
    probe_parser.add_argument("output", type=Path, help="six-channel PCM16 probe WAV")
    probe_parser.add_argument("--manifest", type=Path)
    probe_parser.add_argument("--level-db", type=float, default=-30.0)
    probe_parser.add_argument("--sweep-seconds", type=float, default=4.0)
    probe_parser.add_argument("--tail-seconds", type=float, default=1.5)
    probe_parser.add_argument("--guard-seconds", type=float, default=0.5)

    deconvolve_parser = sub.add_parser(
        "deconvolve", help="turn a stereo recording of the probe into six BRIR WAVs"
    )
    deconvolve_parser.add_argument("capture", type=Path, help="unaltered stereo PCM16 recording")
    deconvolve_parser.add_argument("output_directory", type=Path)
    deconvolve_parser.add_argument("--probe-manifest", type=Path, required=True)
    deconvolve_parser.add_argument("--ir-seconds", type=float, default=1.5)
    deconvolve_parser.add_argument("--regularization", type=float, default=1e-6)

    matrix_parser = sub.add_parser(
        "matrix", help="apply the exact profile-0/bank-1 input-to-bus matrix"
    )
    matrix_parser.add_argument("input", type=Path, help="48 kHz PCM16 5.1/7.1 WAV")
    matrix_parser.add_argument("output", type=Path, help="eight logical bus PCM16 WAV")
    matrix_parser.add_argument("--report", type=Path)
    matrix_parser.add_argument("--block-frames", type=int, default=DEFAULT_BLOCK_FRAMES)

    circuits_parser = sub.add_parser(
        "circuits",
        help="render the exact matrix and documentary 16-circuit T38 topology",
    )
    circuits_parser.add_argument("input", type=Path, help="48 kHz PCM16 5.1/7.1 WAV")
    circuits_parser.add_argument("output", type=Path, help="16-circuit PCM16 WAV")
    circuits_parser.add_argument("--report", type=Path)
    circuits_parser.add_argument("--block-frames", type=int, default=DEFAULT_BLOCK_FRAMES)

    binaural_parser = sub.add_parser(
        "binaural", help="render a 5.1 WAV through measured stereo impulse responses"
    )
    binaural_parser.add_argument("input", type=Path, help="48 kHz PCM16 HSSR/APAC 5.1 WAV")
    binaural_parser.add_argument("output", type=Path, help="stereo headphone WAV")
    binaural_parser.add_argument("--brir-manifest", type=Path, required=True)
    binaural_parser.add_argument("--report", type=Path)
    binaural_parser.add_argument("--gain-db", type=float, default=0.0)
    binaural_parser.add_argument("--block-frames", type=int, default=DEFAULT_BLOCK_FRAMES)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "inspect":
            inspect_config(args.config, args.speakers, args.circuits)
            return 0
        if args.command == "probe":
            manifest = make_probe(
                args.output,
                args.manifest,
                args.level_db,
                args.sweep_seconds,
                args.tail_seconds,
                args.guard_seconds,
            )
            manifest_path = args.manifest or args.output.with_name(
                args.output.name + ".probe.json"
            )
            print(f"wrote {args.output} ({manifest['duration_seconds']:.3f} s)")
            print(f"wrote {manifest_path}")
            return 0
        if args.command == "deconvolve":
            manifest = deconvolve_probe(
                args.capture,
                args.output_directory,
                args.probe_manifest,
                args.config,
                args.ir_seconds,
                args.regularization,
            )
            deconvolution = manifest["deconvolution"]
            assert isinstance(deconvolution, dict)
            score = deconvolution["sync_normalized_correlation"]
            print(f"wrote {args.output_directory / 'brir.json'}")
            print(f"sync normalized correlation: {score}")
            return 0
        if args.block_frames <= 0:
            raise CabinError("--block-frames must be positive")
        if args.command == "matrix":
            report = render_matrix(
                args.input,
                args.output,
                args.config,
                args.report,
                args.block_frames,
            )
        elif args.command == "circuits":
            report = render_documentary_circuits(
                args.input,
                args.output,
                args.config,
                args.report,
                args.block_frames,
            )
        else:
            report = render_binaural(
                args.input,
                args.output,
                args.config,
                args.brir_manifest,
                args.report,
                args.gain_db,
                args.block_frames,
            )
        aggregate = report.get("aggregates", report.get("output_metrics_preclip"))
        print(json.dumps(aggregate, indent=2, sort_keys=True))
        report_path = args.report or default_report_path(args.output)
        print(f"wrote {args.output}")
        print(f"wrote {report_path}")
        return 0
    except (CabinError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
