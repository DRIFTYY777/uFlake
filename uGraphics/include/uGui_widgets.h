/**
 * @file uGui_widgets.h
 * @brief Widget Library - Reusable GUI components
 * 
 * Provides ready-to-use widgets:
 * - Alert dialogs (OK, OK/Cancel, Yes/No)
 * - Loading indicators (dots, spinner, progress bar)
 * - Message boxes
 * - Input boxes
 * - List widgets
 * 
 * All widgets are theme-aware and focus-managed automatically.
 */

#ifndef UGUI_WIDGETS_H
#define UGUI_WIDGETS_H

#include "uGui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// DIALOG WIDGETS
// ============================================================================

/**
 * @brief Show an OK dialog
 * 
 * Modal dialog with title, message, and OK button.
 * Blocks lower layers until dismissed.
 * 
 * @param title Dialog title
 * @param message Dialog message
 * @param callback Callback when OK pressed (can be NULL)
 * @param userdata User data for callback
 * @return Dialog object, NULL on error
 */
lv_obj_t *ugui_dialog_ok(const char *title, const char *message, 
                          ugui_dialog_cb_t callback, void *userdata);

/**
 * @brief Show an OK/Cancel dialog
 * 
 * @param title Dialog title
 * @param message Dialog message
 * @param callback Callback when button pressed (UGUI_DIALOG_BTN_OK or CANCEL)
 * @param userdata User data for callback
 * @return Dialog object, NULL on error
 */
lv_obj_t *ugui_dialog_ok_cancel(const char *title, const char *message,
                                 ugui_dialog_cb_t callback, void *userdata);

/**
 * @brief Show a Yes/No dialog
 * 
 * @param title Dialog title
 * @param message Dialog message
 * @param callback Callback when button pressed (UGUI_DIALOG_BTN_YES or NO)
 * @param userdata User data for callback
 * @return Dialog object, NULL on error
 */
lv_obj_t *ugui_dialog_yes_no(const char *title, const char *message,
                              ugui_dialog_cb_t callback, void *userdata);

/**
 * @brief Show a message box (auto-dismiss after timeout)
 * 
 * @param message Message to display
 * @param duration_ms Display duration in milliseconds (0 = no timeout)
 * @return Message box object, NULL on error
 */
lv_obj_t *ugui_show_message(const char *message, uint32_t duration_ms);

/**
 * @brief Close a dialog or message box
 * 
 * @param dialog Dialog object
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_close_dialog(lv_obj_t *dialog);

// ============================================================================
// LOADING INDICATORS
// ============================================================================

/**
 * @brief Show a loading indicator
 * 
 * Displays loading animation with optional message.
 * Shown in system layer (always on top).
 * 
 * @param message Loading message (can be NULL)
 * @param style Loading style (dots, spinner, or bar)
 * @return Loading widget object, NULL on error
 */
lv_obj_t *ugui_show_loading(const char *message, ugui_loading_style_t style);

/**
 * @brief Update loading message
 * 
 * @param loading Loading widget object
 * @param message New message
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_loading_set_message(lv_obj_t *loading, const char *message);

/**
 * @brief Update loading progress (for progress bar style)
 * 
 * @param loading Loading widget object
 * @param percent Progress percentage (0-100)
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_loading_set_progress(lv_obj_t *loading, uint8_t percent);

/**
 * @brief Hide loading indicator
 * 
 * @param loading Loading widget object
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_hide_loading(lv_obj_t *loading);

// ============================================================================
// INPUT WIDGETS
// ============================================================================

/**
 * @brief Input box callback
 */
typedef void (*ugui_input_cb_t)(const char *text, void *userdata);

/**
 * @brief Show an input box
 * 
 * Modal input dialog with text area.
 * 
 * @param title Dialog title
 * @param placeholder Placeholder text
 * @param callback Callback with entered text
 * @param userdata User data for callback
 * @return Input box object, NULL on error
 */
lv_obj_t *ugui_input_box(const char *title, const char *placeholder,
                          ugui_input_cb_t callback, void *userdata);

// ============================================================================
// LIST WIDGETS
// ============================================================================

/**
 * @brief List item callback
 */
typedef void (*ugui_list_cb_t)(uint32_t index, const char *item, void *userdata);

/**
 * @brief Create a scrollable list widget
 * 
 * @param parent Parent container
 * @param items Array of item strings
 * @param item_count Number of items
 * @param callback Callback when item selected
 * @param userdata User data for callback
 * @return List widget object, NULL on error
 */
lv_obj_t *ugui_list_create(lv_obj_t *parent, const char **items, uint32_t item_count,
                            ugui_list_cb_t callback, void *userdata);

/**
 * @brief Add item to list
 * 
 * @param list List widget object
 * @param item Item text
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_list_add_item(lv_obj_t *list, const char *item);

/**
 * @brief Remove item from list
 * 
 * @param list List widget object
 * @param index Item index
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_list_remove_item(lv_obj_t *list, uint32_t index);

/**
 * @brief Clear all items from list
 * 
 * @param list List widget object
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_list_clear(lv_obj_t *list);

/**
 * @brief Scroll to item in list
 * 
 * @param list List widget object
 * @param index Item index
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_list_scroll_to(lv_obj_t *list, uint32_t index);

// ============================================================================
// IMAGE WIDGETS
// ============================================================================

/**
 * @brief Create an image widget (loads JPEG from SD card)
 * 
 * @param parent Parent container
 * @param path Path to JPEG file (e.g., "/sdcard/image.jpg")
 * @param width Desired width (0 = original)
 * @param height Desired height (0 = original)
 * @return Image widget object, NULL on error
 */
lv_obj_t *ugui_image_create(lv_obj_t *parent, const char *path, uint16_t width, uint16_t height);

/**
 * @brief Update image from new path
 * 
 * @param img Image widget object
 * @param path New image path
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_image_set_src(lv_obj_t *img, const char *path);

// ============================================================================
// HELPER WIDGETS
// ============================================================================

/**
 * @brief Create a themed button
 * 
 * @param parent Parent container
 * @param text Button text
 * @param width Button width (0 = auto)
 * @param height Button height (0 = auto)
 * @return Button object, NULL on error
 */
lv_obj_t *ugui_button_create(lv_obj_t *parent, const char *text, uint16_t width, uint16_t height);

/**
 * @brief Create a themed label
 * 
 * @param parent Parent container
 * @param text Label text
 * @return Label object, NULL on error
 */
lv_obj_t *ugui_label_create(lv_obj_t *parent, const char *text);

/**
 * @brief Create a themed panel
 * 
 * @param parent Parent container
 * @param width Panel width
 * @param height Panel height
 * @param transparent true for transparent background
 * @return Panel object, NULL on error
 */
lv_obj_t *ugui_panel_create(lv_obj_t *parent, uint16_t width, uint16_t height, bool transparent);

#ifdef __cplusplus
}
#endif

#endif // UGUI_WIDGETS_H
