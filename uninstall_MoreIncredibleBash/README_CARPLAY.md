# Remove CarPlay integration through M.I.B.

For **MHI2Q_US_AUG22_P5087 / MU1316**.

1. Copy the **contents** of this folder over the root of the M.I.B. SD card,
   replacing `/mod/custom.sh` with this removal script.
2. Disconnect CarPlay/iPhone, insert the card in **SD1**, then run
   **GEM → M.I.B. → Advanced Settings → Run Custom Script**.
3. Wait for **DONE**, then reboot the HU manually.

No build download or `UNINSTALL` marker is required. M.I.B.'s generic
“execution finished” message is not a success indicator; check **DONE/FAILED**.

The script restores the original SI JSON from `.orig`, removes our runtime
files/JAR and restores SI/DEVA images backed up by our installer as
`.carplay-stock`. Patches already present before installation are retained.
Persistent PF port widening is retained. If a required backup is missing or
invalid, it refuses rather than deleting files needed by the current config.

Logs: `/tmp/carplay_uninstall.log` and `/mod/last-carplay-uninstall.log`.
No processes are stopped and no automatic reboot occurs.

The single removal script is generated from the install script by
`python3 scripts/package_mib.py`, keeping common checks identical.
