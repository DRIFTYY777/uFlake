/**
 * @file uGui_focus.c
 * @brief Focus Manager Implementation
 */

#include "uGui_focus.h"
#include "logger.h"
#include <string.h>

static const char *TAG = "uGUI_Focus";

// ============================================================================
// FOCUS MANAGER STATE
// ============================================================================

#define MAX_FOCUS_CONTEXTS 16

typedef struct {
    bool initialized;
    ugui_focus_ctx_t contexts[MAX_FOCUS_CONTEXTS];
    lv_group_t *focus_groups[5];  // One per layer
    uflake_mutex_t *mutex;
    ugui_layer_t active_layer;
    uint32_t registered_count;
} focus_manager_t;

static focus_manager_t g_focus_mgr = {0};

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static inline bool is_valid_layer(ugui_layer_t layer) {
    return layer >= UGUI_LAYER_BACKGROUND && layer <= UGUI_LAYER_SYSTEM;
}

static ugui_focus_ctx_t *find_context_by_obj(lv_obj_t *obj) {
    for (int i = 0; i < MAX_FOCUS_CONTEXTS; i++) {
        if (g_focus_mgr.contexts[i].focused_obj == obj) {
            return &g_focus_mgr.contexts[i];
        }
    }
    return NULL;
}

static ugui_focus_ctx_t *find_free_context(void) {
    for (int i = 0; i < MAX_FOCUS_CONTEXTS; i++) {
        if (g_focus_mgr.contexts[i].focused_obj == NULL) {
            return &g_focus_mgr.contexts[i];
        }
    }
    return NULL;
}

