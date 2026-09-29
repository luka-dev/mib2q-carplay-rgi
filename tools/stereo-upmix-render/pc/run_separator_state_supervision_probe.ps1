# One GPU, exact TRAIN anchor, frozen decoder and pure state-regression arm.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$out = "$runs\separator_state_supervision_v2"
$lease = "$runs\GPU_LEASE"
if (Test-Path $out) { throw 'Output exists; preserve it' }
$hashes = @{
    'separator_state_supervision_probe.py' = '51BC861EA09533AA1D2AE58F98E0CFB38B81F6B8A9B909758990A9FA0C8B48D1'
    'separator_fit_probe.py' = 'CEB3C158D1955E3A384C1F21317D00282F50E1C52B3E56ECB698AA5C05FCEA0D'
    'sep_net.py' = '11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905'
}
foreach ($file in $hashes.Keys) {
    if ((Get-FileHash "$tools\$file" -Algorithm SHA256).Hash -ne $hashes[$file]) {
        throw "Source changed: $file"
    }
}
if ((Get-FileHash "$runs\separator_fit_probe_v1\fit.pt" -Algorithm SHA256).Hash -ne
    '0E560B9924FEEA88F3E61B97253734C60A02A3901C7636779E107653D7E77E41') {
    throw 'Original TRAIN checkpoint changed'
}
if ((Get-FileHash "$runs\separator_decoder_oracle_probe_v1\decoder_result.json" -Algorithm SHA256).Hash -ne
    'E802C7A214B55C4EEFA73FEFF9C2F5B174682B4D842BA71F9031F12C56F0AC0F') {
    throw 'Privileged decoder reference changed'
}
$pending = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw 'Rapid queue active' }
$busy = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -eq 'python.exe' -or
    ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')
})
if ($busy.Count -ne 0) { throw 'Python work or rapid daemon active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("state_supervision $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }
try {
    Set-Location $tools
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & python "$tools\separator_state_supervision_probe.py" --cache 'D:\upmix\datasets\masknet_cache7' `
            --fit "$runs\separator_fit_probe_v1" `
            --teacher-reference "$runs\separator_decoder_oracle_probe_v1\decoder_result.json" `
            --prepared-targets "$runs\separator_state_supervision_v1" `
            --out $out --steps 2000 2>&1 | Tee-Object -FilePath "$out.log"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or -not (Test-Path "$out\complete.json")) {
        throw "State supervision diagnostic failed: exit=$exit"
    }
    Get-Content "$out\complete.json" -Raw
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
