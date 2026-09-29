# One GPU: matched execution smokes, then one fixed-TRAIN band-allocation run.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$control = "$runs\separator_fit_probe_v1"
$out = "$runs\separator_redistributed_bands_fit_v1"
$lease = "$runs\GPU_LEASE"
if (Test-Path $out) { throw 'Main output exists; preserve it' }
if (-not (Test-Path "$control\fit_complete.json")) { throw 'Frozen control missing' }
if ((Get-FileHash "$control\fit.pt" -Algorithm SHA256).Hash -ne
    '0E560B9924FEEA88F3E61B97253734C60A02A3901C7636779E107653D7E77E41') {
    throw 'Frozen anchor checkpoint changed'
}
if ((Get-FileHash "$control\fit_complete.json" -Algorithm SHA256).Hash -ne
    '47B4C447BAB0BC421CBBDF4D733F522BD5B16944ADBF4179E0893C2A60448515') {
    throw 'Frozen anchor result changed'
}
$pending = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw 'Rapid queue active' }
$hashes = @{
    'separator_band_probe.py' = '74E818D85D8F08350DD13B946DF63098CC2C18B28323CA691F8EC4CF35880805'
    'separator_band_geometry.py' = '6743B18527F29CE61C8F61018057D03AA5917E5734E05293B993C4CCED3F40DA'
    'separator_fit_probe.py' = 'CEB3C158D1955E3A384C1F21317D00282F50E1C52B3E56ECB698AA5C05FCEA0D'
    'sep_net.py' = '11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905'
}
foreach ($file in $hashes.Keys) {
    if ((Get-FileHash "$tools\$file" -Algorithm SHA256).Hash -ne $hashes[$file]) {
        throw "Source changed: $file"
    }
}
$busy = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -eq 'python.exe' -or
    ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')
})
if ($busy.Count -ne 0) { throw 'Python work or rapid daemon active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("band_probe $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }

function Invoke-Probe($mode, $destination, $steps) {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & python "$tools\separator_band_probe.py" --cache 'D:\upmix\datasets\masknet_cache7' `
            --control $control --out $destination --mode $mode --steps $steps 2>&1 |
            Tee-Object -FilePath "$destination.log"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or -not (Test-Path "$destination\complete.json")) {
        throw "Band probe failed: $mode exit=$exit"
    }
}

try {
    Set-Location $tools
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    $smokeControl = "$runs\separator_band_smoke_original_v1"
    $smokeCandidate = "$runs\separator_band_smoke_redistributed_v1"
    Invoke-Probe 'original' $smokeControl 20
    Invoke-Probe 'redistributed' $smokeCandidate 20
    $old = Get-Content "$control\progress.json" -Raw | ConvertFrom-Json
    $base = Get-Content "$smokeControl\progress.json" -Raw | ConvertFrom-Json
    $candidate = Get-Content "$smokeCandidate\progress.json" -Raw | ConvertFrom-Json
    if ([Math]::Abs($old.rows[0].loss - $base.rows[0].loss) -gt 0.000001) {
        throw 'Control initialization/objective parity failed'
    }
    $baseManifest = Get-Content "$smokeControl\manifest.json" -Raw | ConvertFrom-Json
    $newManifest = Get-Content "$smokeCandidate\manifest.json" -Raw | ConvertFrom-Json
    if ($baseManifest.parameters -ne $newManifest.parameters -or
        $baseManifest.estimated_neural_macs_s -ne $newManifest.estimated_neural_macs_s) {
        throw 'Nominal budget parity failed'
    }
    Invoke-Probe 'redistributed' $out 10000
    Get-Content "$out\complete.json" -Raw
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
