# One-change 4k matrix student: paired vocal-level consistency at fixed weight.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$C = 'D:\upmix\datasets\masknet_cache7'
$TC = 'D:\upmix\datasets\teacher_cache4'
$R = 'D:\upmix\runs'
$O = "$R\vocal_invariance4k\focused"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("vocal_invariance4k $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
    if ((Get-FileHash "$T\sep_net.py" -Algorithm SHA256).Hash -ne
        'A8B40E0781D9AD9FFAF228B6C6131C5D1F15EDA74408CC3F32F9BE61676D9A46') {
        throw 'PC separator source hash changed'
    }
    $control = "$R\matrix_source_loss4k\focused\step4000.pt"
    if ((Get-FileHash $control -Algorithm SHA256).Hash -ne
        '35B042EBBD44B03697F01444E03861C4D4A185C13F46B23376DD121B1B67FA4A') {
        throw 'Matched matrix source-loss control checkpoint changed'
    }
    if (Test-Path "$O\train_done.txt") { throw 'Vocal-invariance probe already finished; preserving it' }
    New-Item -ItemType Directory -Force $O | Out-Null
    $env:PATH = "$T\ffmpeg\bin;" + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    $args = @("$T\sep_net.py", 'train', '--arch', 'band', '--mask', 'matrix2',
        '--norm', 'stream', '--dec-skip', '--feat', '36', '--h1', '24', '--h2', '24', '--bhg', '24',
        '--source-attr', '0.0734755788', '--vocal-invariance', '0.0738809192',
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
        throw 'Vocal-invariance probe failed or did not reach step 4000'
    }
    Write-Output 'vocal-invariance probe complete'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
