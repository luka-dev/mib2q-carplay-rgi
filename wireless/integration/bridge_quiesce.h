#ifndef MHI2Q_BRIDGE_QUIESCE_H
#define MHI2Q_BRIDGE_QUIESCE_H

#include "../receiver_bridge/protocol.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/* Only the guarded property-handoff variant enables this protocol. A positive
 * reply means the bridge drained copy-property and closed the AirPlay port. */
static int mhi2q_bridge_quiesce_required(void)
{
#ifdef MHI2Q_REQUIRE_QUIESCE_ALWAYS
    return 1;
#else
    const char *value = getenv("MHI2Q_REQUIRE_BRIDGE_QUIESCE");
    return value && value[0] == '1' && value[1] == 0;
#endif
}

static int mhi2q_bridge_command(uint8_t operation)
{
    uint8_t request[MHI2Q_RBR_REQUEST_SIZE] = {
        'C', 'P', 'R', 'X', MHI2Q_RBR_VERSION, 0,
        0, 0, 0, 0, 0, 0, 0x58, 0x1b, 0, 0
    };
    uint8_t response[MHI2Q_RBR_RESPONSE_SIZE];
    struct sockaddr_in address;
    struct timeval timeout = { 2, 0 };
    const char *text = getenv("MHI2Q_OWNER_BRIDGE_PORT");
    unsigned long port = 57343;
    size_t used;
    int fd;
    if (operation != MHI2Q_RBR_QUIESCE &&
        operation != MHI2Q_RBR_WIRED_DRAIN)
        return -1;
    request[5] = operation;
    /* Handoff must address the same receiver as iAP2 START. The historical
     * 280 fixture used 7000; the stock210 candidate explicitly supplies 5000. */
    const char *receiver_port = getenv("MHI2Q_CP_AIRPLAY_PORT");
    if (receiver_port && *receiver_port) {
        char *end;
        unsigned long value = strtoul(receiver_port, &end, 10);
        if (*end || !value || value > 65535) return -1;
        request[12] = (uint8_t)value;
        request[13] = (uint8_t)(value >> 8);
    }
    if (text && *text) {
        char *end;
        port = strtoul(text, &end, 10);
        if (*end || port == 0 || port > 65535)
            return -1;
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    used = 0;
    while (used < sizeof(request)) {
        ssize_t step = send(fd, request + used, sizeof(request) - used, 0);
        if (step <= 0) { close(fd); return -1; }
        used += (size_t)step;
    }
    used = 0;
    while (used < sizeof(response)) {
        ssize_t step = recv(fd, response + used, sizeof(response) - used, 0);
        if (step <= 0) { close(fd); return -1; }
        used += (size_t)step;
    }
    close(fd);
    if (memcmp(response, "CPRA", 4) != 0 ||
        response[4] != MHI2Q_RBR_VERSION ||
        response[6] != 0 || response[7] != 0)
        return -1;
    return response[5] == MHI2Q_RBR_OK ||
           response[5] == MHI2Q_RBR_BUSY ? response[5] : -1;
}

static inline int mhi2q_bridge_quiesce(void)
{
    return mhi2q_bridge_command(MHI2Q_RBR_QUIESCE);
}

static inline int mhi2q_bridge_wired_drain(void)
{
    return mhi2q_bridge_command(MHI2Q_RBR_WIRED_DRAIN);
}

#endif
