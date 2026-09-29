# Minimal process/queue snapshot; does not print command lines or credentials.
$runs = 'D:\upmix\runs'
Get-CimInstance Win32_Process | Where-Object {
    $_.Name -in @('powershell.exe', 'python.exe') -and
    $_.CommandLine -match 'rapid_daemon\.ps1|sep_net\.py train|upmix_stem_tests'
} | ForEach-Object {
    $kind = if ($_.CommandLine -match 'rapid_daemon\.ps1') { 'rapid_daemon' }
            elseif ($_.CommandLine -match 'sep_net\.py train') { 'separator_train' }
            else { 'stem_harness' }
    "$kind PID=$($_.ProcessId) parent=$($_.ParentProcessId)"
}
if (Test-Path "$runs\GPU_LEASE") {
    "LEASE=" + (Get-Content "$runs\GPU_LEASE" -Raw).Trim()
} else { 'LEASE=absent' }
$jobs = @(Get-Content "$runs\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
"QUEUE=$($jobs.Count)"
foreach ($tag in @('cambridge3_ctrl_s1', 'cambridge3_add_s1')) {
    $dir = "$runs\rapid\$tag"
    "${tag}: folder=$(Test-Path $dir) step4000=$(Test-Path "$dir\a\step4000.pt") done=$(Test-Path "$dir\a\train_done.txt")"
    if (Test-Path "$dir\train.log") { Get-Content "$dir\train.log" -Tail 3 }
}
