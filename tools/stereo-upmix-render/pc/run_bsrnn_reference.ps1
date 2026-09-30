# Strict pretrained inference only; exclusive GPU; never modify upstream or Track A.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$out = "$runs\bsrnn_pretrained_train4_v1"
$py = 'D:\upmix\venvs\bsrnn_reference\Scripts\python.exe'
$lease = "$runs\GPU_LEASE"
if (Test-Path $out) { throw 'Output exists; preserve the previous run' }
if (-not (Test-Path $py)) { throw 'Isolated reference environment missing' }
if ((Get-FileHash "$tools\bsrnn_reference_infer.py" -Algorithm SHA256).Hash -ne
    '1FC41E4451E212FA0FF662D699B000FF8080E06DD9B7956926B2E130E4AAA30B') {
    throw 'Inference source changed'
}
$pending = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count) { throw 'Rapid queue active' }
$busy = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -eq 'python.exe' -or
    ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')
})
if ($busy.Count) { throw 'Python work or rapid daemon active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("bsrnn_reference $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }
try {
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    $env:PATH = "$tools\ffmpeg\bin;" + $env:PATH
    Set-Location $tools
    $ErrorActionPreference = 'Continue'
    & $py -u "$tools\bsrnn_reference_infer.py" `
        --root 'D:\upmix\third_party\bsrnn_reference' `
        --cache 'D:\upmix\datasets\masknet_cache7' `
        --manifest "$runs\separator_fit_probe_v1\manifest.json" `
        --out $out 2>&1 | Tee-Object -FilePath "$out.log"
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0 -or -not (Test-Path "$out\complete.json")) {
        throw "BSRNN inference failed: exit=$code"
    }
    & $py -m pip freeze | Set-Content -Encoding UTF8 "$out\runtime_freeze.txt"
    Write-Output 'STRICT_REFERENCE_COMPLETE'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
