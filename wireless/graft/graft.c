/* Offline MHI2Q btstack graft prototype. Never compile this for deployment
 * without the binary/ABI checks in build_offline.py and a separate review.
 * Fixed addresses belong to target SHA-256 154c17826f0773a57b2004f5b17d818e3437c46fcbb6a857d40c696898c29bc5.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define MAX_DATA 5035u
#define QUEUE_SLOTS 8u
#define STATE_ADDR 0x510000u
#define STATE_BYTES 0x20000u
#define BRIDGE_PORT 57342u
#define FRAME_DATA 2u
#define FRAME_CONNECTED 1u
#define FRAME_DISCONNECTED 3u
#define POLL_FAILURES 0x1060u /* QNX 6.5 POLLNVAL | POLLHUP | POLLERR */

struct RfEvent {
    u8 code, reserved1;
    u16 status, detail;
    u8 reason, reserved7;
    u16 length, reserved10;
    void *data;
};
struct RfChannel { u8 bytes[0xB0]; } __attribute__((aligned(4)));
struct RfPacket { u8 bytes[0x64]; } __attribute__((aligned(4)));
struct SdpAttribute { u16 id, length; const u8 *value; u16 cache, reserved; };
struct SdpRecord { u8 bytes[0x24]; } __attribute__((aligned(4)));
struct BridgeFrame { u8 type, reserved; u16 length; u32 generation; u8 data[MAX_DATA]; };
struct PollFd { int fd; short events, revents; };
struct SockAddrIn { u8 length, family, port_hi, port_lo, ip[4], zero[8]; };

struct GraftState {
    volatile u32 once, status, lock, generation, connected;
    volatile u32 queue_read, queue_write, queue_count;
    volatile u32 credit_generation, tx_busy, overflow;
    u32 worker_tid;
    u8 granted_channel, reserved[3];
    struct RfChannel channel;
    struct SdpRecord record;
    struct SdpAttribute attributes[2];
    u8 service_class[19], protocol[14];
    struct SdpRecord carplay_record;
    struct SdpAttribute carplay_attribute;
    u8 carplay_service_class[19];
    struct RfPacket tx_packet;
    u8 tx_data[MAX_DATA];
    struct BridgeFrame queue[QUEUE_SLOTS];
    u8 from_daemon[8 + MAX_DATA];
    volatile u32 discovery_ready;
    volatile u32 bridge_ready;
    u8 peer_address[6], peer_valid;
};

typedef char check_event_size[(sizeof(struct RfEvent) == 16) ? 1 : -1];
typedef char check_attr_size[(sizeof(struct SdpAttribute) == 12) ? 1 : -1];
typedef char check_record_size[(sizeof(struct SdpRecord) == 0x24) ? 1 : -1];
typedef char check_state_size[(sizeof(struct GraftState) <= STATE_BYTES - 4u) ? 1 : -1];
typedef char check_packet_align[(__builtin_offsetof(struct GraftState, tx_packet) % 4 == 0) ? 1 : -1];
typedef char check_granted_offset[(__builtin_offsetof(struct GraftState, granted_channel) == 0x30) ? 1 : -1];

#define ST ((struct GraftState *)STATE_ADDR)
#ifdef MHI2Q_GRAFT_QEMU_IAP2_MARKER
/* Dedicated tail word of the disposable QEMU RW segment, outside GraftState. */
#define QEMU_REPLY_FLAGS ((volatile u32 *)(STATE_ADDR + STATE_BYTES - 4u))
#endif
#define RF_REGISTER ((int (*)(void *,u8 *,u16))0x277B98u)
#define RF_DEREGISTER ((int (*)(void *,u8 *))0x277B34u)
#define SDP_ADD ((int (*)(void *))0x2073C4u)
#define SDP_REMOVE ((int (*)(void *))0x207344u)
/* Same Blue SDK request used after each successful SDP_ADD. */
#define EIR_REQUEST ((int (*)(void))0x1DB648u)
#define RF_ACCEPT ((int (*)(void *,int))0x2774BCu)
#define RF_DISCONNECT ((int (*)(void *))0x277578u)
#define RF_SEND ((int (*)(void *,void *))0x277378u)
#define RF_MAX_PAYLOAD ((int (*)(void *))0x2771F4u)
#define RF_CREDIT ((int (*)(void *,int))0x2772DCu)
#define PTHREAD_CREATE ((int (*)(u32 *,const void *,void *(*)(void *),void *))0x1173FCu)
#define SOCKET ((int (*)(int,int,int))0x117618u)
#define CONNECT ((int (*)(int,const void *,unsigned))0x117660u)
#define POLL ((int (*)(struct PollFd *,unsigned,int))0x116D00u)
#define SEND ((int (*)(int,const void *,unsigned,int))0x117318u)
#define RECV ((int (*)(int,void *,unsigned,int))0x117BE8u)
#define CLOSE ((int (*)(int))0x1174B0u)
#define USLEEP ((int (*)(unsigned))0x116E74u)

