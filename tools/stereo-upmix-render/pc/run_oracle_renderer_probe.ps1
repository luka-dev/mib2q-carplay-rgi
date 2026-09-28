# CPU-only direct C-renderer mechanism probe on oracle eligible audio.
# Run after the rapid 10k pair has finished to avoid competing for PC I/O.
# This enriched screen includes all original swing songs; it cannot certify a
# general improvement without a subsequent full-40 and untouched-20 check.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\oracle_renderer_probe"
$Q = "$R\rapid\QUEUE.txt"
$pending = @(Get-Content $Q -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs; wait until it finishes" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object { $_.CommandLine -match 'sep_net.py train|upmix_stem_tests' })
if ($busy.Count -ne 0) { throw "Training or stem harness is active; wait until it finishes" }
New-Item -ItemType Directory -Force $O | Out-Null
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\prepare_oracle_renderer_probe.py" --oracle-run "$R\iface8\inject_oracle" --out $O
if ($LASTEXITCODE -ne 0) { throw 'Could not freeze oracle renderer probe' }
$common = "--render $T\upmix_render_inj.exe --model $T\hssr_model.bin --guard $T\hssr_guard.bin " +
    "--moisesdb D:\upmix\datasets\moisesdb\moisesdb_v0.1 --split-file $O\split.json " +
    "--split probe --songs 20 --seed 1 --variant shipped --jobs 1 --inject oracle"
$points = @(
    @{ Tag = 'rear_body0'; Extra = '--mask-drive 1.0 --rear-body 0' },
    @{ Tag = 'rear_air_off'; Extra = '--mask-drive 1.0 --rear-air -40' },
    @{ Tag = 'front_retention025'; Extra = '--mask-drive 1.0 --front-retention 0.25' },
    @{ Tag = 'front_retention075'; Extra = '--mask-drive 1.0 --front-retention 0.75' }
)
foreach ($point in $points) {
    $out = "$O\$($point.Tag)"
    $log = "$O\$($point.Tag).log"
    $args = "$common --render-extra `"$($point.Extra)`""
    & "$T\stems_guard.ps1" -Out $out -Log $log -HArgs $args
    if (-not (Test-Path "$out\stems.csv")) { throw "C renderer screen failed: $log" }
    python "$T\compare_rapid_pair.py" --anchor-csv "$O\baseline_stems.csv" `
        --candidate-csv "$out\stems.csv" --out "$O\$($point.Tag)_paired.json" --expected-songs 20
    if ($LASTEXITCODE -ne 0) { throw "Paired comparison failed: $($point.Tag)" }
    Write-Output "Completed $($point.Tag): $out\stems.csv"
}
