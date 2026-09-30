param([switch]$Resume)
$ErrorActionPreference='Stop'
$tools='D:\upmix\tools'
$runs='D:\upmix\runs'
$out="$runs\spectral_context_fixed_decoder_fit_v1"
$parent="$runs\unified_capacity_continuation_v1"
$fixtures="$runs\separator_causal_reference_v1"
$py='D:\upmix\venvs\bsrnn_reference\Scripts\python.exe'
$lease="$runs\GPU_LEASE"
$expected=@{
    'expand_unified_upmix.py'='af56cd6d31d1b7875cece507a812f5c6aaa92c9ec111ecf6b961a3cf19ec514d'
    'mask_net.py'='5caae687105ae492f5bbb5de5843593882da43c73e6cf90878bf28af8f54be40'
    'sep_net.py'='11e0c4fe89c82d3be3814e5d2a421c5fd73c40d4e3fa505904c57ae1d7dba905'
    'separator_causal_reference_probe.py'='e514fff84f29b0cc52236b6f4a2095cccd79b5c1777a77eb5f29a60347f37ccc'
    'separator_fit_probe.py'='ceb3c158d1955e3a384c1f21317d00282f50e1c52b3e56ecb698aa5c05fcea0d'
    'train_causal_reference_generalization.py'='43ca0739a3d110031d72648bdaf10e47cb83c1c1569026603a4b91a901a5dffb'
    'train_spectral_context_fit.py'='4c2b18067e0bc73b1626410902651be05dd4b844ec1fac34e72566353394c9a6'
    'train_unified_upmix_fit.py'='8b777f2ed4914b8184e94f728d47c98f08550f04e69e98d18dcc85746a86bb78'
    'train_unified_upmix_generalization.py'='921dd4a3edae2e9d7ed612d69777f61fc4a9d01ec66d166ba9e3a6f144e26a2d'
    'unified_spectral_context_net.py'='5d67e9c5ab7b92b145887aac0ae754db96b10f99c9541bc66c0434e3e6494335'
    'unified_upmix_audio.py'='0692531b4290d12adb114648fbd072f1cc781a36ce03fe8510816e9f67a051b2'
    'unified_upmix_net.py'='c7aa0abc74dda830a5c306a5f61c30ae95ece5f8bdf61cd693ee4db57ff3fc46'
    'unified_upmix_stream.py'='590eee8e93dc0eb29b757f64490fd9854910076a3ae28a86dbe1285eb056e098'
}
foreach($file in $expected.Keys){if((Get-FileHash "$tools\$file" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected[$file]){throw "Frozen source differs: $file"}}
$completed=Get-Content "$parent\full40\complete.json" -Raw|ConvertFrom-Json
if(-not $completed.complete -or $completed.arms.expanded.nominal_observed_gates_pass){throw 'Completed failing capacity pair required'}
if(Test-Path "$out\complete.json"){throw 'Preserve completed fitting pair'}
if((Test-Path $out) -and -not $Resume){throw 'Own partial study needs verified resume'}
if((Get-PSDrive -Name D).Free -lt 8GB){throw 'Insufficient disk reserve'}
$pending=@(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue|Where-Object{$_ -match '\|' -and $_ -notmatch '^\s*#'})
if($pending.Count){throw 'Rapid queue active'}
$busy=@(Get-CimInstance Win32_Process|Where-Object{$_.Name -eq 'python.exe' -or ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')})
if($busy.Count){throw 'Python/rapid owner active'}
$token=[guid]::NewGuid().ToString()
$handle=[IO.File]::Open($lease,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try{$bytes=[Text.Encoding]::ASCII.GetBytes("spectral_context_fixed_decoder_fit_v1 $token");$handle.Write($bytes,0,$bytes.Length)}finally{$handle.Dispose()}
function Invoke-Worker([string[]]$Arguments) {
    $tag=[guid]::NewGuid().ToString()
    $stdout=Join-Path $env:TEMP ("upmix_unified_fit_$tag.stdout.log")
    $stderr=Join-Path $env:TEMP ("upmix_unified_fit_$tag.stderr.log")
    Write-Output "WORKER_ARGS $($Arguments -join ' ')"
    Write-Output "WORKER_STDOUT $stdout"
    Write-Output "WORKER_STDERR $stderr"
    $process=Start-Process -FilePath $py -ArgumentList $Arguments -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $retainedHandle=$process.Handle
    $workerId=$process.Id
    Write-Output "WORKER_PID $workerId"
    $lastOutput=Get-Date
    while(-not $process.HasExited) {
        Start-Sleep -Seconds 5
        $process.Refresh()
        foreach($file in @($stdout,$stderr)) {
            $info=Get-Item $file -ErrorAction SilentlyContinue
            if($info -and $info.LastWriteTime -gt $lastOutput) {$lastOutput=$info.LastWriteTime}
        }
        if(-not $process.HasExited -and ((Get-Date)-$lastOutput).TotalMinutes -gt 20) {
            & taskkill.exe /PID $workerId /T /F | Out-Null
            $process.WaitForExit()
            throw "Own worker stalled: $workerId; logs and checkpoints preserved"
        }
    }
    $process.WaitForExit()
    Write-Output "WORKER_EXIT $($process.ExitCode)"
    Get-Content $stdout -Tail 8 -ErrorAction SilentlyContinue
    if($null -eq $process.ExitCode -or $process.ExitCode -ne 0) {
        Get-Content $stderr -Tail 20 -ErrorAction SilentlyContinue
        throw "Worker exit $($process.ExitCode); verified resume required"
    }
}

$common=@('--cache','D:\upmix\datasets\masknet_cache7','--fixtures',$fixtures,'--parent',$parent,'--out',$out)
function Train-Arm([string]$Mode,[switch]$Smoke){
 $suffix=if($Smoke){'_smoke'}else{''};$folder="$out\$Mode$suffix"
 if(Test-Path "$folder\train_complete.json"){
  $m=Get-Content "$folder\train_complete.json" -Raw|ConvertFrom-Json
  $steps=if($Smoke){20}else{20000}
  if(-not $m.complete -or $m.mode -ne $Mode -or $m.step -ne $steps -or $m.smoke -ne [bool]$Smoke -or (Get-FileHash "$folder\step$steps.pt" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $m.checkpoint_sha256){throw 'Invalid completed fitting arm'}
  return
 }
 $arguments=@('-u',"$tools\train_spectral_context_fit.py",'train','--mode',$Mode)+$common
 if($Smoke){$arguments+='--smoke'}
 if(Test-Path $folder){if(-not $Resume -or -not (Test-Path "$folder\last.pt")){throw 'Own partial arm needs verified resume'};$arguments+='--resume'}
 Invoke-Worker $arguments
}
try{
 Set-Location $tools
 Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
 if(-not (Test-Path "$out\prepared.json")){Invoke-Worker (@('-u',"$tools\train_spectral_context_fit.py",'prepare','--device','cpu')+$common)}
 foreach($mode in @('control','spectral')){Train-Arm $mode -Smoke}
 Invoke-Worker (@('-u',"$tools\train_spectral_context_fit.py",'audit','--smoke','--device','cpu')+$common)
 $smoke=Get-Content "$out\smoke_audit.json" -Raw|ConvertFrom-Json
 if(-not $smoke.audit_verified -or -not $smoke.smoke){throw 'Actual20-step fixed-decoder numeric audit required'}
 foreach($mode in @('control','spectral')){Train-Arm $mode}
 Invoke-Worker (@('-u',"$tools\train_spectral_context_fit.py",'audit','--device','cpu')+$common)
 $final=Get-Content "$out\complete.json" -Raw|ConvertFrom-Json
 if(-not $final.complete -or -not $final.audit_verified -or $final.smoke){throw 'Terminal saved audit absent'}
 Write-Output 'SPECTRAL_CONTEXT_ENCODER_FIT_COMPLETE'
}catch{
 if(Test-Path $out){@{failed=$true;reason=$_.Exception.Message;time_utc=(Get-Date).ToUniversalTime().ToString('o')}|ConvertTo-Json|Set-Content -Encoding UTF8 "$out\runner_failure.json"}
 throw
}finally{if((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")){Remove-Item $lease -Force}}
