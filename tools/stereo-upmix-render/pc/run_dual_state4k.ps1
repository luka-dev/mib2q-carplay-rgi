# Review-35 matched persistent-vs-reset dual-state training, one GPU tenant.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$C = 'D:\upmix\datasets\masknet_cache7'
$TC = 'D:\upmix\datasets\teacher_cache4'
$R = 'D:\upmix\runs'
$O = "$R\dual_state4k"
$lease = "$R\GPU_LEASE"
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
if (Test-Path $lease) { throw 'GPU lease is active' }
$token = [guid]::NewGuid().ToString()
$handle = [System.IO.File]::Open($lease, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    $data = [System.Text.Encoding]::ASCII.GetBytes("dual_state4k $token")
    $handle.Write($data, 0, $data.Length)
} finally { $handle.Dispose() }
try {
    $busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
        $_.CommandLine -match 'sep_net.py train|sep_net_state.py|upmix_stem_tests|oracle_renderer_sweep.py'
    })
    if ($busy.Count -ne 0) { throw 'Training or renderer harness is active' }
    $hashes = @{
        'sep_net_state.py' = '16B5029B2B24D88E0267D25D0B4E956765C5C731C91846BC2124839E190E3EEC'
        'sep_net.py' = '31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7'
    }
    foreach ($name in $hashes.Keys) {
        if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
            throw "PC source hash changed: $name"
        }
    }
    if ((Test-Path "$O\reset2\train_done.txt") -and
        (Test-Path "$O\persistent\train_done.txt")) {
        throw 'Both dual-state arms already finished; preserving them'
    }
    $env:PATH = "$T\ffmpeg\bin;" + $env:PATH
    Remove-Item Env:CUDA_VISIBLE_DEVICES -ErrorAction SilentlyContinue
    Set-Location $T
    foreach ($mode in @('reset2', 'persistent')) {
        $arm = "$O\$mode"
        if (Test-Path "$arm\train_done.txt") {
            Write-Output "$mode already complete; preserving checkpoint"
            continue
        }
        New-Item -ItemType Directory -Force $arm | Out-Null
        $cfg = @{
            state_mode = $mode
            arch = 'band'; mask = 'matrix2'; norm = 'stream'; dec_skip = $true
            feat = 36; h1 = 24; h2 = 24; bhg = 24; band_rnn = 0
            lookahead = 0; bidir = $false; ht = 16; hg = 64; dec = 24
            source_attr = 0.0734755788; role_ce = 0.279254
            attr = 0.0; role_focus = 0.0; vocal_invariance = 0.0; vocal_entry_target = 0.0
            cache = $C; teacher_cache = $TC; half_half = $true; truth_frac = 0.5
            out = $arm; steps = 4000; batch = 6; seconds = 16.0; accum = 1
            eval_every = 1000; save_at = @(4000); lr = 0.0003; warmup = 2000; seed = 1
            loss = 'lin'; loss_floor = 0.003; loss_scale = 1.0; bandbal = 0.0
            sisdr_after = 0; init = $null
        }
        if (Test-Path "$arm\last.pt") { $cfg.init = "$arm\last.pt" }
        $cfg | ConvertTo-Json -Depth 4 | Set-Content "$arm\config.json" -Encoding Ascii
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & python "$T\sep_net_state.py" --config "$arm\config.json" 2>&1 |
                Tee-Object -FilePath "$arm\train.log"
            $exit = $LASTEXITCODE
        } finally { $ErrorActionPreference = $previousPreference }
        if ($exit -ne 0 -or -not (Test-Path "$arm\train_done.txt") -or
            -not (Test-Path "$arm\step4000.pt")) {
            throw "$mode training failed or did not reach step 4000"
        }
        Write-Output "$mode complete"
    }
} finally {
    if ((Test-Path $lease) -and ((Get-Content $lease -Raw) -like "*$token*")) {
        Remove-Item $lease -Force
    }
}
