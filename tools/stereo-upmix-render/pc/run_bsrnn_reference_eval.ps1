# One GPU tenant: TRAIN closure smoke then frozen full40 C/source/entry evaluation.
param([switch]$Resume)
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$out = "$runs\bsrnn_reference_check40_v1"
$smoke = "$runs\bsrnn_reference_check_smoke_v1"
$py = 'D:\upmix\venvs\bsrnn_reference\Scripts\python.exe'
$lease = "$runs\GPU_LEASE"
if (-not $Resume -and ((Test-Path $out) -or (Test-Path $smoke))) { throw 'Output exists; preserve previous attempt' }
$expected = @{
    'bsrnn_reference_eval.py' = 'E5DED4764041D9E5EE65FFCBF60B8FC47F2F782427A9121D8889F628ED409C51'
    'bsrnn_reference_infer.py' = '1FC41E4451E212FA0FF662D699B000FF8080E06DD9B7956926B2E130E4AAA30B'
    'upmix_stem_tests.py' = '184CC28B493DDE68B0106410515CE578A2719CDEEA4558D38976C625A02573FE'
    'diag_transitions.py' = 'B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
    'upmix_behavior_tests.py' = 'C28574A4642B59FEB179F0BBD6ADAAEDA870D6FFB7F37223BF5726A40E30913D'
    'mask_net.py' = '5CAAE687105AE492F5BBB5DE5843593882DA43C73E6CF90878BF28AF8F54BE40'
}
foreach ($file in $expected.Keys) {
    if ((Get-FileHash "$tools\$file" -Algorithm SHA256).Hash -ne $expected[$file]) {
        throw "Source changed: $file"
    }
}
$pending = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count) { throw 'Rapid queue active' }
$busy = @(Get-CimInstance Win32_Process | Where-Object {
    ($_.Name -eq 'python.exe' -and -not (Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue).HasExited) -or
    ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')
})
if ($busy.Count) { throw 'Python work or rapid daemon active' }
if ($Resume -and (Test-Path $lease)) {
    $old = Get-CimInstance Win32_Process -Filter 'ProcessId=21788'
    if (-not $old -or $old.CommandLine -notmatch 'bsrnn_reference_eval\.py' -or
        -not (Get-Process -Id 21788).HasExited -or
        (Get-Content $lease -Raw) -notmatch '^bsrnn_eval ') {
        throw 'Existing lease does not belong to the verified exited evaluator'
    }
    Move-Item $lease "$runs\GPU_LEASE.bsrnn_exited_21788"
}
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("bsrnn_eval $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }
function Invoke-Check([string]$destination, [bool]$smokeOnly) {
    $argsForPython = @('-u', "$tools\bsrnn_reference_eval.py",
        '--root', 'D:\upmix\third_party\bsrnn_reference', '--runs', $runs,
        '--tools', $tools, '--cache', 'D:\upmix\datasets\masknet_cache7',
        '--moisesdb', 'D:\upmix\datasets\moisesdb\moisesdb_v0.1', '--out', $destination)
    if ($smokeOnly) { $argsForPython += '--smoke' }
    if ($Resume -and -not $smokeOnly) { $argsForPython += '--resume' }
    $logPath = if ($Resume) { Join-Path $env:TEMP ('upmix_bsrnn_evaluation_' + [guid]::NewGuid().ToString() + '.log') } else { "$destination.log" }
    Write-Output "EVALUATION_LOG $logPath"
    $ErrorActionPreference = 'Continue'
    & $py @argsForPython 2>&1 | Tee-Object -FilePath $logPath
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0 -or -not (Test-Path "$destination\complete.json")) {
        throw "Reference check failed: exit=$code; $destination"
    }
}
try {
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    $env:PATH = "$tools\ffmpeg\bin;" + $env:PATH
    Set-Location $tools
    if (-not $Resume) { Invoke-Check $smoke $true }
    $test = Get-Content "$smoke\complete.json" -Raw | ConvertFrom-Json
    if ($test.finite_retention_bins -lt 1 -or $test.parity.final_source_sum_nse -gt 0.000000001) {
        throw 'Attribution smoke gate failed'
    }
    Invoke-Check $out $false
    Write-Output 'BSRNN_REFERENCE_FULL40_COMPLETE'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
