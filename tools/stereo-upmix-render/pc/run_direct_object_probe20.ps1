# Host-only architecture screen on the eight original oracle-audio swing songs
# plus twelve frozen controls. Track A source was copied at build time, not edited.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$B = "$T\direct_object_host"
$O = "$R\direct_object_probe20"
$Original = "$R\oracle_renderer_probe"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
$build = Get-Content "$B\manifest.json" -Raw | ConvertFrom-Json
foreach ($name in @('control', 'direct_object', 'direct_no_bed')) {
    $file = "$B\upmix_$name.exe"
    if (-not (Test-Path $file)) { throw "Missing host binary $file" }
    $actual = (Get-FileHash $file -Algorithm SHA256).Hash
    $expected = $build.builds.PSObject.Properties[$name].Value.sha256
    if ($actual -ne $expected) { throw "Host binary hash mismatch: $name" }
}
New-Item -ItemType Directory -Force $O | Out-Null
Copy-Item "$Original\manifest.json" "$O\manifest.json" -Force
Copy-Item "$Original\baseline_stems.csv" "$O\baseline_stems.csv" -Force
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
$overrides = "direct_object=$B\upmix_direct_object.exe,direct_no_bed=$B\upmix_direct_no_bed.exe"
python "$T\oracle_renderer_sweep.py" --render "$B\upmix_control.exe" `
    --render-overrides $overrides --model "$T\hssr_model.bin" `
    --guard "$T\hssr_guard.bin" --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --probe $O --expected-songs 20 --points 'baseline,direct_object,direct_no_bed'
if ($LASTEXITCODE -ne 0) { throw "Direct-object C screen failed; inspect $O and resume the same command" }
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope probe `
    --points 'baseline,direct_object,direct_no_bed' --out "$O\decision.json" `
    --source-label 'enriched 20-song true-eligible-audio host-only direct-object C screen; not device or listening evidence'
if ($LASTEXITCODE -ne 0) { throw 'Direct-object screen gate failed' }
