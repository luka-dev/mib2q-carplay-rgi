# CPU-only paired whole-file versus 8-second-reset inference on six fixed DEV songs.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\continuous_context_audit"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py|diag_long_context.py'
})
if ($busy.Count -ne 0) { throw 'Training or another harness is active' }
if ((Get-FileHash "$T\diag_long_context.py" -Algorithm SHA256).Hash -ne
    'F249994280B7D22055635BAF4CE1177636A906F7ACEEC31733D42321BD4CE4D1') {
    throw 'Long-context diagnostic source hash changed'
}
if ((Get-FileHash "$T\sep_net.py" -Algorithm SHA256).Hash -ne
    'A4855311959C6ED61A190635F878BD63C495608599008E263848950D8F9F6842') {
    throw 'Separator source hash changed'
}
$control = "$R\dual_head_causal4k\complex_control\step4000.pt"
$deep = "$R\causal_deep2_4k\step4000.pt"
if ((Get-FileHash $control -Algorithm SHA256).Hash -ne
    'A38DCCB9DAD5AF382AC34EB38CBD3FB2E6114E60C0C34B4BA8C40279ABB93F12') {
    throw 'Complex control checkpoint changed'
}
if ((Get-FileHash $deep -Algorithm SHA256).Hash -ne
    '31E4F9F1344EEB2FCEAED63DA324E0B82D6FC687D9054F97DABF148B1EEB8745') {
    throw 'Two-tap checkpoint changed'
}
New-Item -ItemType Directory -Force $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\diag_long_context.py" $C $control $deep --songs 6 `
    --out "$O\result.json" > "$O\run.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Long-context audit failed' }
Get-Content "$O\run.txt"
