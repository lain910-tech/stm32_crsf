/**
 * @file pwm_output.c
 * @brief PWM output implementation for STM32F407
 */

#include "pwm_output.h"
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_tim.h"

/* ============================================================================
 * PWM Configuration Constants
 * ========================================================================== */

// For 50Hz with 84MHz timer clock:
// ARR = 84,000,000 / (50Hz * 1680) - 1 = 999 (approx)
// This gives 1000 ticks per 20ms period
// 1 tick = 20µs / 1000 = 20µs
// To get 1µs resolution, use different PSC and ARR

// Better: PSC=84-1, ARR=20000-1 gives:
// Timer frequency = 84MHz / 84 = 1MHz
// Period = 20000µs = 20ms (50Hz)
// Resolution = 1µs

#define PWM_TIMER_CLOCK_MHZ             84
#define PWM_PRESCALER                   (PWM_TIMER_CLOCK_MHZ - 1)  // 83 for 1MHz timer
#define PWM_PERIOD_TICKS                20000   // 20000µs = 20ms
#define PWM_MIN_TICKS                   1000    // 1000µs
#define PWM_MAX_TICKS                   2000    // 2000µs
#define PWM_CENTER_TICKS                1500    // 1500µs

/* ============================================================================
 * PWM Output State
 * ========================================================================== */

static struct {
    uint16_t channel_values[PWM_CHANNEL_COUNT];  // Current PWM values in µs
    uint8_t enabled;
} pwm_state = {
    .enabled = 0
};

/* ============================================================================
 * Helper Functions
 * ========================================================================== */

static uint32_t pwm_us_to_ticks(uint16_t us)
{
    if (us > PWM_MAX_TICKS) us = PWM_MAX_TICKS;
    if (us < PWM_MIN_TICKS) us = PWM_MIN_TICKS;
    return us;  // Already in ticks (1µs resolution)
}

static uint16_t pwm_ticks_to_us(uint32_t ticks)
{
    if (ticks > PWM_MAX_TICKS) ticks = PWM_MAX_TICKS;
    if (ticks < PWM_MIN_TICKS) ticks = PWM_MIN_TICKS;
    return (uint16_t)ticks;
}

/* ============================================================================
 * Timer Initialization Functions
 * ========================================================================== */

void pwm_timer2_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    
    // Enable GPIOA and Timer2 clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    
    // Configure PA0, PA1, PA2 as alternate function
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // Connect to Timer2
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_TIM2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_TIM2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_TIM2);
    
    // Timer base configuration
    TIM_TimeBaseInitStructure.TIM_Period = PWM_PERIOD_TICKS - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler = PWM_PRESCALER;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
    
    // PWM mode configuration
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = PWM_CENTER_TICKS;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_OC3Init(TIM2, &TIM_OCInitStructure);
    
    // Enable preload
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);
    
    // Enable Timer2
    TIM_Cmd(TIM2, ENABLE);
}

void pwm_timer3_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    
    // Enable GPIOA, GPIOB and Timer3 clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    
    // Configure PA6, PA7 as alternate function
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // Configure PB0 as alternate function
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // Connect to Timer3
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_TIM3);
    
    // Timer base configuration
    TIM_TimeBaseInitStructure.TIM_Period = PWM_PERIOD_TICKS - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler = PWM_PRESCALER;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
    
    // PWM mode configuration
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = PWM_CENTER_TICKS;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    TIM_OC2Init(TIM3, &TIM_OCInitStructure);
    TIM_OC3Init(TIM3, &TIM_OCInitStructure);
    
    // Enable preload
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    
    // Enable Timer3
    TIM_Cmd(TIM3, ENABLE);
}

void pwm_timer4_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    
    // Enable GPIOB and Timer4 clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    
    // Configure PB6, PB7, PB8 as alternate function
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // Connect to Timer4
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_TIM4);
    
    // Timer base configuration
    TIM_TimeBaseInitStructure.TIM_Period = PWM_PERIOD_TICKS - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler = PWM_PRESCALER;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);
    
    // PWM mode configuration
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = PWM_CENTER_TICKS;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    TIM_OC1Init(TIM4, &TIM_OCInitStructure);
    TIM_OC2Init(TIM4, &TIM_OCInitStructure);
    TIM_OC3Init(TIM4, &TIM_OCInitStructure);
    
    // Enable preload
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);
    
    // Enable Timer4
    TIM_Cmd(TIM4, ENABLE);
}

