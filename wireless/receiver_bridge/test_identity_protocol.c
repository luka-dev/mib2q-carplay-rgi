/* Host test of actual bridge decoding, including stale-identity rejection. */
#include "protocol.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    char id[MHI2Q_RBR_ID_MAX + 1];
    uint8_t request[16] = {'C','P','R','X',2,5,0,0,37,0,0,0,0x88,0x13,0,0};
    uint8_t maximum[256] = {255, 'p', 'i', '='};
    mhi2q_rbr_request decoded;
    assert(mhi2q_rbr_decode_request(request, &decoded) == 0);
    assert(decoded.operation == 5 && decoded.generation == 37 && decoded.airplay_port == 5000);
    request[5] = 8;
    assert(mhi2q_rbr_decode_request(request, &decoded) != 0);
    assert(mhi2q_rbr_copy_pairing_id("\6pi=abc\6pk=def", 14, id) == 0 && !strcmp(id, "abc"));
    assert(mhi2q_rbr_copy_pairing_id("\6pi=abc\6PI=def", 14, id) != 0 && !id[0]);
    assert(mhi2q_rbr_copy_pairing_id("\6pk=def", 7, id) != 0 && !id[0]);
    assert(mhi2q_rbr_copy_pairing_id("\6pi=a\0b", 7, id) != 0 && !id[0]);
    assert(mhi2q_rbr_copy_pairing_id("\6pi=a b", 7, id) != 0 && !id[0]);
    assert(mhi2q_rbr_copy_pairing_id("\6pi=abc\3xy", 10, id) != 0 && !id[0]);
    assert(mhi2q_rbr_copy_pairing_id("\3pi=", 4, id) != 0 && !id[0]);
    memset(maximum + 4, 'a', sizeof(maximum) - 4);
    assert(mhi2q_rbr_copy_pairing_id(maximum, sizeof(maximum), id) == 0 && strlen(id) == 252);
    assert(mhi2q_rbr_copy_pairing_id(NULL, 0, id) != 0 && !id[0]);
    puts("receiver identity TXT and IPC checks passed");
    return 0;
}
