#!/usr/bin/env python3
"""Physical real-audio synthesis for Track B complex half-spectrum outputs.

cuFFT C2R requires real DC/Nyquist endpoints. Never hand an unconstrained
complex neural mask's endpoints to inverse real FFT on any backend.
"""
import torch
import sep_net as SN


def real_endpoints(spectra):
    if not spectra.is_complex() or spectra.shape[-2] != SN.N_BINS:
        raise ValueError('Complex 2048-point stereo half-spectrum required')
    # Functional projection preserves autograd and does not mutate the neural
    # operator or its source-attribution copies. Imaginary endpoint gradients0.
    first=torch.complex(spectra[...,:1,:].real,torch.zeros_like(spectra[...,:1,:].real))
    last=torch.complex(spectra[...,-1:,:].real,torch.zeros_like(spectra[...,-1:,:].real))
    return torch.cat((first,spectra[...,1:-1,:],last),dim=-2)


def istft_centered(spectra, samples):
    return SN.istft(torch,real_endpoints(spectra),samples)


def objective(estimate, target, target_waveform, samples):
    # Keep the historical spectral objective, including endpoint supervision.
    # Only the synthesis obeys the real-audio boundary condition explicitly.
    return SN.loss_audio(torch,estimate,target)+SN.multires_loss(
        torch,istft_centered(estimate,samples),target_waveform)
