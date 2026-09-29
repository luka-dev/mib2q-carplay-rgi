# One GPU tenant; training-pool gradient calibration before matched training.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$C = 'D:\upmix\datasets\masknet_cache7'
$TC = 'D:\upmix\datasets\teacher_cache4'
$R = 'D:\upmix\runs'
$O = "$R\operator_anchor4k"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("operator_anchor_calibrate $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|sep_net_state.py|sep_net_fine_role.py|sep_net_operator_anchor.py|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
    $hashes = @{
        'sep_net_operator_anchor.py' = 'A14F5BDD7D3C5ECC8CF906A92E26659F8BD690DDDDF4724F9B8736226AB50804'
        'diag_spatial_oracle.py' = 'AB9F6B6B5FA5BF00FF96758C3CF4DDBA99DEE5A38E764A2A106A183547546DB3'
        'sep_net_state.py' = '16B5029B2B24D88E0267D25D0B4E956765C5C731C91846BC2124839E190E3EEC'
        'sep_net.py' = '31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7'
        'mask_net.py' = '5CAAE687105AE492F5BBB5DE5843593882DA43C73E6CF90878BF28AF8F54BE40'
    }
    foreach ($name in $hashes.Keys) {
        if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
            throw "PC source hash changed: $name"
        }
    }
    New-Item -ItemType Directory -Force $O | Out-Null
    if (Test-Path "$O\calibration.json") { throw 'Calibration already finished; preserving it' }
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        python "$T\sep_net_operator_anchor.py" calibrate $C $TC --out "$O\calibration.json" > "$O\calibration.txt" 2>&1
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0) { Get-Content "$O\calibration.txt" -Tail 60; throw 'Operator-anchor calibration failed' }
    Get-Content "$O\calibration.txt" -Tail 5
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
