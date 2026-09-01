/*
 * Non-blocking TCP server for command reception.
 *
 * Single client, reconnectable. Accumulates partial reads
 * into complete 48-byte packets.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef CR_SERVER_H
#define CR_SERVER_H

#include "protocol.h"

/* Initialize server socket on given port. Returns 0 on success. */
int cr_server_init(int port);

/* Poll for new connections and incoming data. Call once per frame. */
void cr_server_poll(void);

/* Read next complete command packet. Returns 1 if a command was read, 0 if none available. */
int cr_server_read_cmd(cr_cmd_t *out);

/* Shutdown server and close all sockets. */
void cr_server_shutdown(void);

/* Returns 1 if the peer (Java) cleanly closed the connection — main
 * loop should exit when this is true. */
int cr_server_peer_closed(void);
void cr_server_clear_peer_closed(void);

/* Send a single EVT_HEARTBEAT packet to Java.  Caller is responsible
 * for throttling to ~1 Hz (Java's SO_TIMEOUT is 5 s). */
void cr_server_send_heartbeat(void);

/* Announce lifecycle readiness to Java.  These are sticky: if the TCP
 * reconnects after the renderer is already ready, the events are replayed
 * immediately on the new socket. */
void cr_server_mark_ready(void);
void cr_server_mark_frame_ready(void);
/* Forget pixel readiness after the peer/session disappears.  READY remains
 * process-sticky, but FRAME_READY must be earned by a new successful swap. */
void cr_server_clear_frame_ready(void);
/* Acknowledge the CMD_CLEAR barrier.  Unlike READY/FRAME_READY this event is not
 * sticky: a new TCP generation starts cleared by definition. */
void cr_server_mark_frame_cleared(void);

#endif /* CR_SERVER_H */
