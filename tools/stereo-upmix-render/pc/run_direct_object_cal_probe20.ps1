# One precomputed gain calibration: 0.4 * 10^(-1.843525/20) = 0.323507.
# The 1.843525 dB paired rear-RMS delta came from the 12 nonswing controls
# in the frozen 20-song screen, not from the eight selected swing songs.
# Fixed to 0.3235 before this rerender; no adaptive per-song gain.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$B = "$T\direct_object_host"
$O = "$R\direct_object_probe20"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
if (-not (Test-Path "$O\manifest.json")) { throw 'Frozen direct-object screen missing' }
$build = Get-Content "$B\manifest.json" -Raw | ConvertFrom-Json
$actual = (Get-FileHash "$B\upmix_direct_no_bed.exe" -Algorithm SHA256).Hash
if ($actual -ne $build.builds.direct_no_bed.sha256) { throw 'Direct/no-bed host binary hash mismatch' }
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\oracle_renderer_sweep.py" --render "$B\upmix_control.exe" `
    --render-overrides "direct_no_bed_body0_cal=$B\upmix_direct_no_bed.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --probe $O --expected-songs 20 --points 'baseline,direct_no_bed_body0_cal'
if ($LASTEXITCODE -ne 0) { throw 'Calibrated direct/no-bed C screen failed' }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope probe `
    --points 'baseline,direct_no_bed_body0_cal' --out "$O\cal_decision.json" `
    --source-label 'enriched 20-song true-eligible-audio host-only direct/no-bed/body-0 with control-calibrated surround C screen'
if ($LASTEXITCODE -ne 0) { throw 'Calibrated direct/no-bed screen gate failed' }
