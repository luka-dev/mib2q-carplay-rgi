# Independent 20-song test-split check after the full-40 oracle C validation.
# CPU only; the true eligible stem injection is non-deployable.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\oracle_renderer_test20"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
New-Item -ItemType Directory -Force $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\prepare_oracle_test_holdout.py" --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --split-file 'D:\upmix\datasets\masknet_cache7\split.json' --out $O --songs 20 --seed 1610
if ($LASTEXITCODE -ne 0) { throw 'Could not freeze the independent 20-song set' }
python "$T\oracle_renderer_sweep.py" --render "$T\upmix_render_inj.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' --probe $O `
    --expected-songs 20 --points 'baseline,rear_air_off,front_retention075' --skip-original-verify
if ($LASTEXITCODE -ne 0) { throw "Independent C render failed; inspect $O and resume the same command" }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope test20
if ($LASTEXITCODE -ne 0) { throw "Independent decision gate failed; inspect $O" }
