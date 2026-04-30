/*
 * Non-blocking TCP server for command reception.
 *
 * Single client, reconnectable. Accumulates partial reads
 * into complete 48-byte packets.
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

/* Send a single EVT_HEARTBEAT packet to Java.  Caller is responsible
 * for throttling to ~1 Hz (Java's SO_TIMEOUT is 5 s). */
void cr_server_send_heartbeat(void);

#endif /* CR_SERVER_H */
