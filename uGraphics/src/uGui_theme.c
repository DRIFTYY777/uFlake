/**
 * @file uGui_theme.c
 * @brief Theme and Background Manager Implementation
 */

#include "uGui_theme.h"
#include "uGui_notification.h"
#include "uGui.h"
#include "logger.h"
#include "imageCodec.h"
#include "sdCard.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "uGUI_Theme";

// ============================================================================
// THEME MANAGER STATE
// ============================================================================

typedef struct
{
    bool initialized;
    ugui_theme_t current_theme;
    ugui_background_t background;
    lv_obj_t *bg_object;
    lv_obj_t *bg_image;
    lv_image_dsc_t *bg_image_dsc;
    uint8_t *bg_image_data;
    uflake_mutex_t *mutex;
} theme_manager_t;

static theme_manager_t g_theme_mgr = {0};

// ============================================================================
// PRE-DEFINED THEMES
// ============================================================================

static const ugui_theme_t THEME_DARK = {
    .primary = {.red = 0x21, .green = 0x96, .blue = 0xF3},   // Blue
    .secondary = {.red = 0xFF, .green = 0x98, .blue = 0x00}, // Orange
    .background = {.red = 0x12, .green = 0x12, .blue = 0x12},
    .text = {.red = 0xFF, .green = 0xFF, .blue = 0xFF},
    .notification_bg = {.red = 0x00, .green = 0x00, .blue = 0x00},
    .notification_fg = {.red = 0xFF, .green = 0xFF, .blue = 0xFF},
    .opacity = 200};

static const ugui_theme_t THEME_LIGHT = {
    .primary = {.red = 0x21, .green = 0x96, .blue = 0xF3},
    .secondary = {.red = 0xFF, .green = 0x57, .blue = 0x22},
    .background = {.red = 0xF5, .green = 0xF5, .blue = 0xF5},
    .text = {.red = 0x00, .green = 0x00, .blue = 0x00},
    .notification_bg = {.red = 0xEE, .green = 0xEE, .blue = 0xEE},
    .notification_fg = {.red = 0x00, .green = 0x00, .blue = 0x00},
    .opacity = 220};

static const ugui_theme_t THEME_BLUE = {
    .primary = {.red = 0x00, .green = 0xBF, .blue = 0xFF}, // Flipper blue
    .secondary = {.red = 0xFF, .green = 0x45, .blue = 0x00},
    .background = {.red = 0x00, .green = 0x00, .blue = 0x00},
    .text = {.red = 0xFF, .green = 0xFF, .blue = 0xFF},
    .notification_bg = {.red = 0x00, .green = 0x1F, .blue = 0x3F},
    .notification_fg = {.red = 0x00, .green = 0xBF, .blue = 0xFF},
    .opacity = 200};

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// SD Card reader for imageCodec
typedef struct
{
    FILE *fp;
} sd_ctx_t;

static bool sd_open(void *ctx, const char *path)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    sd->fp = fopen(path, "rb");
    return (sd->fp != NULL);
}

static size_t sd_read(void *ctx, void *dst, size_t len)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    return fread(dst, 1, len, sd->fp);
}

static bool sd_seek(void *ctx, size_t offset)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    return fseek(sd->fp, offset, SEEK_SET) == 0;
}

static size_t sd_size(void *ctx)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    long pos = ftell(sd->fp);
    fseek(sd->fp, 0, SEEK_END);
    size_t size = ftell(sd->fp);
    fseek(sd->fp, pos, SEEK_SET);
    return size;
}

static void sd_close(void *ctx)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    if (sd->fp)
    {
        fclose(sd->fp);
        sd->fp = NULL;
    }
}

static img_reader_t sd_reader = {
    .open = sd_open,
    .read = sd_read,
    .seek = sd_seek,
    .size = sd_size,
    .close = sd_close,
};

