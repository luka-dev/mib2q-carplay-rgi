param([switch]$Resume)
$ErrorActionPreference='Stop'
$tools='D:\upmix\tools'
$runs='D:\upmix\runs'
$out="$runs\unified_upmix_fit20k_v1"
$initial="$runs\separator_causal_reference_v1"
$parent="$runs\unified_upmix_fit_v1"
$py='D:\upmix\venvs\bsrnn_reference\Scripts\python.exe'
$lease="$runs\GPU_LEASE"
$expected=@{
    'unified_upmix_net.py'='C7AA0ABC74DDA830A5C306A5F61C30AE95ECE5F8BDF61CD693EE4DB57FF3FC46'
    'train_unified_upmix_fit.py'='8B777F2ED4914B8184E94F728D47C98F08550F04E69E98D18DCC85746A86BB78'
    'extend_unified_upmix_fit.py'='71C1EBBFD49EBC84F7BDF4BE6C482D05BEEB0D770E481D991304E1C3963ECF30'
    'unified_upmix_audio.py'='0692531B4290D12ADB114648FBD072F1CC781A36CE03FE8510816E9F67A051B2'
    'audit_unified_upmix_fit.py'='46B8D80F3F84F51F2DAA89393357E838E361766F767BE7F8C37BA48A011A0574'
    'separator_causal_reference_probe.py'='E514FFF84F29B0CC52236B6F4A2095CCCD79B5C1777A77EB5F29A60347F37CCC'
    'separator_fit_probe.py'='CEB3C158D1955E3A384C1F21317D00282F50E1C52B3E56ECB698AA5C05FCEA0D'
    'sep_net.py'='11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905'
    'mask_net.py'='5CAAE687105AE492F5BBB5DE5843593882DA43C73E6CF90878BF28AF8F54BE40'
}
foreach($file in $expected.Keys) {
    if((Get-FileHash "$tools\$file" -Algorithm SHA256).Hash -ne $expected[$file]) {throw "Frozen source changed: $file"}
}
if((Get-FileHash "$initial\manifest.json" -Algorithm SHA256).Hash -ne '95EDC076B2BF10FB0019B440DA147D64046DCEA16128BB038F4022C6B72BBC99') {throw 'Frozen initial fixtures changed'}
if((Get-FileHash "$parent\audited.json" -Algorithm SHA256).Hash -ne '8E0B3D427BB36CEC5DF9256CDE00D16265F71F33249EB86835E8E3414284C15B') {throw 'Verified parent audit differs'}
if(Test-Path "$out\complete.json") {throw 'Preserve completed pair'}
if((Test-Path $out) -and -not $Resume) {throw 'Partial run requires explicit verified resume'}
$pending=@(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object {$_ -match '\|' -and $_ -notmatch '^\s*#'})
if($pending.Count) {throw 'Rapid queue active'}
$busy=@(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -eq 'python.exe' -or ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')
})
if($busy.Count) {throw 'Python or rapid daemon active'}
$token=[guid]::NewGuid().ToString()
$handle=[IO.File]::Open($lease,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try {$bytes=[Text.Encoding]::ASCII.GetBytes("unified_upmix_fit20k_v1 $token");$handle.Write($bytes,0,$bytes.Length)} finally {$handle.Dispose()}
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
$common=@('--cache','D:\upmix\datasets\masknet_cache7','--initial',$initial,'--parent',$parent,'--out',$out)
try {
    Set-Location $tools
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    if(-not (Test-Path "$out\prepared.json")) {
        Invoke-Worker (@('-u',"$tools\extend_unified_upmix_fit.py",'prepare')+$common)
    }
    foreach($mode in @('eligible_only','joint_audio')) {
        $marker="$out\$mode\complete.json"
        if(Test-Path $marker) {
            $finished=Get-Content $marker -Raw | ConvertFrom-Json
            if(-not $finished.complete -or $finished.step -ne 20000 -or $finished.mode -ne $mode -or
                (Get-FileHash "$out\$mode\last.pt" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $finished.checkpoint_sha256) {throw 'Invalid terminal arm'}
            continue
        }
        $arguments=@('-u',"$tools\extend_unified_upmix_fit.py",'train','--mode',$mode)+$common
        if(Test-Path "$out\$mode") {
            if(-not $Resume -or -not (Test-Path "$out\$mode\last.pt")) {throw 'Partial arm needs checkpoint and verified resume'}
            $arguments+='--resume'
        }
        Invoke-Worker $arguments
    }
    Invoke-Worker (@('-u',"$tools\extend_unified_upmix_fit.py",'audit')+$common)
    $final=Get-Content "$out\complete.json" -Raw | ConvertFrom-Json
    if(-not $final.complete -or -not $final.audit_verified) {throw 'Terminal CPU audit absent'}
    Write-Output "UNIFIED_FIT_PAIR_COMPLETE gate=$($final.fitting_gate_pass)"
} catch {
    if(Test-Path $out) {
        @{failed=$true;reason=$_.Exception.Message;time_utc=(Get-Date).ToUniversalTime().ToString('o')} | ConvertTo-Json | Set-Content -Encoding UTF8 "$out\runner_failure.json"
    }
    throw
} finally {
    if((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {Remove-Item $lease -Force}
}
