#!/usr/bin/env python3
"""astra review 14, first throughput change: a SEQUENTIAL precision benchmark on identical batches and one
checkpoint - fp32 (current), TF32 matmul, bf16 autocast (the losses and SI-SDR kept in fp32).  Reports step time,
peak memory, and the loss / gradient agreement with fp32.  Needs the GPU to itself (run between queued runs).
  bench_precision.py CACHE TEACHER_CACHE [--steps 30] [--batch 12]"""
import argparse, os, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN, sep_net as SN


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("cache"); ap.add_argument("tcache"); ap.add_argument("--steps", type=int, default=30); ap.add_argument("--batch", type=int, default=12)
    a = ap.parse_args()
    torch = SN._torch(); dev = "cuda"; torch.manual_seed(1); rng = np.random.default_rng(1)
    split = MN.load_split(a.cache)
    truth = [os.path.join(a.cache, t + ".npy") for t in split["train"]]
    pseudo = [os.path.join(a.tcache, f) for f in sorted(os.listdir(a.tcache)) if f.endswith(".npy") and os.path.exists(os.path.join(a.tcache, f[:-4] + ".json"))]
    batches = [SN.windows(torch, (truth, pseudo), 8.0, a.batch, rng) for _ in range(a.steps)]
    net = SN.BandSepNet(torch).to(dev)
    with torch.no_grad():
        lm = torch.cat([torch.log(SN.stft(torch, b[0].to(dev)).abs() + 1e-5).flatten() for b in batches[:4]])
        SN.STATS["mean"], SN.STATS["std"] = float(lm.mean()), float(lm.std() + 1e-3)
    state0 = {k: v.clone() for k, v in net.modules.state_dict().items()}
    ref_grads = None

    def run(mode):
        nonlocal ref_grads
        net.modules.load_state_dict(state0)
        torch.backends.cuda.matmul.allow_tf32 = mode == "tf32"; torch.backends.cudnn.allow_tf32 = mode == "tf32"
        opt = torch.optim.AdamW(net.parameters(), lr=3e-4)
        torch.cuda.reset_peak_memory_stats(); torch.cuda.synchronize()
        losses = []; grad_l2 = []; first = None; t0 = None
        for i, (mix, elig) in enumerate(batches):
            if i == 5: torch.cuda.synchronize(); t0 = time.time()                    # warm-up excluded
            mix, elig = mix.to(dev), elig.to(dev)
            opt.zero_grad()
            with torch.autocast("cuda", dtype=torch.bfloat16, enabled=(mode == "bf16")):
                X = SN.stft(torch, mix); Est = net.separate_stft(X)
            Est = Est.to(torch.complex64); Tt = SN.stft(torch, elig)
            loss = SN.loss_audio(torch, Est, Tt) + SN.multires_loss(torch, SN.istft(torch, Est, mix.shape[-1]), elig)
            loss.backward()
            g = torch.sqrt(sum((p.grad.float() ** 2).sum() for p in net.parameters() if p.grad is not None))
            if i == 0:
                first = (float(loss), [p.grad.detach().float().clone() for p in net.parameters()])
            torch.nn.utils.clip_grad_norm_(net.parameters(), 5.0); opt.step()
            losses.append(float(loss)); grad_l2.append(float(g))
        torch.cuda.synchronize(); dt = (time.time() - t0) / (len(batches) - 5)
        mem = torch.cuda.max_memory_allocated() / 2 ** 30
        agree = ""
        if mode == "fp32":
            ref_grads = first
        else:
            cos = float(sum((x * y).sum() for x, y in zip(first[1], ref_grads[1])) / (torch.sqrt(sum((x ** 2).sum() for x in first[1])) * torch.sqrt(sum((y ** 2).sum() for y in ref_grads[1])) + 1e-12))
            agree = f" | first-step loss {first[0]:.5f} vs fp32 {ref_grads[0]:.5f} (rel {abs(first[0] - ref_grads[0]) / ref_grads[0]:.2e}), grad cosine {cos:.6f}"
        print(f"{mode:5s}: {dt * 1000:7.1f} ms/step ({1000 / dt * 0 + 1000 * dt / 1000:.3f} s) -> {40000 * dt / 3600:.2f} h per 40k | peak mem {mem:.2f} GB | mean loss {np.mean(losses[5:]):.4f} grad L2 {np.mean(grad_l2[5:]):.3f}{agree}", flush=True)

    for mode in ("fp32", "tf32", "bf16"):
        run(mode)


if __name__ == "__main__":
    main()
