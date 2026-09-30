param([switch]$Resume)
$ErrorActionPreference='Stop'
$tools='D:\upmix\tools'
$runs='D:\upmix\runs'
$out="$runs\unified_capacity_continuation_v1"
$parent="$runs\unified_source_generalization_v2"
$expansion="$runs\unified_expansion_prepare_v1"
$initial="$runs\unified_upmix_fit20k_v1"
$py='D:\upmix\venvs\bsrnn_reference\Scripts\python.exe'
$lease="$runs\GPU_LEASE"
$partition="$tools\UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30_candidate_holdout.json"
$monitor="$runs\causal_reference_generalization_v1\manifest.json"
$moisesdb='D:\upmix\datasets\moisesdb\moisesdb_v0.1'
$expected=@{
    'expand_unified_upmix.py'='af56cd6d31d1b7875cece507a812f5c6aaa92c9ec111ecf6b961a3cf19ec514d'
    'train_unified_upmix_generalization.py'='921dd4a3edae2e9d7ed612d69777f61fc4a9d01ec66d166ba9e3a6f144e26a2d'
    'train_unified_capacity.py'='ea761cbb329adaeedd206205c8fcdbfabe0b957c26eb9b44f2644aaa3dc7692c'
    'eval_unified_capacity.py'='3a13ebf92b3d826833bcf7bfc6b941e0609e2589415fb360a1d7776b1d4e4b4d'
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
$parentResult=Get-Content "$parent\full40\complete.json" -Raw | ConvertFrom-Json
if(-not $parentResult.complete -or $parentResult.arms.joint_audio.nominal_observed_gates_pass) {throw 'Completed failing source pair required; do not bypass a passing candidate'}
$expanded=Get-Content "$expansion\complete.json" -Raw | ConvertFrom-Json
if(-not $expanded.complete -or -not $expanded.passed) {throw 'Structural function-preserving expansion required'}
if((Get-PSDrive -Name D).Free -lt 35GB) {throw 'Insufficient reserved disk for full C artifacts'}
$declared=Get-Content "$runs\direct_object_full40\manifest.json" -Raw | ConvertFrom-Json
foreach($track in $declared.song_track_ids.PSObject.Properties.Value) {
    if(-not (Test-Path "$moisesdb\$track\data.json")) {throw 'Full40 raw source preflight failed'}
}
if((Test-Path $out) -and -not $Resume) {throw 'Partial experiment requires verified resume'}
$pending=@(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object {$_ -match '\|' -and $_ -notmatch '^\s*#'})
if($pending.Count) {throw 'Rapid queue active'}
$busy=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'python.exe' -or ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1') })
if($busy.Count) {throw 'Python/rapid worker active'}
$token=[guid]::NewGuid().ToString()
$handle=[IO.File]::Open($lease,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try {$bytes=[Text.Encoding]::ASCII.GetBytes("unified_capacity_continuation_v1 $token");$handle.Write($bytes,0,$bytes.Length)} finally {$handle.Dispose()}
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

$common=@('--cache','D:\upmix\datasets\masknet_cache7','--initial',$initial,'--partition',$partition,'--monitor',$monitor,'--parent',$parent,'--expansion',$expansion,'--out',$out)
function Invoke-TrainArm([string]$Mode,[switch]$Smoke) {
    $suffix=if($Smoke){'_smoke'}else{''}
    $folder="$out\$Mode$suffix"
    if(Test-Path "$folder\train_complete.json") {
        $f=Get-Content "$folder\train_complete.json" -Raw | ConvertFrom-Json
        $steps=if($Smoke){20}else{20000}
        if(-not $f.complete -or $f.step -ne $steps -or $f.mode -ne $Mode -or $f.smoke -ne [bool]$Smoke -or
            (Get-FileHash "$folder\step$steps.pt" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $f.checkpoint_sha256) {throw 'Invalid terminal training arm'}
        return
    }
    $arguments=@('-u',"$tools\train_unified_capacity.py",'train','--mode',$Mode)+$common
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
    if(-not (Test-Path "$out\manifest.json")) {Invoke-Worker (@('-u',"$tools\train_unified_capacity.py",'prepare')+$common)}
    Invoke-Worker (@('-u',"$tools\train_unified_capacity.py",'initial-audit')+$common)
    $init=Get-Content "$out\initial_audit.json" -Raw | ConvertFrom-Json
    if(-not $init.verified) {throw 'Actual TRAIN initial-function equality required'}
    foreach($mode in @('control','expanded')) {Invoke-TrainArm $mode -Smoke}
    Invoke-Worker (@('-u',"$tools\train_unified_capacity.py",'audit','--smoke')+$common)
    $smoke=Get-Content "$out\smoke_audit.json" -Raw | ConvertFrom-Json
    if(-not $smoke.verified -or -not $smoke.smoke) {throw 'TRAIN20-step execution audit required'}
    foreach($mode in @('control','expanded')) {Invoke-TrainArm $mode}
    if($Resume -and (Test-Path "$out\audit.json")) {
        $saved=Get-Content "$out\audit.json" -Raw | ConvertFrom-Json
        if(-not $saved.verified -or $saved.smoke -or
            $saved.protocol_sha256 -ne (Get-FileHash "$out\manifest.json" -Algorithm SHA256).Hash.ToLowerInvariant()) {throw 'Saved terminal audit differs'}
        foreach($mode in @('control','expanded')) {
            if($saved.arms.$mode.checkpoint_sha256 -ne (Get-FileHash "$out\$mode\step20000.pt" -Algorithm SHA256).Hash.ToLowerInvariant()) {throw 'Saved audited checkpoint differs'}
        }
        if(Test-Path "$out\full40\protocol.json") {
            $oldProtocol=Get-Content "$out\full40\protocol.json" -Raw | ConvertFrom-Json
            if($oldProtocol.audit_sha256 -ne (Get-FileHash "$out\audit.json" -Algorithm SHA256).Hash.ToLowerInvariant()) {throw 'Saved evaluator audit identity differs'}
        }
        Write-Output 'REUSED_VERIFIED_TERMINAL_AUDIT'
    } else {
        Invoke-Worker (@('-u',"$tools\train_unified_capacity.py",'audit')+$common)
    }
    $audit=Get-Content "$out\audit.json" -Raw | ConvertFrom-Json
    if(-not $audit.verified -or $audit.smoke) {throw 'Terminal checkpoint audit required'}
    $arguments=@('-u',"$tools\eval_unified_capacity.py")+$common+@('--tools',$tools,'--runs',$runs,'--moisesdb',$moisesdb)
    if(Test-Path "$out\full40\protocol.json") {
        if(-not $Resume) {throw 'Own partial C evaluation requires verified resume'}
        $arguments+='--resume'
    }
    Invoke-Worker $arguments
    $final=Get-Content "$out\full40\complete.json" -Raw | ConvertFrom-Json
    if(-not $final.complete) {throw 'Full40 terminal marker absent'}
    Write-Output 'UNIFIED_CAPACITY_AND_REAL_C_COMPLETE'
} catch {
    if(Test-Path $out) {@{failed=$true;reason=$_.Exception.Message;time_utc=(Get-Date).ToUniversalTime().ToString('o')} | ConvertTo-Json | Set-Content -Encoding UTF8 "$out\runner_failure.json"}
    throw
} finally {
    if((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {Remove-Item $lease -Force}
}
