# One GPU tenant: 20-step paired smoke, fixed 10k pair, independent CPU audit.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$fit = "$runs\separator_fit_probe_v1"
$out = "$runs\separator_causal_reference_v1"
$smoke = "$runs\separator_causal_reference_smoke_v1"
$py = 'C:\Users\LuKa\AppData\Local\Programs\Python\Python313\python.exe'
$lease = "$runs\GPU_LEASE"
if ((Test-Path $out) -or (Test-Path $smoke)) { throw 'Preserve prior output' }
$expected = @{
    'separator_causal_reference.py' = '764A594242D26E7F75884D122B91656ADC3037B53A7C54F602B74678F58A07D0'
    'separator_causal_reference_probe.py' = 'E514FFF84F29B0CC52236B6F4A2095CCCD79B5C1777A77EB5F29A60347F37CCC'
    'audit_separator_causal_reference.py' = '1765C9407C850EF640707F54BF1327C0EF1AA2CB71D9A691D0DDE0AC703ADD3F'
    'separator_fit_probe.py' = 'CEB3C158D1955E3A384C1F21317D00282F50E1C52B3E56ECB698AA5C05FCEA0D'
    'sep_net.py' = '11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905'
    'mask_net.py' = '5CAAE687105AE492F5BBB5DE5843593882DA43C73E6CF90878BF28AF8F54BE40'
}
foreach ($file in $expected.Keys) {
    if ((Get-FileHash "$tools\$file" -Algorithm SHA256).Hash -ne $expected[$file]) {
        throw "Source changed: $file"
    }
}
if ((Get-FileHash "$fit\fit.pt" -Algorithm SHA256).Hash -ne
    '0E560B9924FEEA88F3E61B97253734C60A02A3901C7636779E107653D7E77E41') { throw 'Anchor changed' }
$pending = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count) { throw 'Rapid queue active' }
$busy = @(Get-CimInstance Win32_Process | Where-Object {
    ($_.Name -eq 'python.exe' -and -not (Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue).HasExited) -or
    ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')
})
if ($busy.Count) { throw 'Python/rapid daemon active' }
$token = [guid]::NewGuid().ToString()
$handle = [IO.File]::Open($lease,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try { $b=[Text.Encoding]::ASCII.GetBytes("causal_reference $token"); $handle.Write($b,0,$b.Length) }
finally { $handle.Dispose() }
function Invoke-Pair($destination,$steps) {
    $log = Join-Path $env:TEMP ('upmix_causal_reference_'+[guid]::NewGuid().ToString()+'.log')
    Write-Output "PAIR_LOG $log"
    $ErrorActionPreference='Continue'
    & $py -u "$tools\separator_causal_reference_probe.py" --cache 'D:\upmix\datasets\masknet_cache7' `
        --fit $fit --out $destination --steps $steps 2>&1 | Tee-Object -FilePath $log
    $code=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if ($code -ne 0 -or -not (Test-Path "$destination\complete.json")) { throw "Pair failed: exit=$code" }
}
try {
    Set-Location $tools
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Invoke-Pair $smoke 20
    $s=Get-Content "$smoke\complete.json" -Raw | ConvertFrom-Json
    if ($s.arms.original.architecture.frozen_decoder_sha256 -ne $s.arms.residual96x4.architecture.frozen_decoder_sha256 -or
        $s.arms.original.architecture.global_gru_initial_sha256 -ne $s.arms.residual96x4.architecture.global_gru_initial_sha256) {
        throw 'Paired smoke construction parity failed'
    }
    Invoke-Pair $out 10000
    $env:CUDA_VISIBLE_DEVICES='-1'
    $ErrorActionPreference='Continue'
    & $py -u "$tools\audit_separator_causal_reference.py" --cache 'D:\upmix\datasets\masknet_cache7' `
        --fit $fit --out $out
    $code=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if ($code -ne 0 -or -not (Test-Path "$out\audit.json")) { throw 'CPU audit failed' }
    Write-Output 'CAUSAL_REFERENCE_PAIR_AND_AUDIT_COMPLETE'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) { Remove-Item $lease -Force }
}
