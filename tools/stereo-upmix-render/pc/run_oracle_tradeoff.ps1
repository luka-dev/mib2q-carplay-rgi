# CPU-only oracle-cue tradeoff probe on the frozen 20-song DEV screen.
# Ground-truth stems make this non-deployable. Keep the GPU leased to sep9/rapid.
param(
    [string]$Tag = "oracle_fast_444_g12",
    [double]$VocalWeight = 4,
    [double]$DrumWeight = 4,
    [double]$BassWeight = 4,
    [double]$Drive = 1.2,
    [double]$RearBody = -4
)
$T = "D:\upmix\tools"
$R = "D:\upmix\runs\membership"
$O = "$R\$Tag"
$S = "${O}_screen"
New-Item -ItemType Directory -Force $O, $S | Out-Null
# stems_guard resumes by song. Never reuse cached rows for changed parameters.
$Config = "v=$VocalWeight d=$DrumWeight b=$BassWeight gamma=0.5 drive=$Drive rearbody=$RearBody"
$ConfigPath = "$O\probe_config.txt"
if ((Test-Path $ConfigPath) -and ((Get-Content $ConfigPath -Raw).Trim() -ne $Config)) {
    throw "Tag $Tag already has different parameters; choose a new tag"
}
$Config | Set-Content $ConfigPath
$env:CUDA_VISIBLE_DEVICES = '-1'
Set-Location $T
python "$T\oracle_cue_mask.py" --moisesdb D:\upmix\datasets\moisesdb\moisesdb_v0.1 `
    --split-file D:\upmix\datasets\masknet_cache7\split.json --split dev --songs 20 --seed 1 `
    --vocal-weight $VocalWeight --drum-weight $DrumWeight --bass-weight $BassWeight `
    --gamma 0.5 --out $O > "$O\mask.log" 2>&1
if ($LASTEXITCODE -ne 0) { throw "oracle mask generation failed: $O\mask.log" }
$HArgs = "--render $T\upmix_render_inj.exe --model $T\hssr_model.bin --guard $T\hssr_guard.bin " +
    "--moisesdb D:\upmix\datasets\moisesdb\moisesdb_v0.1 --split-file D:\upmix\datasets\masknet_cache7\split.json " +
    "--split dev --songs 20 --seed 1 --variant shipped --jobs 2 --mask-dir $O " +
    "--render-extra `"--mask-drive $Drive --rear-body $RearBody`""
& "$T\stems_guard.ps1" -Out $S -Log "${S}.log" -HArgs $HArgs
if (-not (Test-Path "$S\stems.csv")) { throw "C renderer screen failed: $S" }
Write-Output "Completed $Tag : $S\stems.csv"
