#ifndef MEMSIM_CLI_SHA256_H_
#define MEMSIM_CLI_SHA256_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} ms_sha256_ctx;

void ms_sha256_init(ms_sha256_ctx *ctx);
void ms_sha256_update(ms_sha256_ctx *ctx, const void *data, size_t len);
void ms_sha256_final(ms_sha256_ctx *ctx, uint8_t hash[32]);

void ms_sha256_hash_str(const void *data, size_t len, char out_hex[65]);
bool ms_sha256_file(const char *path, char out_hex[65]);
bool ms_sha256_self_test(void);

#endif /* MEMSIM_CLI_SHA256_H_ */
