/**
 * @file dual_receiver_manager.c
 * @brief Dual CRSF receiver management implementation
 */

#include "dual_receiver_manager.h"
#include "pwm_output.h"
#include <string.h>

/* ============================================================================
 * Static Helper Functions
 * ========================================================================== */

/**
 * @brief Update receiver status based on signal quality
 */
static void drm_update_receiver_status(dual_receiver_manager_t *manager, 
                                       receiver_index_t rx_index)
{
    receiver_quality_metrics_t *metrics = &manager->metrics[rx_index];
    
    if (manager->receivers[rx_index].frame_count == 0) {
        metrics->status = RX_STATUS_NO_SIGNAL;
        return;
    }
    
    // Determine status based on RSSI and link quality
    if (metrics->rssi_dbm <= -95 || metrics->link_quality < 30) {
        metrics->status = RX_STATUS_WEAK_SIGNAL;
    } else if (metrics->rssi_dbm <= -85 || metrics->link_quality < 50) {
        metrics->status = RX_STATUS_GOOD;
    } else {
        metrics->status = RX_STATUS_EXCELLENT;
    }
}

/**
 * @brief Make receiver switching decision
 */
static void drm_make_switching_decision(dual_receiver_manager_t *manager,
                                        uint32_t current_time)
{
    receiver_quality_metrics_t *primary = &manager->metrics[RX_PRIMARY];
    receiver_quality_metrics_t *secondary = &manager->metrics[RX_SECONDARY];
    receiver_index_t new_active = manager->active_receiver;
    
    // Check if enough time has passed since last switch (hysteresis)
    uint32_t time_since_switch = current_time - manager->last_switch_time;
    if (time_since_switch < SWITCH_HYSTERESIS_MS) {
        return;  // Don't switch yet - too soon
    }
    
    // Current switching logic:
    // 1. If current receiver is good, stay with it
    // 2. If current receiver is bad and other is good, switch
    // 3. If both are bad, stay with current or prefer primary
    // 4. If both are good, prefer primary
    
    if (manager->active_receiver == RX_PRIMARY) {
        // Currently on primary
        if (primary->status >= RX_STATUS_GOOD) {
            // Primary is good, stay
            return;
        } else if (secondary->status >= RX_STATUS_GOOD) {
            // Primary is bad, secondary is good - switch
            new_active = RX_SECONDARY;
        }
        // Both bad - stay on primary for now
    } else {
        // Currently on secondary
        if (secondary->status >= RX_STATUS_GOOD) {
            // Secondary is good, stay
            return;
        } else if (primary->status > secondary->status) {
            // Primary is better - switch
            new_active = RX_PRIMARY;
        }
    }
    
    // Apply switch if needed
    if (new_active != manager->active_receiver) {
        manager->active_receiver = new_active;
        manager->last_switch_time = current_time;
        manager->switch_count++;
    }
}

/**
 * @brief Extract and convert RC channel values to PWM
 */
static void drm_extract_rc_channels(dual_receiver_manager_t *manager)
{
    crsf_receiver_t *active_rx = &manager->receivers[manager->active_receiver];
    
    if (!active_rx->has_rc_channels) {
        // No valid RC data - use failsafe
        for (int i = 0; i < RC_CHANNEL_COUNT; i++) {
            manager->output_channels[i] = FAILSAFE_PWM_VALUE;
        }
        manager->failsafe_active = 1;
        return;
    }
    
    // Extract first 9 channels from CRSF packed data
    // Note: This is simplified - actual extraction depends on bit packing
    crsf_rc_channels_t *channels = &active_rx->rc_channels;
    
    manager->output_channels[0] = crsf_channel_to_us(channels->ch1);
    manager->output_channels[1] = crsf_channel_to_us(channels->ch2);
    manager->output_channels[2] = crsf_channel_to_us(channels->ch3);
    manager->output_channels[3] = crsf_channel_to_us(channels->ch4);
    manager->output_channels[4] = crsf_channel_to_us(channels->ch5);
    manager->output_channels[5] = crsf_channel_to_us(channels->ch6);
    manager->output_channels[6] = crsf_channel_to_us(channels->ch7);
    manager->output_channels[7] = crsf_channel_to_us(channels->ch8);
    manager->output_channels[8] = crsf_channel_to_us(channels->ch9);
    
    manager->failsafe_active = 0;
}

/* ============================================================================
 * Public Manager Functions
 * ========================================================================== */

