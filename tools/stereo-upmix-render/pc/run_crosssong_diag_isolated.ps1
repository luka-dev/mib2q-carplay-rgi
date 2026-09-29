# Frozen review-43 source, role and vocal-entry diagnostic, one GPU tenant.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools\crosssong_frozen'
$R = 'D:\upmix\runs'
$O = "$R\crosssong4k"
$baseline = "$R\fine_role4k\fine\step4000.pt"
$candidate = "$O\fine\step4000.pt"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("crosssong_diag $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|sep_net_crosssong.py|diag_crosssong.py|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer diagnostic is active' }
    $hashes = @{
        'diag_crosssong.py' = 'A7A176018490E4A4EE88356C6F2BD14F673977DAFD5CB8BCC7E5E824D2139BB5'
        'diag_dual_state.py' = 'BAA6788BCDC3D44112AF4D0AA93923409D11E999B1C3B6AC7307DB76D992847F'
        'diag_fine_role.py' = '125ED865F1763F822614B69306400596E99B8A3B4389534A43E52C262A617A1E'
        'diag_role_selectivity.py' = '7606F3E655558376C2334737100C6FA090085D34175A759759001BCAE0A689FB'
        'diag_transitions.py' = 'B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
        'role_confidence.py' = '0DC51ABAE78C861082DEDB6FDDE3037E85F2CFDDCDA0B5734A66CFD21EE6B0E2'
        'sep_net_fine_role.py' = 'B2D9CC188A6ABF112A6D4D09385AD2947E6FA939036872F060BAECB4198BADB6'
        'sep_net_state.py' = '16B5029B2B24D88E0267D25D0B4E956765C5C731C91846BC2124839E190E3EEC'
        'sep_net.py' = '31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7'
        'mask_net.py' = '5CAAE687105AE492F5BBB5DE5843593882DA43C73E6CF90878BF28AF8F54BE40'
    }
    foreach ($name in $hashes.Keys) {
        if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
            throw "PC source hash changed: $name"
        }
    }
    if ((Get-FileHash $baseline -Algorithm SHA256).Hash -ne
        '826061FD92AE515FD0C55798DB848C98831D7DF3DB15CA1B4E562DE73B381F29') {
        throw 'Baseline checkpoint changed'
    }
    if (-not (Test-Path "$O\fine\train_done.txt") -or -not (Test-Path $candidate)) {
        throw 'Candidate 4k training is incomplete'
    }
    New-Item -ItemType Directory -Force "$O\diag" | Out-Null
    $env:PATH = 'D:\upmix\tools\ffmpeg\bin;' + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & python "$T\diag_crosssong.py" 'D:\upmix\datasets\masknet_cache7' `
            $baseline $candidate --out "$O\diag" 2>&1 |
            Tee-Object -FilePath "$O\diag.log"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or -not (Test-Path "$O\diag\summary.json")) {
        throw "Review-43 diagnostic failed: exit=$exit"
    }
    Write-Output 'Review-43 diagnostic complete'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
