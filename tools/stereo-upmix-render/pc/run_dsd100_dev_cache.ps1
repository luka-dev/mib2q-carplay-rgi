# Convert validated DSD100 Dev (not Test) into the seven-slot TRAIN cache.
$ErrorActionPreference = 'Stop'
$tools = 'D:\upmix\tools'
$archive = 'D:\upmix\datasets\_downloads\DSD100.zip'
$run = 'D:\upmix\runs\dsd100_integrity'
$out = 'D:\upmix\datasets\dsd100_dev_cache7'
$validator = "$run\validation.json"
$script = "$tools\cache_dsd100_dev.py"
$scriptSha = 'D11E7BA9D4486DC7331D60E269F6E43390478C919948574092BF7D0C1B79D530'
if (-not (Test-Path $validator)) { throw 'DSD100 validation marker missing' }
$record = Get-Content $validator -Raw | ConvertFrom-Json
if ($record.zip_crc -ne 'passed' -or $record.dev_tracks -ne 50 -or $record.test_tracks -ne 50 -or
    $record.bytes -ne 15989815305L -or $record.sha256 -notmatch '^[0-9A-Fa-f]{64}$') {
    throw 'DSD100 validation marker incomplete'
}
if (-not (Test-Path $archive) -or (Get-Item $archive).Length -ne $record.bytes) {
    throw 'Validated DSD100 archive absent or wrong length'
}
if ((Get-FileHash $script -Algorithm SHA256).Hash -ne $scriptSha) {
    throw 'DSD100 Dev cache source hash changed'
}
if (Test-Path 'D:\upmix\runs\GPU_LEASE') { throw 'GPU lease active; defer disk-heavy cache' }
$busy = @(Get-CimInstance Win32_Process | Where-Object {
    ($_.Name -eq 'python.exe' -and $_.CommandLine -match 'sep_net.py train|upmix_stem_tests') -or
    ($_.Name -eq 'curl.exe' -and $_.CommandLine -match 'DSD100\.zip')
})
if ($busy.Count -ne 0) { throw 'Trainer, stem harness or downloader is active' }
if (Test-Path "$run\cache_complete.json") { throw 'DSD100 Dev cache already marked complete' }
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & python $script --archive $archive --archive-sha256 $record.sha256 `
        --out $out --resume 2>&1 | Tee-Object -FilePath "$run\cache_dev.log"
    $exit = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
if ($exit -ne 0) { throw "DSD100 Dev cache failed: exit=$exit" }
$splitPath = "$out\split.json"
if (-not (Test-Path $splitPath)) { throw 'DSD100 Dev split.json missing after cache' }
$split = Get-Content $splitPath -Raw | ConvertFrom-Json
$ids = @($split.split.PSObject.Properties.Name)
if ($ids.Count -ne 50 -or @($ids | Where-Object { $split.split.$_ -ne 'train' }).Count -ne 0 -or
    $split.archive_sha256 -ne $record.sha256.ToLower() -or
    @(Get-ChildItem $out -Filter '*.npy').Count -ne 50) {
    throw 'DSD100 Dev cache split/count/hash validation failed'
}
$metas = @($ids | ForEach-Object { Get-Content "$out\$_.json" -Raw | ConvertFrom-Json })
if (@($metas | Where-Object { $_.published_split -ne 'Dev' -or $_.cache_split -ne 'train' -or
    $_.seconds -lt 60 -or $_.master_residual_db -gt -20 }).Count -ne 0) {
    throw 'DSD100 Dev cache per-song audit failed'
}
$done = [pscustomobject]@{
    archive_sha256 = $record.sha256
    cache_script_sha256 = $scriptSha
    split_sha256 = (Get-FileHash $splitPath -Algorithm SHA256).Hash
    tracks = 50
    min_seconds = ($metas | Measure-Object seconds -Minimum).Minimum
    worst_master_residual_db = ($metas | Measure-Object master_residual_db -Maximum).Maximum
    completed_at = (Get-Date).ToString('o')
    test_use = 'No DSD100 Test audio decoded or cached'
}
$tmp = "$run\cache_complete.json.new"
[System.IO.File]::WriteAllText($tmp, ($done | ConvertTo-Json -Depth 4),
    (New-Object System.Text.UTF8Encoding($false)))
Move-Item $tmp "$run\cache_complete.json"
Write-Output "DSD100 Dev cache validated: 50 TRAIN, worst master residual $($done.worst_master_residual_db) dB"