static void update_background_display(void)
{
    if (!g_theme_mgr.bg_object)
    {
        return;
    }

    if (g_theme_mgr.background.type == UGUI_BG_SOLID_COLOR)
    {
        // Solid color background - hide any image widget
        if (g_theme_mgr.bg_image)
        {
            lv_obj_add_flag(g_theme_mgr.bg_image, LV_OBJ_FLAG_HIDDEN);
        }

        // Apply solid color styling
        lv_obj_set_style_bg_color(g_theme_mgr.bg_object, g_theme_mgr.background.color, 0);
        lv_obj_set_style_bg_opa(g_theme_mgr.bg_object, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(g_theme_mgr.bg_object, 0, 0);

        // Mark for redraw
        lv_obj_invalidate(g_theme_mgr.bg_object);
    }
    else
    {
        // Image background
        if (g_theme_mgr.bg_image && g_theme_mgr.bg_image_dsc)
        {
            // Make background transparent so image shows through
            lv_obj_set_style_bg_opa(g_theme_mgr.bg_object, LV_OPA_TRANSP, 0);
            lv_obj_clear_flag(g_theme_mgr.bg_image, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            // Fallback to color if image failed
            lv_obj_set_style_bg_color(g_theme_mgr.bg_object, g_theme_mgr.current_theme.background, 0);
            lv_obj_set_style_bg_opa(g_theme_mgr.bg_object, LV_OPA_COVER, 0);
        }
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t ugui_theme_init(void)
{
    if (g_theme_mgr.initialized)
    {
        UFLAKE_LOGW(TAG, "Theme manager already initialized");
        return UFLAKE_OK;
    }

    memset(&g_theme_mgr, 0, sizeof(theme_manager_t));

    // Create mutex
    if (uflake_mutex_create(&g_theme_mgr.mutex) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create theme mutex");
        return UFLAKE_ERROR;
    }

    // Apply default theme (dark)
    g_theme_mgr.current_theme = THEME_DARK;

    // Default background (solid color)
    g_theme_mgr.background.type = UGUI_BG_SOLID_COLOR;
    g_theme_mgr.background.color = g_theme_mgr.current_theme.background;

    // Create background object (lowest layer)
    g_theme_mgr.bg_object = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_theme_mgr.bg_object, UGUI_DISPLAY_WIDTH, UGUI_DISPLAY_HEIGHT);
    lv_obj_set_pos(g_theme_mgr.bg_object, 0, 0);
    lv_obj_set_style_bg_color(g_theme_mgr.bg_object, g_theme_mgr.background.color, 0);
    lv_obj_set_style_bg_opa(g_theme_mgr.bg_object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_theme_mgr.bg_object, 0, 0);
    lv_obj_set_style_pad_all(g_theme_mgr.bg_object, 0, 0);
    lv_obj_set_style_radius(g_theme_mgr.bg_object, 0, 0);
    lv_obj_clear_flag(g_theme_mgr.bg_object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_background(g_theme_mgr.bg_object); // Send to back

    // Don't pre-create bg_image - will be created on demand as proper lv_image widget
    g_theme_mgr.bg_image = NULL;

    g_theme_mgr.initialized = true;

    UFLAKE_LOGI(TAG, "Theme manager initialized");

    return UFLAKE_OK;
}

// ============================================================================
// THEME MANAGEMENT
// ============================================================================

uflake_result_t ugui_theme_set(const ugui_theme_t *theme)
{
    if (!g_theme_mgr.initialized || !theme)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_theme_mgr.mutex, 100);

    g_theme_mgr.current_theme = *theme;

    // Update notification bar theme
    ugui_notification_set_theme(theme);

    // Update background if solid color
    if (g_theme_mgr.background.type == UGUI_BG_SOLID_COLOR)
    {
        g_theme_mgr.background.color = theme->background;

        // Acquire GUI mutex for LVGL operations
        uflake_mutex_t *gui_mutex = uGui_get_mutex();
        if (gui_mutex && uflake_mutex_lock(gui_mutex, 100) == UFLAKE_OK)
        {
            update_background_display();
            uflake_mutex_unlock(gui_mutex);
        }
    }

    UFLAKE_LOGI(TAG, "Theme updated");

    uflake_mutex_unlock(g_theme_mgr.mutex);

    return UFLAKE_OK;
}

uflake_result_t ugui_theme_get(ugui_theme_t *theme)
{
    if (!g_theme_mgr.initialized || !theme)
    {
        return UFLAKE_ERROR;
    }

    *theme = g_theme_mgr.current_theme;

    return UFLAKE_OK;
}

uflake_result_t ugui_theme_set_primary_color(lv_color_t color)
{
    if (!g_theme_mgr.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_theme_mgr.current_theme.primary = color;
    ugui_notification_set_theme(&g_theme_mgr.current_theme);

    return UFLAKE_OK;
}

lv_color_t ugui_theme_get_primary_color(void)
{
    return g_theme_mgr.current_theme.primary;
}

// ============================================================================
// PRE-DEFINED THEMES
// ============================================================================

uflake_result_t ugui_theme_apply_dark(void)
{
    return ugui_theme_set(&THEME_DARK);
}

uflake_result_t ugui_theme_apply_light(void)
{
    return ugui_theme_set(&THEME_LIGHT);
}

uflake_result_t ugui_theme_apply_blue(void)
{
    return ugui_theme_set(&THEME_BLUE);
}

uflake_result_t ugui_theme_apply_by_name(const char *theme_name)
{
    if (!theme_name)
    {
        return UFLAKE_ERROR;
    }

    if (strcmp(theme_name, "dark") == 0)
    {
        return ugui_theme_apply_dark();
    }
    else if (strcmp(theme_name, "light") == 0)
    {
        return ugui_theme_apply_light();
    }
    else if (strcmp(theme_name, "blue") == 0)
    {
        return ugui_theme_apply_blue();
    }

    UFLAKE_LOGW(TAG, "Unknown theme: %s", theme_name);
    return UFLAKE_ERROR;
}

// ============================================================================
// BACKGROUND MANAGEMENT
// ============================================================================

uflake_result_t ugui_theme_set_bg_color(lv_color_t color)
{
    if (!g_theme_mgr.initialized)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_theme_mgr.mutex, 100);

    // Free any existing image data
    if (g_theme_mgr.bg_image_data)
    {
        uflake_free(g_theme_mgr.bg_image_data);
        g_theme_mgr.bg_image_data = NULL;
    }
    if (g_theme_mgr.bg_image_dsc)
    {
        uflake_free(g_theme_mgr.bg_image_dsc);
        g_theme_mgr.bg_image_dsc = NULL;
    }

    g_theme_mgr.background.type = UGUI_BG_SOLID_COLOR;
    g_theme_mgr.background.color = color;

    // Acquire GUI mutex for LVGL operations
    uflake_mutex_t *gui_mutex = uGui_get_mutex();
    if (gui_mutex && uflake_mutex_lock(gui_mutex, 100) == UFLAKE_OK)
    {
        update_background_display();
        uflake_mutex_unlock(gui_mutex);
    }

    uflake_mutex_unlock(g_theme_mgr.mutex);

    return UFLAKE_OK;
}

uflake_result_t ugui_theme_set_bg_image_sdcard(const char *path)
{
    if (!g_theme_mgr.initialized || !path)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_theme_mgr.mutex, 100);

    UFLAKE_LOGI(TAG, "Attempting to load background image: %s", path);

    // Free previous image if exists
    if (g_theme_mgr.bg_image_data)
    {
        uflake_free(g_theme_mgr.bg_image_data);
        g_theme_mgr.bg_image_data = NULL;
    }
    if (g_theme_mgr.bg_image_dsc)
    {
        uflake_free(g_theme_mgr.bg_image_dsc);
        g_theme_mgr.bg_image_dsc = NULL;
    }

    // Setup SD card reader
    sd_ctx_t ctx = {0};
    sd_reader.user_ctx = &ctx;

    // Decode options: resize to fit display (240x240)
    img_decode_opts_t opts = {
        .resize = true,
        .new_width = UGUI_DISPLAY_WIDTH,
        .new_height = UGUI_DISPLAY_HEIGHT,
        .rotate = IMG_ROTATE_0,
        .scale = IMG_SCALE_NONE};

    // Decode JPEG to RGB565
    img_rgb565_t img;
    if (img_decode_rgb565_ex(path, IMG_FMT_JPEG, &sd_reader, &opts, &img))
    {
        UFLAKE_LOGI(TAG, "Background image decoded: %dx%d, %zu bytes",
                    img.width, img.height, img.size);

        // Create LVGL image descriptor
        g_theme_mgr.bg_image_dsc = (lv_image_dsc_t *)uflake_malloc(sizeof(lv_image_dsc_t), UFLAKE_MEM_INTERNAL);
        if (g_theme_mgr.bg_image_dsc)
        {
            memset(g_theme_mgr.bg_image_dsc, 0, sizeof(lv_image_dsc_t));
            g_theme_mgr.bg_image_dsc->header.w = img.width;
            g_theme_mgr.bg_image_dsc->header.h = img.height;
            g_theme_mgr.bg_image_dsc->header.cf = LV_COLOR_FORMAT_NATIVE;
            g_theme_mgr.bg_image_dsc->data = img.pixels;
            g_theme_mgr.bg_image_dsc->data_size = img.size;

            // Store pointer for cleanup
            g_theme_mgr.bg_image_data = img.pixels;

            // Acquire GUI mutex before LVGL operations
            uflake_mutex_t *gui_mutex = uGui_get_mutex();
            if (gui_mutex && uflake_mutex_lock(gui_mutex, 100) == UFLAKE_OK)
            {
                // Delete old image widget if exists and recreate
                if (g_theme_mgr.bg_image)
                {
                    lv_obj_delete(g_theme_mgr.bg_image);
                    g_theme_mgr.bg_image = NULL;
                }

                // Create new LVGL image widget
                g_theme_mgr.bg_image = lv_image_create(g_theme_mgr.bg_object);
                lv_obj_set_size(g_theme_mgr.bg_image, img.width, img.height);
                lv_obj_set_pos(g_theme_mgr.bg_image, 0, 0);
                lv_obj_set_style_radius(g_theme_mgr.bg_image, 0, 0);
                lv_obj_set_style_border_width(g_theme_mgr.bg_image, 0, 0);
                lv_obj_set_style_pad_all(g_theme_mgr.bg_image, 0, 0);
                lv_obj_clear_flag(g_theme_mgr.bg_image, LV_OBJ_FLAG_SCROLLABLE);

                // Set image source
                lv_image_set_src(g_theme_mgr.bg_image, g_theme_mgr.bg_image_dsc);

                g_theme_mgr.background.type = UGUI_BG_IMAGE_SDCARD;
                strncpy(g_theme_mgr.background.image_path, path,
                        sizeof(g_theme_mgr.background.image_path) - 1);

                // Update background display (makes bg transparent, shows image)
                update_background_display();

                uflake_mutex_unlock(gui_mutex);

                UFLAKE_LOGI(TAG, "Background image loaded successfully");

                uflake_mutex_unlock(g_theme_mgr.mutex);
                return UFLAKE_OK;
            }
            else
            {
                // Failed to allocate descriptor, free image data
                img_free(&img);
                UFLAKE_LOGE(TAG, "Failed to allocate image descriptor");
            }
        }
        else
        {
            UFLAKE_LOGW(TAG, "Failed to decode image: %s", path);
        }

        // Fallback to solid color background
        UFLAKE_LOGI(TAG, "Using solid color background as fallback");

        // Acquire GUI mutex before LVGL operations
        uflake_mutex_t *gui_mutex = uGui_get_mutex();
        if (gui_mutex && uflake_mutex_lock(gui_mutex, 100) == UFLAKE_OK)
        {
            // Delete any existing image widget
            if (g_theme_mgr.bg_image)
            {
                lv_obj_delete(g_theme_mgr.bg_image);
                g_theme_mgr.bg_image = NULL;
            }

            // Clear image pointers
            g_theme_mgr.bg_image_data = NULL;
            g_theme_mgr.bg_image_dsc = NULL;

            // Set background to solid color
            g_theme_mgr.background.type = UGUI_BG_SOLID_COLOR;
            g_theme_mgr.background.color = g_theme_mgr.current_theme.background;

            // Update display through proper function
            update_background_display();

            uflake_mutex_unlock(gui_mutex);
        }

        uflake_mutex_unlock(g_theme_mgr.mutex);

        return UFLAKE_ERROR;
    }
    return UFLAKE_ERROR;
}

uflake_result_t ugui_theme_set_bg_image_flash(const char *path)
{
    if (!g_theme_mgr.initialized || !path)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_theme_mgr.mutex, 100);

    g_theme_mgr.background.type = UGUI_BG_IMAGE_FLASH;
    strncpy(g_theme_mgr.background.image_path, path, sizeof(g_theme_mgr.background.image_path) - 1);

    UFLAKE_LOGI(TAG, "Flash background set: %s", path);

    // Acquire GUI mutex for LVGL operations
    uflake_mutex_t *gui_mutex = uGui_get_mutex();
    if (gui_mutex && uflake_mutex_lock(gui_mutex, 100) == UFLAKE_OK)
    {
        update_background_display();
        uflake_mutex_unlock(gui_mutex);
    }

    uflake_mutex_unlock(g_theme_mgr.mutex);

    return UFLAKE_OK;
}

uflake_result_t ugui_theme_get_background(ugui_background_t *bg)
{
    if (!g_theme_mgr.initialized || !bg)
    {
        return UFLAKE_ERROR;
    }

    *bg = g_theme_mgr.background;

    return UFLAKE_OK;
}

uflake_result_t ugui_theme_refresh_background(void)
{
    if (!g_theme_mgr.initialized)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_theme_mgr.mutex, 100);

    // Acquire GUI mutex for LVGL operations
    uflake_mutex_t *gui_mutex = uGui_get_mutex();
    if (gui_mutex && uflake_mutex_lock(gui_mutex, 100) == UFLAKE_OK)
    {
        update_background_display();
        uflake_mutex_unlock(gui_mutex);
    }

    uflake_mutex_unlock(g_theme_mgr.mutex);

    return UFLAKE_OK;
}

