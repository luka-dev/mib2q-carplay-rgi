# CPU-only, fixed source and vocal-transition screen for both matched 4k checkpoints.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\dual_head_causal4k"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
if ((Get-FileHash "$T\sep_net.py" -Algorithm SHA256).Hash -ne
    '052DFEF7A4D9AE3DDE70E3FD84692D4546F8393F649E567DB081616C7771BBEC') {
    throw 'PC separator source hash changed'
}
$anchor = "$R\causal_source_pair4k\anchor\step4000.pt"
$control = "$O\complex_control\step4000.pt"
$candidate = "$O\complex_role\step4000.pt"
foreach ($p in @($anchor, $control, $candidate)) {
    if (-not (Test-Path $p)) { throw "Missing checkpoint: $p" }
}
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
foreach ($arm in @(@('complex_control', $control), @('complex_role', $candidate))) {
    $name, $ckpt = $arm
    python "$T\diag_role_selectivity.py" $C $anchor $ckpt `
        --out "$O\$name.selectivity.json" > "$O\$name.selectivity.txt" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$name source diagnostic failed" }
    Get-Content "$O\$name.selectivity.txt"
}
python "$T\diag_transitions.py" $C $control $candidate --songs 20 > "$O\transitions.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Dual-head transition diagnostic failed' }
Get-Content "$O\transitions.txt" | Select-String '#####|entry +n='
