# CPU-only diagnosis of the original true-eligible-audio C-render swing windows.
# Read-only inputs; output is a separate JSON beside the existing timing report.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\diag_oracle_depth_components.py" `
    --oracle-run "$R\iface8\inject_oracle" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --manifest "$R\oracle_renderer_probe\manifest.json" `
    --timing "$R\oracle_renderer_probe\swing_timing.json" `
    --out "$R\oracle_renderer_probe\depth_components.json"
if ($LASTEXITCODE -ne 0) { throw 'Depth-component diagnosis failed' }
foreach ($condition in @('rear_air_off', 'front_retention075')) {
    python "$T\diag_oracle_depth_components.py" `
        --oracle-run "$R\oracle_renderer_probe\$condition" `
        --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
        --manifest "$R\oracle_renderer_probe\manifest.json" `
        --timing "$R\oracle_renderer_probe\swing_timing.json" `
        --allow-share-change `
        --out "$R\oracle_renderer_probe\depth_components_$condition.json"
    if ($LASTEXITCODE -ne 0) { throw "Depth-component diagnosis failed: $condition" }
}
