# CPU-only ideal bounded-mask/audio representation bound for the frozen host
# direct/no-bed/body-0 renderer at surround 0.3235. Run only after full40
# true-eligible-audio result; neither source nor renderer is deployable.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$B = "$T\direct_object_host"
$O = "$R\direct_object_irm_full40"
$Original = "$R\irm_renderer_full40_ret070"
$full = Get-Content "$R\direct_object_full40\decision.json" -Raw | ConvertFrom-Json
if (-not $full.variants.direct_no_bed_body0_cal.meets_full40_render_gates) {
    throw 'The frozen true-audio direct-object full40 gate did not pass'
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
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' --source irm `
    --probe $O --expected-songs 40 --points 'baseline,direct_no_bed_body0_cal'
if ($LASTEXITCODE -ne 0) { throw 'Full40 ideal-mask-audio direct-object render failed' }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope full40 `
    --points 'baseline,direct_no_bed_body0_cal' --out "$O\decision.json" `
    --source-label 'full40 DEV ideal bounded-mask audio, host-only direct/no-bed/body-0, frozen surround 0.3235; noncausal and DEV selected'
if ($LASTEXITCODE -ne 0) { throw 'Full40 ideal-mask-audio decision failed' }
