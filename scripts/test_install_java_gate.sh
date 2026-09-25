#!/bin/sh
# Firmware gates of the M.I.B. installer:
#  - java_jar(): E2P region US/ER/JP -> carplay_hook.jar, CN -> carplay_hook_cn.jar, else none;
#  - p5087_size(): SI/deva are replaced only over the P5087 build;
#  - read_train()/train_field(): the E2P train (modifyE2P r 3A0 19, bytes spaced or not)
#    decodes to text and splits into platform/region/brand/version.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/install_MoreIncredibleBash/mod/custom.sh
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
{ echo 'set -u'
  sed -n '/^file_size()/p;/^p5087_size()/p' "$SRC"
  sed -n '/^java_jar()/,/^}/p;/^E2P_READ=/p;/^read_train()/,/^}/p;/^train_field()/,/^}/p' "$SRC"
} > "$T/f.sh"
fail() { echo "FAIL ($1): $2"; exit 1; }
shells=
for s in /bin/ksh /bin/mksh "$(command -v mksh 2>/dev/null)" /bin/sh; do [ -x "$s" ] && shells="$shells $s"; done
for sh in $shells; do
    for case in MHI2Q_US_AUG22_P508:carplay_hook.jar MHI2Q_ER_AUG22_P509:carplay_hook.jar MHI2Q_JP_AUG22_P101:carplay_hook.jar \
                MHI2Q_CN_AUG22_K100:carplay_hook_cn.jar MHI2Q_KR_AUG22_P500: :; do
        train=${case%%:*}; want=${case#*:}
        got=$("$sh" -c ". $T/f.sh; TRAIN=$train; java_jar")
        [ "$got" = "$want" ] || fail "$sh" "train '$train' -> '$got', want '$want'"
    done

    [ "$("$sh" -c ". $T/f.sh; p5087_size /x/smartphone_integrator")" = 802798 ] || fail "$sh" "SI size"
    [ "$("$sh" -c ". $T/f.sh; p5087_size /x/deva-ctrl-qc.so")" = 694904 ] || fail "$sh" "deva size"

    # modifyE2P output: address, then the bytes, spaced (1-2 lines) or run together
    printf '0x000003a0 4d 48 49 32 51 5f 43 4e 5f 41 55 47 32\n0x000003ad 32 5f 4b 31 30 30 34\n' > "$T/cn"
    printf '0x3a0: 4D484932515F55535F41554732325F5035303837\n' > "$T/us"
    printf 'E2P read\n0x000003a0 4d 48 49 32 5f 45 52 5f 56 57 47 31 33 5f 50 34 35 32 31\n' > "$T/vw"
    printf '0x000003a0 4d 48 49 32 51 5f 45 52 5f 50 4f 47 32 34 5f 50 35 30 30 30\n' > "$T/po"
    for case in "cn:MHI2Q_CN_AUG22_K1004:MHI2Q:CN:AUG22:K1004" "us:MHI2Q_US_AUG22_P5087:MHI2Q:US:AUG22:P5087" \
                "vw:MHI2_ER_VWG13_P4521:MHI2:ER:VWG13:P4521" "po:MHI2Q_ER_POG24_P5000:MHI2Q:ER:POG24:P5000"; do
        f=${case%%:*}; want=${case#*:}
        got=$(E2P_READ="cat $T/$f" "$sh" -c ". $T/f.sh; read_train; echo \$TRAIN:\$(train_field 1):\$(train_field 2):\$(train_field 3):\$(train_field 4)")
        [ "$got" = "$want" ] || fail "$sh" "E2P $f -> '$got', want '$want'"
    done
    got=$(E2P_READ="false" "$sh" -c ". $T/f.sh; read_train; echo \"[\$TRAIN]\"")
    [ "$got" = "[]" ] || fail "$sh" "unreadable E2P -> '$got'"
done
echo "Firmware gates (Java variant by E2P region, SI/deva by P5087 size, E2P train parse):$shells PASS"
