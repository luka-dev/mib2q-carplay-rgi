# One-factor renderer screen: remove rear-body shelf in the direct/no-bed path.
# Reuse the frozen 20-song oracle screen and the fresh control C baseline.
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
    --render-overrides "direct_no_bed_body0=$B\upmix_direct_no_bed.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --probe $O --expected-songs 20 --points 'baseline,direct_no_bed_body0'
if ($LASTEXITCODE -ne 0) { throw 'Direct/no-bed rear-body C screen failed' }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope probe `
    --points 'baseline,direct_no_bed_body0' --out "$O\body0_decision.json" `
    --source-label 'enriched 20-song true-eligible-audio host-only direct/no-bed/rear-body-0 C screen'
if ($LASTEXITCODE -ne 0) { throw 'Direct/no-bed rear-body screen gate failed' }
