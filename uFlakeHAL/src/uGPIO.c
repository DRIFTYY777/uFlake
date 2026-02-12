#include "uGPIO.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "uGPIO";

// ========== ISR Management ==========

#define MAX_GPIO_PINS 49 // ESP32-S3 has up to 49 GPIOs (0-48)

typedef struct
{
    ugpio_isr_callback_t callback;
    void *user_data;
    bool active;
} gpio_isr_context_t;

static gpio_isr_context_t gpio_isr_contexts[MAX_GPIO_PINS] = {0};
static bool isr_service_installed = false;

// ========== PWM Management ==========

#define MAX_PWM_CHANNELS 8
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT
#define LEDC_MAX_DUTY ((1 << LEDC_DUTY_RES) - 1)

typedef struct
{
    gpio_num_t pin;
    ledc_channel_t channel;
    uint32_t frequency;
    bool in_use;
} pwm_channel_info_t;

static pwm_channel_info_t pwm_channels[MAX_PWM_CHANNELS] = {0};
static bool ledc_timer_configured = false;

// ========== Internal GPIO ISR Handler ==========

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    gpio_num_t pin = (gpio_num_t)(int)arg;

    if (pin >= 0 && pin < MAX_GPIO_PINS)
    {
        gpio_isr_context_t *ctx = &gpio_isr_contexts[pin];
        if (ctx->active && ctx->callback)
        {
            ctx->callback(pin, ctx->user_data);
        }
    }
}

// ========== Internal Helper Functions ==========

static esp_err_t ensure_isr_service_installed(void)
{
    if (!isr_service_installed)
    {
        esp_err_t ret = gpio_install_isr_service(0);
        if (ret == ESP_OK)
        {
            isr_service_installed = true;
        }
        else if (ret == ESP_ERR_INVALID_STATE)
        {
            // ISR service already installed
            isr_service_installed = true;
            ret = ESP_OK;
        }
        return ret;
    }
    return ESP_OK;
}

static gpio_int_type_t convert_intr_type(ugpio_intr_type_t intr_type)
{
    switch (intr_type)
    {
    case UGPIO_INTR_POSEDGE:
        return GPIO_INTR_POSEDGE;
    case UGPIO_INTR_NEGEDGE:
        return GPIO_INTR_NEGEDGE;
    case UGPIO_INTR_ANYEDGE:
        return GPIO_INTR_ANYEDGE;
    case UGPIO_INTR_LOW_LEVEL:
        return GPIO_INTR_LOW_LEVEL;
    case UGPIO_INTR_HIGH_LEVEL:
        return GPIO_INTR_HIGH_LEVEL;
    case UGPIO_INTR_DISABLE:
    default:
        return GPIO_INTR_DISABLE;
    }
}

static gpio_drive_cap_t convert_drive_strength(ugpio_drive_strength_t strength)
{
    switch (strength)
    {
    case UGPIO_DRIVE_WEAK:
        return GPIO_DRIVE_CAP_0;
    case UGPIO_DRIVE_STRONGER:
        return GPIO_DRIVE_CAP_1;
    case UGPIO_DRIVE_DEFAULT:
        return GPIO_DRIVE_CAP_2;
    case UGPIO_DRIVE_STRONGEST:
        return GPIO_DRIVE_CAP_3;
    default:
        return GPIO_DRIVE_CAP_2;
    }
}

static ledc_channel_t find_free_pwm_channel(void)
{
    for (int i = 0; i < MAX_PWM_CHANNELS; i++)
    {
        if (!pwm_channels[i].in_use)
        {
            return (ledc_channel_t)i;
        }
    }
    return -1;
}

static int find_pwm_channel_by_pin(gpio_num_t pin)
{
    for (int i = 0; i < MAX_PWM_CHANNELS; i++)
    {
        if (pwm_channels[i].in_use && pwm_channels[i].pin == pin)
        {
            return i;
        }
    }
    return -1;
}

// ========== Basic GPIO Functions ==========

esp_err_t ugpio_init(gpio_num_t pin, gpio_mode_t mode, gpio_pull_mode_t pull_mode)
{
    if (!ugpio_is_valid(pin))
    {
        ESP_LOGE(TAG, "Invalid GPIO pin: %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = mode,
        .pull_up_en = (pull_mode == GPIO_PULLUP_ONLY || pull_mode == GPIO_PULLUP_PULLDOWN) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (pull_mode == GPIO_PULLDOWN_ONLY || pull_mode == GPIO_PULLUP_PULLDOWN) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", pin, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "GPIO %d initialized (mode=%d, pull=%d)", pin, mode, pull_mode);
    return ESP_OK;
}

esp_err_t ugpio_deinit(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Stop PWM if active
    int pwm_idx = find_pwm_channel_by_pin(pin);
    if (pwm_idx >= 0)
    {
        ugpio_pwm_stop(pin);
    }

    // Detach interrupt if active
    if (pin < MAX_GPIO_PINS && gpio_isr_contexts[pin].active)
    {
        ugpio_detach_interrupt(pin);
    }

    return gpio_reset_pin(pin);
}

esp_err_t ugpio_set_level(gpio_num_t pin, uint32_t level)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_set_level(pin, level);
}

int ugpio_get_level(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return -1;
    }

    return gpio_get_level(pin);
}

