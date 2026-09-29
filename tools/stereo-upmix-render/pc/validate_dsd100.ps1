# Validate the official DSD100 archive after the resumable download ends.
# Reads Test ZIP members for CRC only; no audio decoding or model evaluation.
$ErrorActionPreference = 'Stop'
$part = 'D:\upmix\datasets\_downloads\DSD100.zip.part'
$archive = 'D:\upmix\datasets\_downloads\DSD100.zip'
$reportDir = 'D:\upmix\runs\dsd100_integrity'
$expected = 15989815305L
$inspector = 'D:\upmix\tools\inspect_dsd100.py'
$inspectorSha = 'E81FD9E400F0C021E38CC4ED54E0C2DD053AC9DCE98A5EF83CBB48DE23CD1A70'
if (-not (Test-Path $part) -or (Get-Item $part).Length -ne $expected) {
    throw 'DSD100 .part is absent or does not have the official complete byte length'
}
if (Test-Path $archive) { throw 'Validated DSD100.zip already exists' }
if ((Get-FileHash $inspector -Algorithm SHA256).Hash -ne $inspectorSha) {
    throw 'DSD100 inspector source hash changed'
}
$downloaders = @(Get-CimInstance Win32_Process | Where-Object {
    ($_.Name -eq 'curl.exe' -and $_.CommandLine -match 'DSD100\.zip') -or
    ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'download_dsd100\.ps1')
})
if ($downloaders.Count -ne 0) { throw 'DSD100 downloader is still running' }
if (Test-Path 'D:\upmix\runs\GPU_LEASE') { throw 'GPU lease active; defer disk-heavy ZIP audit' }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|upmix_stem_tests'
})
if ($busy.Count -ne 0) { throw 'Trainer or stem harness active' }
New-Item -ItemType Directory -Force $reportDir | Out-Null
if (Test-Path "$reportDir\validation.json") { throw 'DSD100 validation marker already exists' }
$inventory = "$reportDir\inventory.json"
& python $inspector $part --moises-root 'D:\upmix\datasets\moisesdb\moisesdb_v0.1' `
    --teacher-cache 'D:\upmix\datasets\teacher_cache4' --crc --out $inventory
if ($LASTEXITCODE -ne 0) { throw "DSD100 ZIP inspection/CRC failed: exit=$LASTEXITCODE" }
$meta = Get-Content $inventory -Raw | ConvertFrom-Json
if ($meta.zip_crc -ne 'passed' -or $meta.tracks.dev -ne 50 -or $meta.tracks.test -ne 50 -or
    @($meta.incomplete).Count -ne 0) {
    throw 'DSD100 manifest/CRC report did not pass 50 Dev + 50 Test with all roles'
}
$sha = (Get-FileHash $part -Algorithm SHA256).Hash
$done = [pscustomobject]@{
    dataset = 'DSD100'
    bytes = $expected
    sha256 = $sha
    zip_crc = 'passed'
    dev_tracks = 50
    test_tracks = 50
    inspected_at = (Get-Date).ToString('o')
    inventory = $inventory
    test_use = 'ZIP byte-level CRC and filename inventory only; no audio decoding or scoring'
}
$tmp = "$reportDir\validation.json.new"
[System.IO.File]::WriteAllText($tmp, ($done | ConvertTo-Json -Depth 4),
    (New-Object System.Text.UTF8Encoding($false)))
try {
    Rename-Item $part -NewName 'DSD100.zip'
    Move-Item $tmp "$reportDir\validation.json"
} catch {
    if ((Test-Path $archive) -and -not (Test-Path $part)) {
        Rename-Item $archive -NewName 'DSD100.zip.part'
    }
    throw
}
Write-Output "DSD100 validated: bytes=$expected CRC=passed Dev=50 Test=50 SHA256=$sha"
