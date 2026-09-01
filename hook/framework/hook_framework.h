/*
 * CarPlay Hook Framework - Main API
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
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

/* Hook Module Definition.
 *
 * Everything a module needs from the framework is declared here; the framework
 * knows no module by name.  Lifecycle, the iAP2 message/Identify/state seams
 * and the raw transport taps all come from this one table. */
typedef struct {
    const char* name;
    hook_priority_t priority;
    const uint16_t* msg_filter;
    size_t msg_filter_count;
    hook_msg_handler_t on_message;
    hook_identify_patcher_t on_identify;
    hook_state_callback_t on_state;
    hook_transport_callback_t on_transport_send;  /* Called on outgoing transport frames */

    /* Lifecycle.  on_init runs once from the framework's first real Cinemo
     * boundary (never an ELF constructor); on_shutdown runs from the framework
     * destructor in reverse table order, before the injection worker and bus
     * are stopped. */
    void (*on_init)(void);
    void (*on_shutdown)(void);

    /* Raw NmeTransport::Recv link bytes, and a session-boundary reset fired on
     * a new Identify so partial reassembly cannot cross sessions. */
    hook_transport_recv_sink_t on_transport_recv;
    hook_transport_recv_reset_t on_transport_recv_reset;

    void* user_data;
} hook_module_def_t;

/* The shipping module set, in initialisation order (hook/main.c).  This is the
 * only place a module is named; adding one is a line there plus its own def. */
extern const hook_module_def_t* const hook_module_table[];
extern const size_t hook_module_table_count;

/* Stock iAP2 passthrough context.  The link-session id is captured from the
 * most recent stock NmeTransport::Send.  The live ICinemoIAP handle is owned
 * separately by the framework (with a COM AddRef) and SendIAP2 allocates fresh
 * FF5A sequence/ack/checksums inside the stock Cinemo stack. */
typedef struct {
    uint8_t link_session;
    uint32_t generation;
    bool valid;
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

    /* Extra-message injection state. */
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

/* Injection API - queues an additional semantic frame for the dedicated
 * ICinemoIAP::SendIAP2 worker; it never replaces a stock semantic message,
 * copies FF5A state, or blocks the caller in stock transport code. */
hook_result_t hook_inject_frame(const uint8_t* frame, size_t len);
hook_result_t hook_inject_message(uint16_t msgid, const uint8_t* payload, size_t payload_len);

/* State Query */
bool hook_is_ready(void);
bool hook_is_active(void);
uint16_t hook_get_component_id(void);

#endif /* CARPLAY_HOOK_FRAMEWORK_H */
