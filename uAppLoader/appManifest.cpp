#include "appManifest.h"
#include "appLoader.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "APP_MANIFEST";

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

__attribute__((unused)) static const char *app_type_to_string(app_type_t type)
{
    switch (type)
    {
    case APP_TYPE_INTERNAL:
        return "INTERNAL";
    case APP_TYPE_EXTERNAL:
        return "EXTERNAL";
    case APP_TYPE_LAUNCHER:
        return "LAUNCHER";
    case APP_TYPE_SERVICE:
        return "SERVICE";
    default:
        return "UNKNOWN";
    }
}

// Trim whitespace from string
static void trim_whitespace(char *str)
{
    if (!str)
        return;

    // Trim leading spaces
    char *start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;

    // Trim trailing spaces
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        end--;

    // Write result
    size_t len = (end - start) + 1;
    memmove(str, start, len);
    str[len] = '\0';
}

// Parse key=value line
__attribute__((unused)) static bool parse_key_value(const char *line, char *key, char *value, size_t max_len)
{
    const char *equals = strchr(line, '=');
    if (!equals)
        return false;

    size_t key_len = equals - line;
    if (key_len >= max_len)
        key_len = max_len - 1;

    strncpy(key, line, key_len);
    key[key_len] = '\0';
    trim_whitespace(key);

    strncpy(value, equals + 1, max_len - 1);
    value[max_len - 1] = '\0';
    trim_whitespace(value);

    return true;
}

// ============================================================================
// MANIFEST PARSING
// ============================================================================

uflake_result_t app_manifest_parse(const char *path, app_manifest_t *manifest)
{
    if (!path || !manifest)
        return UFLAKE_ERROR_INVALID_PARAM;

    UFLAKE_LOGI(TAG, "Parsing manifest: %s", path);

    // TODO: Implement file parsing for external apps on SD card
    // This would read manifest.txt line by line and parse key=value pairs
    //
    // Example manifest.txt format:
    // name=Counter App
    // version=1.0.0
    // author=uFlake Team
    // description=Simple counter application
    // icon=counter.bmp
    // type=internal
    // stack_size=4096
    // priority=5
    // requires_gui=true
    // requires_sdcard=false
    // requires_network=false
    //
    // For now, just return error since SD card support is not implemented

    UFLAKE_LOGW(TAG, "Manifest parsing not yet implemented (requires SD card support)");
    return UFLAKE_ERROR;

    /*
    FILE *file = fopen(path, "r");
    if (!file)
    {
        UFLAKE_LOGE(TAG, "Failed to open manifest file: %s", path);
        return UFLAKE_ERROR;
    }

    // Initialize manifest with defaults
    app_manifest_create_default(manifest, "Unknown", "0.0.0");

    char line[256];
    char key[64];
    char value[192];

    while (fgets(line, sizeof(line), file))
    {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        if (!parse_key_value(line, key, value, sizeof(value)))
            continue;

        // Parse known keys
        if (strcmp(key, "name") == 0)
        {
            strncpy(manifest->name, value, APP_NAME_MAX_LEN - 1);
        }
        else if (strcmp(key, "version") == 0)
        {
            strncpy(manifest->version, value, APP_VERSION_MAX_LEN - 1);
        }
        else if (strcmp(key, "author") == 0)
        {
            strncpy(manifest->author, value, APP_AUTHOR_MAX_LEN - 1);
        }
        else if (strcmp(key, "description") == 0)
        {
            strncpy(manifest->description, value, APP_DESC_MAX_LEN - 1);
        }
        else if (strcmp(key, "icon") == 0)
        {
            strncpy(manifest->icon, value, APP_ICON_MAX_LEN - 1);
        }
        else if (strcmp(key, "type") == 0)
        {
            if (strcmp(value, "internal") == 0)
                manifest->type = APP_TYPE_INTERNAL;
            else if (strcmp(value, "external") == 0)
                manifest->type = APP_TYPE_EXTERNAL;
            else if (strcmp(value, "launcher") == 0)
                manifest->type = APP_TYPE_LAUNCHER;
            else if (strcmp(value, "service") == 0)
                manifest->type = APP_TYPE_SERVICE;
        }
        else if (strcmp(key, "stack_size") == 0)
        {
            manifest->stack_size = (uint32_t)atoi(value);
        }
        else if (strcmp(key, "priority") == 0)
        {
            manifest->priority = (uint32_t)atoi(value);
        }
        else if (strcmp(key, "requires_gui") == 0)
        {
            manifest->requires_gui = (strcmp(value, "true") == 0);
        }
        else if (strcmp(key, "requires_sdcard") == 0)
        {
            manifest->requires_sdcard = (strcmp(value, "true") == 0);
        }
        else if (strcmp(key, "requires_network") == 0)
        {
            manifest->requires_network = (strcmp(value, "true") == 0);
        }
    }

    fclose(file);

    ESP_LOGI(TAG, "Parsed manifest: %s v%s", manifest->name, manifest->version);
    return UFLAKE_OK;
    */
}

