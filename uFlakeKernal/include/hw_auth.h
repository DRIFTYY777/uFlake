#ifndef UFLAKE_HW_AUTH_H
#define UFLAKE_HW_AUTH_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Hardware authentication status
    typedef enum
    {
        HW_AUTH_GENUINE = 0,        // Authentic hardware detected
        HW_AUTH_CLONE = 1,          // Clone/unauthorized hardware
        HW_AUTH_TAMPERED = 2,       // Hardware appears tampered
        HW_AUTH_UNKNOWN = 3,        // Cannot determine authenticity
        HW_AUTH_NOT_PROVISIONED = 4 // Device not provisioned yet
    } hw_auth_status_t;

    // Hardware identity structure
    typedef struct
    {
        uint8_t device_id[16];     // Unique device identifier (from eFuse)
        uint8_t serial_number[32]; // Device serial number
        uint8_t hw_version;        // Hardware version
        uint8_t board_revision;    // PCB revision
        uint32_t manufacture_date; // Unix timestamp
        uint8_t signature[64];     // ECDSA signature of device data
    } hw_identity_t;

    // Hardware authentication functions
    uflake_result_t uflake_hw_auth_init(void);
    hw_auth_status_t uflake_hw_auth_verify(void);
    uflake_result_t uflake_hw_get_identity(hw_identity_t *identity);
    uflake_result_t uflake_hw_get_unique_id(uint8_t *id, size_t len);
    bool uflake_hw_is_genuine(void);

    // eFuse management
    uflake_result_t uflake_efuse_read_mac(uint8_t *mac);
    uflake_result_t uflake_efuse_read_custom_field(uint32_t block_num, uint8_t *data, size_t len);
    bool uflake_efuse_is_secure_boot_enabled(void);
    bool uflake_efuse_is_flash_encryption_enabled(void);

    // Device provisioning (run once during manufacturing)
    uflake_result_t uflake_hw_provision_device(const hw_identity_t *identity, const uint8_t *private_key);
    uflake_result_t uflake_hw_burn_custom_data(const uint8_t *data, size_t len);

    // Certificate management (using esp_secure_cert_mgr)
    uflake_result_t uflake_cert_init(void);
    uflake_result_t uflake_cert_verify_device(void);
    uflake_result_t uflake_cert_get_public_key(uint8_t *key, size_t *key_len);

    // Callbacks for authentication events
    typedef void (*hw_auth_callback_t)(hw_auth_status_t status);
    void uflake_hw_auth_register_callback(hw_auth_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_HW_AUTH_H
