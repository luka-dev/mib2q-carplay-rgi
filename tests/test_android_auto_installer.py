#!/usr/bin/env python3
"""Exercise profile selection and rollback against a disposable fake head-unit root."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO))
from scripts.stage_android_auto import cksum

INSTALLER = REPO / "install_AndroidAuto_MoreIncredibleBash" / "mod" / "custom.sh"
COMMON_PAYLOADS = (
    "libsq5_cluster_live.so", "cluster-player", "maneuver_render", "flag_atlas.rgba",
    "gal_startup.sh", "sq5_luka_monitor.sh", "sq5_luka_processes.sh", "sq5_luka_cleanup.sh",
    "config_tool", "profiles.list",
)
JAR = "mib2q_android_auto_cluster.jar"
SHELL = shutil.which("sh") or str(Path("C:/Program Files/Git/bin/sh.exe"))
CYGPATH = str(Path(SHELL).parents[1] / "usr/bin/cygpath.exe")


def shell_path(path):
    if os.name != "nt":
        return str(path)
    return subprocess.check_output([CYGPATH, "-u", str(path)], text=True).strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(script, env, expect=0):
    proc = subprocess.run([SHELL, shell_path(script)], text=True, capture_output=True, env=env)
    if proc.returncode != expect:
        raise AssertionError(
            "installer exit %d (expected %d)\nstdout:\n%s\nstderr:\n%s"
            % (proc.returncode, expect, proc.stdout, proc.stderr)
        )
    return proc.stdout


def fingerprint(path):
    crc, size = cksum(path)
    return str(crc), str(size)


def profile_line(profile_id, label, state, gal, receiver, lsd, jar_path):
    gal_crc, gal_size = fingerprint(gal)
    receiver_crc, receiver_size = fingerprint(receiver)
    lsd_crc, lsd_size = fingerprint(lsd)
    return "|".join((profile_id, label, state, gal_crc, gal_size, receiver_crc,
                     receiver_size, lsd_crc, lsd_size, jar_path))


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
        variants = {}
        for profile_id, suffix in (("profile_a", b"A"), ("profile_b", b"B")):
            profile_root = top / profile_id
            profile_root.mkdir()
            files = {}
            for name in ("gal", "receiver", "lsd"):
                path = profile_root / name
                path.write_bytes(b"stock-" + name.encode("ascii") + b"-" + suffix)
                files[name] = path
            jar_path = payload / "profiles" / profile_id / JAR
            jar_path.parent.mkdir(parents=True)
            jar_path.write_bytes(b"jar-" + suffix)
            files["jar"] = jar_path
            variants[profile_id] = files

        pending_root = top / "pending"
        pending_root.mkdir()
        pending_lsd = pending_root / "lsd"
        pending_lsd.write_bytes(b"pending-lsd")
        pending_gal = pending_root / "gal"
        pending_gal.write_bytes(b"pending-gal")
        pending_jar = payload / "profiles/pending" / JAR
        pending_jar.parent.mkdir(parents=True)
        pending_jar.write_bytes(b"jar-pending")

        for name in COMMON_PAYLOADS:
            if name != "profiles.list":
                (payload / name).write_bytes(("payload:" + name + "\n").encode("ascii"))
        config_tool = payload / "config_tool"
        config_tool.write_text(
            "#!/bin/sh\ncmd=$1; file=$2; key=${3:-}\n"
            "if [ \"$cmd\" = get ]; then\n"
            "  if grep -q sq5_android_auto \"$file\"; then\n"
            "    [ \"$key\" = exec ] && echo gal_startup.sh || echo /mnt/app/root/sq5_android_auto\n"
            "  else\n"
            "    [ \"$key\" = exec ] && echo gal || echo /mnt/app/eso/bin/apps\n"
            "  fi\n"
            "elif [ \"$cmd\" = patch ]; then\n"
            "  echo '{\"children\":{\"gal\":{\"exec\":\"gal_startup.sh\",\"path\":\"/mnt/app/root/sq5_android_auto\"}}}'\n"
            "else exit 2; fi\n", encoding="ascii",
        )
        config_tool.chmod(0o755)

        lines = []
        for profile_id in ("profile_a", "profile_b"):
            f = variants[profile_id]
            lines.append(profile_line(profile_id, "Synthetic " + profile_id[-1].upper(), "installable",
                                      f["gal"], f["receiver"], f["lsd"], f"profiles/{profile_id}/{JAR}"))
        lcrc, lsize = fingerprint(pending_lsd)
        _, pending_gal_size = fingerprint(pending_gal)
        lines.append("|".join(("pending", "Synthetic Pending", "native-pending", "", pending_gal_size,
                               "", "", lcrc, lsize, f"profiles/pending/{JAR}")))
        (payload / "profiles.list").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

        manifest = []
        for path in sorted(p for p in payload.rglob("*") if p.is_file()):
            crc, size = cksum(path)
            manifest.append(f"{crc} {size} {path.relative_to(payload).as_posix()}")
        (payload / "manifest.cksum").write_text("\n".join(manifest) + "\n", encoding="ascii", newline="\n")

        env = os.environ.copy()
        env.update(MIB2Q_AA_TEST_ROOT=shell_path(root))
        script = mod / "custom.sh"

        def set_unit(profile_id):
            files = variants[profile_id]
            shutil.copy2(files["gal"], gal)
            shutil.copy2(files["receiver"], receiver)
            shutil.copy2(files["lsd"], lsd)

        for profile_id in ("profile_a", "profile_b"):
            set_unit(profile_id)
            immutable = {p: digest(p) for p in (gal, receiver, lsd)}
            out = run(script, env)
            assert "EXACT INSTALLABLE PROFILE" in out and profile_id[-1].upper() in out, out
            assert cfg.read_bytes() == stock_cfg

            if profile_id == "profile_a":
                (sd / "mib2q_aa_install").touch()
                (sd / "mib2q_aa_uninstall").touch()
                out = run(script, env, expect=1)
                assert "both action markers" in out and cfg.read_bytes() == stock_cfg
                (sd / "mib2q_aa_uninstall").unlink()
            else:
                (sd / "mib2q_aa_install").touch()

            out = run(script, env)
            assert "INSTALLED Synthetic" in out
            assert (jar_dir / JAR).read_bytes() == variants[profile_id]["jar"].read_bytes()
            assert all(digest(path) == before for path, before in immutable.items())
            installed_cfg = cfg.read_bytes()

            (sd / "mib2q_aa_install").unlink()
            (sd / "mib2q_aa_uninstall").touch()
            if profile_id == "profile_a":
                cfg.write_bytes(installed_cfg + b"external edit\n")
                out = run(script, env, expect=1)
                assert "changed after install" in out
                cfg.write_bytes(installed_cfg)
            out = run(script, env)
            assert "UNINSTALLED" in out and cfg.read_bytes() == stock_cfg
            assert all(digest(path) == before for path, before in immutable.items())
            assert not (jar_dir / JAR).exists()
            (sd / "mib2q_aa_uninstall").unlink()

        shutil.copy2(pending_gal, gal)
        receiver.write_bytes(b"unknown-receiver")
        shutil.copy2(pending_lsd, lsd)
        out = run(script, env)
        assert "Synthetic Pending" in out and "BLOCKED" in out
        (sd / "mib2q_aa_install").touch()
        out = run(script, env, expect=1)
        assert "recognized but blocked" in out and cfg.read_bytes() == stock_cfg
        (sd / "mib2q_aa_install").unlink()

        lsd.write_bytes(b"completely-unknown")
        out = run(script, env)
        assert "firmware profile:    unknown" in out
        (sd / "mib2q_aa_install").touch()
        out = run(script, env, expect=1)
        assert "does not match an installable profile" in out and cfg.read_bytes() == stock_cfg

    print("android auto installer profile tests: PASS")


if __name__ == "__main__":
    main()
