# CPU-only gradient calibration before a four-role causal separator comparison.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\diag_role_grad.py" 'D:\upmix\datasets\masknet_cache7' `
    'D:\upmix\datasets\teacher_cache4'
if ($LASTEXITCODE -ne 0) { throw 'Role gradient calibration failed' }
