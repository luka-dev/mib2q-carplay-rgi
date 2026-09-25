# CN delta for the Java patch

`java_patch/` replaces whole stock HMI classes. Six of them differ on the Chinese firmware (MHI2Q_CN_AUG22_K1004, MU1003). Each `.patch` here is only that stock difference, applied on top of our `java_patch/` file. It carries none of our own changes:

| Patch | CN stock difference |
|---|---|
| `PartialPopupManagerEvoHigh.patch` | superclass `PartialPopupManager` instead of `AbstractPartialPopupManager` |
| `DisplayManagerMIB2High.patch` | extra display context `dc[34] = {16, 50, 51}` |
| `CarplayDSILifecycleController.patch` | `IDSIResource` getters take no argument; transfer priority is `flag ? 2 : 1` |
| `ClusterService.patch` | no sat-map provider flag, no `updateDestinationInfo(null, 0, 0)` on route end |
| `HighPriorityResourceTracker.patch` | no `ISmartphoneProperties` (constructor, `MURVCActive`), no non-CarPlay parking branch, no emergency-number methods in the eCall stub |
| `ExternalEventsListener.patch` | no `ISmartphoneProperties` (constructor, `MUHFPPhonecallActive`) |

`scripts/build_java.sh` copies `java_patch/`, applies these with `patch -p1 -F0` and compiles against `java_stock/CN_MU1003_lsd.jar` into `build/carplay_hook_cn.jar`. `-F0` means a hunk whose context changed fails the build instead of landing in the wrong place. `scripts/audit_java_variants.sh` then requires 0 linkage errors for each jar on its own firmware.

## When a patch stops applying

An edit in `java_patch/` touched lines next to a CN difference. Regenerate that one patch:

```sh
C=de/audi/tghu/navi/app/cluster/ClusterService      # the class named in the build error
cp java_patch/$C.java /tmp/cn.java
patch /tmp/cn.java < java_patch_cn/$(basename $C).patch   # apply what fits, then finish the .rej by hand
diff -u --label a/$C.java --label b/$C.java java_patch/$C.java /tmp/cn.java > java_patch_cn/$(basename $C).patch
```

The CN stock sources for comparison are the decompile in `Tools/jxe2jar/out/MU1003CN-vf/`; the US ones are in `MU1316-vf/`.