esp_err_t ugpio_toggle(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    int current_level = gpio_get_level(pin);
    if (current_level < 0)
    {
        return ESP_FAIL;
    }

    return gpio_set_level(pin, !current_level);
}

esp_err_t ugpio_set_direction(gpio_num_t pin, gpio_mode_t mode)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_set_direction(pin, mode);
}

// ========== Pull Resistor Control ==========

esp_err_t ugpio_set_pull_mode(gpio_num_t pin, gpio_pull_mode_t pull_mode)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;

    switch (pull_mode)
    {
    case GPIO_PULLUP_ONLY:
        ret = gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
        break;
    case GPIO_PULLDOWN_ONLY:
        ret = gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
        break;
    case GPIO_PULLUP_PULLDOWN:
        ret = gpio_set_pull_mode(pin, GPIO_PULLUP_PULLDOWN);
        break;
    case GPIO_FLOATING:
        ret = gpio_set_pull_mode(pin, GPIO_FLOATING);
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t ugpio_enable_pullup(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_pullup_en(pin);
}

esp_err_t ugpio_enable_pulldown(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_pulldown_en(pin);
}

esp_err_t ugpio_disable_pull(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = gpio_pullup_dis(pin);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return gpio_pulldown_dis(pin);
}

// ========== Interrupt Handling ==========

esp_err_t ugpio_attach_interrupt(gpio_num_t pin, ugpio_intr_type_t intr_type,
                                 ugpio_isr_callback_t callback, void *user_data)
{
    if (!ugpio_is_valid(pin) || pin >= MAX_GPIO_PINS)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (callback == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure ISR service is installed
    esp_err_t ret = ensure_isr_service_installed();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        return ret;
    }

    // Remove old handler if exists
    if (gpio_isr_contexts[pin].active)
    {
        gpio_isr_handler_remove(pin);
    }

    // Set interrupt type
    gpio_int_type_t esp_intr_type = convert_intr_type(intr_type);
    ret = gpio_set_intr_type(pin, esp_intr_type);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set interrupt type for GPIO %d: %s", pin, esp_err_to_name(ret));
        return ret;
    }

    // Store callback context
    gpio_isr_contexts[pin].callback = callback;
    gpio_isr_contexts[pin].user_data = user_data;
    gpio_isr_contexts[pin].active = true;

    // Add ISR handler
    ret = gpio_isr_handler_add(pin, gpio_isr_handler, (void *)(int)pin);
    if (ret != ESP_OK)
    {
        gpio_isr_contexts[pin].active = false;
        ESP_LOGE(TAG, "Failed to add ISR handler for GPIO %d: %s", pin, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Interrupt attached to GPIO %d (type=%d)", pin, intr_type);
    return ESP_OK;
}

esp_err_t ugpio_detach_interrupt(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin) || pin >= MAX_GPIO_PINS)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!gpio_isr_contexts[pin].active)
    {
        return ESP_OK; // Already detached
    }

    esp_err_t ret = gpio_isr_handler_remove(pin);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to remove ISR handler for GPIO %d: %s", pin, esp_err_to_name(ret));
        return ret;
    }

    // Clear context
    gpio_isr_contexts[pin].callback = NULL;
    gpio_isr_contexts[pin].user_data = NULL;
    gpio_isr_contexts[pin].active = false;

    // Disable interrupt
    gpio_set_intr_type(pin, GPIO_INTR_DISABLE);

    ESP_LOGD(TAG, "Interrupt detached from GPIO %d", pin);
    return ESP_OK;
}

esp_err_t ugpio_enable_interrupt(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_intr_enable(pin);
}

esp_err_t ugpio_disable_interrupt(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_intr_disable(pin);
}

// ========== PWM Functions ==========

