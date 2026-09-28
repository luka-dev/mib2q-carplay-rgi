# One GPU tenant: causal current-plus-previous complex filter, matched to saved complex control.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$C = 'D:\upmix\datasets\masknet_cache7'
$TC = 'D:\upmix\datasets\teacher_cache4'
$R = 'D:\upmix\runs'
$O = "$R\causal_deep2_4k"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("causal_deep2_4k $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
    if ((Get-FileHash "$T\sep_net.py" -Algorithm SHA256).Hash -ne
        'A4855311959C6ED61A190635F878BD63C495608599008E263848950D8F9F6842') {
        throw 'PC separator source hash changed'
    }
    if (-not (Test-Path "$R\dual_head_causal4k\complex_control\step4000.pt")) {
        throw 'Matched complex control is missing'
    }
    if (Test-Path "$O\train_done.txt") { throw 'Deep-filter probe already finished; preserving it' }
    New-Item -ItemType Directory -Force $O | Out-Null
    $env:PATH = "$T\ffmpeg\bin;" + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    $args = @("$T\sep_net.py", 'train', '--arch', 'band', '--mask', 'deep2',
        '--norm', 'stream', '--dec-skip', '--feat', '36', '--h1', '24', '--h2', '24', '--bhg', '24',
        '--cache', $C, '--teacher-cache', $TC, '--half-half', '--out', $O,
        '--steps', '4000', '--batch', '12', '--seconds', '8', '--eval-every', '1000',
        '--save-at', '4000', '--lr', '3e-4', '--warmup', '2000', '--seed', '1')
    if (Test-Path "$O\last.pt") { $args += @('--init', "$O\last.pt") }
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & python @args 2>&1 | Tee-Object -FilePath "$O\train.log"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or -not (Test-Path "$O\train_done.txt")) {
        throw 'Deep-filter probe failed or did not reach step 4000'
    }
    Write-Output 'causal deep2 probe complete'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