/* ============================================================================
 * Public PWM Functions
 * ========================================================================== */

void pwm_output_init(void)
{
    // Initialize all channel values to center
    for (int i = 0; i < PWM_CHANNEL_COUNT; i++) {
        pwm_state.channel_values[i] = PWM_CENTER_VALUE_US;
    }
    
    // Initialize timers
    pwm_timer2_init();
    pwm_timer3_init();
    pwm_timer4_init();
    
    pwm_state.enabled = 1;
}

void pwm_set_pulse(pwm_channel_t channel, uint16_t pulse_width)
{
    if (channel >= PWM_CHANNEL_COUNT) return;
    
    // Clamp to valid range
    if (pulse_width < PWM_MIN_VALUE_US) pulse_width = PWM_MIN_VALUE_US;
    if (pulse_width > PWM_MAX_VALUE_US) pulse_width = PWM_MAX_VALUE_US;
    
    pwm_state.channel_values[channel] = pulse_width;
    
    // Update hardware timer
    uint32_t ticks = pwm_us_to_ticks(pulse_width);
    
    switch (channel) {
        case PWM_CH1: TIM_SetCompare1(TIM2, ticks); break;
        case PWM_CH2: TIM_SetCompare2(TIM2, ticks); break;
        case PWM_CH3: TIM_SetCompare3(TIM2, ticks); break;
        case PWM_CH4: TIM_SetCompare1(TIM3, ticks); break;
        case PWM_CH5: TIM_SetCompare2(TIM3, ticks); break;
        case PWM_CH6: TIM_SetCompare3(TIM3, ticks); break;
        case PWM_CH7: TIM_SetCompare1(TIM4, ticks); break;
        case PWM_CH8: TIM_SetCompare2(TIM4, ticks); break;
        case PWM_CH9: TIM_SetCompare3(TIM4, ticks); break;
        default: break;
    }
}

void pwm_set_all_pulses(const uint16_t *pulse_widths)
{
    if (pulse_widths == NULL) return;
    
    for (int i = 0; i < PWM_CHANNEL_COUNT; i++) {
        pwm_set_pulse((pwm_channel_t)i, pulse_widths[i]);
    }
}

uint16_t pwm_get_pulse(pwm_channel_t channel)
{
    if (channel >= PWM_CHANNEL_COUNT) return 0;
    return pwm_state.channel_values[channel];
}

void pwm_output_enable(void)
{
    TIM_Cmd(TIM2, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
    TIM_Cmd(TIM4, ENABLE);
    pwm_state.enabled = 1;
}

void pwm_output_disable(void)
{
    TIM_Cmd(TIM2, DISABLE);
    TIM_Cmd(TIM3, DISABLE);
    TIM_Cmd(TIM4, DISABLE);
    pwm_state.enabled = 0;
}

void pwm_center_all(void)
{
    for (int i = 0; i < PWM_CHANNEL_COUNT; i++) {
        pwm_set_pulse((pwm_channel_t)i, PWM_CENTER_VALUE_US);
    }
}

void pwm_failsafe_all(void)
{
    for (int i = 0; i < PWM_CHANNEL_COUNT; i++) {
        pwm_set_pulse((pwm_channel_t)i, PWM_MIN_VALUE_US);
    }
}

uint8_t pwm_is_enabled(void)
{
    return pwm_state.enabled;
}

void pwm_timer_set_compare(uint8_t timer_num, uint8_t channel, uint32_t compare)
{
    if (channel < 1 || channel > 3) return;
    
    TIM_TypeDef *tim = NULL;
    if (timer_num == 2) tim = TIM2;
    else if (timer_num == 3) tim = TIM3;
    else if (timer_num == 4) tim = TIM4;
    else return;
    
    if (channel == 1) TIM_SetCompare1(tim, compare);
    else if (channel == 2) TIM_SetCompare2(tim, compare);
    else if (channel == 3) TIM_SetCompare3(tim, compare);
}

uint32_t pwm_timer_get_compare(uint8_t timer_num, uint8_t channel)
{
    if (channel < 1 || channel > 3) return 0;
    
    TIM_TypeDef *tim = NULL;
    if (timer_num == 2) tim = TIM2;
    else if (timer_num == 3) tim = TIM3;
    else if (timer_num == 4) tim = TIM4;
    else return 0;
    
    if (channel == 1) return tim->CCR1;
    else if (channel == 2) return tim->CCR2;
    else if (channel == 3) return tim->CCR3;
    
    return 0;
}
