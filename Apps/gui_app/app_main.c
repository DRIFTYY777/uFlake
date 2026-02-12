#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "appLoader.h"

#include "lvgl.h"

void gui_test_main(void);

LV_IMAGE_DECLARE(ui_img_118050);

static const app_manifest_t gui_test_app_manifest = {
    .name = "GUI APP Test",
    .version = "1.0.0",
    .author = "uFlake Team",
    .description = "A test app demonstrating GUI features.",
    .icon = "input.png",
    .type = APP_TYPE_INTERNAL,
    .stack_size = 4096,
    .priority = 5,
    .requires_sdcard = false,
    .requires_network = false};

const app_bundle_t gui_test_app = {
    .manifest = &gui_test_app_manifest,
    .entry_point = gui_test_main,
    .is_launcher = false};

void gui_test_main(void)
{
    /* ================= BACKGROUND ================= */
    lv_obj_t *bg = lv_obj_create(lv_screen_active());
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
    lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

    /* ================= IMAGE BEHIND GLASS ================= */
    lv_obj_t *img = lv_image_create(bg);
    lv_image_set_src(img, &ui_img_118050);
    lv_obj_center(img);

    /* ================= BLUR DIFFUSION LAYERS ================= */
    /* Outer diffusion */
    lv_obj_t *blur_outer = lv_obj_create(bg);
    lv_obj_set_size(blur_outer, 240, 160);
    lv_obj_center(blur_outer);
    lv_obj_set_style_radius(blur_outer, 26, 0);
    lv_obj_set_style_bg_color(blur_outer, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(blur_outer, LV_OPA_10, 0);
    lv_obj_set_style_border_width(blur_outer, 0, 0);
    lv_obj_remove_flag(blur_outer, LV_OBJ_FLAG_SCROLLABLE);

    /* Middle diffusion */
    lv_obj_t *blur_mid = lv_obj_create(bg);
    lv_obj_set_size(blur_mid, 230, 150);
    lv_obj_center(blur_mid);
    lv_obj_set_style_radius(blur_mid, 24, 0);
    lv_obj_set_style_bg_color(blur_mid, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(blur_mid, (lv_opa_t)((LV_OPA_COVER * 25) / 100), 0);
    lv_obj_set_style_border_width(blur_mid, 0, 0);
    lv_obj_remove_flag(blur_mid, LV_OBJ_FLAG_SCROLLABLE);

    /* ================= MAIN GLASS PANEL ================= */
    lv_obj_t *glass = lv_obj_create(bg);
    lv_obj_set_size(glass, 220, 140);
    lv_obj_center(glass);
    lv_obj_remove_flag(glass, LV_OBJ_FLAG_SCROLLABLE);

    /* Glass body */
    lv_obj_set_style_radius(glass, 20, 0);
    lv_obj_set_style_bg_color(glass, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(glass, (lv_opa_t)((LV_OPA_COVER * 25) / 100), 0);

    /* Gradient = fake blur */
    lv_obj_set_style_bg_grad_color(glass, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_grad_dir(glass, LV_GRAD_DIR_VER, 0);

    /* Frosted border */
    lv_obj_set_style_border_width(glass, 1, 0);
    lv_obj_set_style_border_color(glass, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(glass, LV_OPA_40, 0);

    /* Depth shadow */
    lv_obj_set_style_shadow_width(glass, 28, 0);
    lv_obj_set_style_shadow_color(glass, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(glass, LV_OPA_30, 0);
    lv_obj_set_style_shadow_offset_y(glass, 6, 0);

    /* ================= CONTENT ================= */
    lv_obj_t *label = lv_label_create(glass);
    lv_label_set_text(label, "LVGL 9 Glass");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
}
