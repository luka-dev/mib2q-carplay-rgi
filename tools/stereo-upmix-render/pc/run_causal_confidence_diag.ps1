# CPU-only frozen source and transition screen for two causal confidence maps.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\causal_confidence_map"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$shared = "$R\causal_source_pair4k\anchor\step4000.pt"
$focus = "$R\causal_role_focus4k\focused\step4000.pt"
if (-not (Test-Path $shared) -or -not (Test-Path $focus)) { throw 'Comparison checkpoints missing' }
$hash = (Get-FileHash $focus -Algorithm SHA256).Hash
if ($hash -ne '0434C9E584427B8101148C68F11E31CDD97AB8A2152A741FE61B313EEB90E277') {
    throw 'Focused checkpoint hash changed'
}
New-Item -ItemType Directory -Force -Path $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
foreach ($v in @('soft_static', 'soft_ema80')) {
    python "$T\diag_role_selectivity.py" $C $shared $focus --candidate-role-map $v `
        --out "$O\$v.json" > "$O\$v.source.txt" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$v source diagnostic failed" }
    python "$T\diag_transitions.py" $C $focus --role-map $v --songs 20 > "$O\$v.transitions.txt" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$v transition diagnostic failed" }
    Get-Content "$O\$v.source.txt"
    Get-Content "$O\$v.transitions.txt" | Select-String 'entry +n='
}
