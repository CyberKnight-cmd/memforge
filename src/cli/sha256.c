#include "sha256.h"

#include <stdio.h>
#include <string.h>

#define ROTR(x, n) (((uint32_t)(x) >> (n##U)) | ((uint32_t)(x) << (32U - (n##U))))
#define SHR(x, n) ((uint32_t)(x) >> (n##U))

#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define SIGMA0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIGMA1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIGMA_LOWER0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define SIGMA_LOWER1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))

static const uint32_t K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U,
};

void ms_sha256_init(ms_sha256_ctx *ctx) {
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
    ctx->count = 0;
}

static void sha256_transform(ms_sha256_ctx *ctx, const uint8_t data[64]) {
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];
    uint32_t m[64];

    for (size_t i = 0; i < 16; i++) {
        m[i] = ((uint32_t)data[i * 4] << 24U) | ((uint32_t)data[(i * 4) + 1] << 16U) |
               ((uint32_t)data[(i * 4) + 2] << 8U) | ((uint32_t)data[(i * 4) + 3]);
    }
    for (size_t i = 16; i < 64; i++) {
        m[i] = SIGMA_LOWER1(m[i - 2]) + m[i - 7] + SIGMA_LOWER0(m[i - 15]) + m[i - 16];
    }
    for (size_t i = 0; i < 64; i++) {
        uint32_t t1 = h + SIGMA1(e) + CH(e, f, g) + K[i] + m[i];
        uint32_t t2 = SIGMA0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void ms_sha256_update(ms_sha256_ctx *ctx, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    size_t index = (size_t)(ctx->count & 0x3fU);
    ctx->count += len;

    size_t part_len = 64 - index;
    size_t i = 0;

    if (len >= part_len) {
        memcpy(&ctx->buffer[index], p, part_len);
        sha256_transform(ctx, ctx->buffer);
        for (i = part_len; (i + 63) < len; i += 64) {
            sha256_transform(ctx, &p[i]);
        }
        index = 0;
    }
    memcpy(&ctx->buffer[index], &p[i], len - i);
}

void ms_sha256_final(ms_sha256_ctx *ctx, uint8_t hash[32]) {
    uint8_t final_count[8];
    uint64_t bits = ctx->count * 8U;
    for (size_t i = 0; i < 8; i++) {
        final_count[i] = (uint8_t)((bits >> ((7U - (unsigned int)i) * 8U)) & 0xffU);
    }
    size_t index = (size_t)(ctx->count & 0x3fU);
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);
    static const uint8_t padding[64] = {0x80};
    ms_sha256_update(ctx, padding, pad_len);
    ms_sha256_update(ctx, final_count, 8);

    for (size_t i = 0; i < 8; i++) {
        hash[i * 4] = (uint8_t)((ctx->state[i] >> 24U) & 0xffU);
        hash[(i * 4) + 1] = (uint8_t)((ctx->state[i] >> 16U) & 0xffU);
        hash[(i * 4) + 2] = (uint8_t)((ctx->state[i] >> 8U) & 0xffU);
        hash[(i * 4) + 3] = (uint8_t)(ctx->state[i] & 0xffU);
    }
}

void ms_sha256_hash_str(const void *data, size_t len, char out_hex[65]) {
    ms_sha256_ctx ctx;
    ms_sha256_init(&ctx);
    ms_sha256_update(&ctx, data, len);
    uint8_t hash[32];
    ms_sha256_final(&ctx, hash);
    static const char hex_digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; i++) {
        out_hex[i * 2] = hex_digits[((uint32_t)hash[i] >> 4U) & 0x0fU];
        out_hex[(i * 2) + 1] = hex_digits[(uint32_t)hash[i] & 0x0fU];
    }
    out_hex[64] = '\0';
}

bool ms_sha256_file(const char *path, char out_hex[65]) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    ms_sha256_ctx ctx;
    ms_sha256_init(&ctx);
    uint8_t buf[4096];
    size_t n = 0;
    while (!feof(f) && !ferror(f)) {
        n = fread(buf, 1, sizeof(buf), f);
        if (n == 0) {
            break;
        }
        ms_sha256_update(&ctx, buf, n);
    }
    int err = ferror(f);
    (void)fclose(f);
    if (err != 0) {
        return false;
    }
    uint8_t hash[32];
    ms_sha256_final(&ctx, hash);
    static const char hex_digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; i++) {
        out_hex[i * 2] = hex_digits[((uint32_t)hash[i] >> 4U) & 0x0fU];
        out_hex[(i * 2) + 1] = hex_digits[(uint32_t)hash[i] & 0x0fU];
    }
    out_hex[64] = '\0';
    return true;
}

bool ms_sha256_self_test(void) {
    char hex[65];
    ms_sha256_hash_str("abc", 3, hex);
    return strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0;
}
