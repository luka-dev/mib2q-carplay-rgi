# Explore renderer knobs with Track A's frozen production mask on the enriched
# 20-song DEV set. CPU-only, real C renderer, no oracle cue or audio injection.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\production_renderer_probe20"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
New-Item -ItemType Directory -Force $O | Out-Null
Copy-Item "$R\oracle_renderer_probe\manifest.json" "$O\manifest.json" -Force
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\oracle_renderer_sweep.py" --render "$T\upmix_render_mask_v21a.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' --probe $O `
    --source production --mask-bin "$T\hssr_mask_tracka_v21a.bin" `
    --expected-songs 20 --points 'baseline,rear_air_off,front_retention075' --skip-original-verify
if ($LASTEXITCODE -ne 0) { throw "Production C screen failed; inspect $O and resume the same command" }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope production20
if ($LASTEXITCODE -ne 0) { throw "Production screen evaluation failed; inspect $O" }