// ============================================================================
// COMPONENT STYLING HELPERS
// ============================================================================

uflake_result_t ugui_theme_style_button(lv_obj_t *btn, bool use_theme_color, lv_color_t custom_color)
{
    if (!btn)
    {
        return UFLAKE_ERROR;
    }

    lv_color_t color = use_theme_color ? g_theme_mgr.current_theme.primary : custom_color;

    lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, g_theme_mgr.current_theme.opacity, 0);
    lv_obj_set_style_text_color(btn, g_theme_mgr.current_theme.text, 0);

    return UFLAKE_OK;
}

uflake_result_t ugui_theme_style_label(lv_obj_t *label)
{
    if (!label)
    {
        return UFLAKE_ERROR;
    }

    lv_obj_set_style_text_color(label, g_theme_mgr.current_theme.text, 0);

    return UFLAKE_OK;
}

uflake_result_t ugui_theme_style_panel(lv_obj_t *panel, bool transparent)
{
    if (!panel)
    {
        return UFLAKE_ERROR;
    }

    if (transparent)
    {
        lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    }
    else
    {
        lv_obj_set_style_bg_color(panel, g_theme_mgr.current_theme.background, 0);
        lv_obj_set_style_bg_opa(panel, g_theme_mgr.current_theme.opacity, 0);
    }

    return UFLAKE_OK;
}

uflake_result_t ugui_theme_style_object(lv_obj_t *obj)
{
    if (!obj)
    {
        return UFLAKE_ERROR;
    }

    lv_obj_set_style_bg_color(obj, g_theme_mgr.current_theme.primary, 0);
    lv_obj_set_style_text_color(obj, g_theme_mgr.current_theme.text, 0);

    return UFLAKE_OK;
}

// ============================================================================
// ADVANCED
// ============================================================================

uflake_result_t ugui_theme_save(void)
{
    // TODO: Implement persistent storage
    UFLAKE_LOGW(TAG, "Theme save not yet implemented");
    return UFLAKE_ERROR;
}

uflake_result_t ugui_theme_load(void)
{
    // TODO: Implement persistent storage
    UFLAKE_LOGW(TAG, "Theme load not yet implemented");
    return UFLAKE_ERROR;
}

lv_obj_t *ugui_theme_get_bg_object(void)
{
    return g_theme_mgr.bg_object;
}
