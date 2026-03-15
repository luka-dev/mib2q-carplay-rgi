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

#endif /* CR_SERVER_H */
