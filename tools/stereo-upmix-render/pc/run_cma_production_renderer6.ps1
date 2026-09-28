# Bounded static-knob CMA-ES pilot on six frozen DEV songs through real C.
# This explores the current Track A mask; it does not modify Track A files.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\cma_production_renderer6"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py|cma_production_renderer.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\cma_production_renderer.py" --render "$T\upmix_render_mask_v21a.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --mask-bin "$T\hssr_mask_tracka_v21a.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --manifest "$R\production_renderer_probe20\manifest.json" `
    --baseline-csv "$R\production_renderer_probe20\baseline\stems.csv" `
    --out $O --generations 3 --population 6 --seed 1610
if ($LASTEXITCODE -ne 0) { throw "CMA-ES pilot failed; inspect $O" }
