# results

The raw output the stage-k numbers in `../../STATUS.md` come from. Committed rather than
regenerated on demand: the QNX run takes about twenty minutes, and the tree it measured is named
below.

| file | what |
| --- | --- |
| `qnx_armv7_icount.txt` | QNX 6.5 / armle-v7, GCC 4.9.4 `-O2`, under QEMU with `-icount shift=0`. The numbers are ARMv7 instruction counts. `--reps 2`. |
| `host_arm64.txt` | this M3 Max, clang `-O2`, wall clock in nanoseconds, `--reps 20`. Ends with the real-key cross-check over the capture's own encrypted bytes. |
| `host_sample_profile.txt` | `sample(1)` over the host build, top of stack, as an independent check on where the time goes. |
| `footprint.txt` | pages of the decoder object one packet touches, by mprotect and a fault handler. |
| `gate_check.txt` | which BWE cases leave the spectrum unchanged, and whether the LIBRARY leaves it unchanged on the same ones. |

The armle-v7 log also carries the BWE and LRVQ case measurements — the two paths this capture never
takes — and an `integrity:` line at the end. That line is not decoration: an earlier version of this
benchmark corrupted its own calibration constant through a 512-float overrun, and the probes behind
that line are what localised it. See STATUS.md, "The bench had a memory corruption".

Tree: `4268ec4` for the capture-derived figures, with stage e (the BWE synthesis) landed and the decoder built under the unified
flags of `scripts/apac_flags.sh`. `qnx_armv7_icount.txt` is byte identical to the run against the
tree before stage e was committed — a deterministic instruction count that does not move at all is
that code never being entered on this content.

`sizeof(apac::Decoder)` reads 1 636 036 in the QNX file and 1 637 272 in the host file. That is the
same sources on two architectures, not two versions: the gap is ABI padding, and armle-v7's is the
figure that describes the head unit.

`captures/apac_5_1_live_20260831_0205` is the input in every case — 3980 frames, 3977 of them
carrying an APAC packet. No session key appears in any of these files or in anything that reached
the emulator; see `../README.md`.
