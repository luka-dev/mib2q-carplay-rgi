# CPU-only exploratory operating-point curve for the frozen four-role checkpoint.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\causal_role4k"
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
python "$T\diag_role_temperature.py" 'D:\upmix\datasets\masknet_cache7' `
    "$O\role_softmax\step4000.pt" > "$O\temperature.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Role temperature diagnostic failed' }
Get-Content "$O\temperature.txt"
