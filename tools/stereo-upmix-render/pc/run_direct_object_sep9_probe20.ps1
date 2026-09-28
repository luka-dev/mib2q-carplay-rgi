# Frozen sep9 best checkpoint through the unchanged host control and the
# selected direct/no-bed/body-0 renderer; enriched DEV20, no retraining.
# Source student audio is the same for each C condition. CPU-only diagnostic.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$B = "$T\direct_object_host"
$O = "$R\direct_object_sep9_probe20"
$Original = "$R\direct_object_probe20"
$Ckpt = "$R\sep9\a\sep_net.pt"
$expectedCkpt = 'EA312723D9AC327B35472E23337C47376C58A9DCA9331765F4B622384C30DAC7'
$full = Get-Content "$R\direct_object_irm_full40\decision.json" -Raw | ConvertFrom-Json
if (-not $full.variants.direct_no_bed_body0_cal.meets_full40_render_gates) {
    throw 'The frozen ideal-mask-audio full40 bound did not pass'
}
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue still has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|oracle_renderer_sweep.py'
})
if ($busy.Count -ne 0) { throw 'Training or a renderer harness is active' }
if ((Get-FileHash $Ckpt -Algorithm SHA256).Hash -ne $expectedCkpt) { throw 'sep9 checkpoint hash changed' }
$build = Get-Content "$B\manifest.json" -Raw | ConvertFrom-Json
foreach ($name in @('control', 'direct_no_bed')) {
    $file = "$B\upmix_$name.exe"
    $actual = (Get-FileHash $file -Algorithm SHA256).Hash
    $expected = $build.builds.PSObject.Properties[$name].Value.sha256
    if ($actual -ne $expected) { throw "Host binary hash mismatch: $name" }
}
New-Item -ItemType Directory -Force $O | Out-Null
Copy-Item "$Original\manifest.json" "$O\manifest.json" -Force
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\oracle_renderer_sweep.py" --render "$B\upmix_control.exe" `
    --render-overrides "direct_no_bed_body0_cal=$B\upmix_direct_no_bed.exe" `
    --model "$T\hssr_model.bin" --guard "$T\hssr_guard.bin" `
    --moisesdb 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --source student --student-ckpt $Ckpt --probe $O --expected-songs 20 `
    --points 'baseline,direct_no_bed_body0_cal' --skip-original-verify
if ($LASTEXITCODE -ne 0) { throw 'Sep9 direct-object C render failed' }
$injected = @(Get-ChildItem "$O\baseline" -Filter '*.inj.wav')
if ($injected.Count -ne 20) { throw "Expected 20 paired injected audio files, got $($injected.Count)" }
foreach ($source in $injected) {
    $other = Join-Path "$O\direct_no_bed_body0_cal" $source.Name
    if (-not (Test-Path $other)) { throw "Missing paired injected audio: $($source.Name)" }
    if ((Get-FileHash $source.FullName -Algorithm SHA256).Hash -ne
        (Get-FileHash $other -Algorithm SHA256).Hash) {
        throw "Student audio differs between renderer conditions: $($source.Name)"
    }
}
python "$T\evaluate_oracle_renderer_probe.py" --probe $O --scope student20 `
    --points 'baseline,direct_no_bed_body0_cal' --out "$O\decision.json" `
    --source-label 'enriched DEV20 frozen sep9 best checkpoint, same injected estimate through control and host direct/no-bed/body-0 at surround 0.3235; noncausal whole-window norm'
if ($LASTEXITCODE -ne 0) { throw 'Sep9 direct-object screen decision failed' }
