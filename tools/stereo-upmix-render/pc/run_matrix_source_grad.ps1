# One GPU tenant for four deterministic train-pool parameter-gradient calibration batches.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$TC = 'D:\upmix\datasets\teacher_cache4'
$O = "$R\matrix_source_loss4k"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("matrix_source_grad $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
    if ((Get-FileHash "$T\sep_net.py" -Algorithm SHA256).Hash -ne
        '2AA3BD15874E7399D0A1136754DF04D685387D3591B1901BF66A88C1F48B4737') {
        throw 'PC separator source hash changed'
    }
    if ((Get-FileHash "$T\diag_matrix_source_grad.py" -Algorithm SHA256).Hash -ne
        '5C1E1612A9636531F45F5C52A3EA095D14D39F8319F5B9447755309C7BE3754E') {
        throw 'Gradient calibration source hash changed'
    }
    New-Item -ItemType Directory -Force $O | Out-Null
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        python "$T\diag_matrix_source_grad.py" $C $TC 2>&1 | Tee-Object -FilePath "$O\grad.txt"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0) { throw 'Matrix source gradient calibration failed' }
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
