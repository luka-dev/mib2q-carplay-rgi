/*
 * CarPlay Hook Framework - Main API
 */

#ifndef CARPLAY_HOOK_FRAMEWORK_H
#define CARPLAY_HOOK_FRAMEWORK_H

#include "common.h"
#include "logging.h"
#include "bus.h"
#include "iap2_protocol.h"

typedef struct hook_module hook_module_t;
typedef struct hook_context hook_context_t;

/* Callback Types */
typedef bool (*hook_msg_handler_t)(hook_context_t* ctx, const iap2_frame_t* frame);
typedef size_t (*hook_identify_patcher_t)(hook_context_t* ctx, uint8_t* buf, size_t len, size_t max_len);
typedef void (*hook_state_callback_t)(hook_context_t* ctx, int event, void* event_data);

/* Called when transport sends a frame - for injection triggers */
typedef void (*hook_transport_callback_t)(hook_context_t* ctx, uint16_t msgid);

/* State Events */
#define HOOK_EVENT_INIT             1
#define HOOK_EVENT_SHUTDOWN         2
#define HOOK_EVENT_DISCONNECT       3
#define HOOK_EVENT_IDENTIFY_START   5
#define HOOK_EVENT_IDENTIFY_OK      6
#define HOOK_EVENT_IDENTIFY_END     7
#define HOOK_EVENT_AUTH_DONE        8

/* Hook Module Definition */
typedef struct {
    const char* name;
    hook_priority_t priority;
    const uint16_t* msg_filter;
    size_t msg_filter_count;
    hook_msg_handler_t on_message;
    hook_identify_patcher_t on_identify;
    hook_state_callback_t on_state;
    hook_transport_callback_t on_transport_send;  /* Called on outgoing transport frames */
    void* user_data;
} hook_module_def_t;

/* Link-layer injection context.
 *
 * iAP2 link-layer checksum is fixed at "negated 8-bit sum" by Apple's
 * spec (R12+). We don't carry an algo selector - calculation always
 * uses iap2_cksum_neg() directly. */
typedef struct {
    uint8_t* prefix_buf;        /* Stored link header prefix */
    size_t prefix_len;          /* Prefix length */
    size_t original_total_len;  /* Original total frame length */
    void* transport_self;       /* NmeTransport* for injection */
    bool valid;                 /* Context is valid for injection */
} injection_ctx_t;

/* Hook Context */
struct hook_context {
    msg_direction_t direction;
    uint16_t msgid;
    const uint8_t* raw_buf;
    size_t raw_len;

    /* Session state */
    bool identify_patched;
    bool identify_accepted;
    bool auth_done;
    bool session_active;

    /* Component info */
    uint16_t rgd_component_id;
    bool rgd_component_valid;

    /* Injection context */
    injection_ctx_t inject;

    /* Module being called */
    hook_module_t* current_module;

    void* _priv;
};

/* Framework API */
hook_result_t hook_framework_init(void);
void hook_framework_shutdown(void);
hook_result_t hook_framework_register_module(const hook_module_def_t* def);
hook_result_t hook_framework_unregister_module(const char* name);
hook_context_t* hook_framework_get_context(void);

/* Injection API - uses stored link-layer context */
hook_result_t hook_inject_frame(const uint8_t* frame, size_t len);
hook_result_t hook_inject_message(uint16_t msgid, const uint8_t* payload, size_t payload_len);

/* State Query */
bool hook_is_ready(void);
bool hook_is_active(void);
uint16_t hook_get_component_id(void);

#endif /* CARPLAY_HOOK_FRAMEWORK_H */
