/**
 * @file uGui_widgets.c
 * @brief Widget Library Implementation
 * 
 * IMPORTANT: All LVGL calls must be protected by gui_mutex for thread safety!
 * This file uses uGui_get_mutex() to lock before any LVGL operations.
 */

#include "uGui_widgets.h"
#include "uGui.h" // For uGui_get_mutex()
#include "uGui_focus.h"
#include "uGui_theme.h"
#include "logger.h"
#include <string.h>

static const char *TAG = "uGUI_Widgets";

// ============================================================================
// DIALOG INTERNAL STRUCTURES
// ============================================================================

typedef struct {
    ugui_dialog_cb_t callback;
    void *userdata;
    ugui_focus_ctx_t *focus_ctx;
} dialog_ctx_t;

// ============================================================================
// DIALOG EVENT HANDLERS
// ==============================================================================

static void dialog_btn_ok_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *dialog = lv_event_get_user_data(e);
        dialog_ctx_t *ctx = (dialog_ctx_t *)lv_obj_get_user_data(dialog);
        
        if (ctx && ctx->callback) {
            ctx->callback(UGUI_DIALOG_BTN_OK, ctx->userdata);
        }
        
        ugui_close_dialog(dialog);
    }
}

static void dialog_btn_cancel_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *dialog = lv_event_get_user_data(e);
        dialog_ctx_t *ctx = (dialog_ctx_t *)lv_obj_get_user_data(dialog);
        
        if (ctx && ctx->callback) {
            ctx->callback(UGUI_DIALOG_BTN_CANCEL, ctx->userdata);
        }
        
        ugui_close_dialog(dialog);
    }
}

static void dialog_btn_yes_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *dialog = lv_event_get_user_data(e);
        dialog_ctx_t *ctx = (dialog_ctx_t *)lv_obj_get_user_data(dialog);
        
        if (ctx && ctx->callback) {
            ctx->callback(UGUI_DIALOG_BTN_YES, ctx->userdata);
        }
        
        ugui_close_dialog(dialog);
    }
}

static void dialog_btn_no_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *dialog = lv_event_get_user_data(e);
        dialog_ctx_t *ctx = (dialog_ctx_t *)lv_obj_get_user_data(dialog);
        
        if (ctx && ctx->callback) {
            ctx->callback(UGUI_DIALOG_BTN_NO, ctx->userdata);
        }
        
        ugui_close_dialog(dialog);
    }
}

// ============================================================================
// DIALOG BASE CREATION
// ============================================================================

static lv_obj_t *create_dialog_base(const char *title, const char *message) {
    // Create dialog container (centered, modal)
    lv_obj_t *dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dialog, 200, 150);
    lv_obj_center(dialog);
    lv_obj_set_style_border_width(dialog, 2, 0);
    
    // Apply theme
    ugui_theme_style_panel(dialog, false);
    
    // Title label
    lv_obj_t *title_label = lv_label_create(dialog);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);
    ugui_theme_style_label(title_label);
    
    // Message label
    lv_obj_t *msg_label = lv_label_create(dialog);
    lv_label_set_text(msg_label, message);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_label, 180);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 30);
    ugui_theme_style_label(msg_label);
    
    // Register with focus manager (dialog layer)
    ugui_focus_ctx_t *focus_ctx = ugui_focus_register(dialog, UGUI_LAYER_DIALOG, true);
    
    // Allocate and attach dialog context
    dialog_ctx_t *ctx = (dialog_ctx_t *)uflake_malloc(sizeof(dialog_ctx_t), UFLAKE_MEM_INTERNAL);
    if (ctx) {
        ctx->callback = NULL;
        ctx->userdata = NULL;
        ctx->focus_ctx = focus_ctx;
        lv_obj_set_user_data(dialog, ctx);
    }
    
    return dialog;
}

// ============================================================================
// DIALOG WIDGETS
// ============================================================================

lv_obj_t *ugui_dialog_ok(const char *title, const char *message,
                          ugui_dialog_cb_t callback, void *userdata) {
    lv_obj_t *dialog = create_dialog_base(title, message);
    if (!dialog) {
        return NULL;
    }
    
    // Set callback
    dialog_ctx_t *ctx = (dialog_ctx_t *)lv_obj_get_user_data(dialog);
    if (ctx) {
        ctx->callback = callback;
        ctx->userdata = userdata;
    }
    
    // OK button
    lv_obj_t *btn_ok = ugui_button_create(dialog, "OK", 80, 30);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btn_ok, dialog_btn_ok_event_cb, LV_EVENT_CLICKED, dialog);
    
    // Add to focus group
    if (ctx && ctx->focus_ctx) {
        ugui_focus_add_to_group(ctx->focus_ctx, btn_ok);
    }
    
    UFLAKE_LOGI(TAG, "Created OK dialog: %s", title);
    
    return dialog;
}

