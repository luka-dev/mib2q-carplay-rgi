# Sequential 20-step execution check for the frozen three-song Cambridge pair.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$base = "$runs\cambridge3_smoke"
$control = 'D:\upmix\datasets\cambridge_moises_control3'
$candidate = 'D:\upmix\datasets\cambridge_trial_cache7'
$lease = "$runs\GPU_LEASE"
if ((Get-FileHash "$tools\sep_net.py" -Algorithm SHA256).Hash -ne
    '11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905') {
    throw 'BandSepNet rapid source hash changed'
}
if ((Get-FileHash "$tools\cache_cambridge_trial.py" -Algorithm SHA256).Hash -ne
    'A23FAF68599C99562ECF464978505C64F2BD00B50C1E01268F233F68341BE00A') {
    throw 'Cambridge converter source hash changed'
}
if ((Get-FileHash "$tools\cambridge_trial_map.json" -Algorithm SHA256).Hash -ne
    'A4D6078BED5D368E9313DE66EF5B6D87CC05A013531778EB096D5A085F109327') {
    throw 'Cambridge role map hash changed'
}
if ((Get-FileHash "$control\split.json" -Algorithm SHA256).Hash -ne
    '338FF50105BE8ABC8722084ABECF449050276914CACA117B8ED7BBBB9C6D1022') {
    throw 'Three-song MoisesDB control split changed'
}
$splitFile = "$candidate\split.json"
if (-not (Test-Path $splitFile)) { throw 'Cambridge candidate cache is incomplete' }
$split = Get-Content $splitFile -Raw | ConvertFrom-Json
$ids = @($split.split.PSObject.Properties.Name | Sort-Object)
$expected = @('ajay_ae_mere_humsafar', 'finlay_same_kind_of_life', 'killa_l_window')
if ($ids.Count -ne 3 -or (@(Compare-Object $ids $expected).Count -ne 0)) {
    throw 'Cambridge candidate split has unexpected identities'
}
foreach ($id in $ids) {
    if ($split.split.$id -ne 'train' -or -not (Test-Path "$candidate\$id.npy") -or
        -not (Test-Path "$candidate\$id.json")) { throw "Incomplete TRAIN cache: $id" }
}
$queued = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($queued.Count -ne 0) { throw 'Rapid queue is not empty' }
$daemons = @(Get-CimInstance Win32_Process -Filter "name='powershell.exe'" | Where-Object {
    $_.CommandLine -match 'rapid_daemon\.ps1'
})
if ($daemons.Count -ne 0) { throw 'Rapid daemon is active' }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|diag_gap.py|diag_transitions.py'
})
if ($busy.Count -ne 0 -or (Test-Path $lease)) { throw 'GPU or harness is busy' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("cambridge3_smoke $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }
try {
    New-Item -ItemType Directory -Force $base | Out-Null
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
