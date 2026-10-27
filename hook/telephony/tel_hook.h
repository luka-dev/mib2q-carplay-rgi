/*
 * CarPlay Telephony Hook Module
 *
 * Recents and favorites for the cluster telephone tab (iAP2 Communications
 * ListUpdates), signal/carrier from the stock CommunicationsUpdate, and the
 * hold and microphone mute commands.  Call state itself needs no
 * hook: stock DSICarplay.updateCallState already reaches Java.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef TEL_HOOK_H
#define TEL_HOOK_H

#include "../framework/hook_framework.h"

extern const hook_module_def_t tel_module_def;

/* Exposed for tests: builders and the ListUpdate parser are pure. */
size_t tel_build_start_list_updates(uint8_t* out, size_t out_max,
                                    uint16_t recents_max, uint16_t favorites_max);
/* HoldStatusUpdate / MuteStatusUpdate body: one bool status param. */
size_t tel_build_status(uint8_t* out, size_t out_max, bool on);

#define TEL_LIST_MAX      20
#define TEL_TEXT_MAX      64

typedef struct {
    uint16_t index;
    char a[TEL_TEXT_MAX];       /* param 1: RemoteID (number) */
    char b[TEL_TEXT_MAX];       /* param 2: DisplayName */
    uint8_t type;               /* recents only: 1 incoming, 2 outgoing, 3 missed */
    uint64_t timestamp;         /* recents only: UnixTimestamp */
    uint8_t occurrences;        /* recents only: combined calls, 0 = not sent */
} tel_entry_t;

typedef struct {
    bool count_set;             /* header param 2/7 present in this message */
    uint16_t count;
    size_t n;                   /* entries carried by this message (update) or list length (merged) */
    tel_entry_t e[TEL_LIST_MAX];
} tel_list_t;

typedef struct {
    tel_list_t recents, favorites;
} tel_lists_t;

/* Parses one 0x4171 ListUpdate payload (after the 6-byte message header).
 * Returns false on a malformed TLV chain. */
bool tel_parse_list_update(const uint8_t* payload, size_t len, tel_lists_t* out);

/* Folds one parsed message into the running lists.  iOS sends the whole capped
 * list on every change, split by size: the first message carries the count,
 * follow-ups only entries with their absolute index (accessoryd
 * CoreAccessories_18 ~7380-7420).  A count sets the length; an entry lands at
 * its index. */
void tel_merge_lists(tel_lists_t* last, const tel_lists_t* upd);

/* CommunicationsUpdate 0x4158, read-only (stock subscribes with params 0,1,2,4,5).
 * iOS 26.1 CoreAccessories_18: 0 SignalStrength u8 (0..5), 1 RegistrationStatus u8
 * (0 unknown, 1 not registered, 2 searching, 3 denied, 4 home, 5 roaming, 6 emergency
 * only), 2 AirplaneMode bool, 4 CarrierName string. */
typedef struct {
    bool has_signal, has_registration, has_airplane, has_carrier;
    uint8_t signal, registration;
    bool airplane;
    char carrier[TEL_TEXT_MAX];
} tel_comm_t;

bool tel_parse_comm_update(const uint8_t* payload, size_t len, tel_comm_t* out);
/* Updates only the params present in upd. */
void tel_merge_comm(tel_comm_t* last, const tel_comm_t* upd);

/* PowerUpdate 0xAE01 (MFi R32 95.15.2): BatteryChargingState param 5 (enum: 0 disabled,
 * 1 charging, 2 charged) and BatteryChargeLevel param 6 (uint16, 0-100).  Read-only. */
typedef struct {
    bool has_level, has_state;
    uint16_t level;
    uint8_t state;
} tel_power_t;

bool tel_parse_power_update(const uint8_t* payload, size_t len, tel_power_t* out);
void tel_merge_power(tel_power_t* last, const tel_power_t* upd);

/* Stock's StartPowerUpdates 0xAE00 payload plus the BatteryChargingState (5) and
 * BatteryChargeLevel (6) requests it lacks, every stock param kept.  Returns 0 when
 * stock already asks for both (nothing to add), on a malformed payload or overflow. */
size_t tel_build_power_union(const uint8_t* stock, size_t stock_len, uint8_t* out, size_t out_max);

#endif /* TEL_HOOK_H */