esp_err_t ugpio_pwm_start(gpio_num_t pin, uint32_t frequency, float duty_cycle)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (duty_cycle < 0.0f || duty_cycle > 100.0f)
    {
        ESP_LOGE(TAG, "Invalid duty cycle: %.2f (must be 0-100)", duty_cycle);
        return ESP_ERR_INVALID_ARG;
    }

    // Check if pin already has PWM
    int existing_idx = find_pwm_channel_by_pin(pin);
    if (existing_idx >= 0)
    {
        ESP_LOGW(TAG, "PWM already active on GPIO %d, reconfiguring", pin);
        ugpio_pwm_stop(pin);
    }

    // Find free channel
    ledc_channel_t channel = find_free_pwm_channel();
    if (channel < 0)
    {
        ESP_LOGE(TAG, "No free PWM channels available");
        return ESP_ERR_NO_MEM;
    }

    // Configure timer (only once)
    if (!ledc_timer_configured)
    {
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_MODE,
            .timer_num = LEDC_TIMER,
            .duty_resolution = LEDC_DUTY_RES,
            .freq_hz = frequency,
            .clk_cfg = LEDC_AUTO_CLK};

        esp_err_t ret = ledc_timer_config(&ledc_timer);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
            return ret;
        }
        ledc_timer_configured = true;
    }
    else
    {
        // Update frequency if needed
        ledc_set_freq(LEDC_MODE, LEDC_TIMER, frequency);
    }

    // Configure channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = channel,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = pin,
        .duty = (uint32_t)(duty_cycle / 100.0f * LEDC_MAX_DUTY),
        .hpoint = 0};

    esp_err_t ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Mark channel as in use
    pwm_channels[channel].pin = pin;
    pwm_channels[channel].channel = channel;
    pwm_channels[channel].frequency = frequency;
    pwm_channels[channel].in_use = true;

    ESP_LOGD(TAG, "PWM started on GPIO %d (freq=%lu Hz, duty=%.2f%%)", pin, frequency, duty_cycle);
    return ESP_OK;
}

esp_err_t ugpio_pwm_set_duty(gpio_num_t pin, float duty_cycle)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (duty_cycle < 0.0f || duty_cycle > 100.0f)
    {
        ESP_LOGE(TAG, "Invalid duty cycle: %.2f (must be 0-100)", duty_cycle);
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_pwm_channel_by_pin(pin);
    if (idx < 0)
    {
        ESP_LOGE(TAG, "PWM not started on GPIO %d", pin);
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t duty = (uint32_t)(duty_cycle / 100.0f * LEDC_MAX_DUTY);
    esp_err_t ret = ledc_set_duty(LEDC_MODE, pwm_channels[idx].channel, duty);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ledc_update_duty(LEDC_MODE, pwm_channels[idx].channel);
}

esp_err_t ugpio_pwm_set_frequency(gpio_num_t pin, uint32_t frequency)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_pwm_channel_by_pin(pin);
    if (idx < 0)
    {
        ESP_LOGE(TAG, "PWM not started on GPIO %d", pin);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ledc_set_freq(LEDC_MODE, LEDC_TIMER, frequency);
    if (ret == ESP_OK)
    {
        pwm_channels[idx].frequency = frequency;
    }

    return ret;
}

esp_err_t ugpio_pwm_stop(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_pwm_channel_by_pin(pin);
    if (idx < 0)
    {
        return ESP_OK; // Already stopped
    }

    // Stop PWM output
    esp_err_t ret = ledc_stop(LEDC_MODE, pwm_channels[idx].channel, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop PWM on GPIO %d: %s", pin, esp_err_to_name(ret));
        return ret;
    }

    // Mark channel as free
    pwm_channels[idx].in_use = false;
    pwm_channels[idx].pin = GPIO_NUM_NC;

    // Reset pin to normal GPIO
    gpio_reset_pin(pin);

    ESP_LOGD(TAG, "PWM stopped on GPIO %d", pin);
    return ESP_OK;
}

// ========== Advanced Configuration ==========

esp_err_t ugpio_set_drive_strength(gpio_num_t pin, ugpio_drive_strength_t strength)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_drive_cap_t drive_cap = convert_drive_strength(strength);
    return gpio_set_drive_capability(pin, drive_cap);
}

esp_err_t ugpio_set_open_drain(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
}

esp_err_t ugpio_clear_open_drain(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_set_direction(pin, GPIO_MODE_OUTPUT);
}

esp_err_t ugpio_hold_enable(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_hold_en(pin);
}

esp_err_t ugpio_hold_disable(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_hold_dis(pin);
}

// ========== Utility Functions ==========

bool ugpio_is_valid(gpio_num_t pin)
{
    return GPIO_IS_VALID_GPIO(pin);
}

int ugpio_read_input(gpio_num_t pin)
{
    if (!ugpio_is_valid(pin))
    {
        return -1;
    }

    return gpio_get_level(pin);
}
