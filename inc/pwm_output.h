/**
 * @file pwm_output.h
 * @brief PWM output control via STM32 hardware timers
 * @details Hardware-based PWM generation for 9 RC channels
 */

#ifndef __PWM_OUTPUT_H
#define __PWM_OUTPUT_H

#include <stdint.h>

/* ============================================================================
 * PWM Configuration
 * ========================================================================== */

#define PWM_CHANNEL_COUNT               9
#define PWM_FREQUENCY_HZ                50      ///< 50Hz for RC servos/ESCs
#define PWM_MIN_VALUE_US                1000    ///< Minimum pulse width
#define PWM_MAX_VALUE_US                2000    ///< Maximum pulse width
#define PWM_CENTER_VALUE_US             1500    ///< Center pulse width

/* ============================================================================
 * Timer and Channel Configuration
 * ========================================================================== */

/**
 * PWM Output Mapping (STM32F407):
 * 
 * Timer2 @ 84MHz:
 *   CH1 - PA0  (PWM Channel 1)
 *   CH2 - PA1  (PWM Channel 2)
 *   CH3 - PA2  (PWM Channel 3)
 * 
 * Timer3 @ 84MHz:
 *   CH1 - PA6  (PWM Channel 4)
 *   CH2 - PA7  (PWM Channel 5)
 *   CH3 - PB0  (PWM Channel 6)
 * 
 * Timer4 @ 84MHz:
 *   CH1 - PB6  (PWM Channel 7)
 *   CH2 - PB7  (PWM Channel 8)
 *   CH3 - PB8  (PWM Channel 9)
 * 
 * Prescaler calculation for 50Hz:
 * PSC = (84MHz / (50Hz * 65536)) - 1 = 25 (16-bit ARR)
 * PSC = (84MHz / (50Hz * 1000)) - 1 = 1679 (for 1000 ticks per 20ms)
 */

/**
 * @enum pwm_channel
 * @brief PWM channel enumeration
 */
typedef enum {
    PWM_CH1 = 0,
    PWM_CH2 = 1,
    PWM_CH3 = 2,
    PWM_CH4 = 3,
    PWM_CH5 = 4,
    PWM_CH6 = 5,
    PWM_CH7 = 6,
    PWM_CH8 = 7,
    PWM_CH9 = 8
} pwm_channel_t;

/* ============================================================================
 * PWM Output Functions
 * ========================================================================== */

/**
 * @brief Initialize PWM output system
 * @details Configures all timers and GPIO pins for PWM output
 * 
 * Configuration:
 * - Frequency: 50Hz (20ms period)
 * - Resolution: 1µs (1-2000 ticks for 1000-2000µs)
 * - Duty cycle range: 1000-2000µs
 */
void pwm_output_init(void);

/**
 * @brief Set PWM pulse width for a single channel
 * @param channel PWM channel (0-8)
 * @param pulse_width Pulse width in microseconds (1000-2000)
 * 
 * @note Automatically clamps values to valid range
 * @note Changes take effect immediately
 */
void pwm_set_pulse(pwm_channel_t channel, uint16_t pulse_width);

/**
 * @brief Set PWM pulse width for all channels at once
 * @param pulse_widths Pointer to array of 9 pulse widths (microseconds)
 * 
 * @details More efficient than setting channels individually
 * @note All 9 channels updated atomically
 */
void pwm_set_all_pulses(const uint16_t *pulse_widths);

/**
 * @brief Get current PWM pulse width for a channel
 * @param channel PWM channel (0-8)
 * @return Current pulse width in microseconds
 */
uint16_t pwm_get_pulse(pwm_channel_t channel);

/**
 * @brief Enable PWM output on all channels
 */
void pwm_output_enable(void);

/**
 * @brief Disable PWM output on all channels
 */
void pwm_output_disable(void);

/**
 * @brief Set all channels to center position (1500µs)
 */
void pwm_center_all(void);

/**
 * @brief Set all channels to failsafe position (1000µs)
 */
void pwm_failsafe_all(void);

/**
 * @brief Check if PWM output is enabled
 * @return 1 if enabled, 0 if disabled
 */
uint8_t pwm_is_enabled(void);

/* ============================================================================
 * Hardware Timer Interface (Internal)
 * ========================================================================== */

/**
 * @brief Configure Timer2 for PWM output (internal use)
 */
void pwm_timer2_init(void);

/**
 * @brief Configure Timer3 for PWM output (internal use)
 */
void pwm_timer3_init(void);

/**
 * @brief Configure Timer4 for PWM output (internal use)
 */
void pwm_timer4_init(void);

/**
 * @brief Set timer channel compare value (internal use)
 * @param timer_num Timer number (2, 3, or 4)
 * @param channel Timer channel (1, 2, or 3)
 * @param compare Compare value in timer ticks
 */
void pwm_timer_set_compare(uint8_t timer_num, uint8_t channel, uint32_t compare);

/**
 * @brief Get timer channel compare value (internal use)
 * @param timer_num Timer number (2, 3, or 4)
 * @param channel Timer channel (1, 2, or 3)
 * @return Compare value in timer ticks
 */
uint32_t pwm_timer_get_compare(uint8_t timer_num, uint8_t channel);

#endif /* __PWM_OUTPUT_H */
