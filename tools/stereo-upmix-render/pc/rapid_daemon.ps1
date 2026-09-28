# rapid_daemon.ps1 (astra review 15): the rapid test loop.  Jobs = lines "tag|sep_net args" in D:\upmix\runs\rapid\QUEUE.txt.
# While jobs are queued it holds D:\upmix\runs\GPU_LEASE (the long runners wait on it), waits for the GPU to be free,
# trains 4000 steps with the FULL recipe (+ --save-at 4000), runs the attribution and transition diagnostics on the GPU,
# releases the GPU when nothing else is queued, runs the frozen 20-song screen on the CPU at G 1.0, rebuilds the board.
$T = "D:\upmix\tools"; $C = "D:\upmix\datasets\masknet_cache7"; $TC = "D:\upmix\datasets\teacher_cache4"; $R = "D:\upmix\runs"
$RD = "$R\rapid"; $Q = "$RD\QUEUE.txt"; $LEASE = "$R\GPU_LEASE"
New-Item -ItemType Directory -Force $RD | Out-Null
$env:PATH = "D:\upmix\tools\ffmpeg\bin;" + $env:PATH
$B = "--render `"$T\upmix_render_inj.exe`" --model $T\hssr_model.bin --guard $T\hssr_guard.bin --moisesdb D:\upmix\datasets\moisesdb\moisesdb_v0.1 --split-file $C\split.json --split dev --songs 20 --variant shipped --oracle-ref $R\masknet-oracle\stems_amp_1.0 --jobs 2"
function Busy { Get-CimInstance Win32_Process -Filter "name='python.exe'" | ? { $_.CommandLine -match 'sep_net.py train|upmix_stem_tests' } }
while ($true) {
  $lines = @(Get-Content $Q -ErrorAction SilentlyContinue | ? { $_ -match '\|' -and $_ -notmatch '^\s*#' })
  if ($lines.Count -eq 0) { if (Test-Path $LEASE) { Remove-Item $LEASE -Force }; Start-Sleep 60; continue }
  "rapid $(Get-Date)" | Set-Content $LEASE
  while (Busy) { Start-Sleep 30 }
  $job = $lines[0]; $parts = $job -split '\|', 2; $tag = $parts[0].Trim(); $args = $parts[1].Trim()
  $O = "$RD\$tag"; New-Item -ItemType Directory -Force $O | Out-Null
  ("start $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') args [" + $args + "]") | Add-Content "$O\job.txt"
  Set-Location $T
  Remove-Item Env:\CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
  if (-not (Test-Path "$O\a\train_done.txt")) {
    $extra = ""; if (Test-Path "$O\a\last.pt") { $extra = "--init `"$O\a\last.pt`"" }
    Invoke-Expression "python `"$T\sep_net.py`" train --arch band $args --cache $C --teacher-cache $TC --half-half --out `"$O\a`" --steps 4000 --batch 12 --seconds 8 --eval-every 1000 --save-at 4000 --lr 3e-4 --warmup 2000 --sisdr-after 20000 $extra >> `"$O\train.log`" 2>&1"
  }
  if (Test-Path "$O\a\step4000.pt") {
    if (-not (Test-Path "$O\gap.txt")) { Invoke-Expression "python `"$T\diag_gap.py`" $C `"$O\a\step4000.pt`" > `"$O\gap.txt`" 2>&1" }
    if (-not (Test-Path "$O\transitions.txt")) { Invoke-Expression "python `"$T\diag_transitions.py`" $C `"$O\a\step4000.pt`" --songs 20 > `"$O\transitions.txt`" 2>&1" }
  } else { "training did not produce step4000.pt" | Add-Content "$O\job.txt" }
  $rest = @($lines | Select-Object -Skip 1)
  if ($rest.Count -eq 0) { Remove-Item $LEASE -Force -ErrorAction SilentlyContinue }
  if ((Test-Path "$O\a\step4000.pt") -and -not (Test-Path "$O\screen.log")) {
    $env:CUDA_VISIBLE_DEVICES = '-1'
    & "$T\stems_guard.ps1" -Out "$O\screen" -Log "$O\screen.log" -HArgs "$B --inject student:$O\a\step4000.pt --render-extra `"--mask-drive 1.0`""
    Remove-Item Env:\CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
  }
  ("done $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Add-Content "$O\job.txt"
  $now = @(Get-Content $Q -ErrorAction SilentlyContinue); $now | ? { $_ -ne $job } | Set-Content $Q
  Invoke-Expression "python `"$T\rapid_board.py`" `"$RD`" > `"$RD\board.log`" 2>&1"
}
