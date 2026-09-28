# Matched 4k mechanism pair for the selected direct-object host renderer.
# Both arms: 107 M estimated model MAC/s, shared bounded mask, stream norm,
# L=0, same seed/data/order. The only change is source attribution loss .054.
# This is a short screen, not an int8/device or final-model claim.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$C = 'D:\upmix\datasets\masknet_cache7'
$TC = 'D:\upmix\datasets\teacher_cache4'
$R = 'D:\upmix\runs'
$O = "$R\causal_source_pair4k"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("causal_source_pair4k $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
    if (-not (Test-Path "$T\sep_net.py")) { throw 'Missing PC separator code' }
    if ((Get-FileHash "$T\sep_net.py" -Algorithm SHA256).Hash -ne
        'F017E818FEB372B65C12AF6A319FBD02CFDF1D745066D2AE9E6D5EABC891AF81') {
        throw 'PC separator source hash changed'
    }
    New-Item -ItemType Directory -Force $O | Out-Null
    $env:PATH = "$T\ffmpeg\bin;" + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    foreach ($arm in @(@('anchor', '0'), @('source_attr', '0.054'))) {
        $name, $weight = $arm
        $out = "$O\$name"
        New-Item -ItemType Directory -Force $out | Out-Null
        if (Test-Path "$out\train_done.txt") {
            Write-Output "$name already finished; preserving it"
            continue
        }
        $args = @("$T\sep_net.py", 'train', '--arch', 'band', '--mask', 'shared_sigmoid',
            '--norm', 'stream', '--cache', $C, '--teacher-cache', $TC, '--half-half',
            '--out', $out, '--steps', '4000', '--batch', '12', '--seconds', '8',
            '--eval-every', '1000', '--save-at', '4000', '--lr', '3e-4',
            '--warmup', '2000', '--seed', '1')
        if ($weight -ne '0') { $args += @('--source-attr', $weight) }
        if (Test-Path "$out\last.pt") { $args += @('--init', "$out\last.pt") }
        Write-Output "training $name, source_attr=$weight"
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
    Write-Output 'matched causal source pair complete'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
