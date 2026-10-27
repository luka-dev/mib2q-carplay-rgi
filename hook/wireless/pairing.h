/* Private C boundary inside libcarplay_hook. No SDK/CF objects escape it. */
#ifndef CARPLAY_WIRELESS_PAIRING_H
#define CARPLAY_WIRELESS_PAIRING_H
#include <stddef.h>
#include <stdint.h>
#include <sys/uio.h>

#define CP_PAIR_ID_MAX 252
#define CP_PAIR_BODY_MAX 16384
typedef struct cp_pair_store cp_pair_store;
typedef struct cp_pair_connection cp_pair_connection;

/* One store owner per process; keys survive connections and process restarts. */
int cp_pair_store_open(const char *directory, cp_pair_store **out);
void cp_pair_store_close(cp_pair_store *store);
int cp_pair_identity(cp_pair_store *store, char id[CP_PAIR_ID_MAX + 1], uint8_t pk[32]);
int cp_pair_connection_create(cp_pair_store *store, cp_pair_connection **out);
void cp_pair_connection_free(cp_pair_connection *connection);
/* verify=0: Setup; verify=1: Verify. Output belongs to the caller (free()).
 * done only means that exchange finished. Verify still needs commit after
 * its plaintext response has completely drained. */
int cp_pair_exchange(cp_pair_connection *connection, int verify,
                     const void *input, size_t input_len,
                     uint8_t **output, size_t *output_len, int *done);
int cp_pair_commit(cp_pair_connection *connection, int socket_fd);
int cp_pair_is_encrypted(const cp_pair_connection *connection);
int cp_pair_read(cp_pair_connection *connection, void *buffer, size_t capacity, size_t *length);
int cp_pair_writev(cp_pair_connection *connection, struct iovec **vectors, int *count);
int cp_pair_derive(cp_pair_connection *connection, const char *salt,
                   const char *info, uint8_t key[32]);
/* Independent record transport (events). Owns copied keys, never the fd. */
int cp_pair_transport_create(int fd, const uint8_t read_key[32],
                             const uint8_t write_key[32], cp_pair_connection **out);
int cp_pair_open(const uint8_t key[32], const uint8_t nonce[8],
                 const void *aad, size_t aad_len, const void *cipher, size_t length,
                 const uint8_t tag[16], void *plain);
void cp_pair_seal(const uint8_t key[32], const uint8_t nonce[8],
                  const void *aad, size_t aad_len, const void *plain, size_t length,
                  void *cipher, uint8_t tag[16]);
#endif
