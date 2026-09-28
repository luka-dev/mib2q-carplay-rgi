# CPU source/transition/calibration checks against shared and four-role controls.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\causal_role_focus4k"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$shared = "$R\causal_source_pair4k\anchor\step4000.pt"
$role = "$R\causal_role4k\role_softmax\step4000.pt"
$focus = "$O\focused\step4000.pt"
if (-not (Test-Path $shared) -or -not (Test-Path $role) -or -not (Test-Path $focus)) {
    throw 'Comparison checkpoints missing'
}
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
python "$T\diag_role_selectivity.py" $C $shared $focus `
    --out "$O\focus_vs_shared.json" > "$O\focus_vs_shared.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Focused/shared source diagnostic failed' }
python "$T\diag_role_selectivity.py" $C $role $focus `
    --out "$O\focus_vs_role.json" > "$O\focus_vs_role.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Focused/role source diagnostic failed' }
python "$T\diag_transitions.py" $C $shared $role $focus --songs 20 > "$O\transitions.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Focused transition diagnostic failed' }
python "$T\diag_role_calibration.py" $C $focus > "$O\calibration.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Focused role calibration failed' }
Get-Content "$O\focus_vs_shared.txt"
Get-Content "$O\focus_vs_role.txt"
Get-Content "$O\transitions.txt" | Select-String 'entry +n='
Get-Content "$O\calibration.txt"
