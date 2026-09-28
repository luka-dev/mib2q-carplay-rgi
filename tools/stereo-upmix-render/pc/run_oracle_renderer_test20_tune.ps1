# Independent test-split check of exploratory oracle full-40 winners only.
# The 20-song TEST manifest was frozen before the full-40 results were read.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$Full = "$R\oracle_renderer_full40"
$O = "$R\oracle_renderer_test20"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
if (-not (Test-Path "$O\manifest.json")) { throw 'Frozen independent TEST manifest is missing' }
$decision = Get-Content "$Full\tune_decision.json" -Raw | ConvertFrom-Json
$winners = @($decision.variants.PSObject.Properties | Where-Object { $_.Value.meets_full40_render_gates } | ForEach-Object { $_.Name })
if ($winners.Count -eq 0) { throw 'No full-40 oracle render winner to check on TEST' }
$points = 'baseline,' + ($winners -join ',')
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\oracle_renderer_sweep.py" --render "$T\upmix_render_inj.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' --probe $O `
    --expected-songs 20 --points $points --skip-original-verify
if ($LASTEXITCODE -ne 0) { throw "Independent TEST C render failed; inspect $O" }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope test20 `
    --points $points --out "$O\tune_decision.json"
if ($LASTEXITCODE -ne 0) { throw "Independent TEST evaluation failed; inspect $O" }
