#include "hw_auth.h"
#include "crypto_engine.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_flash_encrypt.h"
#include "esp_secure_boot.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"
#include "esp_secure_cert_read.h"
#include <string.h>

static const char *TAG = "HW_AUTH";

// Public key for signature verification (burned during manufacturing)
// This should be your company's public key
static const uint8_t DEVICE_PUBLIC_KEY[] = {
    // Replace with your actual ECDSA P-256 public key
    0x04, // Uncompressed point format
    // X coordinate (32 bytes)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Y coordinate (32 bytes)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static hw_auth_callback_t auth_callback = NULL;
static hw_auth_status_t cached_auth_status = HW_AUTH_UNKNOWN;

// Expected hardware characteristics (customize for your board)
#define EXPECTED_HW_VERSION 1
#define EXPECTED_BOARD_REVISION 1

uflake_result_t uflake_hw_auth_init(void)
{
    ESP_LOGI(TAG, "Initializing hardware authentication...");

    // Check secure boot and flash encryption status
    bool secure_boot_enabled = uflake_efuse_is_secure_boot_enabled();
    bool flash_encryption_enabled = uflake_efuse_is_flash_encryption_enabled();

    ESP_LOGI(TAG, "Secure Boot: %s", secure_boot_enabled ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "Flash Encryption: %s", flash_encryption_enabled ? "ENABLED" : "DISABLED");

    // Perform initial authentication check
    cached_auth_status = uflake_hw_auth_verify();

    if (cached_auth_status != HW_AUTH_GENUINE)
    {
        ESP_LOGW(TAG, "Hardware authentication failed: status=%d", cached_auth_status);

        // Trigger callback if registered
        if (auth_callback)
        {
            auth_callback(cached_auth_status);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Hardware authentication successful - genuine device");
    }

    return UFLAKE_OK;
}

hw_auth_status_t uflake_hw_auth_verify(void)
{
    ESP_LOGI(TAG, "Verifying hardware authenticity...");

    // Step 1: Check if device is provisioned
    hw_identity_t identity;
    if (uflake_hw_get_identity(&identity) != UFLAKE_OK)
    {
        ESP_LOGW(TAG, "Device not provisioned");
        return HW_AUTH_NOT_PROVISIONED;
    }

    // Step 2: Verify hardware version matches expected
    if (identity.hw_version != EXPECTED_HW_VERSION)
    {
        ESP_LOGW(TAG, "Hardware version mismatch: expected=%d, got=%d",
                 EXPECTED_HW_VERSION, identity.hw_version);
        return HW_AUTH_CLONE;
    }

    // Step 3: Verify digital signature
    // Calculate hash of device data
    uint8_t device_data_hash[32];
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0); // SHA-256

    // Hash device_id, serial, versions, date
    mbedtls_sha256_update(&sha_ctx, identity.device_id, sizeof(identity.device_id));
    mbedtls_sha256_update(&sha_ctx, identity.serial_number, sizeof(identity.serial_number));
    mbedtls_sha256_update(&sha_ctx, &identity.hw_version, 1);
    mbedtls_sha256_update(&sha_ctx, &identity.board_revision, 1);
    mbedtls_sha256_update(&sha_ctx, (uint8_t *)&identity.manufacture_date, sizeof(identity.manufacture_date));

    mbedtls_sha256_finish(&sha_ctx, device_data_hash);
    mbedtls_sha256_free(&sha_ctx);

    // Verify signature using public key
    mbedtls_ecdsa_context ecdsa_ctx;
    mbedtls_ecdsa_init(&ecdsa_ctx);

    // Load ECC group
    int ret = mbedtls_ecp_group_load(&ecdsa_ctx.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to load ECC group: %d", ret);
        mbedtls_ecdsa_free(&ecdsa_ctx);
        return HW_AUTH_UNKNOWN;
    }

    // Read public key point
    ret = mbedtls_ecp_point_read_binary(&ecdsa_ctx.MBEDTLS_PRIVATE(grp), &ecdsa_ctx.MBEDTLS_PRIVATE(Q),
                                        DEVICE_PUBLIC_KEY, sizeof(DEVICE_PUBLIC_KEY));
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to read public key: %d", ret);
        mbedtls_ecdsa_free(&ecdsa_ctx);
        return HW_AUTH_UNKNOWN;
    }

    // Verify signature (first 32 bytes = r, next 32 bytes = s)
    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    mbedtls_mpi_read_binary(&r, identity.signature, 32);
    mbedtls_mpi_read_binary(&s, identity.signature + 32, 32);

    ret = mbedtls_ecdsa_verify(&ecdsa_ctx.MBEDTLS_PRIVATE(grp), device_data_hash, 32,
                               &ecdsa_ctx.MBEDTLS_PRIVATE(Q), &r, &s);

    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_ecdsa_free(&ecdsa_ctx);

    if (ret != 0)
    {
        ESP_LOGE(TAG, "Signature verification failed: %d", ret);
        return HW_AUTH_CLONE;
    }

    // Step 4: Additional checks - verify MAC address is from Espressif range
    uint8_t mac[6];
    if (uflake_efuse_read_mac(mac) == UFLAKE_OK)
    {
        // Espressif MAC addresses start with specific OUI
        // Common prefixes: 0x24:0x0A:0xC4, 0xC4:0x4F:0x33, etc.
        // You can add more specific checks here
        ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    ESP_LOGI(TAG, "Hardware verification successful");
    return HW_AUTH_GENUINE;
}

uflake_result_t uflake_hw_get_identity(hw_identity_t *identity)
{
    if (!identity)
        return UFLAKE_ERROR_INVALID_PARAM;

    // Read from eFuse custom block
    // Block 3 (BLOCK_USR_DATA) is typically available for custom data
    esp_err_t err = esp_efuse_read_block(EFUSE_BLK3, identity, 0, sizeof(hw_identity_t) * 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read device identity from eFuse: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    // Check if identity is blank (all 0xFF means not provisioned)
    bool is_blank = true;
    uint8_t *data = (uint8_t *)identity;
    for (size_t i = 0; i < sizeof(hw_identity_t); i++)
    {
        if (data[i] != 0xFF)
        {
            is_blank = false;
            break;
        }
    }

    if (is_blank)
    {
        return UFLAKE_ERROR_NOT_FOUND;
    }

    return UFLAKE_OK;
}

uflake_result_t uflake_hw_get_unique_id(uint8_t *id, size_t len)
{
    if (!id || len < 16)
        return UFLAKE_ERROR_INVALID_PARAM;

    // Get unique ID from eFuse (combination of MAC + chip ID)
    uint8_t mac[6];
    esp_err_t err = esp_efuse_mac_get_default(mac);
    if (err != ESP_OK)
    {
        return UFLAKE_ERROR;
    }

    // Get chip ID (unique per chip)
    uint8_t chip_id[6];
    esp_efuse_read_field_blob(ESP_EFUSE_WAFER_VERSION_MAJOR, chip_id, 48);

    // Combine MAC + chip ID for unique identifier
    memcpy(id, mac, 6);
    memcpy(id + 6, chip_id, 6);

    // Pad with zeros if needed
    if (len > 12)
    {
        memset(id + 12, 0, len - 12);
    }

    return UFLAKE_OK;
}

bool uflake_hw_is_genuine(void)
{
    if (cached_auth_status == HW_AUTH_UNKNOWN)
    {
        cached_auth_status = uflake_hw_auth_verify();
    }

    return (cached_auth_status == HW_AUTH_GENUINE);
}

uflake_result_t uflake_efuse_read_mac(uint8_t *mac)
{
    if (!mac)
        return UFLAKE_ERROR_INVALID_PARAM;

    esp_err_t err = esp_efuse_mac_get_default(mac);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read MAC from eFuse: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

uflake_result_t uflake_efuse_read_custom_field(uint32_t block_num, uint8_t *data, size_t len)
{
    if (!data)
        return UFLAKE_ERROR_INVALID_PARAM;

    esp_err_t err = esp_efuse_read_block(block_num, data, 0, len * 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read eFuse block %lu: %s", block_num, esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

bool uflake_efuse_is_secure_boot_enabled(void)
{
    return esp_secure_boot_enabled();
}

bool uflake_efuse_is_flash_encryption_enabled(void)
{
    return esp_flash_encryption_enabled();
}

uflake_result_t uflake_hw_provision_device(const hw_identity_t *identity, const uint8_t *private_key)
{
    if (!identity)
        return UFLAKE_ERROR_INVALID_PARAM;

    ESP_LOGW(TAG, "!!! DEVICE PROVISIONING - THIS CAN ONLY BE DONE ONCE !!!");

    // Check if already provisioned
    hw_identity_t existing;
    if (uflake_hw_get_identity(&existing) == UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Device already provisioned! Cannot provision again.");
        return UFLAKE_ERROR;
    }

    // Write identity to eFuse (THIS IS PERMANENT - CANNOT BE CHANGED)
    esp_err_t err = esp_efuse_write_block(EFUSE_BLK3, identity, 0, sizeof(hw_identity_t) * 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write device identity to eFuse: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Device provisioned successfully");
    return UFLAKE_OK;
}

uflake_result_t uflake_cert_init(void)
{
    ESP_LOGI(TAG, "Initializing certificate manager...");

    // Initialize esp_secure_cert (NVS-based storage)
    esp_err_t err = esp_secure_cert_init_nvs_partition();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize secure cert partition: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

uflake_result_t uflake_cert_verify_device(void)
{
    char *cert = NULL;
    uint32_t cert_len = 0;

    // Read device certificate
    esp_err_t err = esp_secure_cert_get_device_cert(&cert, &cert_len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read device certificate: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Device certificate read successfully (%d bytes)", cert_len);

    // Add certificate validation logic here
    // Verify certificate chain, check expiration, etc.

    return UFLAKE_OK;
}

void uflake_hw_auth_register_callback(hw_auth_callback_t callback)
{
    auth_callback = callback;
}
