/*
 * ApStatus - CoreUtils OSStatus names, so a logged status reads as a name.
 *
 * The generic error family is a closed range (kGenericErrorBase -6700 ..
 * kGenericErrorEnd -6779), so one flat table indexed by (-6700 - status)
 * decodes it.  The names are the ones compiled into the head unit's own
 * libairplay.so; the full table, including the Bonjour and errno families,
 * is in docs/reference/CARPLAY_STATUS_CODES.md.
 *
 * suffix() returns a ready-to-append " (kFooErr)" and an empty string for
 * anything it does not know, so a log line can carry it unconditionally.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.core;

public final class ApStatus {
    private static final int FIRST = -6700;

    private static final String[] NAMES = {
        "kUnknownErr",             // -6700
        "kOptionErr",              // -6701
        "kSelectorErr",            // -6702
        "kExecutionStateErr",      // -6703
        "kPathErr",                // -6704
        "kParamErr",               // -6705
        "kUserRequiredErr",        // -6706
        "kCommandErr",             // -6707
        "kIDErr",                  // -6708
        "kStateErr",               // -6709
        "kRangeErr",               // -6710
        "kRequestErr",             // -6711
        "kResponseErr",            // -6712
        "kChecksumErr",            // -6713
        "kNotHandledErr",          // -6714
        "kVersionErr",             // -6715
        "kSignatureErr",           // -6716
        "kFormatErr",              // -6717
        "kNotInitializedErr",      // -6718
        "kAlreadyInitializedErr",  // -6719
        "kNotInUseErr",            // -6720
        "kAlreadyInUseErr",        // -6721
        "kTimeoutErr",             // -6722
        "kCanceledErr",            // -6723
        "kAlreadyCanceledErr",     // -6724
        "kCannotCancelErr",        // -6725
        "kDeletedErr",             // -6726
        "kNotFoundErr",            // -6727
        "kNoMemoryErr",            // -6728
        "kNoResourcesErr",         // -6729
        "kDuplicateErr",           // -6730
        "kImmutableErr",           // -6731
        "kUnsupportedDataErr",     // -6732
        "kIntegrityErr",           // -6733
        "kIncompatibleErr",        // -6734
        "kUnsupportedErr",         // -6735
        "kUnexpectedErr",          // -6736
        "kValueErr",               // -6737
        "kNotReadableErr",         // -6738
        "kNotWritableErr",         // -6739
        "kBadReferenceErr",        // -6740
        "kFlagErr",                // -6741
        "kMalformedErr",           // -6742
        "kSizeErr",                // -6743
        "kNameErr",                // -6744
        "kNotPreparedErr",         // -6745
        "kReadErr",                // -6746
        "kWriteErr",               // -6747
        "kMismatchErr",            // -6748
        "kDateErr",                // -6749
        "kUnderrunErr",            // -6750
        "kOverrunErr",             // -6751
        "kEndingErr",              // -6752
        "kConnectionErr",          // -6753
        "kAuthenticationErr",      // -6754
        "kOpenErr",                // -6755
        "kTypeErr",                // -6756
        "kSkipErr",                // -6757
        "kNoAckErr",               // -6758
        "kCollisionErr",           // -6759
        "kBackoffErr",             // -6760
        "kAddressErr",             // -6761
        "kInternalErr",            // -6762
        "kNoSpaceErr",             // -6763
        "kCountErr",               // -6764
        "kEndOfDataErr",           // -6765
        "kWouldBlockErr",          // -6766
        "kLookErr",                // -6767
        "kSecurityRequiredErr",    // -6768
        "kOrderErr",               // -6769
        "kUpgradeErr",             // -6770
        "kAsyncNoErr",             // -6771
        "kDeprecatedErr",          // -6772
        "kPermissionErr",          // -6773
        "kReadWouldBlockErr",      // -6774
        "kWriteWouldBlockErr",     // -6775
    };

    private ApStatus() {
    }

    /** " (kFooErr)" for a CoreUtils generic status, "" for anything else. */
    public static String suffix(int status) {
        // Range-check before the subtraction so no int can overflow it.
        if (status > FIRST || status < FIRST - NAMES.length + 1) {
            return "";
        }
        return " (" + NAMES[FIRST - status] + ")";
    }
}