static ugui_layer_t find_highest_active_layer(void) {
    // Find highest layer with registered objects
    for (int layer = UGUI_LAYER_SYSTEM; layer >= UGUI_LAYER_BACKGROUND; layer--) {
        for (int i = 0; i < MAX_FOCUS_CONTEXTS; i++) {
            if (g_focus_mgr.contexts[i].focused_obj != NULL && 
                g_focus_mgr.contexts[i].layer == layer) {
                return (ugui_layer_t)layer;
            }
        }
    }
    return UGUI_LAYER_BACKGROUND;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t ugui_focus_init(void) {
    if (g_focus_mgr.initialized) {
        UFLAKE_LOGW(TAG, "Focus manager already initialized");
        return UFLAKE_OK;
    }

    memset(&g_focus_mgr, 0, sizeof(focus_manager_t));

    // Create mutex for thread safety
    if (uflake_mutex_create(&g_focus_mgr.mutex) != UFLAKE_OK) {
        UFLAKE_LOGE(TAG, "Failed to create focus mutex");
        return UFLAKE_ERROR;
    }

    // Create LVGL input groups for each layer
    for (int i = 0; i < 5; i++) {
        g_focus_mgr.focus_groups[i] = lv_group_create();
        if (!g_focus_mgr.focus_groups[i]) {
            UFLAKE_LOGE(TAG, "Failed to create focus group for layer %d", i);
            return UFLAKE_ERROR;
        }
    }

    g_focus_mgr.active_layer = UGUI_LAYER_BACKGROUND;
    g_focus_mgr.initialized = true;

    UFLAKE_LOGI(TAG, "Focus manager initialized");
    return UFLAKE_OK;
}

// ============================================================================
// FOCUS REGISTRATION & MANAGEMENT
// ============================================================================

ugui_focus_ctx_t *ugui_focus_register(lv_obj_t *obj, ugui_layer_t layer, bool auto_focus) {
    if (!g_focus_mgr.initialized || !obj || !is_valid_layer(layer)) {
        UFLAKE_LOGE(TAG, "Invalid focus registration parameters");
        return NULL;
    }

    uflake_mutex_lock(g_focus_mgr.mutex, UINT32_MAX);

    // Check if already registered
    ugui_focus_ctx_t *existing = find_context_by_obj(obj);
    if (existing) {
        UFLAKE_LOGW(TAG, "Object already registered");
        uflake_mutex_unlock(g_focus_mgr.mutex);
        return existing;
    }

    // Find free context
    ugui_focus_ctx_t *ctx = find_free_context();
    if (!ctx) {
        UFLAKE_LOGE(TAG, "No free focus contexts (max %d)", MAX_FOCUS_CONTEXTS);
        uflake_mutex_unlock(g_focus_mgr.mutex);
        return NULL;
    }

    // Initialize context
    ctx->focused_obj = obj;
    ctx->layer = layer;
    ctx->input_enabled = true;
    ctx->userdata = NULL;

    g_focus_mgr.registered_count++;

    UFLAKE_LOGI(TAG, "Registered object %p on layer %d", obj, layer);

    // Auto-focus if requested and on top layer
    if (auto_focus && layer >= g_focus_mgr.active_layer) {
        g_focus_mgr.active_layer = layer;
        UFLAKE_LOGI(TAG, "Auto-focused object %p", obj);
    }

    uflake_mutex_unlock(g_focus_mgr.mutex);

    return ctx;
}

uflake_result_t ugui_focus_unregister(ugui_focus_ctx_t *ctx) {
    if (!g_focus_mgr.initialized || !ctx || !ctx->focused_obj) {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_focus_mgr.mutex, UINT32_MAX);

    lv_obj_t *obj = ctx->focused_obj;
    ugui_layer_t layer = ctx->layer;

    // Remove from input group
    if (g_focus_mgr.focus_groups[layer]) {
        lv_group_remove_obj(obj);
    }

    // Clear context
    memset(ctx, 0, sizeof(ugui_focus_ctx_t));
    g_focus_mgr.registered_count--;

    // Update active layer if this was on active layer
    g_focus_mgr.active_layer = find_highest_active_layer();

    UFLAKE_LOGI(TAG, "Unregistered object %p (active layer now %d)", obj, g_focus_mgr.active_layer);

    uflake_mutex_unlock(g_focus_mgr.mutex);

    return UFLAKE_OK;
}

uflake_result_t ugui_focus_request(ugui_focus_ctx_t *ctx) {
    if (!g_focus_mgr.initialized || !ctx || !ctx->focused_obj) {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_focus_mgr.mutex, UINT32_MAX);

    // Only grant focus if layer is top-most or higher
    if (ctx->layer >= g_focus_mgr.active_layer) {
        g_focus_mgr.active_layer = ctx->layer;
        UFLAKE_LOGI(TAG, "Focus granted to object %p on layer %d", ctx->focused_obj, ctx->layer);
        uflake_mutex_unlock(g_focus_mgr.mutex);
        return UFLAKE_OK;
    } else {
        UFLAKE_LOGW(TAG, "Focus denied - layer %d blocked by layer %d", ctx->layer, g_focus_mgr.active_layer);
        uflake_mutex_unlock(g_focus_mgr.mutex);
        return UFLAKE_ERROR;
    }
}

uflake_result_t ugui_focus_release(ugui_focus_ctx_t *ctx) {
    if (!g_focus_mgr.initialized || !ctx) {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_focus_mgr.mutex, UINT32_MAX);

    // Update active layer to next highest
    g_focus_mgr.active_layer = find_highest_active_layer();

    UFLAKE_LOGI(TAG, "Focus released, active layer now %d", g_focus_mgr.active_layer);

    uflake_mutex_unlock(g_focus_mgr.mutex);

    return UFLAKE_OK;
}

bool ugui_focus_has_focus(ugui_focus_ctx_t *ctx) {
    if (!g_focus_mgr.initialized || !ctx) {
        return false;
    }

    return ctx->layer == g_focus_mgr.active_layer && ctx->input_enabled;
}

// ============================================================================
// INPUT GROUP MANAGEMENT
// ============================================================================

uflake_result_t ugui_focus_add_to_group(ugui_focus_ctx_t *ctx, lv_obj_t *obj) {
    if (!g_focus_mgr.initialized || !ctx || !obj) {
        return UFLAKE_ERROR;
    }

    if (!is_valid_layer(ctx->layer)) {
        return UFLAKE_ERROR;
    }

    lv_group_t *group = g_focus_mgr.focus_groups[ctx->layer];
    if (!group) {
        return UFLAKE_ERROR;
    }

    lv_group_add_obj(group, obj);
    UFLAKE_LOGI(TAG, "Added object %p to input group (layer %d)", obj, ctx->layer);

    return UFLAKE_OK;
}

uflake_result_t ugui_focus_remove_from_group(ugui_focus_ctx_t *ctx, lv_obj_t *obj) {
    if (!g_focus_mgr.initialized || !ctx || !obj) {
        return UFLAKE_ERROR;
    }

    lv_group_remove_obj(obj);
    UFLAKE_LOGI(TAG, "Removed object %p from input group", obj);

    return UFLAKE_OK;
}

uflake_result_t ugui_focus_next(ugui_focus_ctx_t *ctx) {
    if (!g_focus_mgr.initialized || !ctx || !is_valid_layer(ctx->layer)) {
        return UFLAKE_ERROR;
    }

    lv_group_t *group = g_focus_mgr.focus_groups[ctx->layer];
    if (!group) {
        return UFLAKE_ERROR;
    }

    lv_group_focus_next(group);
    return UFLAKE_OK;
}

uflake_result_t ugui_focus_prev(ugui_focus_ctx_t *ctx) {
    if (!g_focus_mgr.initialized || !ctx || !is_valid_layer(ctx->layer)) {
        return UFLAKE_ERROR;
    }

    lv_group_t *group = g_focus_mgr.focus_groups[ctx->layer];
    if (!group) {
        return UFLAKE_ERROR;
    }

    lv_group_focus_prev(group);
    return UFLAKE_OK;
}

// ============================================================================
// LAYER MANAGEMENT
// ============================================================================

ugui_layer_t ugui_focus_get_active_layer(void) {
    return g_focus_mgr.active_layer;
}

lv_obj_t *ugui_focus_get_layer_object(ugui_layer_t layer) {
    if (!is_valid_layer(layer)) {
        return NULL;
    }

    for (int i = 0; i < MAX_FOCUS_CONTEXTS; i++) {
        if (g_focus_mgr.contexts[i].focused_obj != NULL && 
            g_focus_mgr.contexts[i].layer == layer) {
            return g_focus_mgr.contexts[i].focused_obj;
        }
    }

    return NULL;
}

uflake_result_t ugui_focus_block_below(ugui_layer_t layer) {
    // Input blocking is handled by checking active_layer in focus_request
    // This is a no-op, but can be extended for explicit blocking
    UFLAKE_LOGI(TAG, "Blocking input below layer %d", layer);
    return UFLAKE_OK;
}

uflake_result_t ugui_focus_unblock_below(ugui_layer_t layer) {
    UFLAKE_LOGI(TAG, "Unblocking input below layer %d", layer);
    return UFLAKE_OK;
}

// ============================================================================
// SAFE DELETION HELPERS
// ============================================================================

uflake_result_t ugui_focus_safe_delete(ugui_focus_ctx_t *ctx, lv_obj_t *obj) {
    if (!obj) {
        return UFLAKE_ERROR;
    }

    // Unregister from focus if registered
    if (ctx) {
        ugui_focus_unregister(ctx);
    }

    // Now safe to delete
    lv_obj_del(obj);

    UFLAKE_LOGI(TAG, "Safely deleted object %p", obj);

    return UFLAKE_OK;
}

uflake_result_t ugui_focus_safe_delete_children(lv_obj_t *parent) {
    if (!parent) {
        return UFLAKE_ERROR;
    }

    // LVGL's lv_obj_clean handles children deletion safely
    // But we need to unregister any focused children first
    uint32_t child_count = lv_obj_get_child_count(parent);
    
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        ugui_focus_ctx_t *ctx = find_context_by_obj(child);
        if (ctx) {
            ugui_focus_unregister(ctx);
        }
    }

    lv_obj_clean(parent);

    UFLAKE_LOGI(TAG, "Safely deleted children of %p", parent);

    return UFLAKE_OK;
}

