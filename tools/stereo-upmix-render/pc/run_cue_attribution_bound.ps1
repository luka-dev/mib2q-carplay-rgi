# CPU-only privileged source-attribution ablation of the frozen fine-role cue.
$ErrorActionPreference = 'Stop'
$T = 'D:\upmix\tools'
$R = 'D:\upmix\runs'
$C = 'D:\upmix\datasets\masknet_cache7'
$K = "$R\fine_role4k\fine\step4000.pt"
$O = "$R\cue_attribution_bound"
if (Test-Path "$R\GPU_LEASE") { throw 'GPU lease is active' }
$pending = @(Get-Content "$R\rapid\QUEUE.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '\|' -and $_ -notmatch '^\s*#' })
if ($pending.Count -ne 0) { throw "Rapid queue has $($pending.Count) jobs" }
$busy = @(Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object {
    $_.CommandLine -match 'sep_net.py train|sep_net_state.py|sep_net_fine_role.py|upmix_stem_tests|oracle_renderer_sweep.py|diag_cue_attribution_bound.py'
})
if ($busy.Count -ne 0) { throw 'Training or cue-attribution diagnostic is active' }
$hashes = @{
    'diag_cue_attribution_bound.py' = 'B0F4B9120F6009CD367736BAF4ABA670C64A674F15F1EA2FC5EC8331CA391B0F'
    'diag_spatial_overlap_bound.py' = '14308ACF2270C559D804C41B74BD097208D89E6904C12FF70B2284A994090FDD'
    'diag_spatial_oracle.py' = 'AB9F6B6B5FA5BF00FF96758C3CF4DDBA99DEE5A38E764A2A106A183547546DB3'
    'diag_irm_overlap_bound.py' = '5FDFE80322BDA9F6C7A4120E05A170DA456E647C90233D6F3E2C73224049D90E'
    'sep_net_fine_role.py' = 'B2D9CC188A6ABF112A6D4D09385AD2947E6FA939036872F060BAECB4198BADB6'
    'sep_net_state.py' = '16B5029B2B24D88E0267D25D0B4E956765C5C731C91846BC2124839E190E3EEC'
    'sep_net.py' = '31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7'
    'mask_net.py' = '5CAAE687105AE492F5BBB5DE5843593882DA43C73E6CF90878BF28AF8F54BE40'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash "$T\$name" -Algorithm SHA256).Hash -ne $hashes[$name]) {
        throw "PC diagnostic source hash changed: $name"
    }
}
if ((Get-FileHash $K -Algorithm SHA256).Hash -ne
    '826061FD92AE515FD0C55798DB848C98831D7DF3DB15CA1B4E562DE73B381F29') {
    throw 'Fine-role checkpoint changed'
}
New-Item -ItemType Directory -Force $O | Out-Null
if (Test-Path "$O\result.json") { throw 'Cue-attribution bound already finished; preserving it' }
$env:CUDA_VISIBLE_DEVICES = '-1'
$env:PYTHONWARNINGS = 'ignore:Mean of empty slice:RuntimeWarning'
Set-Location $T
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    python "$T\diag_cue_attribution_bound.py" $C $K --out "$O\result.json" > "$O\run.txt" 2>&1
    $exit = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
if ($exit -ne 0) { Get-Content "$O\run.txt" -Tail 60; throw 'Cue-attribution bound failed' }
Get-Content "$O\run.txt" -Tail 2
