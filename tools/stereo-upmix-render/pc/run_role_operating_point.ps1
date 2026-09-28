# CPU-only hard-threshold ranking bound for frozen four-role checkpoints.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\causal_role_focus4k"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
foreach ($name in @('role', 'focused')) {
    $ckpt = if ($name -eq 'role') { "$R\causal_role4k\role_softmax\step4000.pt" }
            else { "$O\focused\step4000.pt" }
    python "$T\diag_role_operating_point.py" $C $ckpt > "$O\operating_point_$name.txt" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$name operating point diagnostic failed" }
    Write-Output "$name"
    Get-Content "$O\operating_point_$name.txt"
}
