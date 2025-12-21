#ifndef UFLAKE_CRYPTO_ENGINE_H
#define UFLAKE_CRYPTO_ENGINE_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UFLAKE_SHA256_DIGEST_LENGTH 32
#define UFLAKE_AES_KEY_LENGTH 32
#define UFLAKE_AES_BLOCK_SIZE 16

    typedef struct
    {
        uint8_t key[UFLAKE_AES_KEY_LENGTH];
        uint8_t iv[UFLAKE_AES_BLOCK_SIZE];
    } uflake_aes_context_t;

    uflake_result_t uflake_crypto_init(void);

    // Hash functions
    uflake_result_t uflake_sha256(const uint8_t *input, size_t input_len, uint8_t *output);

    // AES encryption/decryption
    uflake_result_t uflake_aes_encrypt(const uflake_aes_context_t *ctx, const uint8_t *input,
                                       size_t input_len, uint8_t *output);
    uflake_result_t uflake_aes_decrypt(const uflake_aes_context_t *ctx, const uint8_t *input,
                                       size_t input_len, uint8_t *output);

    // Random number generation
    uflake_result_t uflake_random_bytes(uint8_t *output, size_t length);

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_CRYPTO_ENGINE_H