static const u8 service_class_bytes[19] = {
    0x35,0x11,0x1c,0x00,0x00,0x00,0x00,0xde,0xca,0xfa,
    0xde,0xde,0xca,0xde,0xaf,0xde,0xca,0xca,0xff
};
static const u8 protocol_bytes[14] = {
    0x35,0x0c,0x35,0x03,0x19,0x01,0x00,
    0x35,0x05,0x19,0x00,0x03,0x08,0x00
};
__attribute__((used)) static const u8 carplay_service_class_bytes[19] = {
    0x35,0x11,0x1c,0xec,0x88,0x43,0x48,0xcd,0x41,0x40,
    0xa2,0x97,0x27,0x57,0x5d,0x50,0xbf,0x1f,0xd3
};

static void bytes_copy(u8 *dst, const u8 *src, u32 n) {
    for (u32 i = 0; i < n; ++i) dst[i] = src[i];
}
static void bytes_zero(u8 *dst, u32 n) {
    for (u32 i = 0; i < n; ++i) dst[i] = 0;
}
/* Called immediately after the target's 240-byte EIR builder. AD type 0x07
 * is a list of complete 128-bit UUIDs; Bluetooth stores UUID bytes least
 * significant first, unlike the SDP value. */
extern u32 eir_append_carplay_uuid(u8 *eir, u32 length);
u32 graft_augment_eir(u8 *eir, u32 length) {
    struct GraftState *s = ST;
    if (!__atomic_load_n(&s->discovery_ready, __ATOMIC_ACQUIRE) ||
        !eir || length > 240) return length;
    return eir_append_carplay_uuid(eir, length);
}
static void guard_lock(struct GraftState *s) {
    while (__atomic_exchange_n(&s->lock, 1u, __ATOMIC_ACQUIRE)) { }
}
static void guard_unlock(struct GraftState *s) {
    __atomic_store_n(&s->lock, 0u, __ATOMIC_RELEASE);
}
static u16 event_u16(const struct RfEvent *e) { return e->length; }

/* Caller holds lock. On overflow the caller closes this iAP connection rather
 * than silently delivering a corrupt byte stream to the daemon. */
static int queue_push(struct GraftState *s, u8 type, u32 generation,
                      const u8 *data, u16 length) {
    if (s->queue_count == QUEUE_SLOTS) return 0;
    struct BridgeFrame *f = &s->queue[s->queue_write];
    f->type = type;
    f->length = length;
    f->generation = generation;
    if (length) bytes_copy(f->data, data, length);
    s->queue_write = (s->queue_write + 1u) & (QUEUE_SLOTS - 1u);
    ++s->queue_count;
    return 1;
}

