# CPU diagnostics for the four-role probe and frozen 4k anchor.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\causal_role4k"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$a = "$R\causal_source_pair4k\anchor\step4000.pt"
$s = "$O\role_softmax\step4000.pt"
if (-not (Test-Path $a) -or -not (Test-Path $s)) { throw 'Matched checkpoints missing' }
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
python "$T\diag_role_selectivity.py" $C $a $s --out "$O\selectivity.json" > "$O\selectivity.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Role selectivity diagnostic failed' }
python "$T\diag_transitions.py" $C $a $s --songs 20 > "$O\transitions.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Role transition diagnostic failed' }
Get-Content "$O\selectivity.txt"
Get-Content "$O\transitions.txt" | Select-String 'entry +n='
