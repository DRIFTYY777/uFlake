#ifndef IMAGE_CODEC_H
#define IMAGE_CODEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* ============================================================================
     *  IMAGE FORMATS
     * ========================================================================== */

    typedef enum
    {
        IMG_FMT_AUTO = 0,
        IMG_FMT_JPEG,
        IMG_FMT_PNG,
    } img_format_t;

    /* ============================================================================
     *  IMAGE OUTPUT (RGB565)
     * ========================================================================== */

    typedef struct
    {
        uint16_t width;
        uint16_t height;
        uint16_t stride; // bytes per line (usually width * 2)
        uint8_t *pixels; // RGB565 buffer
        size_t size;     // total buffer size in bytes
    } img_rgb565_t;

    /* ============================================================================
     *  FILE READER ABSTRACTION (for decoding)
     * ========================================================================== */

    typedef struct
    {
        void *user_ctx;

        bool (*open)(void *user_ctx, const char *path);
        size_t (*read)(void *user_ctx, void *dst, size_t len);
        bool (*seek)(void *user_ctx, size_t offset);
        size_t (*size)(void *user_ctx);
        void (*close)(void *user_ctx);
    } img_reader_t;

    /* ============================================================================
     *  FILE WRITER ABSTRACTION (for encoding)
     * ========================================================================== */

    typedef struct
    {
        void *user_ctx;

        bool (*open)(void *user_ctx, const char *path);
        size_t (*write)(void *user_ctx, const void *src, size_t len);
        bool (*flush)(void *user_ctx);
        void (*close)(void *user_ctx);
    } img_writer_t;

    /* ============================================================================
     *  DECODE OPTIONS
     * ========================================================================== */

    typedef enum
    {
        IMG_ROTATE_0 = 0,
        IMG_ROTATE_90 = 90,
        IMG_ROTATE_180 = 180,
        IMG_ROTATE_270 = 270,
    } img_rotate_t;

    typedef enum
    {
        IMG_SCALE_NONE = 0, // No scaling
        IMG_SCALE_1_2 = 1,  // 1/2 scale (HW accelerated for JPEG)
        IMG_SCALE_1_4 = 2,  // 1/4 scale (HW accelerated for JPEG)
        IMG_SCALE_1_8 = 3,  // 1/8 scale (HW accelerated for JPEG)
    } img_scale_t;

    typedef struct
    {
        bool resize; // Enable custom resize (overrides scale)
        uint16_t new_width;
        uint16_t new_height;
        img_rotate_t rotate; // Rotation
        img_scale_t scale;   // HW scaling (JPEG only)
    } img_decode_opts_t;

    /* ============================================================================
     *  ENCODE OPTIONS
     * ========================================================================== */

    typedef struct
    {
        uint8_t quality; // JPEG quality (1-100, default 85)
        bool use_psram;  // Allocate buffers in PSRAM
    } img_encode_opts_t;

    /* ============================================================================
     *  DECODER API
     * ========================================================================== */

    /**
     * Decode an image into RGB565 with options
     *
     * @param path     Image path (passed to reader)
     * @param fmt      Image format or AUTO
     * @param reader   Reader callbacks (SD / Flash / etc)
     * @param opts     Decode options (resize, rotate, scale) - can be NULL
     * @param out      Output RGB565 image
     *
     * @return true on success
     */
    bool img_decode_rgb565_ex(const char *path,
                              img_format_t fmt,
                              const img_reader_t *reader,
                              const img_decode_opts_t *opts,
                              img_rgb565_t *out);

    /**
     * Decode an image into RGB565 (simple version)
     */
    bool img_decode_rgb565(const char *path,
                           img_format_t fmt,
                           const img_reader_t *reader,
                           img_rgb565_t *out);

    /* ============================================================================
     *  ENCODER API
     * ========================================================================== */

    /**
     * Encode RGB565 image to JPEG with options
     *
     * @param img      Input RGB565 image
     * @param path     Output path
     * @param writer   Writer callbacks (SD / Flash / etc)
     * @param opts     Encode options (quality, etc) - can be NULL
     *
     * @return true on success
     */
    bool img_encode_jpeg_ex(const img_rgb565_t *img,
                            const char *path,
                            const img_writer_t *writer,
                            const img_encode_opts_t *opts);

    /**
     * Encode RGB565 image to JPEG (simple version, quality=85)
     */
    bool img_encode_jpeg(const img_rgb565_t *img,
                         const char *path,
                         const img_writer_t *writer);

    /* ============================================================================
     *  SCREENSHOT API (LVGL Integration)
     * ========================================================================== */

    /**
     * Capture LVGL screen to RGB565 buffer
     *
     * Note: Requires LVGL to be initialized
     * Buffer is allocated internally - must be freed with img_free()
     *
     * @param out      Output RGB565 image
     *
     * @return true on success
     */
    bool img_screenshot_lvgl(img_rgb565_t *out);

    /**
     * Capture LVGL screen and save directly to JPEG
     *
     * @param path     Output path
     * @param writer   Writer callbacks
     * @param quality  JPEG quality (1-100)
     *
     * @return true on success
     */
    bool img_screenshot_lvgl_to_jpeg(const char *path,
                                     const img_writer_t *writer,
                                     uint8_t quality);

    /* ============================================================================
     *  MEMORY MANAGEMENT
     * ========================================================================== */

    /**
     * Free buffer allocated by decoder/screenshot
     */
    void img_free(img_rgb565_t *img);

    /**
     * Get image info without full decode (JPEG only)
     *
     * @param path     Image path
     * @param reader   Reader callbacks
     * @param width    Output width
     * @param height   Output height
     *
     * @return true on success
     */
    bool img_get_info(const char *path,
                      const img_reader_t *reader,
                      uint16_t *width,
                      uint16_t *height);

#ifdef __cplusplus
}
#endif

#endif // IMAGE_CODEC_H