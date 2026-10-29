#!/bin/sh
# Firmware gates of the M.I.B. installer:
#  - java_jar(): E2P region US/ER/JP -> carplay_hook.jar, CN -> carplay_hook_cn.jar, else none;
#  - train_verified(): only the trains this release was verified on;
#  - read_train()/train_field(): the E2P train (modifyE2P r 3A0 19, bytes spaced or not)
#    decodes to text and splits into platform/region/brand/version;
#  - read_amp(): the amplifier coding byte (modifyE2P r FC 1) -> external / internal / unknown.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/install_MoreIncredibleBash/mod/custom.sh
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
{ echo 'set -u'
  sed -n '/^java_jar()/,/^}/p;/^train_verified()/,/^}/p;/^e2p_bytes()/,/^}/p;/^E2P_READ=/p;/^read_train()/,/^}/p;/^train_field()/,/^}/p;/^E2P_AMP_READ=/p;/^read_amp()/,/^}/p' "$SRC"
} > "$T/f.sh"
fail() { echo "FAIL ($1): $2"; exit 1; }
shells=
# read_train() decodes E2P bytes with ksh base arithmetic ($((16#4d))), which the unit's pdksh has.
# A /bin/sh without it (dash on Debian/Ubuntu) cannot run that installer function at all, so it is
# skipped here instead of failing; macOS /bin/sh (bash) and any ksh/mksh still run every case.
for s in /bin/ksh /bin/mksh "$(command -v mksh 2>/dev/null)" /bin/sh; do
    [ -x "$s" ] || continue
    [ "$("$s" -c 'echo $((16#10))' 2>/dev/null)" = 16 ] && shells="$shells $s" || echo "  (skip $s: no base#n arithmetic)" >&2
done
[ -n "$shells" ] || { echo "SKIP (no ksh-compatible shell; install ksh or mksh)"; exit 0; }
for sh in $shells; do
    for case in MHI2Q_US_AUG22_P508:carplay_hook.jar MHI2Q_ER_AUG22_P509:carplay_hook.jar MHI2Q_JP_AUG22_P101:carplay_hook.jar \
                MHI2Q_CN_AUG22_K100:carplay_hook_cn.jar MHI2Q_KR_AUG22_P500: :; do
        train=${case%%:*}; want=${case#*:}
        got=$("$sh" -c ". $T/f.sh; TRAIN=$train; java_jar")
        [ "$got" = "$want" ] || fail "$sh" "train '$train' -> '$got', want '$want'"
    done
    for case in MHI2Q_US_AUG22_P5087:y MHI2Q_ER_AUG22_P5092:y MHI2Q_ER_AUG22_P5152:y MHI2Q_JP_AUG22_P1017:y MHI2Q_CN_AUG22_K1004:y \
                MHI2Q_US_AUG22_P4246:n MHI2Q_US_AUG22_P4171:n MHI2Q_ER_AUG22_P4170:n MHI2Q_US_AUG22_P508:n \
                MHI2Q_ER_AUG22_P50921:n MHI2Q_KR_AUG22_P5000:n :n; do
        train=${case%:*}; want=${case##*:}
        got=$("$sh" -c ". $T/f.sh; TRAIN=$train; train_verified && echo y || echo n")
        [ "$got" = "$want" ] || fail "$sh" "train_verified '$train' -> '$got', want '$want'"
    done


    # modifyE2P output: address, then the bytes, spaced (1-2 lines) or run together
    printf '0x000003a0 4d 48 49 32 51 5f 43 4e 5f 41 55 47 32\n0x000003ad 32 5f 4b 31 30 30 34\n' > "$T/cn"
    printf '0x3a0: 4D484932515F55535F41554732325F5035303837\n' > "$T/us"
    printf 'E2P read\n0x000003a0 4d 48 49 32 5f 45 52 5f 56 57 47 31 33 5f 50 34 35 32 31\n' > "$T/vw"
    printf '0x000003a0 4d 48 49 32 51 5f 45 52 5f 50 4f 47 32 34 5f 50 35 30 30 30\n' > "$T/po"
    # the train padded with spaces, with 0xFF, or ended by NUL with junk behind it
    printf '0x3a0 \t4d 48 49 32 51 5f 55 53 5f 41 55 47 32 32 5f 50\n0x3b0 \t35 30 38 37 20 20 20 20 20\n' > "$T/sp"
    printf '0x3a0 \t4d 48 49 32 51 5f 55 53 5f 41 55 47 32 32 5f 50\n0x3b0 \t35 30 38 37 ff ff ff ff ff\n' > "$T/ff"
    printf '0x3a0 \t4d 48 49 32 51 5f 45 52 5f 41 55 47 32 32 5f 50\n0x3b0 \t35 30 39 32 00 41 42 20 31\n' > "$T/nul"
    for case in "cn:MHI2Q_CN_AUG22_K1004:MHI2Q:CN:AUG22:K1004" "us:MHI2Q_US_AUG22_P5087:MHI2Q:US:AUG22:P5087" \
                "vw:MHI2_ER_VWG13_P4521:MHI2:ER:VWG13:P4521" "po:MHI2Q_ER_POG24_P5000:MHI2Q:ER:POG24:P5000" \
                "sp:MHI2Q_US_AUG22_P5087:MHI2Q:US:AUG22:P5087" "ff:MHI2Q_US_AUG22_P5087:MHI2Q:US:AUG22:P5087" \
                "nul:MHI2Q_ER_AUG22_P5092:MHI2Q:ER:AUG22:P5092"; do
        f=${case%%:*}; want=${case#*:}
        got=$(E2P_READ="cat $T/$f" "$sh" -c ". $T/f.sh; read_train; echo \$TRAIN:\$(train_field 1):\$(train_field 2):\$(train_field 3):\$(train_field 4)")
        [ "$got" = "$want" ] || fail "$sh" "E2P $f -> '$got', want '$want'"
    done
    got=$(E2P_READ="cat $T/sp" "$sh" -c ". $T/f.sh; read_train; train_verified && echo y || echo n")
    [ "$got" = y ] || fail "$sh" "space-padded P5087 not verified"
    got=$(E2P_READ="false" "$sh" -c ". $T/f.sh; read_train; echo \"[\$TRAIN]\"")
    [ "$got" = "[]" ] || fail "$sh" "unreadable E2P -> '$got'"
    for case in "0x000000fc 02:external" "0xfc: 01:internal" "0x000000fc ff:unknown" ":unknown"; do
        line=${case%:*}; want=${case##*:}
        got=$(E2P_AMP_READ="echo $line" "$sh" -c ". $T/f.sh; read_amp; echo \$AMP")
        [ "$got" = "$want" ] || fail "$sh" "amp '$line' -> '$got', want '$want'"
    done
done
echo "Firmware gates (Java variant by E2P region, verified trains, E2P train parse, amplifier coding):$shells PASS"
