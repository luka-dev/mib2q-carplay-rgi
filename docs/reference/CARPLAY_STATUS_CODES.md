# AirPlay / CarPlay status codes

Every `status=`, `result=` and `rc=` the hook logs for a session command, a control-channel
command or a `SetProperty` is a CoreUtils `OSStatus`. The same numbering is used on both sides,
so a code the phone returns decodes from the same table.

Sources, all three agreeing: the error table compiled into the head unit's own
`/mnt/app/eso/lib/libairplay.so` (records `{int32 code, char *name}` at vaddr `0xb7848`, 374
entries, terminated by `{0, NULL}`), the AirPlay Accessory SDK header `CommonServices.h`, and
the CoreUtils table inside the iOS 27 dyld shared cache. Zero name mismatches between them.

## In the code

The generic family lives in one list, `AP_STATUS_LIST` in `hook/framework/ap_status.h`, and
everything else is generated from it:

| what | where |
|---|---|
| the constants our own return paths use (`return kUnsupportedErr;`) | the guarded `enum` in `hook/framework/ap_status.h` |
| `ap_status(status)`, the `" (kFooErr)"` a log line appends | `hook/framework/logging.c` |
| the ordering check that keeps the two in step | `tests/test_ap_status.c` |
| the same table for Java | `java_patch/com/luka/carplay/core/ApStatus.java` |
| the dumper that reads the names out of a firmware image | `tools/dump_errtab.py` |

Apple's `CommonServices.h` defines the same names as macros. It is in scope only in the
vendored, symbol-namespaced pairing translation units; there it wins and the header adds
nothing, which is what the `#ifndef kGenericErrorBase` guard is for.

## Generic errors (−6700 … −6779)

`kGenericErrorBase = -6700`, `kGenericErrorEnd = -6779`, so the range is capped: −6776 … −6779
are unassigned and nothing outside −6700 … −6779 belongs to this family.

The last column is filled in for the codes we have actually met, with where.