uflake_result_t app_manifest_validate(const app_manifest_t *manifest)
{
    if (!manifest)
        return UFLAKE_ERROR_INVALID_PARAM;

    // Check required fields
    if (strlen(manifest->name) == 0)
    {
        UFLAKE_LOGE(TAG, "Manifest validation failed: name is empty");
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (strlen(manifest->version) == 0)
    {
        UFLAKE_LOGE(TAG, "Manifest validation failed: version is empty");
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    // Validate type
    if (manifest->type > APP_TYPE_SERVICE)
    {
        UFLAKE_LOGE(TAG, "Manifest validation failed: invalid type %d", manifest->type);
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    // Validate stack size
    if (manifest->stack_size > 0 && manifest->stack_size < 1024)
    {
        UFLAKE_LOGW(TAG, "Manifest validation warning: stack size %lu is very small", manifest->stack_size);
    }

    UFLAKE_LOGI(TAG, "Manifest validation passed: %s v%s", manifest->name, manifest->version);
    return UFLAKE_OK;
}

void app_manifest_create_default(app_manifest_t *manifest,
                                 const char *name,
                                 const char *version)
{
    if (!manifest)
        return;

    memset(manifest, 0, sizeof(app_manifest_t));

    if (name)
        strncpy(manifest->name, name, APP_NAME_MAX_LEN - 1);

    if (version)
        strncpy(manifest->version, version, APP_VERSION_MAX_LEN - 1);

    strcpy(manifest->author, "Unknown");
    strcpy(manifest->description, "No description");
    strcpy(manifest->icon, "default.bmp");

    manifest->type = APP_TYPE_INTERNAL;
    manifest->stack_size = 4096;
    manifest->priority = 5;
    manifest->requires_gui = true;
    manifest->requires_sdcard = false;
    manifest->requires_network = false;
}

void app_manifest_print(const app_manifest_t *manifest)
{
    if (!manifest)
        return;

    UFLAKE_LOGI(TAG, "=== App Manifest ===");
    UFLAKE_LOGI(TAG, "Name:        %s", manifest->name);
    UFLAKE_LOGI(TAG, "Version:     %s", manifest->version);
    UFLAKE_LOGI(TAG, "Author:      %s", manifest->author);
    UFLAKE_LOGI(TAG, "Description: %s", manifest->description);
    UFLAKE_LOGI(TAG, "Icon:        %s", manifest->icon);
    UFLAKE_LOGI(TAG, "Type:        %s", app_type_to_string(manifest->type));
    UFLAKE_LOGI(TAG, "Stack Size:  %lu bytes", manifest->stack_size);
    UFLAKE_LOGI(TAG, "Priority:    %lu", manifest->priority);
    UFLAKE_LOGI(TAG, "Requires GUI:     %s", manifest->requires_gui ? "Yes" : "No");
    UFLAKE_LOGI(TAG, "Requires SD Card: %s", manifest->requires_sdcard ? "Yes" : "No");
    UFLAKE_LOGI(TAG, "Requires Network: %s", manifest->requires_network ? "Yes" : "No");
    UFLAKE_LOGI(TAG, "==================");
}
