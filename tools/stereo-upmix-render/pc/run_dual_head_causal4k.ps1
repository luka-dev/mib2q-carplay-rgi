# Matched causal complex-audio pair; the sole candidate change is an auxiliary role head and CE.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$C = 'D:\upmix\datasets\masknet_cache7'
$TC = 'D:\upmix\datasets\teacher_cache4'
$R = 'D:\upmix\runs'
$O = "$R\dual_head_causal4k"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("dual_head_causal4k $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
    if ((Get-FileHash "$T\sep_net.py" -Algorithm SHA256).Hash -ne
        '052DFEF7A4D9AE3DDE70E3FD84692D4546F8393F649E567DB081616C7771BBEC') { throw 'PC separator source hash changed' }
    $env:PATH = "$T\ffmpeg\bin;" + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    foreach ($arm in @(@('complex_control', 'complex2'), @('complex_role', 'complex_role'))) {
        $name, $mask = $arm
        $out = "$O\$name"
        New-Item -ItemType Directory -Force $out | Out-Null
        if (Test-Path "$out\train_done.txt") { Write-Output "$name already finished"; continue }
        $args = @("$T\sep_net.py", 'train', '--arch', 'band', '--mask', $mask,
            '--norm', 'stream', '--dec-skip', '--feat', '36', '--h1', '24', '--h2', '24', '--bhg', '24',
            '--cache', $C, '--teacher-cache', $TC, '--half-half', '--out', $out,
            '--steps', '4000', '--batch', '12', '--seconds', '8', '--eval-every', '1000',
            '--save-at', '4000', '--lr', '3e-4', '--warmup', '2000', '--seed', '1')
        if ($mask -eq 'complex_role') { $args += @('--role-ce', '0.279254') }
        if (Test-Path "$out\last.pt") { $args += @('--init', "$out\last.pt") }
        Write-Output "training $name"
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & python @args 2>&1 | Tee-Object -FilePath "$out\train.log"
            $exit = $LASTEXITCODE
        } finally { $ErrorActionPreference = $previousPreference }
        if ($exit -ne 0 -or -not (Test-Path "$out\train_done.txt")) {
            throw "$name training failed or did not reach step 4000"
        }
    }
    Write-Output 'dual-head causal pair complete'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
