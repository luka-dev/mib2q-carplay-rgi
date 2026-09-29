# Guarded privileged TRAIN decoder diagnostic; no DEV/TEST evaluation or port.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$out = "$runs\separator_decoder_oracle_probe_v1"
$lease = "$runs\GPU_LEASE"
if (-not (Test-Path "$runs\separator_fit_probe_v1\fit_complete.json")) { throw 'Finite fit incomplete' }
if (Test-Path $out) { throw 'Decoder-probe output exists; preserve it' }
$pending = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw 'Rapid queue active' }
if ((Get-FileHash "$tools\separator_decoder_oracle_probe.py" -Algorithm SHA256).Hash -ne
    '7A29D69660C0B18CE284448619AE20123196DE278022AC5799C3F4217C5A03EA') {
    throw 'Decoder diagnostic source changed'
}
if ((Get-FileHash "$tools\sep_net.py" -Algorithm SHA256).Hash -ne
    '11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905') {
    throw 'BandSepNet source changed'
}
$busy = @(Get-CimInstance Win32_Process | Where-Object {
    ($_.Name -eq 'python.exe' -and $_.CommandLine -match
        'sep_net.*train|upmix_stem_tests|diag_gap\.py|diag_transitions\.py|separator_fit_probe\.py|separator_decoder_oracle_probe\.py|audit_separator_source_metrics|inspect_dsd100\.py') -or
    ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')
})
if ($busy.Count -ne 0) { throw 'Training, diagnostic, ZIP audit or daemon active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("decoder_probe $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }
try {
    Set-Location $tools
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & python "$tools\separator_decoder_oracle_probe.py" --cache 'D:\upmix\datasets\masknet_cache7' `
            --fit "$runs\separator_fit_probe_v1" --out $out --steps 2000 2>&1 |
            Tee-Object -FilePath "$runs\separator_decoder_oracle_probe_v1.log"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or -not (Test-Path "$out\complete.json")) {
        throw "Decoder diagnostic failed: exit=$exit"
    }
    Get-Content "$out\complete.json" -Raw
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