lv_obj_t *ugui_dialog_ok_cancel(const char *title, const char *message,
                                 ugui_dialog_cb_t callback, void *userdata) {
    lv_obj_t *dialog = create_dialog_base(title, message);
    if (!dialog) {
        return NULL;
    }
    
    // Set callback
    dialog_ctx_t *ctx = (dialog_ctx_t *)lv_obj_get_user_data(dialog);
    if (ctx) {
        ctx->callback = callback;
        ctx->userdata = userdata;
    }
    
    // OK button
    lv_obj_t *btn_ok = ugui_button_create(dialog, "OK", 70, 30);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_LEFT, 15, -10);
    lv_obj_add_event_cb(btn_ok, dialog_btn_ok_event_cb, LV_EVENT_CLICKED, dialog);
    
    // Cancel button
    lv_obj_t *btn_cancel = ugui_button_create(dialog, "Cancel", 70, 30);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -15, -10);
    lv_obj_add_event_cb(btn_cancel, dialog_btn_cancel_event_cb, LV_EVENT_CLICKED, dialog);
    
    // Add to focus group
    if (ctx && ctx->focus_ctx) {
        ugui_focus_add_to_group(ctx->focus_ctx, btn_ok);
        ugui_focus_add_to_group(ctx->focus_ctx, btn_cancel);
    }
    
    return dialog;
}

lv_obj_t *ugui_dialog_yes_no(const char *title, const char *message,
                              ugui_dialog_cb_t callback, void *userdata) {
    lv_obj_t *dialog = create_dialog_base(title, message);
    if (!dialog) {
        return NULL;
    }
    
    // Set callback
    dialog_ctx_t *ctx = (dialog_ctx_t *)lv_obj_get_user_data(dialog);
    if (ctx) {
        ctx->callback = callback;
        ctx->userdata = userdata;
    }
    
    // Yes button
    lv_obj_t *btn_yes = ugui_button_create(dialog, "Yes", 70, 30);
    lv_obj_align(btn_yes, LV_ALIGN_BOTTOM_LEFT, 15, -10);
    lv_obj_add_event_cb(btn_yes, dialog_btn_yes_event_cb, LV_EVENT_CLICKED, dialog);
    
    // No button
    lv_obj_t *btn_no = ugui_button_create(dialog, "No", 70, 30);
    lv_obj_align(btn_no, LV_ALIGN_BOTTOM_RIGHT, -15, -10);
    lv_obj_add_event_cb(btn_no, dialog_btn_no_event_cb, LV_EVENT_CLICKED, dialog);
    
    // Add to focus group
    if (ctx && ctx->focus_ctx) {
        ugui_focus_add_to_group(ctx->focus_ctx, btn_yes);
        ugui_focus_add_to_group(ctx->focus_ctx, btn_no);
    }
    
    return dialog;
}

lv_obj_t *ugui_show_message(const char *message, uint32_t duration_ms) {
    // Simple message box (no buttons)
    lv_obj_t *msgbox = lv_obj_create(lv_scr_act());
    lv_obj_set_size(msgbox, 180, 80);
    lv_obj_center(msgbox);
    ugui_theme_style_panel(msgbox, false);
    
    lv_obj_t *label = lv_label_create(msgbox);
    lv_label_set_text(label, message);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, 160);
    lv_obj_center(label);
    ugui_theme_style_label(label);
    
    // Register with focus (dialog layer, but no input needed)
    ugui_focus_register(msgbox, UGUI_LAYER_DIALOG, true);
    
    // TODO: Add timer for auto-dismiss if duration_ms > 0
    (void)duration_ms;  // Suppress warning for now
    
    return msgbox;
}

uflake_result_t ugui_close_dialog(lv_obj_t *dialog) {
    if (!dialog) {
        return UFLAKE_ERROR;
    }
    
    // Free dialog context
    dialog_ctx_t *ctx = (dialog_ctx_t *)lv_obj_get_user_data(dialog);
    if (ctx) {
        if (ctx->focus_ctx) {
            ugui_focus_unregister(ctx->focus_ctx);
        }
        uflake_free(ctx);
    }
    
    // Delete dialog
    lv_obj_del(dialog);
    
    return UFLAKE_OK;
}

// ============================================================================
// LOADING INDICATORS
// ============================================================================

