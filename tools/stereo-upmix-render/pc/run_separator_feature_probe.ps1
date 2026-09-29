# One GPU: matched execution smokes, then one fixed-TRAIN invariant-input run.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$control = "$runs\separator_fit_probe_v1"
$out = "$runs\separator_invariant6_fit_v1"
$lease = "$runs\GPU_LEASE"
if (Test-Path $out) { throw 'Main output exists; preserve it' }
if (-not (Test-Path "$control\fit_complete.json")) { throw 'Frozen control missing' }
$pending = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw 'Rapid queue active' }
$hashes = @{
    'separator_feature_probe.py' = '3F95D1D04209DFDC4191D7BEB60EB22ECF08B1D817C67F621ED80EF58F57ECDA'
    'separator_invariant_features.py' = 'FA4185560B05970050756030D27B6B68253502A690EAA01D088E801F4F66DE03'
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
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("feature_probe $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }

function Invoke-Probe($mode, $destination, $steps) {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & python "$tools\separator_feature_probe.py" --cache 'D:\upmix\datasets\masknet_cache7' `
            --control $control --out $destination --mode $mode --steps $steps 2>&1 |
            Tee-Object -FilePath "$destination.log"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or -not (Test-Path "$destination\complete.json")) {
        throw "Feature probe failed: $mode exit=$exit"
    }
}

try {
    Set-Location $tools
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    $smokeControl = "$runs\separator_feature_smoke_absolute6_v1"
    $smokeCandidate = "$runs\separator_feature_smoke_invariant6_v1"
    Invoke-Probe 'absolute6' $smokeControl 20
    Invoke-Probe 'invariant6' $smokeCandidate 20
    $old = Get-Content "$control\progress.json" -Raw | ConvertFrom-Json
    $base = Get-Content "$smokeControl\progress.json" -Raw | ConvertFrom-Json
    $candidate = Get-Content "$smokeCandidate\progress.json" -Raw | ConvertFrom-Json
    if ([Math]::Abs($old.rows[0].loss - $base.rows[0].loss) -gt 0.000001) {
        throw 'Control initialization/objective parity failed'
    }
    foreach ($row in $candidate.rows) {
        if ($row.polarity_mask_nse -gt 0.00000001 -or $row.polarity_output_nse -gt 0.00000001) {
            throw 'Invariant feature mechanism failed'
        }
    }
    Invoke-Probe 'invariant6' $out 10000
    Get-Content "$out\complete.json" -Raw
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
