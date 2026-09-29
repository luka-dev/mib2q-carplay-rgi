# Frozen review-42 diagnostic on the matched 4k checkpoints, one GPU tenant.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools\operator_anchor_frozen'
$O = 'D:\upmix\runs\operator_anchor4k'
$R = 'D:\upmix\runs'
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("operator_anchor_diag $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|sep_net_state.py|sep_net_fine_role.py|sep_net_operator_anchor.py|diag_operator_anchor.py|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer diagnostic is active' }
    $hashes = @{
        'diag_operator_anchor.py' = '332483C091CE1CEF8BA5FC62BB71827A90FE1DD193394B81CE7426E41E9EBA29'
        'diag_role_selectivity.py' = '7606F3E655558376C2334737100C6FA090085D34175A759759001BCAE0A689FB'
        'diag_transitions.py' = 'B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
        'role_confidence.py' = '0DC51ABAE78C861082DEDB6FDDE3037E85F2CFDDCDA0B5734A66CFD21EE6B0E2'
        'sep_net.py' = '31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7'
        'sep_net_operator_anchor.py' = 'A14F5BDD7D3C5ECC8CF906A92E26659F8BD690DDDDF4724F9B8736226AB50804'
        'mask_net.py' = '5CAAE687105AE492F5BBB5DE5843593882DA43C73E6CF90878BF28AF8F54BE40'
    }
    foreach ($name in $hashes.Keys) {
        if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
            throw "PC source hash changed: $name"
        }
    }
    foreach ($arm in @('control', 'anchored')) {
        if (-not (Test-Path "$O\$arm\train_done.txt") -or
            -not (Test-Path "$O\$arm\step4000.pt")) {
            throw "$arm 4k checkpoint is missing"
        }
    }
    New-Item -ItemType Directory -Force "$O\diag" | Out-Null
    $env:PATH = 'D:\upmix\tools\ffmpeg\bin;' + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & python "$T\diag_operator_anchor.py" 'D:\upmix\datasets\masknet_cache7' `
            "$O\control\step4000.pt" "$O\anchored\step4000.pt" --out "$O\diag" 2>&1 |
            Tee-Object -FilePath "$O\diag.log"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or -not (Test-Path "$O\diag\summary.json")) {
        throw "Review-42 diagnostic failed: exit=$exit"
    }
    Write-Output 'Review-42 diagnostic complete'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
