# Install CarPlay integration through M.I.B.

For **MHI2Q_US_AUG22_P5087 / MU1316 (MMX2QC)**.

1. Prepare a FAT32 SD card with **More Incredible Bash**, and install M.I.B. on
   the HU using its own instructions if it is not installed yet.
2. Copy the **contents** of this folder over the card root. The script must be
   at `/mod/custom.sh`.
3. Download the matching release artifacts and put them in `/build/` on the
   card, including `release.json`, `CKSUMS` and `SHA256SUMS`. During development,
   use the prepared files listed in `build/SHA256SUMS`; the build directory also
   contains unrelated development output, so do not copy it wholesale.
4. Disconnect the iPhone/CarPlay. Insert the card in **SD1** and open
   **GEM → M.I.B. → Advanced Settings → Run Custom Script**.
5. Wait for **DONE**, then reboot the HU manually. M.I.B.'s generic “execution
   finished” message also appears after failures: check our **DONE/FAILED** line.

```text
SD1/
  mod/custom.sh
  build/libcarplay_hook.so
  build/carplay_hook.jar
  build/...                     remaining release artifacts
```

One `custom.sh` performs all checks, mounts and installation. It refuses an
active `dio_manager`, verifies the firmware and file checksums, stages complete
files and atomically replaces each destination. The **SI JSON is published
last**, after its dependencies have been flushed. The whole multi-partition
installation is not a power-loss transaction; keep CarPlay disconnected until
it finishes. No process is stopped and no automatic reboot occurs.

The prepared files include the hook/APAC codec, model, renderers, atlas, runtime
scripts, JAR, `carplay_child.json`, final SI re-arm binary and final DEVA mirror
driver. The PF edit opens `carplay0` TCP `6030:6032` in all present stock profiles.
Unknown SI/DEVA versions are refused before writes. Original SI JSON is retained
as `.orig`, PF profiles as `.stock`, and changed firmware images as `.carplay-stock`.
Previously installed final firmware patches are kept without taking ownership.

Logs: `/tmp/carplay_install.log` and `/mod/last-carplay-install.log`.

For removal, copy the contents of **`uninstall_MoreIncredibleBash/`** over the
card instead. Its `custom.sh` removes the integration without a build download.

Host entry points:

```sh
python3 scripts/package_mib.py            # prepare loose artifacts; no ZIP
python3 scripts/package_mib.py --check
./scripts/deploy_over_ssh.sh root@10.173.189.1
./scripts/deploy_over_ssh.sh root@10.173.189.1 check
./scripts/deploy_over_ssh.sh root@10.173.189.1 uninstall
```

SSH uses keys/agent or `SSHPASS` with `sshpass`, uploads only the release
allowlist and invokes the selected `custom.sh`. `check` is preflight only.
The artifact preparer preserves existing compiled outputs; it does not rebuild
or deploy them. See `docs/RELEASE_ARTIFACTS.md` for the file list and target paths.
