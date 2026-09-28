# Posthoc follow-up to two near misses on the frozen full-40 oracle-audio set.
# This tunes rear-air-off gain and front retention, and is exploratory.
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
if (-not (Test-Path "$O\decision.json")) { throw 'Frozen full-40 validation is missing' }
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
$points = 'baseline,rear_air_off_gain0427,front_retention070'
python "$T\oracle_renderer_sweep.py" --render "$T\upmix_render_inj.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' --probe $O `
    --expected-songs 40 --points $points
if ($LASTEXITCODE -ne 0) { throw "Exploratory full-40 C render failed; inspect $O" }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope full40 `
    --points $points --out "$O\tune_decision.json"
if ($LASTEXITCODE -ne 0) { throw "Exploratory full-40 evaluation failed; inspect $O" }
