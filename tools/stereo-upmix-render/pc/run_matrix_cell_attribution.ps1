# CPU-only, fixed DEV12 attribution by true-source time-frequency ownership.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\matrix_cell_attribution"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
if ((Get-FileHash "$T\diag_matrix_cell_attribution.py" -Algorithm SHA256).Hash -ne
    '66CC0148D95F2B29FDE8FEB94BF87BCC545C4C72926F17B4F32856CAA0D43E92') {
    throw 'PC diagnostic source hash changed'
}
$control = "$R\causal_matrix2_4k\step4000.pt"
$candidate = "$R\matrix_source_loss4k\focused\step4000.pt"
if ((Get-FileHash $control -Algorithm SHA256).Hash -ne
    '849C80EDBC6AF2B1D02D647A9EDC956B6787EE53558F8CBA36F2651A0A0F0604') {
    throw 'Matrix control checkpoint changed'
}
if ((Get-FileHash $candidate -Algorithm SHA256).Hash -ne
    '35B042EBBD44B03697F01444E03861C4D4A185C13F46B23376DD121B1B67FA4A') {
    throw 'Source-loss candidate checkpoint changed'
}
New-Item -ItemType Directory -Force $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\diag_matrix_cell_attribution.py" $C $control $candidate --out "$O\dev12.json" |
    Tee-Object -FilePath "$O\dev12.txt"
if ($LASTEXITCODE -ne 0) { throw 'Matrix cell attribution failed' }
