# CPU-only real-C mechanism screen for the matched 4k causal models.
# Same enriched DEV20 manifest and frozen direct/no-bed/body-0 renderer .3235.
# This is not an independent validation or a deployable device measurement.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\causal_source_pair4k"
$B = "$T\direct_object_host"
$Original = "$R\direct_object_probe20"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
$build = Get-Content "$B\manifest.json" -Raw | ConvertFrom-Json
foreach ($name in @('control', 'direct_no_bed')) {
    $file = "$B\upmix_$name.exe"
    if ((Get-FileHash $file -Algorithm SHA256).Hash -ne
        $build.builds.PSObject.Properties[$name].Value.sha256) {
        throw "Host binary hash mismatch: $name"
    }
}
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
foreach ($name in @('anchor', 'source_attr')) {
    $ckpt = "$O\$name\step4000.pt"
    if (-not (Test-Path "$O\$name\train_done.txt") -or -not (Test-Path $ckpt)) {
        throw "$name is not finished at step 4000"
    }
    $probe = "$O\screen_$name"
    New-Item -ItemType Directory -Force $probe | Out-Null
    Copy-Item "$Original\manifest.json" "$probe\manifest.json" -Force
    Write-Output "$name checkpoint sha256 $((Get-FileHash $ckpt -Algorithm SHA256).Hash)"
    python "$T\oracle_renderer_sweep.py" --render "$B\upmix_control.exe" `
        --render-overrides "direct_no_bed_body0_cal=$B\upmix_direct_no_bed.exe" `
        --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
        --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
        --source student --student-ckpt $ckpt --probe $probe --expected-songs 20 `
        --points 'baseline,direct_no_bed_body0_cal' --skip-original-verify
    if ($LASTEXITCODE -ne 0) { throw "$name C render failed" }
    $injected = @(Get-ChildItem "$probe\baseline" -Filter '*.inj.wav')
    if ($injected.Count -ne 20) { throw "$name expected 20 injected WAVs" }
    foreach ($source in $injected) {
        $other = Join-Path "$probe\direct_no_bed_body0_cal" $source.Name
        if (-not (Test-Path $other) -or
            (Get-FileHash $source.FullName -Algorithm SHA256).Hash -ne
            (Get-FileHash $other -Algorithm SHA256).Hash) {
            throw "$name paired renderer inputs differ: $($source.Name)"
        }
    }
    python "$T\evaluate_oracle_renderer_probe.py" --probe $probe --scope student20 `
        --points 'baseline,direct_no_bed_body0_cal' --out "$probe\decision.json" `
        --source-label "enriched DEV20 causal shared-mask $name step4000 through frozen host direct/no-bed/body-0 C; no device/listening claim"
    if ($LASTEXITCODE -ne 0) { throw "$name C gate failed" }
}
python "$T\compare_rapid_pair.py" `
    --anchor-csv "$O\screen_anchor\direct_no_bed_body0_cal\stems.csv" `
    --candidate-csv "$O\screen_source_attr\direct_no_bed_body0_cal\stems.csv" `
    --out "$O\source_attr_vs_anchor_direct_paired.json" --expected-songs 20
if ($LASTEXITCODE -ne 0) { throw 'Matched causal C comparison failed' }
