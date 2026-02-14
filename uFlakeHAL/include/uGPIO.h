#if !defined(UGPIO_H_)
#define UGPIO_H_

#include "kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_err.h"
#include "driver/gpio.h"

    /**
     * @brief GPIO interrupt callback function type
     *
     * @param pin GPIO pin number that triggered the interrupt
     * @param user_data User data passed during interrupt attachment
     */
    typedef void (*ugpio_isr_callback_t)(gpio_num_t pin, void *user_data);

    /**
     * @brief GPIO interrupt trigger types
     */
    typedef enum
    {
        UGPIO_INTR_DISABLE = 0, /**< Disable interrupt */
        UGPIO_INTR_POSEDGE,     /**< Positive edge */
        UGPIO_INTR_NEGEDGE,     /**< Negative edge */
        UGPIO_INTR_ANYEDGE,     /**< Both edges */
        UGPIO_INTR_LOW_LEVEL,   /**< Low level */
        UGPIO_INTR_HIGH_LEVEL   /**< High level */
    } ugpio_intr_type_t;

    /**
     * @brief GPIO drive strength levels
     */
    typedef enum
    {
        UGPIO_DRIVE_WEAK = 0, /**< ~5mA */
        UGPIO_DRIVE_STRONGER, /**< ~10mA */
        UGPIO_DRIVE_DEFAULT,  /**< ~20mA (default) */
        UGPIO_DRIVE_STRONGEST /**< ~40mA */
    } ugpio_drive_strength_t;

    // ========== Basic GPIO Functions ==========

    /**
     * @brief Initialize a GPIO pin
     *
     * @param pin GPIO pin number
     * @param mode GPIO mode (INPUT, OUTPUT, etc.)
     * @param pull_mode Pull mode (PULLUP, PULLDOWN, FLOATING)
     * @return uflake_result_t Result of operation
     */
    esp_err_t ugpio_init(gpio_num_t pin, gpio_mode_t mode, gpio_pull_mode_t pull_mode);

    /**
     * @brief Deinitialize a GPIO pin (reset to default state)
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_deinit(gpio_num_t pin);

    /**
     * @brief Set GPIO output level
     *
     * @param pin GPIO pin number
     * @param level Output level (0 or 1)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_set_level(gpio_num_t pin, uint32_t level);

    /**
     * @brief Get GPIO input level
     *
     * @param pin GPIO pin number
     * @return int Pin level (0 or 1), or -1 on error
     */
    int ugpio_get_level(gpio_num_t pin);

    /**
     * @brief Toggle GPIO output level
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_toggle(gpio_num_t pin);

    /**
     * @brief Set GPIO direction dynamically
     *
     * @param pin GPIO pin number
     * @param mode GPIO mode (INPUT, OUTPUT, etc.)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_set_direction(gpio_num_t pin, gpio_mode_t mode);

    // ========== Pull Resistor Control ==========

    /**
     * @brief Set pull mode for GPIO
     *
     * @param pin GPIO pin number
     * @param pull_mode Pull mode (PULLUP, PULLDOWN, FLOATING)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_set_pull_mode(gpio_num_t pin, gpio_pull_mode_t pull_mode);

    /**
     * @brief Enable internal pull-up resistor
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_enable_pullup(gpio_num_t pin);

    /**
     * @brief Enable internal pull-down resistor
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_enable_pulldown(gpio_num_t pin);

    /**
     * @brief Disable all pull resistors
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_disable_pull(gpio_num_t pin);

    // ========== Interrupt Handling ==========

    /**
     * @brief Attach an interrupt handler to a GPIO pin
     *
     * @param pin GPIO pin number
     * @param intr_type Interrupt trigger type
     * @param callback Callback function to be called on interrupt
     * @param user_data User data to be passed to callback
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_attach_interrupt(gpio_num_t pin, ugpio_intr_type_t intr_type,
                                     ugpio_isr_callback_t callback, void *user_data);

    /**
     * @brief Detach interrupt handler from a GPIO pin
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_detach_interrupt(gpio_num_t pin);

    /**
     * @brief Enable interrupt for a GPIO pin
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_enable_interrupt(gpio_num_t pin);

    /**
     * @brief Disable interrupt for a GPIO pin
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_disable_interrupt(gpio_num_t pin);

    // ========== PWM Functions ==========

    /**
     * @brief Start PWM output on a GPIO pin
     *
     * @param pin GPIO pin number
     * @param frequency PWM frequency in Hz
     * @param duty_cycle Duty cycle (0-100%)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_pwm_start(gpio_num_t pin, uint32_t frequency, float duty_cycle);

    /**
     * @brief Set PWM duty cycle
     *
     * @param pin GPIO pin number
     * @param duty_cycle Duty cycle (0-100%)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_pwm_set_duty(gpio_num_t pin, float duty_cycle);

    /**
     * @brief Set PWM frequency
     *
     * @param pin GPIO pin number
     * @param frequency PWM frequency in Hz
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_pwm_set_frequency(gpio_num_t pin, uint32_t frequency);

    /**
     * @brief Stop PWM output on a GPIO pin
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_pwm_stop(gpio_num_t pin);

    // ========== Advanced Configuration ==========

    /**
     * @brief Set GPIO drive strength
     *
     * @param pin GPIO pin number
     * @param strength Drive strength level
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_set_drive_strength(gpio_num_t pin, ugpio_drive_strength_t strength);

    /**
     * @brief Enable open-drain mode for GPIO
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_set_open_drain(gpio_num_t pin);

    /**
     * @brief Disable open-drain mode for GPIO
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_clear_open_drain(gpio_num_t pin);

    /**
     * @brief Enable GPIO pad hold function (maintain state in deep sleep)
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_hold_enable(gpio_num_t pin);

    /**
     * @brief Disable GPIO pad hold function
     *
     * @param pin GPIO pin number
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t ugpio_hold_disable(gpio_num_t pin);

    // ========== Utility Functions ==========

    /**
     * @brief Validate if GPIO pin number is valid
     *
     * @param pin GPIO pin number
     * @return true if valid, false otherwise
     */
    bool ugpio_is_valid(gpio_num_t pin);

    /**
     * @brief Read the actual pin state (works for both input and output pins)
     *
     * @param pin GPIO pin number
     * @return int Pin level (0 or 1), or -1 on error
     */
    int ugpio_read_input(gpio_num_t pin);

#ifdef __cplusplus
}
#endif

#endif // UGPIO_H_
