# Confirm the two promoted oracle-audio renderer knobs on the original 40 DEV songs.
# CPU-only, real C renderer, true isolated eligible stems: non-deployable upper bound.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\oracle_renderer_full40"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
New-Item -ItemType Directory -Force $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\prepare_oracle_renderer_probe.py" --oracle-run "$R\iface8\inject_oracle" --out $O --all-songs
if ($LASTEXITCODE -ne 0) { throw 'Could not freeze full-40 oracle validation' }
python "$T\oracle_renderer_sweep.py" --render "$T\upmix_render_inj.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' --probe $O `
    --expected-songs 40 --points 'baseline,rear_air_off,front_retention075'
if ($LASTEXITCODE -ne 0) { throw "Full-40 C render failed; inspect $O and resume the same command" }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope full40
if ($LASTEXITCODE -ne 0) { throw "Full-40 decision gate failed; inspect $O" }
