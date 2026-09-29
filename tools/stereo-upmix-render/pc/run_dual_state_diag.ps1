# CPU-only fixed DEV12 sources and full-song DEV20 43-entry attribution.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\dual_state4k"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|sep_net_state.py|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$hashes = @{
    'sep_net_state.py' = '16B5029B2B24D88E0267D25D0B4E956765C5C731C91846BC2124839E190E3EEC'
    'diag_dual_state.py' = 'CD769F676F9B2FC7AA89C5148EA181094660BA3B8434C7888FB90016F3AA33B1'
    'diag_role_selectivity.py' = '7606F3E655558376C2334737100C6FA090085D34175A759759001BCAE0A689FB'
    'diag_transitions.py' = 'B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
        throw "PC diagnostic source hash changed: $name"
    }
}
$reset = "$O\reset2\step4000.pt"
$persistent = "$O\persistent\step4000.pt"
if (-not ((Test-Path "$O\reset2\train_done.txt") -and
          (Test-Path "$O\persistent\train_done.txt") -and
          (Test-Path $reset) -and (Test-Path $persistent))) {
    throw 'Matched 4k checkpoints are not complete'
}
$D = "$O\diag"
New-Item -ItemType Directory -Force $D | Out-Null
if (Test-Path "$D\transitions.json") { throw 'Dual-state diagnostic already finished; preserving it' }
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
python "$T\diag_dual_state.py" $C $reset $persistent --out $D > "$D\run.txt" 2>&1
if ($LASTEXITCODE -ne 0) { Get-Content "$D\run.txt" -Tail 60; throw 'Dual-state diagnostic failed' }
Get-Content "$D\run.txt" -Tail 30
