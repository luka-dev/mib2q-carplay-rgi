param([switch]$SmokeOnly,[switch]$Resume,[switch]$PrioritizeReference)
$ErrorActionPreference='Stop'
$tools='D:\upmix\tools'
$runs='D:\upmix\runs'
$out="$runs\causal_reference_generalization_v1"
$initial="$runs\separator_causal_reference_v1"
$py='D:\upmix\venvs\bsrnn_reference\Scripts\python.exe'
$lease="$runs\GPU_LEASE"
$expected=@{
    'train_causal_reference_generalization.py'='43CA0739A3D110031D72648BDAF10E47CB83C1C1569026603A4B91A901A5DFFB'
    'eval_causal_reference_generalization.py'='00C7B5A5E1A98147C9A8AC3BAB8CC2B39D3D45C650604AC77AE55B2A483E39EC'
    'separator_causal_reference.py'='764A594242D26E7F75884D122B91656ADC3037B53A7C54F602B74678F58A07D0'
    'separator_causal_reference_probe.py'='E514FFF84F29B0CC52236B6F4A2095CCCD79B5C1777A77EB5F29A60347F37CCC'
    'separator_fit_probe.py'='CEB3C158D1955E3A384C1F21317D00282F50E1C52B3E56ECB698AA5C05FCEA0D'
    'sep_net.py'='11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905'
    'mask_net.py'='5CAAE687105AE492F5BBB5DE5843593882DA43C73E6CF90878BF28AF8F54BE40'
    'bsrnn_reference_eval.py'='E5DED4764041D9E5EE65FFCBF60B8FC47F2F782427A9121D8889F628ED409C51'
    'bsrnn_reference_infer.py'='1FC41E4451E212FA0FF662D699B000FF8080E06DD9B7956926B2E130E4AAA30B'
    'diag_transitions.py'='B2626F320F2E77FDC71E32F547FDD894F7E9E3F1A689F52F8CD714AFF65B62C7'
    'upmix_stem_tests.py'='184CC28B493DDE68B0106410515CE578A2719CDEEA4558D38976C625A02573FE'
    'upmix_behavior_tests.py'='C28574A4642B59FEB179F0BBD6ADAAEDA870D6FFB7F37223BF5726A40E30913D'
    'evaluate_oracle_renderer_probe.py'='1DA71A53DBB740E5EE8D032BA58C7CFE970D7A3019B59A8A7C5BC487A29BBBDE'
}
foreach($f in $expected.Keys) { if((Get-FileHash "$tools\$f" -Algorithm SHA256).Hash -ne $expected[$f]) {throw "Source changed: $f"} }
$inputs=@{
    'D:\upmix\runs\causal_reference_generalization_v1\manifest.json'='D5583A4D4B01682DBDD9BC853A5595249C838E3589EB27B7F5F60178C691D9D6'
    'D:\upmix\tools\hssr_model.bin'='96ABE57D9EE703F2E39C0231DE97580C303CE896CE044B7450C14240002B0A0E'
    'D:\upmix\tools\hssr_guard.bin'='AB01463E9359F013C4B40F311D630BD20AF48D761AD361563CB5246F6BD686AE'
    'D:\upmix\tools\direct_object_host\upmix_control.exe'='F67798D6F254DA7F7920CA47643E72E8D0804DA0FCDA410E71CFC8D13E38A563'
    'D:\upmix\tools\direct_object_host\upmix_direct_no_bed.exe'='EAF28CCFEF2D3FC30F0AC3BB2FCE440ED53308D9144DA81D89CFF377249C26A7'
    'D:\upmix\runs\direct_object_full40\manifest.json'='C0407503233C3F5E0526F8B88A3617E87E962B3763B966276206ABC979DAFEE4'
    'D:\upmix\runs\direct_object_full40\baseline\stems.csv'='D3AE0C73ECCC3938201A0271AC33EB3E97B8AB7AFF31DD93102A6339E6244C30'
}
foreach($f in $inputs.Keys) {if((Get-FileHash $f -Algorithm SHA256).Hash -ne $inputs[$f]) {throw "Frozen input changed: $f"}}

