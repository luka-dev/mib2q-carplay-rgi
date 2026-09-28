# the context rows of the rapid board on the SAME frozen 20-song screen (CPU): sep3's best student, the oracle audio,
# the ideal ratio mask, the teacher (htdemucs on CPU, slow - last)
$T = "D:\upmix\tools"; $C = "D:\upmix\datasets\masknet_cache7"; $R = "D:\upmix\runs"; $RD = "$R\rapid"
$env:PATH = "D:\upmix\tools\ffmpeg\bin;" + $env:PATH
$env:CUDA_VISIBLE_DEVICES = '-1'
$B = "--render `"$T\upmix_render_inj.exe`" --model $T\hssr_model.bin --guard $T\hssr_guard.bin --moisesdb D:\upmix\datasets\moisesdb\moisesdb_v0.1 --split-file $C\split.json --split dev --songs 20 --variant shipped --oracle-ref $R\masknet-oracle\stems_amp_1.0 --jobs 2"
foreach ($pair in @(@("ctx_sep3best", "student:$R\sep3\dsp_60k.pt"), @("ctx_oracle", "oracle"), @("ctx_irm", "irm"), @("ctx_teacher", "teacher"))) {
  $name = $pair[0]; $inj = $pair[1]; $O = "$RD\$name"; New-Item -ItemType Directory -Force $O | Out-Null
  if (Test-Path "$O\screen.log") { continue }
  ("start $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') inject [" + $inj + "]") | Add-Content "$O\job.txt"
  & "$T\stems_guard.ps1" -Out "$O\screen" -Log "$O\screen.log" -HArgs "$B --inject $inj --render-extra `"--mask-drive 1.0`""
  ("done $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Add-Content "$O\job.txt"
  Set-Location $T; Invoke-Expression "python `"$T\rapid_board.py`" `"$RD`" > `"$RD\board.log`" 2>&1"
}
