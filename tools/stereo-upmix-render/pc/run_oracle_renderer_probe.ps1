# CPU-only direct C-renderer mechanism probe on oracle eligible audio.
# Run after the rapid 10k pair has finished to avoid competing for PC I/O.
# This enriched screen includes all original swing songs; it cannot certify a
# general improvement without a subsequent full-40 and untouched-20 check.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\oracle_renderer_probe"
$Q = "$R\rapid\QUEUE.txt"
$pending = @(Get-Content $Q -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs; wait until it finishes" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object { $_.CommandLine -match 'sep_net.py train|upmix_stem_tests' })
if ($busy.Count -ne 0) { throw "Training or stem harness is active; wait until it finishes" }
New-Item -ItemType Directory -Force $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\prepare_oracle_renderer_probe.py" --oracle-run "$R\iface8\inject_oracle" --out $O
if ($LASTEXITCODE -ne 0) { throw 'Could not freeze oracle renderer probe' }
python "$T\oracle_renderer_sweep.py" --render "$T\upmix_render_inj.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' --probe $O
if ($LASTEXITCODE -ne 0) { throw "Oracle renderer screen failed; inspect $O and resume the same command" }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O
if ($LASTEXITCODE -ne 0) { throw "Oracle renderer decision gate failed; inspect $O" }
