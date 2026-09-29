# CPU-only cross-term energy, source-attribution and vocal-transition ablation.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\matrix_cross_ablation"
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
    'diag_matrix_cross_energy.py' = '4512C14ACBD7CB9CB06C13E6A42088F5F623F7BF079C10CD0950E4CA467EDD30'
    'diag_role_selectivity.py' = '7606F3E655558376C2334737100C6FA090085D34175A759759001BCAE0A689FB'
    'diag_transitions.py' = 'B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
        throw "PC diagnostic source hash changed: $name"
    }
}
$anchor = "$R\causal_source_pair4k\anchor\step4000.pt"
$matrix = "$R\causal_matrix2_4k\step4000.pt"
if ((Get-FileHash $matrix -Algorithm SHA256).Hash -ne
    '849C80EDBC6AF2B1D02D647A9EDC956B6787EE53558F8CBA36F2651A0A0F0604') {
    throw 'Matrix checkpoint changed'
}
New-Item -ItemType Directory -Force $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
python "$T\diag_matrix_cross_energy.py" $C $matrix --out "$O\energy.json" > "$O\energy.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Cross-output energy diagnostic failed' }
foreach ($arm in @('full', 'zero_cross')) {
    $extra = @()
    if ($arm -eq 'zero_cross') { $extra = @('--candidate-zero-cross') }
    python "$T\diag_role_selectivity.py" $C $anchor $matrix @extra `
        --out "$O\$arm.selectivity.json" > "$O\$arm.selectivity.txt" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$arm source diagnostic failed" }
    $transitionExtra = @()
    if ($arm -eq 'zero_cross') { $transitionExtra = @('--matrix-zero-cross') }
    python "$T\diag_transitions.py" $C $matrix --songs 20 @transitionExtra > "$O\$arm.transitions.txt" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$arm transition diagnostic failed" }
}
Get-Content "$O\energy.txt" -Tail 1
Get-Content "$O\full.selectivity.txt"
Get-Content "$O\zero_cross.selectivity.txt"
Get-Content "$O\full.transitions.txt" | Select-String 'entry +n='
Get-Content "$O\zero_cross.transitions.txt" | Select-String 'entry +n='
