# Privileged four-role power-mask bound through frozen host C on enriched DEV20.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$O = "$R\causal_role4k"
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
$probe = "$O\screen_role_power_oracle20"
New-Item -ItemType Directory -Force $probe | Out-Null
Copy-Item "$Original\manifest.json" "$probe\manifest.json" -Force
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\oracle_renderer_sweep.py" --render "$B\upmix_control.exe" `
    --render-overrides "direct_no_bed_body0_cal=$B\upmix_direct_no_bed.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --source role_power --probe $probe --expected-songs 20 `
    --points 'baseline,direct_no_bed_body0_cal' --skip-original-verify
if ($LASTEXITCODE -ne 0) { throw 'Role-power oracle C render failed' }
$injected = @(Get-ChildItem "$probe\baseline" -Filter '*.inj.wav')
if ($injected.Count -ne 20) { throw 'Expected 20 injected WAVs' }
foreach ($source in $injected) {
    $other = Join-Path "$probe\direct_no_bed_body0_cal" $source.Name
    if (-not (Test-Path $other) -or
        (Get-FileHash $source.FullName -Algorithm SHA256).Hash -ne
        (Get-FileHash $other -Algorithm SHA256).Hash) {
        throw "Paired renderer inputs differ: $($source.Name)"
    }
}
python "$T\evaluate_oracle_renderer_probe.py" --probe $probe --scope student20 `
    --points 'baseline,direct_no_bed_body0_cal' --out "$probe\decision.json" `
    --source-label 'privileged true-stem four-role power mask on enriched DEV20, frozen host C; noncausal upper bound, no device/listening claim'
if ($LASTEXITCODE -ne 0) { throw 'Role-power oracle C evaluation failed' }
