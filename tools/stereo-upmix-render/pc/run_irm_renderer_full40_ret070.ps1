# CPU-only representation bound: optimal bounded real mask -> eligible audio
# through the real C renderer, with the already-chosen front retention 0.70.
# Diagnostic on the original 40 DEV songs; no training, device or listening claim.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\irm_renderer_full40_ret070"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
if (-not (Test-Path "$R\oracle_renderer_full40\manifest.json")) { throw 'Original full-40 manifest missing' }
if (-not (Test-Path "$R\iface8\inject_irm\stems.csv")) { throw 'Original IRM baseline CSV missing' }
New-Item -ItemType Directory -Force $O | Out-Null
Copy-Item "$R\iface8\inject_irm\stems.csv" "$O\baseline_stems.csv" -Force
$manifest = Get-Content "$R\oracle_renderer_full40\manifest.json" -Raw | ConvertFrom-Json
$irmRows = @(Import-Csv "$O\baseline_stems.csv")
$manifest.swing_songs = @($irmRows | Where-Object {
    $_.range_1s -match '^[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?$' -and
    [double]$_.range_1s -gt 0.30
} |
    Select-Object -ExpandProperty song -Unique)
$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText("$O\manifest.json", ($manifest | ConvertTo-Json -Depth 10), $utf8)
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\oracle_renderer_sweep.py" --render "$T\upmix_render_inj.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' --probe $O `
    --source irm --expected-songs 40 --points 'baseline,front_retention070'
if ($LASTEXITCODE -ne 0) { throw "IRM C render failed; inspect $O and resume the same command" }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope full40 `
    --points 'baseline,front_retention070' --out "$O\decision.json" `
    --source-label 'full 40-song ideal bounded real-mask audio C render; noncausal oracle bound, not device or listening evidence'
if ($LASTEXITCODE -ne 0) { throw "Full-40 decision evaluation failed; inspect $O" }