void graft_rf_callback(void *channel, const struct RfEvent *event) {
    struct GraftState *s = ST;
    if (!event || channel != &s->channel) return;
    if (event->code == 1u) {
        int ready = __atomic_load_n(&s->bridge_ready, __ATOMIC_ACQUIRE) &&
                    !__atomic_load_n(&s->overflow, __ATOMIC_ACQUIRE);
        (void)RF_ACCEPT(channel, ready);
        return;
    }
    if (event->code == 2u) {
        guard_lock(s);
        ++s->generation;
        s->connected = 1;
        /* Event 2 carries BtRemoteDevice*, as used by HfRfCallback 0x248370.
         * Its six address bytes are at +13 (copy in 0x248370 event-1 path).
         * Keep SDK byte order as an opaque stable peer key, not a USB serial. */
        s->peer_valid = event->data != 0;
        if (s->peer_valid) bytes_copy(s->peer_address,(const u8 *)event->data+13,6);
        if (!queue_push(s, FRAME_CONNECTED, s->generation,
                        s->peer_address, s->peer_valid?6:0))
            __atomic_store_n(&s->overflow, 1u, __ATOMIC_RELEASE);
        guard_unlock(s);
        return;
    }
    if (event->code == 4u) {
        guard_lock(s);
        if (!s->connected) {
            guard_unlock(s);
            return;
        }
        u32 previous = s->generation;
        s->connected = 0;
        s->peer_valid = 0;
        ++s->generation;
        if (!queue_push(s, FRAME_DISCONNECTED, previous, 0, 0))
            __atomic_store_n(&s->overflow, 1u, __ATOMIC_RELEASE);
        guard_unlock(s);
        return;
    }
    if (event->code == 5u) {
        u16 length = event_u16(event);
        if (event->status == 17u || !event->data || !length || length > MAX_DATA) return;
        guard_lock(s);
        if (!s->connected) {
            guard_unlock(s);
            return;
        }
        int queued = queue_push(s, FRAME_DATA, s->generation,
                                (const u8 *)event->data, length);
        if (!queued)
            __atomic_store_n(&s->overflow, 1u, __ATOMIC_RELEASE);
        guard_unlock(s);
        return;
    }
    if (event->code == 6u && event->data == &s->tx_packet) {
        __atomic_store_n(&s->tx_busy, 0u, __ATOMIC_RELEASE);
    }
}

static void frame_header(u8 *dst, const struct BridgeFrame *f) {
    dst[0] = f->type; dst[1] = 0;
    dst[2] = (u8)f->length; dst[3] = (u8)(f->length >> 8);
    dst[4] = (u8)f->generation; dst[5] = (u8)(f->generation >> 8);
    dst[6] = (u8)(f->generation >> 16); dst[7] = (u8)(f->generation >> 24);
}
static u32 header_generation(const u8 *p) {
    return (u32)p[4] | ((u32)p[5] << 8) | ((u32)p[6] << 16) | ((u32)p[7] << 24);
}

/* All socket operations run here, never in the Bluetooth callback. The
 * daemon listens on 127.0.0.1:57342 and resets its iAP stream on EOF. */
