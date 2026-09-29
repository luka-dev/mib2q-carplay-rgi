# CPU-only frozen role-head quality and direct decoder-input intervention.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\dual_state4k\role_cue"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|sep_net_state.py|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$hashes = @{
    'diag_dual_state_role_cue.py' = '08884B27B8D4EF980CF4869C59EEFCD750ABA1A77FD62D293C00BC105ABAE656'
    'sep_net_state.py' = '16B5029B2B24D88E0267D25D0B4E956765C5C731C91846BC2124839E190E3EEC'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
        throw "PC diagnostic source hash changed: $name"
    }
}
$ckpt = "$R\dual_state4k\persistent\step4000.pt"
if ((Get-FileHash $ckpt -Algorithm SHA256).Hash -ne
    'EB43DC0E2A058957176672FA16BCD2AFEC54AB6C30EF2FDA4C5EE1560787E3EF') {
    throw 'Persistent checkpoint changed'
}
New-Item -ItemType Directory -Force $O | Out-Null
if (Test-Path "$O\result.json") { throw 'Role-cue audit already finished; preserving it' }
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    python "$T\diag_dual_state_role_cue.py" $C $ckpt --out "$O\result.json" > "$O\run.txt" 2>&1
    $exit = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
if ($exit -ne 0) { Get-Content "$O\run.txt" -Tail 60; throw 'Role-cue audit failed' }
Get-Content "$O\run.txt" -Tail 3