| code | name | meaning | met it where |
|---|---|---|---|
| `-6700` | `kUnknownErr` | Unknown error occurred. |  |
| `-6701` | `kOptionErr` | Option was not acceptable. |  |
| `-6702` | `kSelectorErr` | Selector passed in is invalid or unknown. |  |
| `-6703` | `kExecutionStateErr` | Call made in the wrong execution state (e.g. called at interrupt time). |  |
| `-6704` | `kPathErr` | Path is invalid, too long, or otherwise not usable. |  |
| `-6705` | `kParamErr` | Parameter is incorrect, missing, or not appropriate. |  |
| `-6706` | `kUserRequiredErr` | User interaction is required. |  |
| `-6707` | `kCommandErr` | Command invalid or not supported. |  |
| `-6708` | `kIDErr` | Unknown, invalid, or inappropriate identifier. |  |
| `-6709` | `kStateErr` | Not in appropriate state to perform operation. | returned by stock 210 `wireless_setup` when no connection exists |
| `-6710` | `kRangeErr` | Index is out of range or not valid. |  |
| `-6711` | `kRequestErr` | Request was improperly formed or not appropriate. |  |
| `-6712` | `kResponseErr` | Response was incorrect or out of sequence. |  |
| `-6713` | `kChecksumErr` | Checksum does not match the actual data. |  |
| `-6714` | `kNotHandledErr` | Operation was not handled (or not handled completely). | the phone answered our `suggestUI` with it |
| `-6715` | `kVersionErr` | Version is not correct or not compatible. | our `alt_setup` returns it when `streamConnectionID` is absent |
| `-6716` | `kSignatureErr` | Signature did not match what was expected. |  |
| `-6717` | `kFormatErr` | Unknown, invalid, or inappropriate file/data format. |  |
| `-6718` | `kNotInitializedErr` | Action request before needed services were initialized. |  |
| `-6719` | `kAlreadyInitializedErr` | Attempt made to initialize when already initialized. | our `alt_setup` returns it on a second setup / bind failure |
| `-6720` | `kNotInUseErr` | Object not in use (e.g. cannot abort if not already in use). |  |
| `-6721` | `kAlreadyInUseErr` | Object is in use (e.g. cannot reuse active param blocks). |  |
| `-6722` | `kTimeoutErr` | Timeout occurred. |  |
| `-6723` | `kCanceledErr` | Operation canceled (successful cancel). |  |
| `-6724` | `kAlreadyCanceledErr` | Operation has already been canceled. |  |
| `-6725` | `kCannotCancelErr` | Operation could not be canceled (maybe already done or invalid). |  |
| `-6726` | `kDeletedErr` | Object has already been deleted. |  |
| `-6727` | `kNotFoundErr` | Something was not found. |  |
| `-6728` | `kNoMemoryErr` | Not enough memory was available to perform the operation. | our altScreen paths return it on allocation failure |
| `-6729` | `kNoResourcesErr` | Resources unavailable to perform the operation. |  |
| `-6730` | `kDuplicateErr` | Duplicate found or something is a duplicate. |  |
| `-6731` | `kImmutableErr` | Entity is not changeable. |  |
| `-6732` | `kUnsupportedDataErr` | Data is unknown or not supported. |  |
| `-6733` | `kIntegrityErr` | Data is corrupt. |  |
| `-6734` | `kIncompatibleErr` | Data is not compatible or it is in an incompatible format. |  |
| `-6735` | `kUnsupportedErr` | Feature or option is not supported. | the hook answers a viewArea request that arrives before the session exists |
| `-6736` | `kUnexpectedErr` | Error occurred that was not expected. |  |
| `-6737` | `kValueErr` | Value is not appropriate. |  |
| `-6738` | `kNotReadableErr` | Could not read or reading is not allowed. |  |
| `-6739` | `kNotWritableErr` | Could not write or writing is not allowed. |  |
| `-6740` | `kBadReferenceErr` | An invalid or inappropriate reference was specified. | iOS tears the session down over a display it cannot serve |
| `-6741` | `kFlagErr` | An invalid, inappropriate, or unsupported flag was specified. |  |
| `-6742` | `kMalformedErr` | Something was not formed correctly. |  |
| `-6743` | `kSizeErr` | Size was too big, too small, or not appropriate. |  |
| `-6744` | `kNameErr` | Name was not correct, allowed, or appropriate. |  |
| `-6745` | `kNotPreparedErr` | Device or service is not ready. |  |
| `-6746` | `kReadErr` | Could not read. |  |
| `-6747` | `kWriteErr` | Could not write. |  |
| `-6748` | `kMismatchErr` | Something does not match. |  |
| `-6749` | `kDateErr` | Date is invalid or out-of-range. |  |
| `-6750` | `kUnderrunErr` | Less data than expected. |  |
| `-6751` | `kOverrunErr` | More data than expected. |  |
| `-6752` | `kEndingErr` | Connection, session, or something is ending. |  |
| `-6753` | `kConnectionErr` | Connection failed or could not be established. |  |
| `-6754` | `kAuthenticationErr` | Authentication failed or is not supported. |  |
| `-6755` | `kOpenErr` | Could not open file, pipe, device, etc. |  |
| `-6756` | `kTypeErr` | Incorrect or incompatible type (e.g. file, data, etc.). |  |
| `-6757` | `kSkipErr` | Items should be or was skipped. |  |
| `-6758` | `kNoAckErr` | No acknowledge. |  |
| `-6759` | `kCollisionErr` | Collision occurred (e.g. two on bus at same time). |  |
| `-6760` | `kBackoffErr` | Backoff in progress and operation intentionally failed. |  |
| `-6761` | `kAddressErr` | Bad address or no acknowledge of address. |  |
| `-6762` | `kInternalErr` | An error internal to the implementation occurred. |  |
| `-6763` | `kNoSpaceErr` | Not enough space to perform operation. |  |
| `-6764` | `kCountErr` | Count is incorrect. |  |
| `-6765` | `kEndOfDataErr` | Reached the end of the data (e.g. recv returned 0). |  |
| `-6766` | `kWouldBlockErr` | Would need to block to continue (e.g. non-blocking read/write). |  |
| `-6767` | `kLookErr` | Special case that needs to be looked at (e.g. interleaved data). |  |
| `-6768` | `kSecurityRequiredErr` | Security is required for the operation (e.g. must use encryption). |  |
| `-6769` | `kOrderErr` | Order is incorrect. |  |
| `-6770` | `kUpgradeErr` | Must upgrade. |  |
| `-6771` | `kAsyncNoErr` | Async operation successfully started and is now in progress. |  |
| `-6772` | `kDeprecatedErr` | Operation or data is deprecated. |  |
| `-6773` | `kPermissionErr` | Permission denied. |  |
| `-6774` | `kReadWouldBlockErr` | Needs more incoming data. |  |
| `-6775` | `kWriteWouldBlockErr` | Needs more buffer space to write data. |  |

`-6774 kReadWouldBlockErr` and `-6775 kWriteWouldBlockErr` are in the SDK header but absent
from the head unit's table, so that build predates them; everything else matches one to one.

