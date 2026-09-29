# Download a small, explicitly selected multitrack trial. The owner states
# that licensing is obtained; the permission record will be added to the repo.
# Files remain .part until length and ZIP CRC validation by the cache builder.
$ErrorActionPreference = 'Stop'
$outDir = 'D:\upmix\datasets\_downloads\cambridge_trial'
New-Item -ItemType Directory -Force $outDir | Out-Null
if ((Get-PSDrive D).Free -lt 10000000000L) { throw 'Insufficient free space on D:' }
$items = @(
    @{ id = 'ajay_ae_mere_humsafar'; bytes = 398950192L;
       url = 'https://mtkdata.cambridgemusictechnology.co.uk/MTK016/AjayNagarkotiAndVibhuDixit_AeMereHumsafar_Full.zip' },
    @{ id = 'finlay_same_kind_of_life'; bytes = 347121142L;
       url = 'https://mtkdata.cambridgemusictechnology.co.uk/MTK016/Finlay_SameKindOfLife_Full.zip' },
    @{ id = 'killa_l_window'; bytes = 404201579L;
       url = 'https://mtkdata.cambridgemusictechnology.co.uk/MTK016/KillaL_Window_Full.zip' }
)
foreach ($item in $items) {
    $path = Join-Path $outDir ($item.id + '.zip.part')
    if ((Test-Path $path) -and (Get-Item $path).Length -gt $item.bytes) {
        throw "Oversize partial archive: $($item.id)"
    }
    if ((Test-Path $path) -and (Get-Item $path).Length -eq $item.bytes) {
        Write-Output "$($item.id) bytes already complete"
        continue
    }
    $oldPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & curl.exe --fail --location --retry 8 --retry-delay 5 --connect-timeout 20 `
            --user-agent 'Mozilla/5.0' --continue-at - --limit-rate 2M `
            --output $path $item.url 2>&1 |
            Tee-Object -FilePath (Join-Path $outDir ($item.id + '.log'))
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $oldPreference }
    if ($exit -ne 0) { throw "$($item.id) curl failed: $exit" }
    $got = (Get-Item $path).Length
    if ($got -ne $item.bytes) { throw "$($item.id) size $got != $($item.bytes)" }
    Write-Output "$($item.id) bytes complete; ZIP CRC pending"
}
