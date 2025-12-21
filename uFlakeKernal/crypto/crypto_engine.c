#include "crypto_engine.h"
#include "esp_log.h"
#include "mbedtls/sha256.h"
#include "mbedtls/aes.h"
#include "esp_random.h"

static const char *TAG = "CRYPTO";

uflake_result_t uflake_crypto_init(void)
{
    ESP_LOGI(TAG, "Crypto engine initialized with hardware acceleration");
    return UFLAKE_OK;
}

uflake_result_t uflake_sha256(const uint8_t *input, size_t input_len, uint8_t *output)
{
    if (!input || !output)
        return UFLAKE_ERROR_INVALID_PARAM;

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    int ret = mbedtls_sha256_starts(&ctx, 0); // 0 for SHA-256
    if (ret != 0)
    {
        mbedtls_sha256_free(&ctx);
        return UFLAKE_ERROR;
    }

    ret = mbedtls_sha256_update(&ctx, input, input_len);
    if (ret != 0)
    {
        mbedtls_sha256_free(&ctx);
        return UFLAKE_ERROR;
    }

    ret = mbedtls_sha256_finish(&ctx, output);
    mbedtls_sha256_free(&ctx);

    if (ret != 0)
    {
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

uflake_result_t uflake_aes_encrypt(const uflake_aes_context_t *ctx, const uint8_t *input,
                                   size_t input_len, uint8_t *output)
{
    if (!ctx || !input || !output)
        return UFLAKE_ERROR_INVALID_PARAM;

    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);

    int ret = mbedtls_aes_setkey_enc(&aes_ctx, ctx->key, UFLAKE_AES_KEY_LENGTH * 8);
    if (ret != 0)
    {
        mbedtls_aes_free(&aes_ctx);
        return UFLAKE_ERROR;
    }

    uint8_t iv_copy[UFLAKE_AES_BLOCK_SIZE];
    memcpy(iv_copy, ctx->iv, UFLAKE_AES_BLOCK_SIZE);

    ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, input_len, iv_copy, input, output);
    mbedtls_aes_free(&aes_ctx);

    if (ret != 0)
    {
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

uflake_result_t uflake_aes_decrypt(const uflake_aes_context_t *ctx, const uint8_t *input,
                                   size_t input_len, uint8_t *output)
{
    if (!ctx || !input || !output)
        return UFLAKE_ERROR_INVALID_PARAM;

    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);

    int ret = mbedtls_aes_setkey_dec(&aes_ctx, ctx->key, UFLAKE_AES_KEY_LENGTH * 8);
    if (ret != 0)
    {
        mbedtls_aes_free(&aes_ctx);
        return UFLAKE_ERROR;
    }

    uint8_t iv_copy[UFLAKE_AES_BLOCK_SIZE];
    memcpy(iv_copy, ctx->iv, UFLAKE_AES_BLOCK_SIZE);

    ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, input_len, iv_copy, input, output);
    mbedtls_aes_free(&aes_ctx);

    if (ret != 0)
    {
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

uflake_result_t uflake_random_bytes(uint8_t *output, size_t length)
{
    if (!output || length == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    esp_fill_random(output, length);
    return UFLAKE_OK;
}
