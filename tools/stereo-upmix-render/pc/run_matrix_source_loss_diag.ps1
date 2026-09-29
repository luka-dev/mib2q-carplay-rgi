# CPU-only fixed source/entry screen versus both shared anchor and matched matrix control.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\matrix_source_loss4k"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$hashes = @{
    'sep_net.py' = '2AA3BD15874E7399D0A1136754DF04D685387D3591B1901BF66A88C1F48B4737'
    'diag_role_selectivity.py' = '7606F3E655558376C2334737100C6FA090085D34175A759759001BCAE0A689FB'
    'diag_transitions.py' = 'B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
        throw "PC diagnostic source hash changed: $name"
    }
}
$shared = "$R\causal_source_pair4k\anchor\step4000.pt"
$control = "$R\causal_matrix2_4k\step4000.pt"
$candidate = "$O\focused\step4000.pt"
foreach ($p in @($shared, $control, $candidate)) {
    if (-not (Test-Path $p)) { throw "Missing checkpoint: $p" }
}
if ((Get-FileHash $control -Algorithm SHA256).Hash -ne
    '849C80EDBC6AF2B1D02D647A9EDC956B6787EE53558F8CBA36F2651A0A0F0604') {
    throw 'Matched matrix control checkpoint changed'
}
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
foreach ($arm in @(@('shared', $shared), @('matrix', $control))) {
    $name, $anchor = $arm
    python "$T\diag_role_selectivity.py" $C $anchor $candidate `
        --out "$O\focused_vs_$name.json" > "$O\focused_vs_$name.txt" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Focused/$name source diagnostic failed" }
    Get-Content "$O\focused_vs_$name.txt"
}
python "$T\diag_transitions.py" $C $control $candidate --songs 20 > "$O\transitions.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Focused matrix transition diagnostic failed' }
Get-Content "$O\transitions.txt" | Select-String '#####|entry +n='
