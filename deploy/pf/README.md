# PF rulesets (MHI2Q MU1316) — stock + patched + recipe

The only CarPlay change to the firewall is the carplay0 inbound TCP rule: port
`6030` becomes the range `6030:6032` (6031 altScreen, 6032 buffered audio).

- `stock/` — pristine originals pulled off the unit, single-port `6030`. A
  rollback never depends on on-unit `.stock` copies (those were deleted to avoid
  clutter).
- `patched/` — the widened result, `6030:6032`. **Reference/A-B only.** The
  installer does NOT copy these: `install_MoreIncredibleBash/mod/custom.sh` makes
  the same edit in-shell on the live profiles (so it needs no `patch`/sed on the
  HU).
- `patch_pf.sh` — the recipe: regenerates `patched/` from `stock/` with the exact
  transform custom.sh applies (asserts one carplay0 rule per profile).

```
./patch_pf.sh                          # stock/ -> patched/
diff stock/pf.ecm0.conf patched/pf.ecm0.conf   # one line: 6030 -> 6030:6032
```

## Why four profiles
The live ruleset is chosen per external interface. On a car with the LTE modem
the active profile is **`pf.ecm0.conf`**, NOT `pf.conf` — and the selected
profile can change. All four must carry the widened rule; patching only
`pf.conf` can silently leave 6031/6032 blocked. Verify the ACTIVE ruleset:

```
pfctl -sr | grep -E 'carplay0.*603'     # want: port 6030:6032
```

## Rollback (per profile)
```
mount -uw /mnt/system
cat stock/pf.ecm0.conf | ssh root@HU 'cat > /mnt/system/etc/pf.ecm0.conf'
# takes effect on next boot, or: pfctl -f /mnt/system/etc/pf.ecm0.conf
```

## Note on pf.ppp0.conf
`pfctl -n -f pf.ppp0.conf` reports `SIOCGIFMTU: No such device or address` — the
box has no ppp0. This is TRUE OF THE STOCK FILE TOO (verified by parsing the
pristine copy); it is not caused by the port-range patch.
