# Review-43 fine-role cross-song true-stem 4k train; one GPU tenant.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$C = 'D:\upmix\datasets\masknet_cache7'
$TC = 'D:\upmix\datasets\teacher_cache4'
$R = 'D:\upmix\runs'
$O = "$R\crosssong4k\fine"
$baseline = "$R\fine_role4k\fine\step4000.pt"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("crosssong4k $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|sep_net_state.py|sep_net_fine_role.py|sep_net_operator_anchor.py|sep_net_crosssong.py|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
    $hashes = @{
        'sep_net_crosssong.py' = '40A59A1FDE2ECAB59ECE41605AAFE30C89E59356DD3916A55D89A652E55AC888'
        'sep_net_fine_role.py' = 'B2D9CC188A6ABF112A6D4D09385AD2947E6FA939036872F060BAECB4198BADB6'
        'sep_net_state.py' = '16B5029B2B24D88E0267D25D0B4E956765C5C731C91846BC2124839E190E3EEC'
        'sep_net.py' = '31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7'
        'mask_net.py' = '5CAAE687105AE492F5BBB5DE5843593882DA43C73E6CF90878BF28AF8F54BE40'
    }
    foreach ($name in $hashes.Keys) {
        if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
            throw "PC source hash changed: $name"
        }
    }
    if ((Get-FileHash $baseline -Algorithm SHA256).Hash -ne
        '826061FD92AE515FD0C55798DB848C98831D7DF3DB15CA1B4E562DE73B381F29') {
        throw 'Baseline checkpoint changed'
    }
    if (Test-Path "$O\train_done.txt") {
        throw 'Cross-song arm already finished; preserving checkpoint'
    }
    $split = Get-Content "$C\split.json" -Raw | ConvertFrom-Json
    $truth = @($split.split.PSObject.Properties | Where-Object {
        $_.Value -eq 'train' -and (Test-Path "$C\$($_.Name).npy") })
    if ($truth.Count -ne 176) { throw "Unexpected MoisesDB TRAIN cache coverage: $($truth.Count)" }
    New-Item -ItemType Directory -Force $O | Out-Null
    $cfg = @{
        role_mode = 'fine'
        arch = 'band'; mask = 'matrix2'; norm = 'stream'; dec_skip = $true
        feat = 36; h1 = 24; h2 = 24; bhg = 24; band_rnn = 0
        lookahead = 0; bidir = $false; ht = 16; hg = 64; dec = 24
        source_attr = 0.0734755788; role_ce = 0.279254
        attr = 0.0; role_focus = 0.0; vocal_invariance = 0.0; vocal_entry_target = 0.0
        cache = $C; teacher_cache = $TC; half_half = $true; truth_frac = 0.5
        out = $O; steps = 4000; batch = 6; seconds = 16.0; accum = 1
        eval_every = 1000; save_at = @(4000); lr = 0.0003; warmup = 2000; seed = 1
        loss = 'lin'; loss_floor = 0.003; loss_scale = 1.0; bandbal = 0.0
        sisdr_after = 0; init = $null
    }
    if (Test-Path "$O\last.pt") { $cfg.init = "$O\last.pt" }
    $cfg | ConvertTo-Json -Depth 4 | Set-Content "$O\config.json" -Encoding Ascii
    $env:PATH = "$T\ffmpeg\bin;" + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & python "$T\sep_net_crosssong.py" --config "$O\config.json" 2>&1 |
            Tee-Object -FilePath "$O\train.log"
        $exit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or -not (Test-Path "$O\train_done.txt") -or
        -not (Test-Path "$O\step4000.pt")) {
        throw 'Cross-song training failed or did not reach step 4000'
    }
    Write-Output 'cross-song fine-role arm complete'
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
