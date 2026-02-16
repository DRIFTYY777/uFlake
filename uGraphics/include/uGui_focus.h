/**
 * @file uGui_focus.h
 * @brief Focus Manager - Automatic, crash-free focus handling for uFlake GUI
 * 
 * This module solves the critical LVGL focus/input routing problem:
 * - Automatic focus management (no manual lv_group_focus())
 * - Safe object deletion (no crashes when deleting focused objects)
 * - Layer-based priority system
 * - Input routing to correct objects
 * 
 * PROBLEM SOLVED:
 * When you create lv_obj_create() and add components, you need to manually
 * force focus. When deleting objects, LVGL can crash if focus isn't handled.
 * This manager automates everything and prevents crashes.
 */

#ifndef UGUI_FOCUS_H
#define UGUI_FOCUS_H

#include "uGui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize the focus manager
 * Must be called after LVGL init
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_init(void);

// ============================================================================
// FOCUS REGISTRATION & MANAGEMENT
// ============================================================================

/**
 * @brief Register an object for focus management
 * 
 * This registers an LVGL object to be part of the focus system.
 * The object will automatically receive focus based on its layer priority.
 * When deleted, focus is safely transferred.
 * 
 * @param obj LVGL object to register
 * @param layer Focus layer (determines priority)
 * @param auto_focus Auto-focus when registered (true for app windows)
 * @return Focus context handle, NULL on error
 */
ugui_focus_ctx_t *ugui_focus_register(lv_obj_t *obj, ugui_layer_t layer, bool auto_focus);

/**
 * @brief Unregister an object from focus management
 * 
 * CRITICAL: Always call this BEFORE lv_obj_del() to prevent crashes.
 * This safely transfers focus away from the object before deletion.
 * 
 * @param ctx Focus context to unregister
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_unregister(ugui_focus_ctx_t *ctx);

/**
 * @brief Request focus for an object (layer-aware)
 * 
 * @param ctx Focus context
 * @return UFLAKE_OK on success, UFLAKE_ERROR if blocked by higher layer
 */
uflake_result_t ugui_focus_request(ugui_focus_ctx_t *ctx);

/**
 * @brief Release focus from an object
 * 
 * @param ctx Focus context
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_release(ugui_focus_ctx_t *ctx);

/**
 * @brief Check if object has focus
 * 
 * @param ctx Focus context
 * @return true if focused
 */
bool ugui_focus_has_focus(ugui_focus_ctx_t *ctx);

// ============================================================================
// INPUT GROUP MANAGEMENT
// ============================================================================

/**
 * @brief Add an LVGL object to the focus input group
 * 
 * This enables keyboard navigation for the object.
 * Objects must be focusable (buttons, text areas, etc.)
 * 
 * @param ctx Focus context
 * @param obj LVGL object to add
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_add_to_group(ugui_focus_ctx_t *ctx, lv_obj_t *obj);

/**
 * @brief Remove an LVGL object from the focus input group
 * 
 * @param ctx Focus context
 * @param obj LVGL object to remove
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_remove_from_group(ugui_focus_ctx_t *ctx, lv_obj_t *obj);

/**
 * @brief Navigate to next focusable object in group
 * 
 * @param ctx Focus context
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_next(ugui_focus_ctx_t *ctx);

/**
 * @brief Navigate to previous focusable object in group
 * 
 * @param ctx Focus context
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_prev(ugui_focus_ctx_t *ctx);

// ============================================================================
// LAYER MANAGEMENT
// ============================================================================

/**
 * @brief Get the currently active focus layer
 * 
 * @return Current top-most active layer
 */
ugui_layer_t ugui_focus_get_active_layer(void);

/**
 * @brief Get the focused object on a specific layer
 * 
 * @param layer Layer to query
 * @return Focused LVGL object, NULL if no focus on layer
 */
lv_obj_t *ugui_focus_get_layer_object(ugui_layer_t layer);

/**
 * @brief Block input to lower layers (for modals/dialogs)
 * 
 * @param layer Layer to block below
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_block_below(ugui_layer_t layer);

/**
 * @brief Unblock input to lower layers
 * 
 * @param layer Layer to unblock
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_unblock_below(ugui_layer_t layer);

// ============================================================================
// SAFE DELETION HELPERS
// ============================================================================

/**
 * @brief Safely delete an LVGL object with focus management
 * 
 * This is a wrapper around lv_obj_del() that automatically handles focus.
 * Use this instead of lv_obj_del() for focus-registered objects.
 * 
 * Example:
 *   ugui_focus_ctx_t *ctx = ugui_focus_register(my_obj, UGUI_LAYER_APP_WINDOW, true);
 *   // ... use object ...
 *   ugui_focus_safe_delete(ctx, my_obj); // Safe - no crash
 * 
 * @param ctx Focus context (can be NULL if not registered)
 * @param obj LVGL object to delete
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_safe_delete(ugui_focus_ctx_t *ctx, lv_obj_t *obj);

/**
 * @brief Safely delete an LVGL object's children with focus management
 * 
 * Deletes all children of an object, handling focus correctly.
 * 
 * @param parent Parent object
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_safe_delete_children(lv_obj_t *parent);

// ============================================================================
// DEBUG AND INFO
// ============================================================================

/**
 * @brief Get focus manager statistics
 * 
 * @param registered_count Output: number of registered objects
 * @param active_layer Output: active layer
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_focus_get_stats(uint32_t *registered_count, ugui_layer_t *active_layer);

/**
 * @brief Print focus manager debug info
 */
void ugui_focus_debug_print(void);

#ifdef __cplusplus
}
#endif

#endif // UGUI_FOCUS_H
