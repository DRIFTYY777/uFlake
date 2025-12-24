#ifndef UNVS_H
#define UNVS_H

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"
#include "kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UNVS_NAMESPACE_MAX_LEN 15
#define UNVS_KEY_MAX_LEN 15

    // NVS namespace types
    typedef enum
    {
        UNVS_NAMESPACE_SYSTEM = 0,
        UNVS_NAMESPACE_APPS,
        UNVS_NAMESPACE_SERVICES,
        UNVS_NAMESPACE_CONFIG,
        UNVS_NAMESPACE_USER
    } unvs_namespace_type_t;

    // Initialize NVS subsystem
    uflake_result_t unvs_init(void);
    esp_err_t unvs_erase_all(void);

    // Basic read/write operations
    esp_err_t unvs_write_u8(const char *namespace_name, const char *key, uint8_t value);
    esp_err_t unvs_read_u8(const char *namespace_name, const char *key, uint8_t *value);
    esp_err_t unvs_write_u32(const char *namespace_name, const char *key, uint32_t value);
    esp_err_t unvs_read_u32(const char *namespace_name, const char *key, uint32_t *value);
    esp_err_t unvs_write_string(const char *namespace_name, const char *key, const char *value);
    esp_err_t unvs_read_string(const char *namespace_name, const char *key, char *value, size_t *len);
    esp_err_t unvs_write_blob(const char *namespace_name, const char *key, const void *value, size_t len);
    esp_err_t unvs_read_blob(const char *namespace_name, const char *key, void *value, size_t *len);

    // Key operations
    esp_err_t unvs_erase_key(const char *namespace_name, const char *key);
    esp_err_t unvs_key_exists(const char *namespace_name, const char *key, bool *exists);

#ifdef __cplusplus
}
#endif

#endif // UNVS_H