void drm_init(dual_receiver_manager_t *manager)
{
    if (manager == NULL) return;
    
    memset(manager, 0, sizeof(dual_receiver_manager_t));
    
    // Initialize receivers
    crsf_receiver_init(&manager->receivers[RX_PRIMARY]);
    crsf_receiver_init(&manager->receivers[RX_SECONDARY]);
    
    // Set initial active receiver to primary
    manager->active_receiver = RX_PRIMARY;
    manager->last_active = RX_PRIMARY;
    
    // Initialize output channels to center/failsafe
    for (int i = 0; i < RC_CHANNEL_COUNT; i++) {
        manager->output_channels[i] = FAILSAFE_PWM_VALUE;
    }
    
    manager->failsafe_active = 1;
}

uint8_t drm_process_byte(dual_receiver_manager_t *manager,
                         receiver_index_t rx_index, uint8_t byte)
{
    if (manager == NULL || rx_index >= RECEIVER_COUNT) return 0;
    
    return crsf_receiver_process_byte(&manager->receivers[rx_index], byte);
}

uint8_t drm_update(dual_receiver_manager_t *manager, uint32_t current_time)
{
    if (manager == NULL) return 0;
    
    uint8_t switched = 0;
    uint32_t time_since_last_decision = current_time - manager->last_decision_time;
    
    // Update metrics for both receivers
    for (int i = 0; i < RECEIVER_COUNT; i++) {
        crsf_receiver_t *rx = &manager->receivers[i];
        receiver_quality_metrics_t *metrics = &manager->metrics[i];
        
        // Check if receiver has valid link statistics
        if (rx->has_link_stats) {
            metrics->rssi_dbm = -rx->link_stats.downlink_rssi;  // Convert to negative
            metrics->link_quality = rx->link_stats.downlink_link_quality;
            metrics->last_frame_time = current_time;
            metrics->frames_received = rx->frame_count;
        }
        
        // Check for signal timeout
        uint32_t time_since_frame = current_time - metrics->last_frame_time;
        if (time_since_frame > RECEIVER_TIMEOUT_MS) {
            metrics->status = RX_STATUS_NO_SIGNAL;
        } else {
            drm_update_receiver_status(manager, (receiver_index_t)i);
        }
    }
    
    // Update switching decision periodically (every 50ms)
    if (time_since_last_decision >= 50) {
        receiver_index_t old_active = manager->active_receiver;
        drm_make_switching_decision(manager, current_time);
        manager->last_decision_time = current_time;
        switched = (old_active != manager->active_receiver);
        manager->last_active = manager->active_receiver;
    }
    
    // Extract RC channels from active receiver
    drm_extract_rc_channels(manager);
    
    // Update PWM output
    pwm_set_all_pulses(manager->output_channels);
    
    return switched;
}

const uint16_t* drm_get_output_channels(dual_receiver_manager_t *manager)
{
    if (manager == NULL) return NULL;
    return manager->output_channels;
}

receiver_index_t drm_get_active_receiver(const dual_receiver_manager_t *manager)
{
    if (manager == NULL) return RX_PRIMARY;
    return manager->active_receiver;
}

receiver_status_t drm_get_receiver_status(const dual_receiver_manager_t *manager,
                                          receiver_index_t rx_index)
{
    if (manager == NULL || rx_index >= RECEIVER_COUNT) return RX_STATUS_NO_SIGNAL;
    return manager->metrics[rx_index].status;
}

int8_t drm_get_rssi(const dual_receiver_manager_t *manager, receiver_index_t rx_index)
{
    if (manager == NULL || rx_index >= RECEIVER_COUNT) return 0;
    return manager->metrics[rx_index].rssi_dbm;
}

uint8_t drm_get_link_quality(const dual_receiver_manager_t *manager,
                             receiver_index_t rx_index)
{
    if (manager == NULL || rx_index >= RECEIVER_COUNT) return 0;
    return manager->metrics[rx_index].link_quality;
}

void drm_force_receiver(dual_receiver_manager_t *manager, receiver_index_t rx_index)
{
    if (manager == NULL || rx_index >= RECEIVER_COUNT) return;
    manager->active_receiver = rx_index;
}

void drm_reset_to_auto(dual_receiver_manager_t *manager)
{
    if (manager == NULL) return;
    manager->active_receiver = RX_PRIMARY;
    manager->last_switch_time = 0;
}

uint32_t drm_get_switch_count(const dual_receiver_manager_t *manager)
{
    if (manager == NULL) return 0;
    return manager->switch_count;
}

uint8_t drm_is_failsafe(const dual_receiver_manager_t *manager)
{
    if (manager == NULL) return 1;
    return manager->failsafe_active;
}
