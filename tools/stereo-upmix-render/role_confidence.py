#!/usr/bin/env python3
"""Host-only causal confidence mapping for a frozen four-role separator.

The frozen rule maps eligible-versus-protected logit margin with a .55
probability centre and .10 logit softness. The optional one-pole memory is
causal and takes 80 ms to move 63% toward a new logit; it adds no lookahead.
This is a prototype, not an int8 head-unit implementation.
"""

import math

import sep_net as SN

PROBABILITY_CENTRE = 0.55
LOGIT_CENTRE = math.log(PROBABILITY_CENTRE / (1 - PROBABILITY_CENTRE))
SOFTNESS = 0.10
EMA_MS = 80.0


def eligible_margin(torch, logits):
    """Four role logits [B,T,F,4] -> eligible vs combined protected margin."""
    return logits[..., 3] - torch.logsumexp(logits[..., :3], -1)


def causal_ema(torch, values, state=None, tau_ms=EMA_MS):
    """Causal per-bin EMA with an explicit state for exact chunk continuation."""
    if tau_ms <= 0:
        return values, state
    decay = math.exp(-SN.HOP / (SN.SR * tau_ms / 1000))
    out = torch.empty_like(values)
    first = 0
    if state is None:
        state = values[:, 0]
        out[:, 0] = state
        first = 1
    for frame in range(first, values.shape[1]):
        state = decay * state + (1 - decay) * values[:, frame]
        out[:, frame] = state
    return out, state


def mask_from_logits(torch, logits, variant, state=None):
    """Return shared real mask [B,T,F,1] and next state; variants are frozen."""
    if variant not in ("soft_static", "soft_ema80"):
        raise ValueError(f"unknown role-confidence variant: {variant}")
    margin = eligible_margin(torch, logits)
    smoothed, next_state = causal_ema(
        torch, margin, state, EMA_MS if variant == "soft_ema80" else 0.0)
    return torch.sigmoid((smoothed - LOGIT_CENTRE) / SOFTNESS)[..., None], next_state
