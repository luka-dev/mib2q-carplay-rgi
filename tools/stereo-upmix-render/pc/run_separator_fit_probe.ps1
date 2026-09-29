# Guarded finite TRAIN fitting diagnostic; no DEV/TEST evaluation or port.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$out = "$runs\separator_fit_probe_v1"
$lease = "$runs\GPU_LEASE"
if (Test-Path $out) { throw 'Fit-probe output exists; preserve it' }
$pending = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw 'Rapid queue active' }
if ((Get-FileHash "$tools\separator_fit_probe.py" -Algorithm SHA256).Hash -ne
    'CEB3C158D1955E3A384C1F21317D00282F50E1C52B3E56ECB698AA5C05FCEA0D') {
    throw 'Fit diagnostic source changed'
}
if ((Get-FileHash "$tools\sep_net.py" -Algorithm SHA256).Hash -ne
    '11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905') {
    throw 'BandSepNet source changed'
}
$busy = @(Get-CimInstance Win32_Process | Where-Object {
    ($_.Name -eq 'python.exe' -and $_.CommandLine -match
        'sep_net.*train|upmix_stem_tests|diag_gap\.py|diag_transitions\.py|separator_fit_probe\.py|audit_separator_source_metrics|inspect_dsd100\.py') -or
    ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')
})
if ($busy.Count -ne 0) { throw 'Training, diagnostic, ZIP audit or daemon active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("finite_fit $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }
try {
    Set-Location $tools
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & python "$tools\separator_fit_probe.py" --cache 'D:\upmix\datasets\masknet_cache7' `
            --out $out --steps 10000 --eval-every 250 2>&1 |
            Tee-Object -FilePath "$runs\separator_fit_probe_v1.log"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or -not (Test-Path "$out\fit_complete.json")) {
        throw "Finite fit diagnostic failed: exit=$exit"
    }
    Get-Content "$out\fit_complete.json" -Raw
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