static void *graft_worker(void *ignored) {
    (void)ignored;
    struct GraftState *s = ST;
    const struct SockAddrIn endpoint = {
        16, 2, (u8)(BRIDGE_PORT >> 8), (u8)BRIDGE_PORT,
        {127,0,0,1}, {0,0,0,0,0,0,0,0}
    };
    int fd = -1;
    u8 header[8];
    u32 out_offset = 0, out_length = 0;
    u32 in_used = 0, in_need = 8;
    u32 tx_length = 0, tx_offset = 0, tx_generation = 0;
    u32 snapshot_generation = 0;
    int snapshot_pending = 0;
    int disconnect_requested = 0;
#ifdef MHI2Q_GRAFT_QEMU_IAP2_MARKER
    u8 qemu_previous_reply_byte = 0;
#endif
    for (;;) {
        if (__atomic_load_n(&s->overflow, __ATOMIC_ACQUIRE))
            goto bridge_broken;
        if (fd < 0) {
            /* After a daemon loss, never attach its old phone generation to
             * a fresh daemon socket. Event 4 must end the RFCOMM session. */
            guard_lock(s);
            int old_phone_link = s->connected != 0;
            guard_unlock(s);
            if (old_phone_link) {
                if (!disconnect_requested) {
                    (void)RF_DISCONNECT(&s->channel);
                    disconnect_requested = 1;
                }
                (void)USLEEP(200000);
                continue;
            }
            disconnect_requested = 0;
            fd = SOCKET(2, 1, 0);
            if (fd < 0 || CONNECT(fd, &endpoint, 16) != 0) {
                if (fd >= 0) (void)CLOSE(fd);
                fd = -1;
                (void)USLEEP(200000);
                continue;
            }
            guard_lock(s);
            snapshot_pending = s->connected != 0;
            snapshot_generation = s->generation;
            guard_unlock(s);
            out_offset = out_length = in_used = 0;
            in_need = 8;
            tx_length = tx_offset = 0;
            __atomic_store_n(&s->bridge_ready, 1u, __ATOMIC_RELEASE);
#ifdef MHI2Q_GRAFT_QEMU_IAP2_MARKER
            qemu_previous_reply_byte = 0;
#endif
        }

        /* A daemon frame is a byte stream, not one RFCOMM packet. The peer's
         * payload limit can be smaller than our requested 5035-byte MTU.
         * Keep the frame intact and reuse the SDK packet only after event 6. */
        if (tx_length && !__atomic_load_n(&s->tx_busy, __ATOMIC_ACQUIRE)) {
            if (!s->connected || s->generation != tx_generation) {
                tx_length = tx_offset = 0;
                in_used = 0; in_need = 8;
            } else {
#ifdef MHI2Q_GRAFT_QEMU_IAP2_MARKER
                /* The disposable QEMU image has no RFCOMM peer. Exercise
                 * the same chunk scheduler at a three-byte peer limit. */
                int limit = 3;
#else
                int limit = RF_MAX_PAYLOAD(&s->channel);
#endif
                if (limit <= 0) goto bridge_broken;
                u32 part = tx_length - tx_offset;
                if (part > (u32)limit) part = (u32)limit;
                bytes_copy(s->tx_data, s->from_daemon + 8u + tx_offset, part);
                bytes_zero(s->tx_packet.bytes, 0x64);
                *(u8 **)(s->tx_packet.bytes + 8) = s->tx_data;
                *(u16 *)(s->tx_packet.bytes + 12) = (u16)part;
                __atomic_store_n(&s->tx_busy, 1u, __ATOMIC_RELEASE);
#ifdef MHI2Q_GRAFT_QEMU_IAP2_MARKER
                if (part < tx_length) {
                    __atomic_fetch_or(QEMU_REPLY_FLAGS, 4u, __ATOMIC_RELEASE);
                }
                if (tx_offset == 0 && tx_length >= 6 &&
                    s->from_daemon[8] == 0xff && s->from_daemon[9] == 0x55 &&
                    s->from_daemon[10] == 0x02 && s->from_daemon[11] == 0 &&
                    s->from_daemon[12] == 0xee && s->from_daemon[13] == 0x10)
                    __atomic_fetch_or(QEMU_REPLY_FLAGS, 1u, __ATOMIC_RELEASE);
                for (u32 i = 0; i < part; ++i) {
                    if (qemu_previous_reply_byte == 0xff && s->tx_data[i] == 0x5a)
                        __atomic_fetch_or(QEMU_REPLY_FLAGS, 2u, __ATOMIC_RELEASE);
                    qemu_previous_reply_byte = s->tx_data[i];
                }
                struct RfEvent completed = {0};
                completed.code = 6;
                completed.data = &s->tx_packet;
                graft_rf_callback(&s->channel, &completed);
#else
                if (RF_SEND(&s->channel, &s->tx_packet) != 2) {
                    __atomic_store_n(&s->tx_busy, 0u, __ATOMIC_RELEASE);
                    goto bridge_broken;
                }
#endif
                tx_offset += part;
                if (tx_offset == tx_length) {
                    tx_length = tx_offset = 0;
                    in_used = 0; in_need = 8;
                }
            }
        }

        struct BridgeFrame *out = 0;
        if (!out_length) {
            if (snapshot_pending) {
                struct BridgeFrame snap;
                snap.type = FRAME_CONNECTED; snap.length = 0;
                snap.generation = snapshot_generation;
                frame_header(header, &snap);
                out_length = 8;
            } else {
                guard_lock(s);
                if (s->queue_count) out = &s->queue[s->queue_read];
                if (out) {
                    frame_header(header, out);
                    out_length = 8u + out->length;
                }
                guard_unlock(s);
            }
        }

        struct PollFd pfd = { fd, 0, 0 };
        if (out_length) pfd.events |= 0x0002; /* POLLOUT */
        if (!tx_length && !__atomic_load_n(&s->tx_busy, __ATOMIC_ACQUIRE))
            pfd.events |= 0x0001; /* POLLRDNORM */
        int polled = POLL(&pfd, 1, 20);
        if (polled < 0 || (pfd.revents & POLL_FAILURES)) goto bridge_broken;

        if ((pfd.revents & 0x0002) && out_length) {
            const u8 *data;
            u32 count;
            if (out_offset < 8) {
                data = header + out_offset;
                count = 8u - out_offset;
            } else {
                if (!out) out = &s->queue[s->queue_read];
                data = out->data + out_offset - 8u;
                count = out_length - out_offset;
            }
            int written = SEND(fd, data, count, 0x80); /* MSG_DONTWAIT */
            if (written == 0) goto bridge_broken;
            if (written > 0) out_offset += (u32)written;
            if (out_offset == out_length) {
                u8 sent_type = header[0];
                u32 sent_generation = header_generation(header);
                if (snapshot_pending) {
                    snapshot_pending = 0;
                } else {
                    guard_lock(s);
                    s->queue_read = (s->queue_read + 1u) & (QUEUE_SLOTS - 1u);
                    --s->queue_count;
                    guard_unlock(s);
                }
                out_offset = out_length = 0;
                if (sent_type == FRAME_CONNECTED &&
                    s->connected && s->generation == sent_generation &&
                    s->credit_generation != sent_generation) {
                    s->credit_generation = sent_generation;
                    (void)RF_CREDIT(&s->channel, 4);
                }
                if (sent_type == FRAME_DATA &&
                    s->connected && s->generation == sent_generation)
                    (void)RF_CREDIT(&s->channel, 1);
            }
        }

        if (pfd.revents & 0x0001) {
            int got = RECV(fd, s->from_daemon + in_used, in_need - in_used, 0x80);
            if (got == 0) goto bridge_broken;
            if (got > 0) {
                in_used += (u32)got;
                if (in_used == 8 && in_need == 8) {
                    u16 length = (u16)s->from_daemon[2] |
                                 (u16)((u16)s->from_daemon[3] << 8);
                    if (s->from_daemon[0] != FRAME_DATA || s->from_daemon[1] != 0 ||
                        !length ||
                        length > MAX_DATA) goto bridge_broken;
                    in_need = 8u + length;
                }
                if (in_used == in_need && in_need > 8) {
                    u32 generation = header_generation(s->from_daemon);
                    u16 length = (u16)(in_need - 8u);
                    if (s->connected && s->generation == generation) {
                        tx_length = length;
                        tx_offset = 0;
                        tx_generation = generation;
                    } else {
                        in_used = 0; in_need = 8;
                    }
                }
            }
        }
        continue;

bridge_broken:
        __atomic_store_n(&s->bridge_ready, 0u, __ATOMIC_RELEASE);
        if (fd >= 0) (void)CLOSE(fd);
        fd = -1;
        out_offset = out_length = in_used = 0;
        in_need = 8;
        tx_length = tx_offset = 0;
        snapshot_pending = 0;
#ifdef MHI2Q_GRAFT_QEMU_IAP2_MARKER
        qemu_previous_reply_byte = 0;
#endif
        guard_lock(s);
        s->queue_read = s->queue_write = s->queue_count = 0;
        __atomic_store_n(&s->overflow, 0u, __ATOMIC_RELEASE);
        int was_connected = s->connected != 0;
        guard_unlock(s);
        if (was_connected && !disconnect_requested) {
            (void)RF_DISCONNECT(&s->channel);
            disconnect_requested = 1;
        }
        (void)USLEEP(200000);
    }
}