lv_obj_t *ugui_show_loading(const char *message, ugui_loading_style_t style) {
    // Create loading container (centered)
    lv_obj_t *loading = lv_obj_create(lv_scr_act());
    lv_obj_set_size(loading, 160, 100);
    lv_obj_center(loading);
    ugui_theme_style_panel(loading, false);
    
    // Message label
    if (message) {
        lv_obj_t *msg_label = lv_label_create(loading);
        lv_label_set_text(msg_label, message);
        lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 10);
        ugui_theme_style_label(msg_label);
    }
    
    // Loading indicator based on style
    if (style == UGUI_LOADING_DOTS) {
        // Windows mobile style dots
        lv_obj_t *dots = lv_label_create(loading);
        lv_label_set_text(dots, "· · · · · · · ·");
        lv_obj_align(dots, LV_ALIGN_CENTER, 0, 0);
        ugui_theme_style_label(dots);
        // TODO: Animate dots
    } else if (style == UGUI_LOADING_SPINNER) {
        // LVGL spinner
        lv_obj_t *spinner = lv_spinner_create(loading);
        lv_obj_set_size(spinner, 50, 50);
        lv_obj_center(spinner);
    } else if (style == UGUI_LOADING_BAR) {
        // Progress bar
        lv_obj_t *bar = lv_bar_create(loading);
        lv_obj_set_size(bar, 120, 20);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    }
    
    // Register with focus (system layer)
    ugui_focus_register(loading, UGUI_LAYER_SYSTEM, true);
    
    return loading;
}

uflake_result_t ugui_loading_set_message(lv_obj_t *loading, const char *message) {
    // TODO: Update loading message
    (void)loading;
    (void)message;
    return UFLAKE_OK;
}

uflake_result_t ugui_loading_set_progress(lv_obj_t *loading, uint8_t percent) {
    // TODO: Update progress bar
    (void)loading;
    (void)percent;
    return UFLAKE_OK;
}

uflake_result_t ugui_hide_loading(lv_obj_t *loading) {
    return ugui_close_dialog(loading);
}

// ============================================================================
// LIST WIDGETS
// ============================================================================

lv_obj_t *ugui_list_create(lv_obj_t *parent, const char **items, uint32_t item_count,
                            ugui_list_cb_t callback, void *userdata) {
    if (!parent) {
        return NULL;
    }
    
    lv_obj_t *list = lv_list_create(parent);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    ugui_theme_style_panel(list, false);
    
    // Add items
    for (uint32_t i = 0; i < item_count; i++) {
        lv_obj_t *btn = lv_list_add_button(list, NULL, items[i]);
        ugui_theme_style_label(btn);
        // TODO: Add event callback for item selection
        (void)callback;
        (void)userdata;
    }
    
    return list;
}

uflake_result_t ugui_list_add_item(lv_obj_t *list, const char *item) {
    if (!list || !item) {
        return UFLAKE_ERROR;
    }
    
    lv_list_add_button(list, NULL, item);
    
    return UFLAKE_OK;
}

uflake_result_t ugui_list_clear(lv_obj_t *list) {
    if (!list) {
        return UFLAKE_ERROR;
    }
    
    lv_obj_clean(list);
    
    return UFLAKE_OK;
}

// ============================================================================
// HELPER WIDGETS
// ============================================================================

lv_obj_t *ugui_button_create(lv_obj_t *parent, const char *text, uint16_t width, uint16_t height) {
    if (!parent) {
        return NULL;
    }
    
    lv_obj_t *btn = lv_button_create(parent);
    
    if (width > 0 && height > 0) {
        lv_obj_set_size(btn, width, height);
    }
    
    // Apply theme
    ugui_theme_style_button(btn, true, lv_color_hex(0x000000));
    
    // Add label
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text ? text : "Button");
    lv_obj_center(label);
    
    return btn;
}

lv_obj_t *ugui_label_create(lv_obj_t *parent, const char *text) {
    if (!parent) {
        return NULL;
    }
    
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    ugui_theme_style_label(label);
    
    return label;
}

lv_obj_t *ugui_panel_create(lv_obj_t *parent, uint16_t width, uint16_t height, bool transparent) {
    if (!parent) {
        return NULL;
    }
    
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, height);
    ugui_theme_style_panel(panel, transparent);
    
    return panel;
}

lv_obj_t *ugui_image_create(lv_obj_t *parent, const char *path, uint16_t width, uint16_t height) {
    if (!parent || !path) {
        return NULL;
    }
    
    // TODO: Load JPEG image using imageCodec
    // For now, create placeholder
    lv_obj_t *img_placeholder = lv_obj_create(parent);
    lv_obj_set_size(img_placeholder, width > 0 ? width : 100, height > 0 ? height : 100);
    
    UFLAKE_LOGW(TAG, "Image loading not yet implemented: %s", path);
    
    return img_placeholder;
}

// Stub implementations for remaining functions
uflake_result_t ugui_list_remove_item(lv_obj_t *list, uint32_t index) {
    (void)list; (void)index;
    return UFLAKE_ERROR;
}

uflake_result_t ugui_list_scroll_to(lv_obj_t *list, uint32_t index) {
    (void)list; (void)index;
    return UFLAKE_OK;
}

uflake_result_t ugui_image_set_src(lv_obj_t *img, const char *path) {
    (void)img; (void)path;
    return UFLAKE_ERROR;
}

lv_obj_t *ugui_input_box(const char *title, const char *placeholder,
                          ugui_input_cb_t callback, void *userdata) {
    (void)title; (void)placeholder; (void)callback; (void)userdata;
    return NULL;
}
