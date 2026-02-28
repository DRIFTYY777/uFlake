/**
 * @file uGui_theme.c
 * @brief Theme and Background Manager Implementation
 */

#include "uGui_theme.h"
#include "uGui_notification.h"
#include "uGui.h"
#include "gui_types.h"
#include "kernel.h"
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
} theme_manager_t;

static theme_manager_t g_theme_mgr = {};

// ============================================================================
// PRE-DEFINED THEMES (using lv_color_make() for C++ compatibility)
// ============================================================================

static ugui_theme_t create_theme_dark()
{
    ugui_theme_t theme = {};
    theme.primary = lv_color_hex(0x2196F3);   // Blue
    theme.secondary = lv_color_hex(0xFF9800); // Orange
    theme.background = lv_color_hex(0x121212);
    theme.text = lv_color_hex(0xFFFFFF);
    theme.notification_bg = lv_color_hex(0x000000);
    theme.notification_fg = lv_color_hex(0xFFFFFF);
    theme.opacity = 200;
    return theme;
}

static ugui_theme_t create_theme_light()
{
    ugui_theme_t theme = {};
    theme.primary = lv_color_hex(0x2196F3);
    theme.secondary = lv_color_hex(0xFF5722);
    theme.background = lv_color_hex(0xF5F5F5);
    theme.text = lv_color_hex(0x000000);
    theme.notification_bg = lv_color_hex(0xEEEEEE);
    theme.notification_fg = lv_color_hex(0x000000);
    theme.opacity = 220;
    return theme;
}

static ugui_theme_t create_theme_blue()
{
    ugui_theme_t theme = {};
    theme.primary = lv_color_hex(0x00BFFF); // Flipper blue
    theme.secondary = lv_color_hex(0xFF4500);
    theme.background = lv_color_hex(0x000000);
    theme.text = lv_color_hex(0xFFFFFF);
    theme.notification_bg = lv_color_hex(0x001F3F);
    theme.notification_fg = lv_color_hex(0x00BFFF);
    theme.opacity = 200;
    return theme;
}

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

static img_reader_t sd_reader = {};

static void init_sd_reader()
{
    sd_reader.open = sd_open;
    sd_reader.read = sd_read;
    sd_reader.seek = sd_seek;
    sd_reader.size = sd_size;
    sd_reader.close = sd_close;
}

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

    // Initialize SD reader
    init_sd_reader();

    // Apply default theme (dark)
    g_theme_mgr.current_theme = create_theme_dark();

    // Default background (solid color)
    g_theme_mgr.background.type = UGUI_BG_SOLID_COLOR;
    g_theme_mgr.background.color = g_theme_mgr.current_theme.background;

    // Set screen background directly - no extra object needed
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, g_theme_mgr.background.color, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // Store reference to screen for later updates
    g_theme_mgr.bg_object = screen;

    // Don't pre-create bg_image - will be created on demand
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

    g_theme_mgr.current_theme = *theme;

    // Update notification bar theme
    ugui_notification_set_theme(theme);

    // Update background if solid color
    if (g_theme_mgr.background.type == UGUI_BG_SOLID_COLOR)
    {
        g_theme_mgr.background.color = theme->background;
        update_background_display();
    }

    UFLAKE_LOGI(TAG, "Theme updated");

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
    ugui_theme_t theme = create_theme_dark();
    return ugui_theme_set(&theme);
}

uflake_result_t ugui_theme_apply_light(void)
{
    ugui_theme_t theme = create_theme_light();
    return ugui_theme_set(&theme);
}

uflake_result_t ugui_theme_apply_blue(void)
{
    ugui_theme_t theme = create_theme_blue();
    return ugui_theme_set(&theme);
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

    update_background_display();

    return UFLAKE_OK;
}

uflake_result_t ugui_theme_set_bg_image_sdcard(const char *path)
{
    if (!g_theme_mgr.initialized || !path)
    {
        return UFLAKE_ERROR;
    }

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
    sd_ctx_t ctx = {};
    sd_reader.user_ctx = &ctx;

    // Decode options: resize to fit display (240x240)
    img_decode_opts_t opts;
    opts.resize = true;
    opts.new_width = UGUI_DISPLAY_WIDTH;
    opts.new_height = UGUI_DISPLAY_HEIGHT;
    opts.rotate = IMG_ROTATE_0;
    opts.scale = IMG_SCALE_NONE;

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
            lv_obj_move_background(g_theme_mgr.bg_image); // Send to back behind all content

            // Set image source
            lv_image_set_src(g_theme_mgr.bg_image, g_theme_mgr.bg_image_dsc);

            g_theme_mgr.background.type = UGUI_BG_IMAGE_SDCARD;
            strncpy(g_theme_mgr.background.image_path, path,
                    sizeof(g_theme_mgr.background.image_path) - 1);

            // Update background display (makes bg transparent, shows image)
            update_background_display();

            UFLAKE_LOGI(TAG, "Background image loaded successfully");
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

    return UFLAKE_ERROR;
}

uflake_result_t ugui_theme_set_bg_image_flash(const char *path)
{
    if (!g_theme_mgr.initialized || !path)
    {
        return UFLAKE_ERROR;
    }

    g_theme_mgr.background.type = UGUI_BG_IMAGE_FLASH;
    strncpy(g_theme_mgr.background.image_path, path, sizeof(g_theme_mgr.background.image_path) - 1);

    UFLAKE_LOGI(TAG, "Flash background set: %s", path);

    update_background_display();

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

    update_background_display();

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

    lv_obj_set_style_bg_color(btn, color, 0);
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
