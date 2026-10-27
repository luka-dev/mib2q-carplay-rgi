# Stock HMI classes per firmware

Each jar is the firmware's `lsd.jxe` (`ifs_hmi` image) run through `jxe2jar`: `src/jxe2jar.py` convert, `tools/combine.sh --appimg` with that same release's own app partition (`eso/bundles`, `eso/bundles_prod`, `eso/hmi/lsd/jars`), no un-inlining (the uninlined `*-final.jar` in `Tools/jxe2jar/out/` is only for decompiling). They are for compiling and for checking `carplay_hook.jar`, which replaces whole stock classes, against other trains (`tests/JavaStockLinkageAudit.java`).

| Jar | Firmware | `/ifs/lsd.jxe` size |
|---|---|---:|
| `US_MU1316_lsd.jar` | MHI2Q_US_AUG22_P5087 (MU1316) | 60604861 |
| `EU_MU1329_lsd.jar` | MHI2Q_ER_AUG22_P5092 (MU1329) | 60608471 |
| `CN_MU1003_lsd.jar` | MHI2Q_CN_AUG22_K1004 (MU1003) | 60285367 |