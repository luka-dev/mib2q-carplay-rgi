# CPU-only fixed DEV12 source/role and full-song DEV20 transition screen.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\fine_role4k"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|sep_net_state.py|sep_net_fine_role.py|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$hashes = @{
    'diag_fine_role.py' = '125ED865F1763F822614B69306400596E99B8A3B4389534A43E52C262A617A1E'
    'sep_net_fine_role.py' = 'B2D9CC188A6ABF112A6D4D09385AD2947E6FA939036872F060BAECB4198BADB6'
    'diag_dual_state.py' = 'BAA6788BCDC3D44112AF4D0AA93923409D11E999B1C3B6AC7307DB76D992847F'
    'diag_role_selectivity.py' = '7606F3E655558376C2334737100C6FA090085D34175A759759001BCAE0A689FB'
    'diag_transitions.py' = 'B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
        throw "PC diagnostic source hash changed: $name"
    }
}
$coarse = "$O\coarse\step4000.pt"
$fine = "$O\fine\step4000.pt"
if (-not ((Test-Path "$O\coarse\train_done.txt") -and
          (Test-Path "$O\fine\train_done.txt") -and
          (Test-Path $coarse) -and (Test-Path $fine))) {
    throw 'Matched 4k checkpoints are not complete'
}
$D = "$O\diag"
New-Item -ItemType Directory -Force $D | Out-Null
if (Test-Path "$D\summary.json") { throw 'Fine-role diagnostic already finished; preserving it' }
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    python "$T\diag_fine_role.py" $C $coarse $fine --out $D > "$D\run.txt" 2>&1
    $exit = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
if ($exit -ne 0) { Get-Content "$D\run.txt" -Tail 60; throw 'Fine-role diagnostic failed' }
Get-Content "$D\run.txt" -Tail 1
