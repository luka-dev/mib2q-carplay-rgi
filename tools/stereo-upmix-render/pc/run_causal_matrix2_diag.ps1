# CPU-only frozen full-matrix source attribution and vocal-entry screen.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\causal_matrix2_4k"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$hashes = @{
    'sep_net.py' = '3AB06ADF52192C06BBD4891EE2A3628F24CA53D54B69DE8408F8CF04BE8C66D8'
    'diag_role_selectivity.py' = 'F672C6750B3A11A1534C92FB471D974D621DC38B1F08A779BB0D18E7959C8DAB'
    'diag_transitions.py' = '9210200DF5F823F5A8E61872A133DE261D2CC5D6516B62FD5A6B89D29CE03464'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
        throw "PC diagnostic source hash changed: $name"
    }
}
$anchor = "$R\causal_source_pair4k\anchor\step4000.pt"
$control = "$R\dual_head_causal4k\complex_control\step4000.pt"
$candidate = "$O\step4000.pt"
foreach ($p in @($anchor, $control, $candidate)) {
    if (-not (Test-Path $p)) { throw "Missing checkpoint: $p" }
}
if ((Get-FileHash $control -Algorithm SHA256).Hash -ne
    'A38DCCB9DAD5AF382AC34EB38CBD3FB2E6114E60C0C34B4BA8C40279ABB93F12') {
    throw 'Matched control checkpoint changed'
}
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
foreach ($arm in @(@('complex_control', $control), @('matrix2', $candidate))) {
    $name, $ckpt = $arm
    python "$T\diag_role_selectivity.py" $C $anchor $ckpt `
        --out "$O\$name.selectivity.json" > "$O\$name.selectivity.txt" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$name source diagnostic failed" }
    Get-Content "$O\$name.selectivity.txt"
}
python "$T\diag_transitions.py" $C $control $candidate --songs 20 > "$O\transitions.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Matrix transition diagnostic failed' }
Get-Content "$O\transitions.txt" | Select-String '#####|entry +n='
