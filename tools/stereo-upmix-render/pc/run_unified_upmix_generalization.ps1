param([switch]$Resume)
$ErrorActionPreference='Stop'
$tools='D:\upmix\tools'
$runs='D:\upmix\runs'
$out="$runs\unified_source_generalization_v2"
$initial="$runs\unified_upmix_fit20k_v1"
$py='D:\upmix\venvs\bsrnn_reference\Scripts\python.exe'
$lease="$runs\GPU_LEASE"
$partition="$tools\UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30_candidate_holdout.json"
$monitor="$runs\causal_reference_generalization_v1\manifest.json"
$expected=@{
    'train_unified_upmix_generalization.py'='921dd4a3edae2e9d7ed612d69777f61fc4a9d01ec66d166ba9e3a6f144e26a2d'
    'eval_unified_upmix_generalization.py'='09367324d9642466ab18305865ebdb140f6dd5d52dda2b89b0736b946d184b3c'
    'unified_upmix_net.py'='c7aa0abc74dda830a5c306a5f61c30ae95ece5f8bdf61cd693ee4db57ff3fc46'
    'unified_upmix_stream.py'='590eee8e93dc0eb29b757f64490fd9854910076a3ae28a86dbe1285eb056e098'
    'unified_upmix_audio.py'='0692531b4290d12adb114648fbd072f1cc781a36ce03fe8510816e9f67a051b2'
    'train_unified_upmix_fit.py'='8b777f2ed4914b8184e94f728d47c98f08550f04e69e98d18dcc85746a86bb78'
    'train_causal_reference_generalization.py'='43ca0739a3d110031d72648bdaf10e47cb83c1c1569026603a4b91a901a5dffb'
    'separator_causal_reference_probe.py'='e514fff84f29b0cc52236b6f4a2095cccd79b5c1777a77eb5f29a60347f37ccc'
    'separator_fit_probe.py'='ceb3c158d1955e3a384c1f21317d00282f50e1c52b3e56ecb698aa5c05fcea0d'
    'sep_net.py'='11e0c4fe89c82d3be3814e5d2a421c5fd73c40d4e3fa505904c57ae1d7dba905'
    'mask_net.py'='5caae687105ae492f5bbb5de5843593882da43c73e6cf90878bf28af8f54be40'
}
foreach($file in $expected.Keys) {
    if((Get-FileHash "$tools\$file" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected[$file]) {throw "Frozen source changed: $file"}
}
if((Get-FileHash $partition -Algorithm SHA256).Hash.ToLowerInvariant() -ne '7a887b1b9f51080abe51adaddabd98cc45a3c77d0e9715951af609e2c1bbd66f') {throw 'Frozen partition differs'}
$fit=Get-Content "$initial\complete.json" -Raw | ConvertFrom-Json
if(-not $fit.complete -or -not $fit.audit_verified -or -not $fit.fitting_gate_pass) {throw 'Passed audited20k prerequisite required'}
if(Test-Path "$out\full40\complete.json") {throw 'Preserve completed full40 pair'}
if((Test-Path $out) -and -not $Resume) {throw 'Partial experiment requires verified resume'}
$pending=@(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object {$_ -match '\|' -and $_ -notmatch '^\s*#'})
if($pending.Count) {throw 'Rapid queue active'}
$busy=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'python.exe' -or ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1') })
if($busy.Count) {throw 'Python/rapid worker active'}
$token=[guid]::NewGuid().ToString()
$handle=[IO.File]::Open($lease,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try {$bytes=[Text.Encoding]::ASCII.GetBytes("unified_source_generalization_v2 $token");$handle.Write($bytes,0,$bytes.Length)} finally {$handle.Dispose()}
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

$common=@('--cache','D:\upmix\datasets\masknet_cache7','--initial',$initial,'--partition',$partition,'--monitor',$monitor,'--out',$out)
function Invoke-TrainArm([string]$Mode,[switch]$Smoke) {
    $suffix=if($Smoke){'_smoke'}else{''}
    $folder="$out\$Mode$suffix"
    if(Test-Path "$folder\train_complete.json") {
        $f=Get-Content "$folder\train_complete.json" -Raw | ConvertFrom-Json
        $steps=if($Smoke){20}else{40000}
        if(-not $f.complete -or $f.step -ne $steps -or $f.mode -ne $Mode -or $f.smoke -ne [bool]$Smoke -or
            (Get-FileHash "$folder\step$steps.pt" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $f.checkpoint_sha256) {throw 'Invalid terminal training arm'}
        return
    }
    $arguments=@('-u',"$tools\train_unified_upmix_generalization.py",'train','--mode',$Mode)+$common
    if($Smoke) {$arguments+='--smoke'}
    if(Test-Path $folder) {
        if(-not $Resume -or -not (Test-Path "$folder\last.pt")) {throw 'Own partial arm needs verified resume'}
        $arguments+='--resume'
    }
    Invoke-Worker $arguments
}
try {
    Set-Location $tools
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    if(-not (Test-Path "$out\manifest.json")) {Invoke-Worker (@('-u',"$tools\train_unified_upmix_generalization.py",'prepare')+$common)}
    foreach($mode in @('eligible_only','joint_audio')) {Invoke-TrainArm $mode -Smoke}
    Invoke-Worker (@('-u',"$tools\train_unified_upmix_generalization.py",'audit','--smoke')+$common)
    $smoke=Get-Content "$out\smoke_audit.json" -Raw | ConvertFrom-Json
    if(-not $smoke.verified -or -not $smoke.smoke) {throw 'TRAIN20-step execution audit required'}
    foreach($mode in @('eligible_only','joint_audio')) {Invoke-TrainArm $mode}
    Invoke-Worker (@('-u',"$tools\train_unified_upmix_generalization.py",'audit')+$common)
    $audit=Get-Content "$out\audit.json" -Raw | ConvertFrom-Json
    if(-not $audit.verified -or $audit.smoke) {throw 'Terminal checkpoint audit required'}
    $arguments=@('-u',"$tools\eval_unified_upmix_generalization.py")+$common+@('--tools',$tools,'--runs',$runs,'--moisesdb','D:\upmix\datasets\moisesdb')
    if(Test-Path "$out\full40\protocol.json") {
        if(-not $Resume) {throw 'Own partial C evaluation requires verified resume'}
        $arguments+='--resume'
    }
    Invoke-Worker $arguments
    $final=Get-Content "$out\full40\complete.json" -Raw | ConvertFrom-Json
    if(-not $final.complete) {throw 'Full40 terminal marker absent'}
    Write-Output 'UNIFIED_BROAD_AND_REAL_C_COMPLETE'
} catch {
    if(Test-Path $out) {@{failed=$true;reason=$_.Exception.Message;time_utc=(Get-Date).ToUniversalTime().ToString('o')} | ConvertTo-Json | Set-Content -Encoding UTF8 "$out\runner_failure.json"}
    throw
} finally {
    if((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {Remove-Item $lease -Force}
}
