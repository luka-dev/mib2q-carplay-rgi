# One GPU tenant; calibrate anchored vocal-entry target on training batches.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$TC = 'D:\upmix\datasets\teacher_cache4'
$O = "$R\vocal_entry_target4k"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("vocal_entry_target_grad $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
    $hashes = @{
        'sep_net.py' = '31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7'
        'diag_vocal_entry_target_grad.py' = '04BE623AD872FD1BE146F7DD3934DF26F29F1E51FD59D00F0ABB5D4C9D703FA9'
    }
    foreach ($name in $hashes.Keys) {
        if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
            throw "PC source hash changed: $name"
        }
    }
    New-Item -ItemType Directory -Force $O | Out-Null
    if (Test-Path "$O\grad.txt") { throw 'Gradient calibration already exists; preserving it' }
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        python "$T\diag_vocal_entry_target_grad.py" $C $TC 2>&1 |
            Tee-Object -FilePath "$O\grad.txt"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0) { throw 'Vocal-entry gradient calibration failed' }
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
