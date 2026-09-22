"""Verified J525 stage math, NOT an end-to-end or acoustic cabin emulator.

Finite audio only; float64 reference arithmetic, not SHARC bit equivalence.
Callers must supply signals at the actual stage boundary. In particular a
ten-bus input is NOT obtained by padding the eight input-matrix buses: the
unmodelled complex/stateful processor cannot silently become an identity.
Evidence: docs/Audio/SHARC DSP Blocks.md.
"""
from __future__ import annotations

from dataclasses import dataclass
import numpy as np


def audio_block(samples: np.ndarray) -> np.ndarray:
    block = np.asarray(samples, dtype=np.float64)
    if block.ndim != 2 or block.shape[1] == 0 or not np.isfinite(block).all():
        raise ValueError("expected finite frames x channels audio")
    return block


def select(samples: np.ndarray, indices: list[int]) -> np.ndarray:
    """VISA 0x128910. Signed or uint32 -1 and other invalid indices mute."""
    block = audio_block(samples)
    indices_array = np.asarray(indices)
    if indices_array.ndim != 1 or indices_array.dtype.kind not in "iu":
        raise ValueError("selector indices must be integers")
    valid = (indices_array >= 0) & (indices_array < block.shape[1])
    result = np.zeros((len(block), len(indices_array)), dtype=np.float64)
    result[:, valid] = block[:, indices_array[valid].astype(np.int64)]
    return result


def pair_mix(samples: np.ndarray, interleaved_gains: np.ndarray) -> np.ndarray:
    """VISA 0x12694C: inputs [A0..AN-1,B0..BN-1], gains [a0,b0,...]."""
    block = audio_block(samples)
    gains = np.asarray(interleaved_gains, dtype=np.float64)
    if block.shape[1] % 2 or gains.shape != (block.shape[1],) or not np.isfinite(gains).all():
        raise ValueError("pair mixer requires 2N inputs and 2N finite gains")
    n = block.shape[1] // 2
    return block[:, :n] * gains[0::2] + block[:, n:] * gains[1::2]


@dataclass(frozen=True)
class PairState:
    """Explicit steady-state hypotheses, not decoded live MMI settings."""
    level: int
    processing_enabled: bool
    b01_valid: bool
    asad: int
    # Calibration table result, NOT the UI Surround number. Required for C0.
    surround_delta_tenths_db: int | None = None


def pair_gains(audit: dict, state: PairState) -> tuple[int, np.ndarray]:
    if state.level not in range(4) or state.asad not in (0xC0, 0xFC):
        raise ValueError("only audited levels 0..3 and ASAD C0/FC are supported")
    if not isinstance(state.processing_enabled, bool) or not isinstance(state.b01_valid, bool):
        raise ValueError("explicit boolean gate states are required")
    row = state.level + 1 if state.processing_enabled and state.b01_valid else 0
    stored = np.asarray(audit["ten_bus_pair_gain_rows_db"], dtype=np.float64)
    if stored.shape != (5, 20) or not np.isfinite(stored).all():
        raise ValueError("expected all five audited 20-value gain rows")
    tenths = stored[row].copy() * 10.0
    if row > 0 and state.asad == 0xC0:
        delta = state.surround_delta_tenths_db
        if not isinstance(delta, int):
            raise ValueError("C0 needs the actual calibration-derived Surround delta")
        tenths[1::2] = np.clip(tenths[1::2] + delta, -990, 200)
    # This exact-mute sentinel belongs to pair gains, NOT the input matrix.
    gains = np.where(tenths <= -1000, 0.0, np.power(10.0, tenths / 200.0))
    return row, gains


class PackedOutputEQ:
    """Steady coefficients from V850 five-word records [-a1,-a2,b0,b1,b2].

    Counts follow SHARC processing-slot order. Zero-count channels bypass
    without consuming a record. Unused capacity records are ignored.
    Coefficients must be supplied: design-record Q/type conversion, live
    coefficient smoothing and IIR accelerator rounding are NOT emulated.
    """
    def __init__(self, counts: list[int], coefficients: np.ndarray, capacity: int = 144):
        if (not counts or any(type(n) is not int or n < 0 for n in counts)
                or sum(counts) > capacity):
            raise ValueError("invalid packed EQ counts/capacity")
        coefficients = np.asarray(coefficients, dtype=np.float64)
        if (coefficients.ndim != 2 or coefficients.shape[1] != 5
                or len(coefficients) < sum(counts) or len(coefficients) > capacity
                or not np.isfinite(coefficients).all()):
            raise ValueError("need finite five-word coefficients for every used stage")
        self.counts = tuple(counts)
        self.coefficients = coefficients[:sum(counts)].copy()
        self.state = np.zeros((sum(counts), 2), dtype=np.float64)

    def reset(self) -> None:
        self.state.fill(0.0)

    def process(self, samples: np.ndarray) -> np.ndarray:
        result = audio_block(samples).copy()
        if result.shape[1] != len(self.counts):
            raise ValueError("EQ block does not match processing-slot channel count")
        cursor = 0
        for channel, count in enumerate(self.counts):
            for record in range(cursor, cursor + count):
                ma1, ma2, b0, b1, b2 = self.coefficients[record]
                d1, d2 = self.state[record]
                for frame in range(len(result)):
                    x = result[frame, channel]
                    y = b0 * x + d1
                    d1, d2 = b1 * x + ma1 * y + d2, b2 * x + ma2 * y
                    result[frame, channel] = y
                self.state[record] = d1, d2
            cursor += count
        if not np.isfinite(result).all():
            raise ValueError("non-finite EQ output; coefficients/state require investigation")
        return result


def run_ordered(signals: dict[int, np.ndarray], stages: list[dict]) -> dict[int, np.ndarray]:
    """Execute explicit selectors/mixers/EQ in order, respecting reused IDs.

    EQ stages carry a persistent PackedOutputEQ instance under 'processor'.
    An unknown stage is an error, never an implicit pass-through.
    """
    values = {key: audio_block(value).copy() for key, value in signals.items()}
    for stage in stages:
        # Snapshot all inputs before replacing any reused output ID.
        block = np.concatenate([values[key] for key in stage["inputs"]], axis=1)
        kind = stage["kind"]
        if kind == "selector":
            result = select(block, stage["indices"])
        elif kind == "pair_mixer":
            result = pair_mix(block, stage["gains"])
        elif kind == "packed_output_eq":
            result = stage["processor"].process(block)
        else:
            raise ValueError(f"unmodelled stage: {kind}")
        outputs = stage["outputs"]
        if len(outputs) != result.shape[1] or len(set(outputs)) != len(outputs):
            raise ValueError("stage output IDs must uniquely match output channels")
        for i, key in enumerate(outputs):
            values[key] = result[:, i:i + 1].copy()
    return values
