# Twenty-step CPU/data/GPU-path smoke for the matched cross-corpus remix pair.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\cross_corpus_smoke"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw 'Rapid queue is not empty' }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("cross_corpus_smoke $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|sep_net_crosssong.py|diag_crosssong.py|upmix_stem_tests'
    })
    if ($busy.Count -ne 0) { throw 'Another trainer or harness is active' }
    if ((Get-FileHash "$T\sep_net.py" -Algorithm SHA256).Hash -ne
        '11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905') {
        throw 'Cross-corpus source hash changed'
    }
    $env:PATH = "$T\ffmpeg\bin;" + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    $shared = @('train', '--arch', 'band',
        '--cache', 'D:\upmix\datasets\masknet_cache7',
        '--teacher-cache', 'D:\upmix\datasets\teacher_cache4', '--half-half',
        '--steps', '20', '--batch', '12', '--seconds', '8', '--eval-every', '20',
        '--save-at', '20', '--lr', '0.0003', '--warmup', '2000', '--sisdr-after', '20000',
        '--remix', '0.5', '--extra-truth', 'D:\upmix\datasets\onair_cache7',
        '--extra-truth-frac', '0.25')
    foreach ($arm in @('control', 'crossed')) {
        $out = "$O\$arm"
        New-Item -ItemType Directory -Force $out | Out-Null
        if ((Test-Path "$out\train_done.txt") -and (Test-Path "$out\step20.pt")) {
            Write-Output "$arm smoke already complete"
            continue
        }
        $extra = @()
        if ($arm -eq 'crossed') { $extra += '--cross-corpus-remix' }
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & python "$T\sep_net.py" @shared @extra --out $out 2>&1 |
                Tee-Object -FilePath "$out\train.log"
            $exit = $LASTEXITCODE
        } finally { $ErrorActionPreference = $previousPreference }
        if ($exit -ne 0 -or -not (Test-Path "$out\train_done.txt") -or
            -not (Test-Path "$out\step20.pt")) {
            throw "$arm 20-step smoke failed: exit=$exit"
        }
        Write-Output "$arm smoke complete"
    }
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
