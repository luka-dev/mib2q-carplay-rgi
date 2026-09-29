# CPU-only privileged true-stem gate on a frozen matrix checkpoint.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\protected_veto_upper_bound"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$hashes = @{
    'diag_protected_veto_upper_bound.py' = 'FB933061601ADDA6787A51E0816EDF34995145158D77F82BA0E1A08D0D4F155F'
    'diag_transitions.py' = 'B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
    'sep_net.py' = '2AA3BD15874E7399D0A1136754DF04D685387D3591B1901BF66A88C1F48B4737'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
        throw "PC source hash changed: $name"
    }
}
$candidate = "$R\matrix_source_loss4k\focused\step4000.pt"
if ((Get-FileHash $candidate -Algorithm SHA256).Hash -ne
    '35B042EBBD44B03697F01444E03861C4D4A185C13F46B23376DD121B1B67FA4A') {
    throw 'Source-loss candidate checkpoint changed'
}
New-Item -ItemType Directory -Force $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
python "$T\diag_protected_veto_upper_bound.py" $C $candidate --out "$O\dev.json" |
    Tee-Object -FilePath "$O\dev.txt"
if ($LASTEXITCODE -ne 0) { throw 'Protected-veto upper-bound diagnostic failed' }
