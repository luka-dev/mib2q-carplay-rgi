# results

The raw output the stage-k numbers in `../../PORT_STATUS.md` come from. Committed rather than
regenerated on demand: the QNX run takes about twenty minutes, and the tree it measured is named
below.

| file | what |
| --- | --- |
| `qnx_armv7_icount.txt` | QNX 6.5 / armle-v7, GCC 4.9.4 `-O2`, under QEMU with `-icount shift=0`. The numbers are ARMv7 instruction counts. `--reps 2`. |
| `host_arm64.txt` | this M3 Max, clang `-O2`, wall clock in nanoseconds, `--reps 20`. Ends with the real-key cross-check over the capture's own encrypted bytes. |
| `host_sample_profile.txt` | `sample(1)` over the host build, top of stack, as an independent check on where the time goes. |
| `footprint.txt` | pages of the decoder object one packet touches, by mprotect and a fault handler. |

Tree: `4f5f080` plus the in-flight BWE synthesis work, which is why `sizeof(apac::Decoder)` reads
1 636 036 rather than the 1 462 096 the earlier stages record. The BWE tables are `.rodata` and are
not touched on this content.

`captures/apac_5_1_live_20260831_0205` is the input in every case — 3980 frames, 3977 of them
carrying an APAC packet. No session key appears in any of these files or in anything that reached
the emulator; see `../README.md`.