// ============================================================================
// DEBUG AND INFO
// ============================================================================

uflake_result_t ugui_focus_get_stats(uint32_t *registered_count, ugui_layer_t *active_layer) {
    if (!g_focus_mgr.initialized) {
        return UFLAKE_ERROR;
    }

    if (registered_count) {
        *registered_count = g_focus_mgr.registered_count;
    }

    if (active_layer) {
        *active_layer = g_focus_mgr.active_layer;
    }

    return UFLAKE_OK;
}

void ugui_focus_debug_print(void) {
    if (!g_focus_mgr.initialized) {
        UFLAKE_LOGE(TAG, "Focus manager not initialized");
        return;
    }

    UFLAKE_LOGI(TAG, "=== Focus Manager Debug ===");
    UFLAKE_LOGI(TAG, "Registered objects: %lu", g_focus_mgr.registered_count);
    UFLAKE_LOGI(TAG, "Active layer: %d", g_focus_mgr.active_layer);

    for (int i = 0; i < MAX_FOCUS_CONTEXTS; i++) {
        if (g_focus_mgr.contexts[i].focused_obj) {
            UFLAKE_LOGI(TAG, "  [%d] obj=%p layer=%d enabled=%d",
                        i,
                        g_focus_mgr.contexts[i].focused_obj,
                        g_focus_mgr.contexts[i].layer,
                        g_focus_mgr.contexts[i].input_enabled);
        }
    }
}