iOS 27 carries the same 74-name sequence from `kUnknownErr` to `kPermissionErr`, with two slots
spelled differently: `kParamCountErr` where the SDK has `kUserRequiredErr` (−6706) and
`kNoAddressAckErr` where it has `kAddressErr` (−6761). That pairing is read off the layout order
of the name strings in the shared cache, not off a decoded record table, so treat the two
renames as inferred; the codes themselves are not in doubt.

## Other families in the same table

| range | family | count |
|---|---|---|
| −65537 … −65568 | `kDNSServiceErr_*` (Bonjour) | 32 |
| −65787 … −65792 | `mStatus_*` (mDNSResponder internal) | 6 |
| ±1 … ±313 | QNX `errno` names, stored under both signs | 130 |

`#define kNoErr 0` prints as `noErr`; `-1` prints as the literal string
`catch-all unknown error`.

### Bonjour / mDNS

| code | name |
|---|---|
| `-65537` | `kDNSServiceErr_Unknown` |
| `-65538` | `kDNSServiceErr_NoSuchName` |
| `-65539` | `kDNSServiceErr_NoMemory` |
| `-65540` | `kDNSServiceErr_BadParam` |
| `-65541` | `kDNSServiceErr_BadReference` |
| `-65542` | `kDNSServiceErr_BadState` |
| `-65543` | `kDNSServiceErr_BadFlags` |
| `-65544` | `kDNSServiceErr_Unsupported` |
| `-65545` | `kDNSServiceErr_NotInitialized` |
| `-65546` | `mStatus_NoCache` |
| `-65547` | `kDNSServiceErr_AlreadyRegistered` |
| `-65548` | `kDNSServiceErr_NameConflict` |
| `-65549` | `kDNSServiceErr_Invalid` |
| `-65550` | `kDNSServiceErr_Firewall` |
| `-65551` | `kDNSServiceErr_Incompatible` |
| `-65552` | `kDNSServiceErr_BadInterfaceIndex` |
| `-65553` | `kDNSServiceErr_Refused` |
| `-65554` | `kDNSServiceErr_NoSuchRecord` |
| `-65555` | `kDNSServiceErr_NoAuth` |
| `-65556` | `kDNSServiceErr_NoSuchKey` |
| `-65557` | `kDNSServiceErr_NATTraversal` |
| `-65558` | `kDNSServiceErr_DoubleNAT` |
| `-65559` | `kDNSServiceErr_BadTime` |
| `-65560` | `kDNSServiceErr_BadSig` |
| `-65561` | `kDNSServiceErr_BadKey` |
| `-65562` | `kDNSServiceErr_Transient` |
| `-65563` | `kDNSServiceErr_ServiceNotRunning` |
| `-65564` | `kDNSServiceErr_NATPortMappingUnsupported` |
| `-65565` | `kDNSServiceErr_NATPortMappingDisabled` |
| `-65566` | `kDNSServiceErr_NoRouter` |
| `-65567` | `kDNSServiceErr_PollingMode` |
| `-65568` | `kDNSServiceErr_Timeout` |
| `-65787` | `mStatus_ConnPending` |
| `-65788` | `mStatus_ConnFailed` |
| `-65789` | `mStatus_ConnEstablished` |
| `-65790` | `mStatus_GrowCache` |
| `-65791` | `mStatus_ConfigChanged` |
| `-65792` | `mStatus_MemFree` |

### QNX errno (both `+n` and `−n` decode to the same name)

