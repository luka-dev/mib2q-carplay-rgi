#!/usr/bin/env python3
"""Exercise the M.I.B. installer against a disposable fake head-unit root."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


REPO = Path(__file__).resolve().parents[1]
INSTALLER = REPO / "install_AndroidAuto_MoreIncredibleBash" / "mod" / "custom.sh"
PAYLOADS = (
    "libsq5_cluster_live.so", "cluster-player", "maneuver_render", "flag_atlas.rgba",
    "gal_startup.sh", "sq5_luka_monitor.sh", "sq5_luka_processes.sh", "sq5_luka_cleanup.sh",
    "mib2q_android_auto_cluster.jar", "config_tool",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(script, env, expect=0):
    proc = subprocess.run(["sh", str(script)], text=True, capture_output=True, env=env)
    if proc.returncode != expect:
        raise AssertionError(
            "installer exit %d (expected %d)\nstdout:\n%s\nstderr:\n%s"
            % (proc.returncode, expect, proc.stdout, proc.stderr)
        )
    return proc.stdout


def main():
    with tempfile.TemporaryDirectory(prefix="mib2q-aa-installer-") as td:
        top = Path(td)
        root = top / "unit"
        mod = top / "card" / "install_AndroidAuto_MoreIncredibleBash" / "mod"
        payload = mod / "android-auto"
        sd = root / "fs" / "sda0"
        cfg = root / "mnt/system/etc/eso/production/smartphone_integrator.json"
        gal = root / "mnt/app/eso/bin/apps/gal"
        receiver = root / "mnt/app/eso/lib/libautoreceiver.so"
        lsd = root / "ifs/lsd.jxe"
        jar_dir = root / "mnt/app/eso/hmi/lsd/jars"
        for directory in (payload, sd, cfg.parent, gal.parent, receiver.parent, lsd.parent, jar_dir):
            directory.mkdir(parents=True, exist_ok=True)
        shutil.copy2(INSTALLER, mod / "custom.sh")

        stock_cfg = b'{"children":{"gal":{"exec":"gal","path":"/mnt/app/eso/bin/apps"}}}\n'
        cfg.write_bytes(stock_cfg)
        gal.write_bytes(b"stock-gal")
        receiver.write_bytes(b"stock-receiver")
        lsd.write_bytes(b"stock-lsd")
        immutable = {p: digest(p) for p in (gal, receiver, lsd)}

        for name in PAYLOADS:
            (payload / name).write_bytes(("payload:" + name + "\n").encode("ascii"))
        config_tool = payload / "config_tool"
        config_tool.write_text(
            "#!/bin/sh\n"
            "cmd=$1; file=$2; key=${3:-}\n"
            "if [ \"$cmd\" = get ]; then\n"
            "  if grep -q sq5_android_auto \"$file\"; then\n"
            "    [ \"$key\" = exec ] && echo gal_startup.sh || echo /mnt/app/root/sq5_android_auto\n"
            "  else\n"
            "    [ \"$key\" = exec ] && echo gal || echo /mnt/app/eso/bin/apps\n"
            "  fi\n"
            "elif [ \"$cmd\" = patch ]; then\n"
            "  echo '{\"children\":{\"gal\":{\"exec\":\"gal_startup.sh\",\"path\":\"/mnt/app/root/sq5_android_auto\"}}}'\n"
            "else exit 2; fi\n",
            encoding="ascii",
        )
        config_tool.chmod(0o755)
        manifest = []
        for name in PAYLOADS:
            result = subprocess.check_output(["cksum", str(payload / name)], text=True).split()
            manifest.append("%s %s %s" % (result[0], result[1], name))
        (payload / "manifest.cksum").write_text("\n".join(manifest) + "\n", encoding="ascii")

        env = os.environ.copy()
        env.update(MIB2Q_AA_TEST_ROOT=str(root), MIB2Q_AA_TEST_VALIDATED="1")
        script = mod / "custom.sh"

        out = run(script, env)
        assert "AUDIT ONLY" in out and cfg.read_bytes() == stock_cfg

        (sd / "mib2q_aa_install").touch()
        (sd / "mib2q_aa_uninstall").touch()
        out = run(script, env, expect=1)
        assert "both action markers" in out and cfg.read_bytes() == stock_cfg
        (sd / "mib2q_aa_uninstall").unlink()

        out = run(script, env)
        assert "INSTALLED" in out and b"sq5_android_auto" in cfg.read_bytes()
        assert all(digest(path) == before for path, before in immutable.items())
        installed_cfg = cfg.read_bytes()

        (sd / "mib2q_aa_install").unlink()
        (sd / "mib2q_aa_uninstall").touch()
        cfg.write_bytes(installed_cfg + b"external edit\n")
        out = run(script, env, expect=1)
        assert "changed after install" in out
        cfg.write_bytes(installed_cfg)

        out = run(script, env)
        assert "UNINSTALLED" in out and cfg.read_bytes() == stock_cfg
        assert all(digest(path) == before for path, before in immutable.items())
        assert not (jar_dir / "mib2q_android_auto_cluster.jar").exists()

    print("android auto installer tests: PASS")


if __name__ == "__main__":
    main()
