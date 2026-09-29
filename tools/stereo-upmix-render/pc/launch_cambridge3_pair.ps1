# Guard and enqueue the frozen Cambridge three-song matched 4k rapid pair.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$runs = 'D:\upmix\runs'
$rapid = "$runs\rapid"
$queue = "$rapid\QUEUE.txt"
$lease = "$runs\GPU_LEASE"
$sourceHashes = @(
    @("$tools\sep_net.py", '11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905'),
    @("$tools\rapid_daemon.ps1", 'BE2094DD0C2BC43B3FFAAB88B71B6F432438775F35F90B5569929F685A884FB3'),
    @("$tools\upmix_render_inj.exe", '730A2CA9AF8BF84DB8B8AC9B4ABACADEFF70190ABB1FC7B78F0536012BB74520'),
    @('D:\upmix\datasets\cambridge_trial_cache7\split.json', '484DB95F51E9C6C1079F8A9339D752D0A097032A99519F45E3AB2CF9CDC090CA'),
    @('D:\upmix\datasets\cambridge_moises_control3\split.json', '338FF50105BE8ABC8722084ABECF449050276914CACA117B8ED7BBBB9C6D1022')
)
foreach ($pair in $sourceHashes) {
    if ((Get-FileHash $pair[0] -Algorithm SHA256).Hash -ne $pair[1]) {
        throw "Frozen file changed: $($pair[0])"
    }
}
foreach ($tag in @('control', 'candidate')) {
    $dir = "$runs\cambridge3_smoke\$tag"
    if (-not (Test-Path "$dir\train_done.txt") -or -not (Test-Path "$dir\step20.pt")) {
        throw "$tag smoke did not complete"
    }
}
if ((Test-Path "$rapid\cambridge3_ctrl_s1") -or (Test-Path "$rapid\cambridge3_add_s1")) {
    throw 'Cambridge rapid tag already exists'
}
$queued = @(Get-Content $queue -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($queued.Count -ne 0) { throw 'Rapid queue is not empty' }
$daemon = @(Get-CimInstance Win32_Process -Filter "name='powershell.exe'" | Where-Object {
    $_.CommandLine -match 'rapid_daemon\.ps1'
})
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests|diag_gap.py|diag_transitions.py'
})
if ($daemon.Count -ne 0 -or $busy.Count -ne 0 -or (Test-Path $lease)) {
    throw 'Rapid daemon, trainer, harness, or GPU lease is active'
}
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes("rapid cambridge3 launch $token")
    $handle.Write($bytes, 0, $bytes.Length)
} finally { $handle.Dispose() }
$lines = @(
    'cambridge3_ctrl_s1|--remix 0.5 --extra-truth D:\upmix\datasets\cambridge_moises_control3 --extra-truth-frac 0.5',
    'cambridge3_add_s1|--remix 0.5 --extra-truth D:\upmix\datasets\cambridge_trial_cache7 --extra-truth-frac 0.5'
)
try {
    [System.IO.File]::WriteAllLines("$queue.new", $lines)
    Move-Item "$queue.new" $queue -Force
} catch {
    if ((Test-Path $queue) -and ((Get-Content $queue -Raw) -like '*cambridge3_ctrl_s1*')) {
        [System.IO.File]::WriteAllText($queue, '')
    }
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
    throw
}
Write-Output 'ENQUEUED Cambridge3 pair; running daemon in this persistent session'
& powershell.exe -NoProfile -File "$tools\rapid_daemon.ps1"
throw 'Rapid daemon returned; inspect process, queue, lease and job logs before recovery'
