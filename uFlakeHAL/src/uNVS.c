#include "uNVS.h"

static const char *TAG = "UNVS";
static bool unvs_initialized = false;

uflake_result_t unvs_init(void)
{
    if (unvs_initialized)
    {
        return UFLAKE_OK;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        UFLAKE_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    unvs_initialized = true;
    UFLAKE_LOGI(TAG, "NVS subsystem initialized");
    return UFLAKE_OK;
}

esp_err_t unvs_write_u8(const char *namespace_name, const char *key, uint8_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;
    err = nvs_set_u8(handle, key, value);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t unvs_read_u8(const char *namespace_name, const char *key, uint8_t *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_get_u8(handle, key, value);
    nvs_close(handle);
    return err;
}

esp_err_t unvs_write_u32(const char *namespace_name, const char *key, uint32_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_u32(handle, key, value);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t unvs_read_u32(const char *namespace_name, const char *key, uint32_t *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_get_u32(handle, key, value);
    nvs_close(handle);
    return err;
}

esp_err_t unvs_write_string(const char *namespace_name, const char *key, const char *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t unvs_read_string(const char *namespace_name, const char *key, char *value, size_t *len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_get_str(handle, key, value, len);
    nvs_close(handle);
    return err;
}

esp_err_t unvs_write_blob(const char *namespace_name, const char *key, const void *value, size_t len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_blob(handle, key, value, len);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t unvs_read_blob(const char *namespace_name, const char *key, void *value, size_t *len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_get_blob(handle, key, value, len);
    nvs_close(handle);
    return err;
}

esp_err_t unvs_erase_key(const char *namespace_name, const char *key)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_erase_key(handle, key);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t unvs_key_exists(const char *namespace_name, const char *key, bool *exists)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        *exists = false;
        return err;
    }

    size_t len;
    err = nvs_get_blob(handle, key, NULL, &len);
    *exists = (err == ESP_OK || err == ESP_ERR_NVS_INVALID_LENGTH);
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t unvs_erase_all(void)
{
    return nvs_flash_erase();
}
