"""Privileged cell-wise output-representation bounds, without a neural encoder.

Feasible coefficients are a constructive certificate. A failed construction
alone never proves infeasibility; the convex rear objective has a box tangent
lower bound for that purpose. This oracle is not causal source inference.
"""
import torch


def raw_to_output(raw, x, net):
    """Identical identity, endpoint and projection convention as stock forward."""
    operators = net.identity_operators+torch.complex(
        raw[..., 0], raw[..., 1]*net.imaginary_endpoint[None, None, :, None, None])
    d, right = net.downmix.to(x.dtype), net.right_inverse.to(x.dtype)
    error = net.input_identity-torch.einsum('oc,btfci->btfoi', d, operators)
    operators = operators+torch.einsum('co,btfoi->btfci', right, error)
    return torch.einsum('btfci,btfi->btfc', operators, x)


def construct(x, target, net, bounded=True):
    baseline = torch.einsum('ci,btfi->btfc', net.identity_operators, x)
    delta = target-baseline
    power = x.abs().square().sum(-1)
    coefficients = delta[..., None]*x.conj()[..., None, :]/power.clamp_min(1e-20)[..., None, None]
    raw = torch.stack([coefficients.real, coefficients.imag], -1)
    raw[..., 1] *= net.imaginary_endpoint[None, None, :, None, None]
    # Strict interior: each raw value has finite atanh(raw/limit) head logits.
    if bounded:
        raw = raw.clamp(-net.config.coefficient_limit*(1-1e-5), net.config.coefficient_limit*(1-1e-5))
    return raw


def rear_nmse(output, target, warm):
    error, truth = output[:, warm:, :, 3:]-target[:, warm:, :, 3:], target[:, warm:, :, 3:]
    return error.abs().square().sum((0, 1, 2))/truth.abs().square().sum((0, 1, 2))


def refine(raw, x, target, net, warm, steps=1000):
    """Projected quadratic minimization plus a certified tangent lower bound.

    Mean normalized Ls/Rs squared error; front error is reported separately.
    Optimize only scored frames, keeping the constructive warm prefix.
    """
    limit = net.config.coefficient_limit*(1-1e-5)
    d, right = net.downmix.double(), net.right_inverse.double()
    projection = torch.eye(5, dtype=torch.float64)-right@d
    powers = target[:, warm:].abs().square().sum((0, 1, 2))
    weights = torch.zeros(5, dtype=torch.float64)
    weights[3:] = .5/powers[3:].double()
    eigen = torch.linalg.eigvalsh(projection.T@torch.diag(weights)@projection).max()
    projection = projection.to(x.dtype)
    weights = weights.to(x.real.dtype)
    power = x.abs().square().sum(-1)
    lipschitz = (2*eigen.to(power.dtype)*power).clamp_min(1e-20)
    history = []
    for step in range(steps):
        output = raw_to_output(raw, x, net)
        error = (output-target)*weights
        error[:, :warm] = 0
        grad = 2*torch.einsum('co,btfo->btfc', projection.T, error)[..., None]*x.conj()[..., None, :]
        gradient = torch.stack([grad.real, grad.imag], -1)
        gradient[..., 1] *= net.imaginary_endpoint[None, None, :, None, None]
        update = (raw-gradient/lipschitz[..., None, None, None]).clamp(-limit, limit)
        update[:, :warm] = raw[:, :warm]
        raw = update
        if (step+1)%100 == 0 or step+1 == steps:
            output = raw_to_output(raw, x, net)
            objective = rear_nmse(output, target, warm).mean()
            error = (output-target)*weights
            error[:, :warm] = 0
            grad = 2*torch.einsum('co,btfo->btfc', projection.T, error)[..., None]*x.conj()[..., None, :]
            gradient = torch.stack([grad.real, grad.imag], -1)
            gradient[..., 1] *= net.imaginary_endpoint[None, None, :, None, None]
            gap = (gradient*raw).sum()+limit*gradient.abs().sum()
            lower = max(0., float(objective-gap))
            history.append(dict(step=step+1, mean_rear_nmse=float(objective),
                                tangent_lower_bound=lower, primal_gap=float(gap)))
    return raw, history
