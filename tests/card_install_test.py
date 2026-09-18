#!/usr/bin/env python3
"""Exercise the actual single-file QNX installer against isolated HU trees."""
import importlib.util
import contextlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "install_MoreIncredibleBash/mod/custom.sh"
PF_STOCK = ROOT / "deploy/smartphone_integrator/pf_stock"
PROFILES = ("pf.conf", "pf.ecm0.conf", "pf.mlan0.conf", "pf.ppp0.conf")
spec = importlib.util.spec_from_file_location("package_mib", ROOT / "scripts/package_mib.py")
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)
STOCK = {
    "policy": {"enabled": True},
    "children": {
        "usb": {"exec": "usb_manager", "params": "unrelated"},
        "carplay": {"exec": "dio_manager", "path": "/mnt/app/eso/bin/apps",
                    "envs": ["LD_LIBRARY_PATH=/eso/lib"]},
        "bt": {"exec": "bt_manager"},
    },
}


class InstallTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="carplay-install-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.app, self.sys = self.base / "app", self.base / "system"
        self.hooks = self.app / "root/hooks"
        self.jars = self.app / "eso/hmi/lsd/jars"
        self.cfg = self.sys / "etc/eso/production/smartphone_integrator.json"
        self.payload = self.base / "card/build"
        self.commands = self.base / "commands"
        for path in (self.hooks, self.jars, self.cfg.parent, self.payload, self.base / "card/mod", self.commands):
            path.mkdir(parents=True)
        self.cfg.write_text(json.dumps(STOCK, indent=2) + "\n")
        self.cfg.chmod(0o640)
        self.stock = self.cfg.read_bytes()
        (self.app / "version_info.txt").write_text(
            "Product = mmx2qc\nHMI = HMI_AU_MIB2_HQC_NAR_G22_C10_180803\n")
        (self.jars.parent / "lsd.sh").write_text(
            'JARS=$(find $BASE_DIR/lsd/jars/ -name "*.jar")\n'
            'BOOTCLASSPATH="$BOOTCLASSPATH:$jar"\n'
            '$J9 -Xbootclasspath:$BOOTCLASSPATH\n')
        self.script = self.payload.parent / "mod/custom.sh"
        shutil.copy2(SCRIPT, self.script)
        for name in package.BINARIES:
            (self.payload / name).write_text(f"payload: {name}\n")
        (self.payload / "release.json").write_text('{}\n')
        child = (ROOT / "deploy/smartphone_integrator/carplay_child.json").read_bytes().rstrip()
        (self.payload / "carplay_child.json").write_bytes(child)
        for name in ("carplay_pf.patch", "patch_si_rearm.py"):
            (self.payload / name).write_text("fixture artifact\n")
        self.firmware_targets = {
            "smartphone_integrator": self.app / "eso/bin/apps/smartphone_integrator",
            "deva-ctrl-qc.so": self.app / "armle/lib/dll/deva-ctrl-qc.so",
        }
        # Small fixture images, with explicitly substituted baseline signatures.
        # Production baseline signatures are checked against real images below.
        signatures = {"smartphone_integrator": ("56251098 802798", "996060264 802798"),
                      "deva-ctrl-qc.so": ("325683668 694904", "751699153 694904")}
        source = self.script.read_text()
        self.fixture_signatures = {}
        for name, target in self.firmware_targets.items():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(f"stock: {name}\n")
            target.chmod(0o755)
            self.fixture_signatures[signatures[name][0]] = package.cksum(target)
            self.fixture_signatures[signatures[name][1]] = package.cksum(self.payload / name)
        for production, fixture in self.fixture_signatures.items():
            source = source.replace(production, fixture)
        self.script.write_text(source)
        self.rehash()
        for name in PROFILES:
            shutil.copy2(PF_STOCK / name, self.sys / "etc" / name)
        self.tool("mount", 'echo mount >> "$EVENTS"\n[ "${FAIL_MOUNT:-0}" = 0 ]\n')
        self.tool("sync", 'echo sync >> "$EVENTS"\n')
        self.tool("pidin", '''
n=0; [ ! -f "$COUNTER" ] || read n < "$COUNTER"
n=$((n + 1)); echo "$n" > "$COUNTER"
case "${PID_MODE:-idle}" in
    fail) exit 12 ;;
    empty) exit 0 ;;
    malformed) echo '???'; exit 0 ;;
    busy) echo '123 /mnt/app/eso/bin/apps/dio_manager --foo'; exit 0 ;;
    bare) echo '123 dio_manager'; exit 0 ;;
    startup) echo '123 /bin/sh /mnt/app/root/hooks/carplay_startup.sh'; exit 0 ;;
    late) if [ "$n" -ge 2 ]; then echo '123 dio_manager'; exit 0; fi ;;
    config_race) if [ "$n" = 2 ]; then echo ' ' >> "$SYS_ROOT/etc/eso/production/smartphone_integrator.json"; fi ;;
esac
echo 'pid Arguments'
echo '1 /proc/boot/procnto-smp'
echo '555 /bin/sh custom.sh'
echo '556 /bin/cat /tmp/dio_manager.log'
''')
        cp, mv = shutil.which("cp"), shutil.which("mv")
        self.tool("cp", f'''
for arg do last=$arg; done
case "$last" in *.carplay_hook.*.new)
    [ "${{FAIL_STAGE:-0}}" = 0 ] || exit 83 ;;
    *.orig.carplay.*.new) [ "${{FAIL_BACKUP:-0}}" = 0 ] || exit 85 ;;
esac
exec '{cp}' "$@"
''')
        self.tool("mv", f'''
for arg do last=$arg; done
echo "mv $last" >> "$EVENTS"
case "$last" in */smartphone_integrator.json)
    [ "${{FAIL_SI:-0}}" = 0 ] || exit 84 ;;
esac
exec '{mv}' "$@"
''')
        for name in ("kill", "slay", "reboot", "sed", "awk", "find", "dirname", "tr"):
            self.tool(name, 'echo "$0" >> "$FORBIDDEN"\nexit 99\n')
        self.env = dict(os.environ, APP_ROOT=str(self.app), SYS_ROOT=str(self.sys),
                        PATH=str(self.commands) + os.pathsep + os.environ["PATH"],
                        INSTALL_LOCK_DIR=str(self.base / "lock"), LOG=str(self.base / "install.log"),
                        COUNTER=str(self.base / "pidin-count"), EVENTS=str(self.base / "events"),
                        FORBIDDEN=str(self.base / "forbidden"))

    def tool(self, name, body):
        path = self.commands / name
        path.write_text("#!/bin/sh\n" + body)
        path.chmod(0o755)

    def rehash(self):
        (self.payload / "CKSUMS").write_text("".join(
            f"{package.cksum(self.payload / name)} {name}\n" for name in package.FILES))

    def snapshot(self):
        return {str(p.relative_to(self.base)): (p.read_bytes(), p.stat().st_mode & 0o777)
                for folder in (self.app, self.sys) for p in folder.rglob("*") if p.is_file()}

    def run_installer(self, action="install", success=True, **env):
        (self.base / "pidin-count").unlink(missing_ok=True)
        result = subprocess.run(["sh", str(self.script)] + ([action] if action else []),
                                env=dict(self.env, **env), capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        self.assertFalse((self.base / "forbidden").exists(), "forbidden target command used")
        self.assertFalse((self.base / "lock").exists())
        self.assertFalse(list(self.sys.rglob("*.new")))
        self.assertFalse(list(self.app.rglob("*.new")))
        self.assertFalse(list(self.app.rglob(".carplay-stage.*")))
        if not success:
            self.assertNotIn("DONE.", result.stdout)
        self.assertEqual((self.script.parent / f"last-carplay-{action or 'uninstall'}.log").read_text(),
                         (self.base / "install.log").read_text())
        return result

    def assert_refused_unchanged(self, **env):
        before = self.snapshot()
        result = self.run_installer(success=False, **env)
        self.assertEqual(self.snapshot(), before)
        return result

    def test_install_atomic_files_idempotence_uninstall(self):
        (self.hooks / "cursor_render").write_text("old cursor")
        jar = self.jars / "carplay_hook.jar"
        jar.write_text("old jar")
        with contextlib.ExitStack() as handles, self.cfg.open("rb") as old_cfg, jar.open("rb") as old_jar:
            old_firmware = {name: handles.enter_context(path.open("rb"))
                            for name, path in self.firmware_targets.items()}
            self.run_installer()
            self.assertEqual(old_cfg.read(), self.stock)
            self.assertEqual(old_jar.read(), b"old jar")
            for name, handle in old_firmware.items():
                self.assertEqual(handle.read(), f"stock: {name}\n".encode())
        cfg = json.loads(self.cfg.read_text())
        expected = json.loads((ROOT / "deploy/smartphone_integrator/carplay_child.json").read_text())
        self.assertEqual(cfg["children"].pop("carplay"), expected)
        stock = json.loads(self.stock)
        stock["children"].pop("carplay")
        self.assertEqual(cfg, stock)
        self.assertEqual(self.cfg.stat().st_mode & 0o777, 0o640)
        self.assertEqual(Path(str(self.cfg) + ".orig").read_bytes(), self.stock)
        self.assertFalse((self.hooks / "cursor_render").exists())
        for name in package.BINARIES:
            target = self.firmware_targets.get(name, jar if name == "carplay_hook.jar" else self.hooks / name)
            self.assertEqual(target.read_bytes(), (self.payload / name).read_bytes())
        for name in PROFILES:
            old = (PF_STOCK / name).read_bytes()
            self.assertEqual((self.sys / "etc" / name).read_bytes(), old.replace(b" 6030,", b" 6030:6032,"))
            self.assertEqual((self.sys / "etc" / (name + ".stock")).read_bytes(), old)
        events = (self.base / "events").read_text().splitlines()
        si = events.index(f"mv {self.cfg}")
        self.assertEqual(events[si - 1], "sync")
        self.assertGreater(si, events.index(f"mv {jar}"))
        before = self.snapshot()
        self.run_installer()
        self.assertEqual(before, self.snapshot())
        # Standalone removal folder, no build files and no arguments/marker.
        remove = self.base / "uninstall/mod/custom.sh"
        remove.parent.mkdir(parents=True)
        removal_source = (ROOT / "uninstall_MoreIncredibleBash/mod/custom.sh").read_text()
        for production, fixture in self.fixture_signatures.items():
            removal_source = removal_source.replace(production, fixture)
        remove.write_text(removal_source)
        self.script = remove
        shutil.rmtree(self.payload)
        self.run_installer(action=None)
        for name, target in self.firmware_targets.items():
            self.assertEqual(target.read_text(), f"stock: {name}\n")
        self.assertEqual(self.cfg.read_bytes(), self.stock)
        self.assertFalse(jar.exists())
        self.assertFalse(any(self.hooks.iterdir()))

    def test_check_is_read_only(self):
        before = self.snapshot()
        self.run_installer("check")
        self.assertEqual(before, self.snapshot())
        self.assertFalse((self.base / "events").exists())

    def test_process_detection(self):
        for mode in ("busy", "bare", "startup", "fail", "empty", "malformed", "late"):
            with self.subTest(mode=mode):
                result = self.assert_refused_unchanged(PID_MODE=mode)
                self.assertIn("FAILED", result.stdout)

    def test_corruption_missing_and_duplicate_checksums(self):
        target = self.payload / "libcarplay_hook.so"
        target.write_text("damaged")
        self.assertIn("Checksum mismatch", self.assert_refused_unchanged().stdout)
        target.unlink()
        self.assertIn("Missing/empty", self.assert_refused_unchanged().stdout)
        target.write_text("correct again")
        self.rehash()
        manifest = self.payload / "CKSUMS"
        manifest.write_text(manifest.read_text() * 2)
        self.assertIn("Duplicate", self.assert_refused_unchanged().stdout)
        self.assertFalse((self.base / "events").exists())

    def test_wrong_firmware_loader_and_config(self):
        version = self.app / "version_info.txt"
        old = version.read_bytes()
        version.write_text("Product = mmx2\n")
        self.assert_refused_unchanged()
        version.write_bytes(old)
        loader = self.jars.parent / "lsd.sh"
        old = loader.read_bytes()
        loader.write_text("no JAR loader")
        self.assert_refused_unchanged()
        loader.write_bytes(old)
        for data in ('{"children": {"carplay": {"exec": "dio_manager"}}}\n',
                     self.stock.decode().replace('"carplay"', '"unknown"'),
                     self.stock.decode().replace('"carplay": {', '"carplay": {\n  {')):
            self.cfg.write_text(data)
            self.assert_refused_unchanged()
        self.assertFalse((self.base / "events").exists())

    def test_pf_ranges_and_scoped_matching(self):
        for port_range in ("6030:6031", "6030:6032", "6030:6033"):
            original = (PF_STOCK / "pf.conf").read_text().replace(" 6030,", f" {port_range},")
            original += "# unrelated 6030:6032\npass on ecm0 proto tcp to any port 6030:6033\n"
            path = self.sys / "etc/pf.conf"
            path.write_text(original)
            self.run_installer()
            self.assertEqual(path.read_text(), original.replace(f" {port_range},", " 6030:6032,"))

    def test_invalid_last_pf_profile_blocks_all_writes(self):
        path = self.sys / "etc/pf.ppp0.conf"
        old = path.read_text()
        for data in ("# only a comment 6030:6032\n", old * 2):
            path.write_text(data)
            self.assert_refused_unchanged()
        self.assertFalse((self.base / "events").exists())

    def test_mount_and_staging_failure(self):
        self.assert_refused_unchanged(FAIL_MOUNT="1")
        self.assert_refused_unchanged(FAIL_STAGE="1")
        self.assert_refused_unchanged(FAIL_BACKUP="1")

    def test_missing_pf_profiles(self):
        for name in PROFILES:
            (self.sys / "etc" / name).unlink()
        self.assertIn("No PF profiles", self.assert_refused_unchanged().stdout)

    def test_config_change_during_staging_is_preserved(self):
        result = self.run_installer(success=False, PID_MODE="config_race")
        self.assertIn("SI config changed", result.stdout)
        self.assertEqual(self.cfg.read_bytes(), self.stock + b" \n")
        self.assertFalse(any(self.hooks.iterdir()))
        self.assertFalse(any(self.jars.iterdir()))

    def test_lock_preserved_on_concurrent_install(self):
        lock = self.base / "lock"
        lock.mkdir()
        marker = lock / "owner"
        marker.write_text("other installer")
        result = subprocess.run(["sh", str(self.script)], env=self.env,
                                capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("another installer", result.stdout)
        self.assertEqual(marker.read_text(), "other installer")

    def test_si_rename_failure_retains_complete_old_config(self):
        result = self.run_installer(success=False, FAIL_SI="1")
        self.assertEqual(self.cfg.read_bytes(), self.stock)
        self.assertIn("Installation is incomplete", result.stdout)
        self.run_installer()  # retry completes the interrupted release

    def test_uninstall_requires_stock_backup(self):
        self.run_installer()
        Path(str(self.cfg) + ".orig").unlink()
        before = self.snapshot()
        self.run_installer("uninstall", success=False)
        self.assertEqual(before, self.snapshot())

    def test_unknown_firmware_and_backup_are_rejected_before_mount(self):
        for name, target in self.firmware_targets.items():
            original = target.read_bytes()
            target.write_text("unrecognized firmware version")
            self.assertIn("Unknown firmware", self.assert_refused_unchanged().stdout)
            target.write_bytes(original)
            backup = Path(str(target) + ".carplay-stock")
            backup.write_text("corrupt backup")
            self.assertIn("Invalid stock backup", self.assert_refused_unchanged().stdout)
            backup.unlink()
        self.assertFalse((self.base / "events").exists())

    def test_intermediate_firmware_payload_rejected(self):
        (self.payload / "deva-ctrl-qc.so").write_text("intermediate driver")
        self.rehash()
        self.assertIn("Wrong final payload", self.assert_refused_unchanged().stdout)
        self.assertFalse((self.base / "events").exists())

    def test_preexisting_final_firmware_patches_retained_on_removal(self):
        for name, target in self.firmware_targets.items():
            target.write_bytes((self.payload / name).read_bytes())
        self.run_installer()
        for target in self.firmware_targets.values():
            self.assertFalse(Path(str(target) + ".carplay-stock").exists())
        self.run_installer("uninstall")
        for name, target in self.firmware_targets.items():
            self.assertEqual(target.read_bytes(), (self.payload / name).read_bytes())

    def test_real_firmware_crc_signatures_and_generated_removal(self):
        source = SCRIPT.read_text()
        self.assertEqual((ROOT / "uninstall_MoreIncredibleBash/mod/custom.sh").read_text(),
                         package.uninstall_script())
        stock_root = ROOT.parents[1] / "Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app"
        images = {
            ("smartphone_integrator", "stock"): ROOT / "deploy/usb_replug_recovery/bundle_mu1316_p5087/stock/smartphone_integrator",
            ("smartphone_integrator", "patched"): ROOT / "deploy/usb_replug_recovery/bundle_mu1316_p5087/payload/smartphone_integrator",
            ("deva-ctrl-qc.so", "stock"): stock_root / "armle/lib/dll/deva-ctrl-qc.so",
            ("deva-ctrl-qc.so", "patched"): ROOT / "deploy/deva-ctrl-qc/deva-ctrl-qc-mirror.so",
        }
        for (name, state), path in images.items():
            if path.exists():
                self.assertIn(f"{name}:{state}) echo '{package.cksum(path)}'", source)

    def test_real_mu1316_config_when_available(self):
        firmware = ROOT.parents[1] / "Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/system/etc/eso/production/smartphone_integrator.json"
        if not firmware.exists():
            self.skipTest("local MU1316 extraction unavailable")
        self.cfg.write_bytes(firmware.read_bytes())
        def parse(text):
            return json.loads("\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#")))
        before = parse(self.cfg.read_text())
        self.run_installer()
        after = parse(self.cfg.read_text())
        before["children"]["carplay"] = json.loads((ROOT / "deploy/smartphone_integrator/carplay_child.json").read_text())
        self.assertEqual(before, after)
        first = self.cfg.read_bytes()
        self.run_installer()
        self.assertEqual(first, self.cfg.read_bytes())

    def test_actual_release_files_and_firmware_when_available(self):
        stock_root = ROOT.parents[1] / "Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app"
        deva_stock = stock_root / "armle/lib/dll/deva-ctrl-qc.so"
        if not deva_stock.exists() or not (ROOT / "build/CKSUMS").exists():
            self.skipTest("local firmware extraction/release artifacts unavailable")
        shutil.copy2(SCRIPT, self.script)
        for name in package.FILES:
            if name != "../mod/custom.sh":
                shutil.copy2(ROOT / "build" / name, self.payload / name)
        shutil.copy2(ROOT / "build/CKSUMS", self.payload / "CKSUMS")
        shutil.copy2(ROOT / "deploy/usb_replug_recovery/bundle_mu1316_p5087/stock/smartphone_integrator",
                     self.firmware_targets["smartphone_integrator"])
        shutil.copy2(deva_stock, self.firmware_targets["deva-ctrl-qc.so"])
        self.run_installer("check")
        self.run_installer()
        for name, target in self.firmware_targets.items():
            self.assertEqual(target.read_bytes(), (ROOT / "build" / name).read_bytes())
        self.run_installer("uninstall")
        self.assertEqual(self.firmware_targets["deva-ctrl-qc.so"].read_bytes(), deva_stock.read_bytes())


