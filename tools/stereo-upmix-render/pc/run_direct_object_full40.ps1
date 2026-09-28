# Full DEV confirmation of the unchanged 20-song screen winner.
# True eligible audio, real host C, one global surround=0.3235 coefficient.
# This is still oracle injection and selected on DEV, not an independent holdout.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$B = "$T\direct_object_host"
$O = "$R\direct_object_full40"
$Original = "$R\oracle_renderer_full40"
$screen = Get-Content "$R\direct_object_probe20\cal_decision.json" -Raw | ConvertFrom-Json
if (-not $screen.variants.direct_no_bed_body0_cal.promote_to_full40) {
    throw 'The frozen 20-song architecture screen did not pass'
}
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
$build = Get-Content "$B\manifest.json" -Raw | ConvertFrom-Json
foreach ($name in @('control', 'direct_no_bed')) {
    $file = "$B\upmix_$name.exe"
    $actual = (Get-FileHash $file -Algorithm SHA256).Hash
    $expected = $build.builds.PSObject.Properties[$name].Value.sha256
    if ($actual -ne $expected) { throw "Host binary hash mismatch: $name" }
}
New-Item -ItemType Directory -Force $O | Out-Null
Copy-Item "$Original\manifest.json" "$O\manifest.json" -Force
Copy-Item "$Original\baseline_stems.csv" "$O\baseline_stems.csv" -Force
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\oracle_renderer_sweep.py" --render "$B\upmix_control.exe" `
    --render-overrides "direct_no_bed_body0_cal=$B\upmix_direct_no_bed.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --probe $O --expected-songs 40 --points 'baseline,direct_no_bed_body0_cal'
if ($LASTEXITCODE -ne 0) { throw 'Full40 direct-object C render failed' }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope full40 `
    --points 'baseline,direct_no_bed_body0_cal' --out "$O\decision.json" `
    --source-label 'full40 DEV true-eligible-audio host-only direct/no-bed/body-0 with control-calibrated surround; no independent, device or listening claim'
if ($LASTEXITCODE -ne 0) { throw 'Full40 direct-object gate failed' }
