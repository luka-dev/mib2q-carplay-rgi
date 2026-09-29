# Twenty-step real-cache/GPU smoke for the frozen DSD100 50-song data pair.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$base = "$runs\dsd50_smoke"
$control = 'D:\upmix\datasets\dsd100_moises_control50'
$candidate = 'D:\upmix\datasets\dsd100_dev_cache7'
$cacheDone = "$runs\dsd100_integrity\cache_complete.json"
$lease = "$runs\GPU_LEASE"
if (-not (Test-Path $cacheDone)) { throw 'DSD100 Dev cache completion marker missing' }
$cache = Get-Content $cacheDone -Raw | ConvertFrom-Json
if ($cache.tracks -ne 50 -or $cache.archive_sha256 -notmatch '^[0-9A-Fa-f]{64}$' -or
    (Get-FileHash "$candidate\split.json" -Algorithm SHA256).Hash -ne $cache.split_sha256) {
    throw 'DSD100 Dev cache completion marker/hash invalid'
}
if ((Get-FileHash "$control\split.json" -Algorithm SHA256).Hash -ne
    'E84A9BE9E4DD7250031F8B47D3D65F1C0E9CF798DA35E465D04193747A2C67EA') {
    throw 'MoisesDB 50-song control split hash changed'
}
if ((Get-FileHash "$tools\sep_net.py" -Algorithm SHA256).Hash -ne
    '11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905') {
    throw 'BandSepNet rapid source hash changed'
}
$queued = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($queued.Count -ne 0) { throw 'Rapid queue is not empty' }
$daemons = @(Get-CimInstance Win32_Process -Filter "name='powershell.exe'" | Where-Object {
    $_.CommandLine -match 'rapid_daemon\.ps1'
})
if ($daemons.Count -ne 0) { throw 'Rapid daemon is active' }
if (Test-Path $lease) { throw 'GPU lease active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("dsd50_smoke $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|diag_gap.py|diag_transitions.py'
    })
    if ($busy.Count -ne 0) { throw 'Another trainer or harness is active' }
    $env:PATH = "$tools\ffmpeg\bin;" + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $tools
    $shared = @('train', '--arch', 'band',
        '--cache', 'D:\upmix\datasets\masknet_cache7',
        '--teacher-cache', 'D:\upmix\datasets\teacher_cache4', '--half-half',
        '--steps', '20', '--batch', '12', '--seconds', '8', '--eval-every', '20',
        '--save-at', '20', '--lr', '0.0003', '--warmup', '2000', '--sisdr-after', '20000',
        '--remix', '0.5')
    foreach ($tag in @('control', 'candidate')) {
        $pool = if ($tag -eq 'control') { $control } else { $candidate }
        $out = "$base\$tag"
        New-Item -ItemType Directory -Force $out | Out-Null
        if ((Test-Path "$out\train_done.txt") -and (Test-Path "$out\step20.pt")) {
            Write-Output "$tag smoke already complete"
            continue
        }
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & python "$tools\sep_net.py" @shared --extra-truth $pool `
                --extra-truth-frac '0.5' --out $out 2>&1 |
                Tee-Object -FilePath "$out\train.log"
            $exit = $LASTEXITCODE
        } finally { $ErrorActionPreference = $previousPreference }
        if ($exit -ne 0 -or -not (Test-Path "$out\train_done.txt") -or
            -not (Test-Path "$out\step20.pt")) {
            throw "$tag 20-step smoke failed: exit=$exit"
        }
        Write-Output "$tag smoke complete"
    }
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