if(Test-Path "$out\complete.json") {throw 'Completed experiment must be preserved'}
$pending=@(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue | Where-Object {$_ -match '\|' -and $_ -notmatch '^\s*#'})
if($pending.Count) {throw 'Rapid queue active'}
$busy=@(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -eq 'python.exe' -or ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'rapid_daemon\.ps1')
})
if($busy.Count) {throw 'Python or rapid daemon active'}
if($PrioritizeReference) {
    if(-not $Resume -or $SmokeOnly) {throw 'Priority order requires explicit full-run resume'}
    $amendment=Get-Content "$out\priority_amendment.json" -Raw | ConvertFrom-Json
    if(-not $amendment.verified -or $amendment.protocol_sha256 -ne $inputs["$out\manifest.json"].ToLowerInvariant() -or
        (Get-FileHash "$out\priority_pause\original_last.pt" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $amendment.paused_checkpoint_sha256) {
        throw 'Verified immutable control pause record required'
    }
}
$token=[guid]::NewGuid().ToString()
$handle=[IO.File]::Open($lease,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try {$bytes=[Text.Encoding]::ASCII.GetBytes("causal_generalization $token");$handle.Write($bytes,0,$bytes.Length)} finally {$handle.Dispose()}
function Invoke-Worker([string[]]$Arguments) {
    $tag=[guid]::NewGuid().ToString()
    $stdout=Join-Path $env:TEMP ("upmix_causal_generalization_$tag.stdout.log")
    $stderr=Join-Path $env:TEMP ("upmix_causal_generalization_$tag.stderr.log")
    Write-Output "WORKER_ARGS $($Arguments -join ' ')"
    Write-Output "WORKER_STDOUT $stdout"
    Write-Output "WORKER_STDERR $stderr"
    $process=Start-Process -FilePath $py -ArgumentList $Arguments -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    # Start-Process may otherwise lose the native exit status after Refresh.
    $retainedHandle=$process.Handle
    $processId=$process.Id
    Write-Output "WORKER_PID $processId"
    $lastOutput=Get-Date
    while(-not $process.HasExited) {
        Start-Sleep -Seconds 5
        $process.Refresh()
        foreach($file in @($stdout,$stderr)) {
            $info=Get-Item $file -ErrorAction SilentlyContinue
            if($info -and $info.LastWriteTime -gt $lastOutput) {$lastOutput=$info.LastWriteTime}
        }
        if(-not $process.HasExited -and ((Get-Date)-$lastOutput).TotalMinutes -gt 20) {
            # Only this launched worker's process tree; no PC, unit or driver reset.
            & taskkill.exe /PID $processId /T /F | Out-Null
            $process.WaitForExit()
            throw "Own worker stalled: $processId; preserved logs/checkpoints"
        }
    }
    $process.WaitForExit()
    Write-Output "WORKER_EXIT $($process.ExitCode)"
    Get-Content $stdout -Tail 8 -ErrorAction SilentlyContinue
    if($null -eq $process.ExitCode -or $process.ExitCode -ne 0) {
        Get-Content $stderr -Tail 20 -ErrorAction SilentlyContinue
        throw "Worker exit $($process.ExitCode); explicit verified resume required"
    }
}
$common=@('--cache','D:\upmix\datasets\masknet_cache7','--initial',$initial,'--out',$out)
try {
    Set-Location $tools
    $env:PATH="$tools\ffmpeg\bin;"+$env:PATH
    if(-not(Test-Path "$out\smoke_audit.json")) {
        Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
        foreach($mode in @('original','residual96x4')) {
            $marker="$out\${mode}_smoke\train_complete.json"
            if(Test-Path $marker) {
                $finished=Get-Content $marker -Raw | ConvertFrom-Json
                if(-not $finished.complete -or $finished.step -ne 20 -or -not $finished.smoke -or
                    $finished.protocol_sha256 -ne $inputs["$out\manifest.json"].ToLowerInvariant()) {
                    throw 'Completed smoke semantics differ'
                }
                Write-Output "REUSE_COMPLETED_SMOKE $mode; CPU audit still required"
                continue
            }
            Invoke-Worker (@('-u',"$tools\train_causal_reference_generalization.py",'train','--mode',$mode,'--smoke')+$common)
        }
        $env:CUDA_VISIBLE_DEVICES='-1'
        Invoke-Worker (@('-u',"$tools\train_causal_reference_generalization.py",'audit','--smoke')+$common)
    }
    $smoke=Get-Content "$out\smoke_audit.json" -Raw | ConvertFrom-Json
    if(-not $smoke.verified -or $smoke.protocol_sha256 -ne $inputs["$out\manifest.json"].ToLowerInvariant()) {throw 'Smoke audit differs'}
    if($SmokeOnly) {Write-Output 'SMOKE_AND_CPU_AUDIT_COMPLETE';return}
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    $modeOrder=if($PrioritizeReference) {@('residual96x4','original')} else {@('original','residual96x4')}
    Write-Output "FULL_ARM_ORDER $($modeOrder -join ',')"
    foreach($mode in $modeOrder) {
        $marker="$out\$mode\train_complete.json"
        if(Test-Path $marker) {
            $finished=Get-Content $marker -Raw | ConvertFrom-Json
            if(-not $finished.complete -or $finished.step -ne 40000 -or $finished.smoke) {throw 'Invalid completed arm'}
            continue
        }
        $argsForPython=@('-u',"$tools\train_causal_reference_generalization.py",'train','--mode',$mode)+$common
        if(Test-Path "$out\$mode") {
            if(-not $Resume -or -not(Test-Path "$out\$mode\last.pt")) {throw 'Partial arm requires verified resume/checkpoint'}
            $argsForPython+='--resume'
        }
        Invoke-Worker $argsForPython
    }
    $env:CUDA_VISIBLE_DEVICES='-1'
    Invoke-Worker (@('-u',"$tools\train_causal_reference_generalization.py",'audit')+$common)
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    $argsForPython=@('-u',"$tools\eval_causal_reference_generalization.py")+$common+@('--runs',$runs,'--tools',$tools,'--moisesdb','D:\upmix\datasets\moisesdb\moisesdb_v0.1')
    if(Test-Path "$out\full40\protocol.json") {
        if(-not $Resume) {throw 'Partial evaluation requires verified resume'}
        $argsForPython+='--resume'
    }
    Invoke-Worker $argsForPython
    if(-not(Test-Path "$out\complete.json")) {throw 'Terminal comparison marker absent'}
    Write-Output 'GENERALIZATION_40K_PAIR_CPU_AUDIT_AND_FULL40_COMPLETE'
} finally {
    if((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {Remove-Item $lease -Force}
}