class SshWrapperTest(unittest.TestCase):
    def test_staging_failure_propagation_and_cleanup_without_network(self):
        # Exercise macOS /bin/bash 3.2 as well as newer Bash. The wrapper's
        # transport and package checker are mocked; no remote commands execute.
        with tempfile.TemporaryDirectory(prefix="carplay-ssh-") as tmp:
            base = Path(tmp)
            commands = base / "commands"
            commands.mkdir()
            stub = '''
import json, os, pathlib, re, sys
name = pathlib.Path(sys.argv[0]).name
with open(os.environ['SSH_EVENTS'], 'a') as log:
    log.write(json.dumps([name, *sys.argv[1:]]) + '\\n')
cmd = sys.argv[-1]
if name == 'python3': sys.exit(int(os.environ.get('CHECK_RC', '0')))
if name == 'scp': sys.exit(int(os.environ.get('SCP_RC', '0')))
if cmd.startswith('if [ -d'):
    path = re.search(r'echo (/tmp/[^;]+)', cmd)[1]
    print(('/net/mmx' if os.environ.get('RCC') else '') + path)
elif cmd.startswith('/bin/sh '):
    sys.exit(int(os.environ.get('INSTALL_RC', '0')))
'''
            for name in ("ssh", "scp", "python3"):
                path = commands / name
                path.write_text(f"#!{sys.executable}\n" + stub)
                path.chmod(0o755)
            events = base / "events"
            env = dict(os.environ, PATH=str(commands) + os.pathsep + os.environ["PATH"],
                       SSH_EVENTS=str(events), CARPLAY_BUILD_DIR=str(base / "build with spaces"))
            env.pop("SSHPASS", None)
            build = Path(env["CARPLAY_BUILD_DIR"])
            build.mkdir()
            (build / "SHA256SUMS").write_text('hash  ../mod/custom.sh\nhash  carplay_hook.jar\n')
            for overrides, expected, action in (({}, 0, "install"), ({"RCC": "1"}, 0, "install"),
                                                ({"INSTALL_RC": "23"}, 23, "install"),
                                                ({"SCP_RC": "24"}, 24, "install"),
                                                ({"CHECK_RC": "25"}, 25, "install"),
                                                ({}, 0, "uninstall")):
                with self.subTest(overrides=overrides):
                    if action == "uninstall":
                        shutil.rmtree(build)
                    events.unlink(missing_ok=True)
                    result = subprocess.run(["/bin/bash", str(ROOT / "scripts/deploy_over_ssh.sh"),
                                             "root@fixture", action], env=dict(env, **overrides),
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
                    calls = [json.loads(line) for line in events.read_text().splitlines()]
                    if "CHECK_RC" in overrides:
                        self.assertEqual(len(calls), 1)  # local rejection: no transport
                        continue
                    self.assertEqual(calls[-1][0], "ssh")
                    self.assertTrue(calls[-1][-1].startswith("rm -rf '/"))
                    uploads = [c for c in calls if c[0] == "scp"]
                    self.assertEqual(len(uploads), 1 if "SCP_RC" in overrides or action == "uninstall" else 2)
                    selected = "uninstall_MoreIncredibleBash" if action == "uninstall" else "install_MoreIncredibleBash"
                    self.assertIn(str(ROOT / selected / "mod"), uploads[0])
                    if len(uploads) == 2:
                        self.assertIn(str(build / "carplay_hook.jar"), uploads[1])
                    installs = [c for c in calls if c[0] == "ssh" and c[-1].startswith("/bin/sh ")]
                    self.assertEqual(len(installs), 0 if "SCP_RC" in overrides else 1)
                    if expected:
                        self.assertNotIn("Completed.", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
