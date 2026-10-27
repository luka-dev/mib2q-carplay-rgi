/*
 * CoreUtils OSStatus names - one list, three consumers.
 *
 * Every status the stock AirPlay/CarPlay code hands us is a CoreUtils
 * OSStatus, and the generic family is a closed range (kGenericErrorBase
 * -6700 .. kGenericErrorEnd -6779).  AP_STATUS_LIST below is the single
 * source of truth: it generates the constants used in our own return paths
 * (here), the " (kFooErr)" strings ap_status() appends to a log line
 * (logging.c), and the consistency check (tests/test_ap_status.c).  The Java
 * side carries the same table in java_patch/com/luka/carplay/core/ApStatus.java.
 *
 * The names are the ones compiled into the head unit's own libairplay.so; the
 * full picture, including the Bonjour/mDNS and errno families, is in
 * docs/reference/CARPLAY_STATUS_CODES.md.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef HOOK_AP_STATUS_H
#define HOOK_AP_STATUS_H

/* X(value, name), contiguous and descending from -6700.  ap_status() indexes
 * the generated table with (-6700 - status), so keep the order. */
#define AP_STATUS_LIST(X) \
    X(-6700, kUnknownErr           ) \
    X(-6701, kOptionErr            ) \
    X(-6702, kSelectorErr          ) \
    X(-6703, kExecutionStateErr    ) \
    X(-6704, kPathErr              ) \
    X(-6705, kParamErr             ) \
    X(-6706, kUserRequiredErr      ) \
    X(-6707, kCommandErr           ) \
    X(-6708, kIDErr                ) \
    X(-6709, kStateErr             ) \
    X(-6710, kRangeErr             ) \
    X(-6711, kRequestErr           ) \
    X(-6712, kResponseErr          ) \
    X(-6713, kChecksumErr          ) \
    X(-6714, kNotHandledErr        ) \
    X(-6715, kVersionErr           ) \
    X(-6716, kSignatureErr         ) \
    X(-6717, kFormatErr            ) \
    X(-6718, kNotInitializedErr    ) \
    X(-6719, kAlreadyInitializedErr) \
    X(-6720, kNotInUseErr          ) \
    X(-6721, kAlreadyInUseErr      ) \
    X(-6722, kTimeoutErr           ) \
    X(-6723, kCanceledErr          ) \
    X(-6724, kAlreadyCanceledErr   ) \
    X(-6725, kCannotCancelErr      ) \
    X(-6726, kDeletedErr           ) \
    X(-6727, kNotFoundErr          ) \
    X(-6728, kNoMemoryErr          ) \
    X(-6729, kNoResourcesErr       ) \
    X(-6730, kDuplicateErr         ) \
    X(-6731, kImmutableErr         ) \
    X(-6732, kUnsupportedDataErr   ) \
    X(-6733, kIntegrityErr         ) \
    X(-6734, kIncompatibleErr      ) \
    X(-6735, kUnsupportedErr       ) \
    X(-6736, kUnexpectedErr        ) \
    X(-6737, kValueErr             ) \
    X(-6738, kNotReadableErr       ) \
    X(-6739, kNotWritableErr       ) \
    X(-6740, kBadReferenceErr      ) \
    X(-6741, kFlagErr              ) \
    X(-6742, kMalformedErr         ) \
    X(-6743, kSizeErr              ) \
    X(-6744, kNameErr              ) \
    X(-6745, kNotPreparedErr       ) \
    X(-6746, kReadErr              ) \
    X(-6747, kWriteErr             ) \
    X(-6748, kMismatchErr          ) \
    X(-6749, kDateErr              ) \
    X(-6750, kUnderrunErr          ) \
    X(-6751, kOverrunErr           ) \
    X(-6752, kEndingErr            ) \
    X(-6753, kConnectionErr        ) \
    X(-6754, kAuthenticationErr    ) \
    X(-6755, kOpenErr              ) \
    X(-6756, kTypeErr              ) \
    X(-6757, kSkipErr              ) \
    X(-6758, kNoAckErr             ) \
    X(-6759, kCollisionErr         ) \
    X(-6760, kBackoffErr           ) \
    X(-6761, kAddressErr           ) \
    X(-6762, kInternalErr          ) \
    X(-6763, kNoSpaceErr           ) \
    X(-6764, kCountErr             ) \
    X(-6765, kEndOfDataErr         ) \
    X(-6766, kWouldBlockErr        ) \
    X(-6767, kLookErr              ) \
    X(-6768, kSecurityRequiredErr  ) \
    X(-6769, kOrderErr             ) \
    X(-6770, kUpgradeErr           ) \
    X(-6771, kAsyncNoErr           ) \
    X(-6772, kDeprecatedErr        ) \
    X(-6773, kPermissionErr        ) \
    X(-6774, kReadWouldBlockErr    ) \
    X(-6775, kWriteWouldBlockErr   )

/* Apple's CommonServices.h defines the same names as macros.  It is in scope
 * only in the vendored, symbol-namespaced pairing translation units; there it
 * wins and we add nothing, which also keeps the two spellings from colliding. */
#ifndef kGenericErrorBase
#define kGenericErrorBase (-6700)
#define kGenericErrorEnd  (-6779)
#define AP_STATUS_ENUM(v, n) n = (v),
enum { AP_STATUS_LIST(AP_STATUS_ENUM) };
#undef AP_STATUS_ENUM
#endif

/* " (kFooErr)" for a status in the generic family, "" for anything else -
 * including 0 - so it can be appended to any log line that carries a status:
 *
 *     LOG_INFO(LOG_MODULE, "session command '%s' ack: status=%d%s",
 *              type, status, ap_status(status));
 */
const char *ap_status(int status);

#endif /* HOOK_AP_STATUS_H */
