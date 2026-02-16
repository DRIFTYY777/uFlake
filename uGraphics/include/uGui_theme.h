/**
 * @file uGui_theme.h
 * @brief Theme and Background Manager
 * 
 * Manages:
 * - Global theme colors (transparent with changeable colors)
 * - Background wallpaper (solid color or JPEG from SD card)
 * - Per-component color overrides
 */

#ifndef UGUI_THEME_H
#define UGUI_THEME_H

#include "uGui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize theme manager
 * Must be called after LVGL init
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_init(void);

// ============================================================================
// THEME MANAGEMENT
// ============================================================================

/**
 * @brief Set global theme
 * 
 * @param theme Theme configuration
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_set(const ugui_theme_t *theme);

/**
 * @brief Get current theme
 * 
 * @param theme Output: current theme
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_get(ugui_theme_t *theme);

/**
 * @brief Set primary theme color
 * 
 * @param color Primary color
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_set_primary_color(lv_color_t color);

/**
 * @brief Get primary theme color
 * 
 * @return Primary color
 */
lv_color_t ugui_theme_get_primary_color(void);

// ============================================================================
// PRE-DEFINED THEMES
// ============================================================================

/**
 * @brief Apply dark theme
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_apply_dark(void);

/**
 * @brief Apply light theme
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_apply_light(void);

/**
 * @brief Apply blue theme (Flipper-like)
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_apply_blue(void);

/**
 * @brief Apply custom theme by name
 * 
 * Available themes: "dark", "light", "blue", "green", "red", "purple"
 * 
 * @param theme_name Theme name
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_apply_by_name(const char *theme_name);

// ============================================================================
// BACKGROUND MANAGEMENT
// ============================================================================

/**
 * @brief Set background to solid color
 * 
 * @param color Background color
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_set_bg_color(lv_color_t color);

/**
 * @brief Set background to image from SD card
 * 
 * Loads a JPEG image from SD card as wallpaper.
 * Image is scaled/cropped to fit 240x240 display.
 * 
 * @param path Path to JPEG file on SD card (e.g., "/sdcard/wallpaper.jpg")
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_set_bg_image_sdcard(const char *path);

/**
 * @brief Set background to image from flash
 * 
 * @param path Path to image in flash filesystem
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_set_bg_image_flash(const char *path);

/**
 * @brief Get current background configuration
 * 
 * @param bg Output: background configuration
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_get_background(ugui_background_t *bg);

/**
 * @brief Refresh background (reload/redraw)
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_refresh_background(void);

// ============================================================================
// COMPONENT STYLING HELPERS
// ============================================================================

/**
 * @brief Apply theme to LVGL button
 * 
 * @param btn Button object
 * @param use_theme_color true to use theme color, false for custom
 * @param custom_color Custom color (ignored if use_theme_color is true)
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_style_button(lv_obj_t *btn, bool use_theme_color, lv_color_t custom_color);

/**
 * @brief Apply theme to LVGL label
 * 
 * @param label Label object
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_style_label(lv_obj_t *label);

/**
 * @brief Apply theme to LVGL panel/container
 * 
 * @param panel Panel object
 * @param transparent true for transparent background
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_style_panel(lv_obj_t *panel, bool transparent);

/**
 * @brief Apply theme to any LVGL object
 * 
 * @param obj LVGL object
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_style_object(lv_obj_t *obj);

// ============================================================================
// ADVANCED
// ============================================================================

/**
 * @brief Save current theme to persistent storage
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_save(void);

/**
 * @brief Load theme from persistent storage
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_theme_load(void);

/**
 * @brief Get background LVGL object
 * For advanced customization
 * 
 * @return Background object, NULL if not initialized
 */
lv_obj_t *ugui_theme_get_bg_object(void);

#ifdef __cplusplus
}
#endif

#endif // UGUI_THEME_H
