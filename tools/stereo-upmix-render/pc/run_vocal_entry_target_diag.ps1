# CPU-only frozen DEV12 source and DEV20 43-entry screen versus source-loss control.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\vocal_entry_target4k"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$hashes = @{
    'sep_net.py' = '31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7'
    'diag_role_selectivity.py' = '7606F3E655558376C2334737100C6FA090085D34175A759759001BCAE0A689FB'
    'diag_transitions.py' = 'B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
        throw "PC diagnostic source hash changed: $name"
    }
}
$shared = "$R\causal_source_pair4k\anchor\step4000.pt"
$control = "$R\matrix_source_loss4k\focused\step4000.pt"
$candidate = "$O\focused\step4000.pt"
foreach ($p in @($shared, $control, $candidate)) {
    if (-not (Test-Path $p)) { throw "Missing checkpoint: $p" }
}
if ((Get-FileHash $control -Algorithm SHA256).Hash -ne
    '35B042EBBD44B03697F01444E03861C4D4A185C13F46B23376DD121B1B67FA4A') {
    throw 'Matched matrix control checkpoint changed'
}
if ((Get-FileHash $candidate -Algorithm SHA256).Hash -ne
    '0A364F10EE346FA537DF74321F3383AEEF647D80F88AD11C4F8D9FFE6D88F0BE') {
    throw 'Anchored-entry candidate checkpoint changed'
}
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
foreach ($arm in @(@('matrix', $control), @('shared', $shared))) {
    $name, $anchor = $arm
    python "$T\diag_role_selectivity.py" $C $anchor $candidate `
        --out "$O\focused_vs_$name.json" > "$O\focused_vs_$name.txt" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Focused/$name source diagnostic failed" }
    Get-Content "$O\focused_vs_$name.txt"
}
python "$T\diag_transitions.py" $C $control $candidate --songs 20 > "$O\transitions.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Focused matrix transition diagnostic failed' }
Get-Content "$O\transitions.txt" | Select-String '#####|entry +n='