void graft_init(void) {
    struct GraftState *s = ST;
    if (__atomic_exchange_n(&s->once, 1u, __ATOMIC_ACQ_REL)) return;
    s->status = 0xFFFF0002u;
    bytes_zero(s->channel.bytes, 0xB0);
    bytes_zero(s->record.bytes, 0x24);
    bytes_zero(s->carplay_record.bytes, 0x24);
    bytes_zero((u8 *)s->attributes, 24);
    bytes_copy(s->service_class, service_class_bytes, 19);
    bytes_copy(s->protocol, protocol_bytes, 14);
    bytes_copy(s->carplay_service_class, carplay_service_class_bytes, 19);
    *(u16 *)(s->channel.bytes + 12) = 5035;
    *(void (**)(void *,const struct RfEvent *))(s->channel.bytes + 8) = graft_rf_callback;
    *(void **)(s->channel.bytes + 20) = s;
    s->granted_channel = 0;
    int status = RF_REGISTER(&s->channel, &s->granted_channel, 0);
    if (status) { s->status = (u32)status; return; }
    s->protocol[13] = s->granted_channel;
    s->attributes[0].id = 1;
    s->attributes[0].length = 19;
    s->attributes[0].value = s->service_class;
    s->attributes[1].id = 4;
    s->attributes[1].length = 14;
    s->attributes[1].value = s->protocol;
    s->record.bytes[8] = 2;
    *(struct SdpAttribute **)(s->record.bytes + 12) = s->attributes;
    status = SDP_ADD(&s->record);
    if (status) {
        (void)RF_DEREGISTER(&s->channel, &s->granted_channel);
        s->status = (u32)status;
        return;
    }
    s->carplay_attribute.id = 1;
    s->carplay_attribute.length = 19;
    s->carplay_attribute.value = s->carplay_service_class;
    s->carplay_record.bytes[8] = 1;
    *(struct SdpAttribute **)(s->carplay_record.bytes + 12) = &s->carplay_attribute;
    status = SDP_ADD(&s->carplay_record);
    if (status) {
        (void)SDP_REMOVE(&s->record);
        (void)RF_DEREGISTER(&s->channel, &s->granted_channel);
        s->status = (u32)status;
        return;
    }
    if (PTHREAD_CREATE(&s->worker_tid, 0, graft_worker, 0) != 0) {
        (void)SDP_REMOVE(&s->carplay_record);
        (void)SDP_REMOVE(&s->record);
        (void)RF_DEREGISTER(&s->channel, &s->granted_channel);
        s->status = 0xFFFF0001u;
    } else {
        s->status = 0;
        __atomic_store_n(&s->discovery_ready, 1u, __ATOMIC_RELEASE);
        /* SDP_ADD queued its EIR refreshes before discovery_ready became
         * visible. Request one more refresh after complete registration. */
        (void)EIR_REQUEST();
#ifdef MHI2Q_GRAFT_QEMU_SYNTHETIC
        /* Disposable QEMU build only: drive the real callback and socket
         * worker without claiming a Blue SDK or phone connection. */
#ifdef MHI2Q_GRAFT_QEMU_IAP2_MARKER
        static const u8 probe_bytes[] = {0xff, 0x55, 0x02, 0x00, 0xee, 0x10};
#else
        static const u8 probe_bytes[] = "IAP2-QEMU-RX";
#endif
        struct RfEvent connected = {0};
        connected.code = 2;
        graft_rf_callback(&s->channel, &connected);
        struct RfEvent received = {0};
        received.code = 5;
#ifdef MHI2Q_GRAFT_QEMU_IAP2_MARKER
        received.length = (u16)sizeof(probe_bytes);
#else
        received.length = (u16)(sizeof(probe_bytes) - 1u);
#endif
        received.data = (void *)probe_bytes;
        graft_rf_callback(&s->channel, &received);
#endif
    }
}

/* At 0x12A710: r7 is the EIR buffer, r0/r2 its current length, r6 is the
 * BluetoothServices instance. Replay its displaced store before resuming. */
__attribute__((naked)) void graft_eir_hook(void) {
    __asm__ volatile(
        "stmdb sp!, {r1,r3,r12,lr}\n"
        "mov r1, r2\n"
        "mov r0, r7\n"
        "bl graft_augment_eir\n"
        "mov r2, r0\n"
        "ldmia sp!, {r1,r3,r12,lr}\n"
        "strb r0, [r6,#0x40c]\n"
        "ldr pc, =0x12a714\n"
    );
}

__attribute__((naked)) void graft_hook(void) {
    __asm__ volatile(
        "stmdb sp!, {r0-r3,r12,lr}\n"
        "bl graft_init\n"
        "ldmia sp!, {r0-r3,r12,lr}\n"
        "ldrb r3, [r5,#0x764]\n"
        "ldr pc, =0x11ecd4\n"
    );
}