| code | name | code | name | code | name | code | name |
|---|---|---|---|---|---|---|---|
| `1` | `EPERM` | `35` | `ENOMSG` | `69` | `ESRMNT` | `246` | `EPFNOSUPPORT` |
| `2` | `ENOENT` | `36` | `EIDRM` | `70` | `ECOMM` | `247` | `EAFNOSUPPORT` |
| `3` | `ESRCH` | `37` | `ECHRNG` | `71` | `EPROTO` | `248` | `EADDRINUSE` |
| `4` | `EINTR` | `38` | `EL2NSYNC` | `74` | `EMULTIHOP` | `249` | `EADDRNOTAVAIL` |
| `5` | `EIO` | `39` | `EL3HLT` | `77` | `EBADMSG` | `250` | `ENETDOWN` |
| `6` | `ENXIO` | `40` | `EL3RST` | `78` | `ENAMETOOLONG` | `251` | `ENETUNREACH` |
| `8` | `ENOEXEC` | `41` | `ELNRNG` | `79` | `EOVERFLOW` | `252` | `ENETRESET` |
| `9` | `EBADF` | `42` | `EUNATCH` | `80` | `ENOTUNIQ` | `253` | `ECONNABORTED` |
| `10` | `ECHILD` | `43` | `ENOCSI` | `81` | `EBADFD` | `254` | `ECONNRESET` |
| `11` | `EWOULDBLOCK` | `44` | `EL2HLT` | `82` | `EREMCHG` | `255` | `ENOBUFS` |
| `12` | `ENOMEM` | `45` | `EDEADLK` | `83` | `ELIBACC` | `256` | `EISCONN` |
| `13` | `EACCES` | `46` | `ENOLCK` | `84` | `ELIBBAD` | `257` | `ENOTCONN` |
| `14` | `EFAULT` | `47` | `ECANCELED` | `85` | `ELIBSCN` | `258` | `ESHUTDOWN` |
| `15` | `ENOTBLK` | `48` | `ENOTSUP` | `86` | `ELIBMAX` | `259` | `ETOOMANYREFS` |
| `16` | `EBUSY` | `49` | `EDQUOT` | `87` | `ELIBEXEC` | `260` | `ETIMEDOUT` |
| `17` | `EEXIST` | `50` | `EBADE` | `88` | `EILSEQ` | `261` | `ECONNREFUSED` |
| `18` | `EXDEV` | `51` | `EBADR` | `89` | `ENOSYS` | `264` | `EHOSTDOWN` |
| `19` | `ENODEV` | `52` | `EXFULL` | `90` | `ELOOP` | `265` | `EHOSTUNREACH` |
| `20` | `ENOTDIR` | `53` | `ENOANO` | `91` | `ERESTART` | `272` | `EBADRPC` |
| `21` | `EISDIR` | `54` | `EBADRQC` | `92` | `ESTRPIPE` | `273` | `ERPCMISMATCH` |
| `22` | `EINVAL` | `55` | `EBADSLT` | `93` | `ENOTEMPTY` | `274` | `EPROGUNAVAIL` |
| `23` | `ENFILE` | `56` | `EDEADLOCK` | `94` | `EUSERS` | `275` | `EPROGMISMATCH` |
| `24` | `EMFILE` | `57` | `EBFONT` | `103` | `EOPNOTSUPP` | `276` | `EPROCUNAVAIL` |
| `25` | `ENOTTY` | `58` | `EOWNERDEAD` | `110` | `EFPOS` | `300` | `ENOREMOTE` |
| `26` | `ETXTBSY` | `60` | `ENOSTR` | `122` | `ESTALE` | `301` | `ENONDP` |
| `27` | `EFBIG` | `61` | `ENODATA` | `236` | `EINPROGRESS` | `302` | `EBADFSYS` |
| `28` | `ENOSPC` | `62` | `ETIME` | `238` | `ENOTSOCK` | `309` | `EMORE` |
| `29` | `ESPIPE` | `63` | `ENOSR` | `239` | `EDESTADDRREQ` | `310` | `ECTRLTERM` |
| `30` | `EROFS` | `64` | `ENONET` | `240` | `EMSGSIZE` | `311` | `ENOLIC` |
| `31` | `EMLINK` | `65` | `ENOPKG` | `241` | `EPROTOTYPE` | `312` | `ESRVRFAULT` |
| `32` | `EPIPE` | `66` | `EREMOTE` | `242` | `ENOPROTOOPT` | `313` | `EENDIAN` |
| `33` | `EDOM` | `67` | `ENOLINK` | `243` | `EPROTONOSUPPORT` |  |  |
| `34` | `ERANGE` | `68` | `EADV` | `244` | `ESOCKTNOSUPPORT` |  |  |

## Positive codes: the OSStatus mapping bases

CoreUtils folds foreign low-numbered error spaces into positive `OSStatus` ranges, so a status
above 300000 is a foreign code plus a base. These are declared in `CommonServices.h` and are not
part of the name table, so they print as bare numbers:

| base | range | family |
|---|---|---|
| `300000` | 300000–300099 | HTTP/2 error codes (`kHTTP2Error_*`) |
| `300100` | 300100–300119 | CoreBluetooth (`CBError`) |
| `300120` | 300120–300139 | CoreBluetooth ATT (`CBATTError`) |
| `300140` | 300140–300169 | CFNetwork |
| `300200` | 300200–300299 | HomeKit |
| `300300` | 300300–300399 | POSIX signal numbers |

## Reproducing the table

```
# the table in the head unit's own library
python3 tools/dump_errtab.py <firmware>/mnt/app/eso/lib/libairplay.so 0xb6838
```
The anchor is found by looking for the record whose code is `-6700` and whose pointer resolves
to the string `kUnknownErr`, then walking back two records to `{0, "noErr"}`.
