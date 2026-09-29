# Download the official DSD100 archive at a low rate while GPU training uses D:.
# Keep the .part file until size and ZIP CRC checks are completed separately.
$ErrorActionPreference = 'Stop'
$out = 'D:\upmix\datasets\_downloads\DSD100.zip.part'
$log = 'D:\upmix\datasets\_downloads\dsd100-download.log'
$expected = 15989815305L
$free = (Get-PSDrive D).Free
if ($free -lt 50000000000L) { throw 'Insufficient free space on D:' }
if ((Test-Path $out) -and (Get-Item $out).Length -gt $expected) {
    throw 'Partial archive exceeds official Content-Length'
}
$active = @(Get-CimInstance Win32_Process -Filter "name='curl.exe'" | Where-Object {
    $_.CommandLine -match 'DSD100\.zip'
})
if ($active.Count -ne 0) { throw 'DSD100 download is already active' }
if ((Test-Path $out) -and (Get-Item $out).Length -eq $expected) {
    Write-Output 'Archive bytes already complete; run ZIP CRC validation'
    exit 0
}
$url = 'http://liutkus.net/DSD100.zip'
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & curl.exe --fail --location --retry 10 --retry-delay 10 --connect-timeout 20 `
        --continue-at - --limit-rate 5M --output $out $url 2>&1 |
        Tee-Object -FilePath $log
    $exit = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
if ($exit -ne 0) { throw "DSD100 download failed: curl exit=$exit" }
$got = (Get-Item $out).Length
if ($got -ne $expected) { throw "Unexpected DSD100 archive size: $got" }
Write-Output "DSD100 bytes complete: $got; ZIP CRC still pending"
