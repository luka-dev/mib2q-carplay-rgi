# CPU-only first-half-fit/second-half-evaluate stereo matrix upper bound.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$O = "$R\spatial_oracle12"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py|diag_spatial_oracle.py'
})
if ($busy.Count -ne 0) { throw 'Training or another harness is active' }
if ((Get-FileHash "$T\diag_spatial_oracle.py" -Algorithm SHA256).Hash -ne
    '20BE50D0841234612B6A1A833AA8855D9DCB22486DE219258D22199D280BEE09') { throw 'Spatial oracle source hash changed' }
New-Item -ItemType Directory -Force $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\diag_spatial_oracle.py" $C --songs 12 --out "$O\result.json" > "$O\run.txt" 2>&1
if ($LASTEXITCODE -ne 0) { throw 'Spatial oracle diagnostic failed' }
Get-Content "$O\run.txt"
