# Pristine stock PF rulesets (MHI2Q MU1316)

Untouched originals pulled off the unit before `install_persistent_pf.sh` widened the carplay0 TCP
rule to `6030:6031` (altScreen). Kept here so a rollback never depends on on-unit `.stock` copies —
those were deleted from the unit to avoid clutter.

| file | bytes |
|---|---|
| pf.conf | 10474 |
| pf.ecm0.conf | 11095 |
| pf.mlan0.conf | 11460 |
| pf.ppp0.conf | 10450 |

All four carry the stock single-port rule:
`pass in quick on carplay0 proto tcp from any to any port { …, 6030, … }`

## Why four
The live ruleset is chosen per external interface. On a car with the LTE modem the active profile is
**`pf.ecm0.conf`**, NOT `pf.conf` — patching only `pf.conf` silently leaves 6031 blocked and altScreen
never starts. Always verify the ACTIVE ruleset, not the file:

```
pfctl -sr | grep -E 'carplay0.*603'     # want: port 6030:6031, not "port = 6030"
```

## Rollback (per profile)
```
mount -uw /mnt/system
cat pf.ecm0.conf | ssh root@HU 'cat > /mnt/system/etc/pf.ecm0.conf'
# takes effect on next boot, or: pfctl -f /mnt/system/etc/pf.ecm0.conf
```

## Note on pf.ppp0.conf
`pfctl -n -f pf.ppp0.conf` reports `SIOCGIFMTU: No such device or address` — the box has no ppp0.
This is TRUE OF THE STOCK FILE TOO (verified by parsing the pristine copy); it is not caused by the
6031 patch.
