/**
 * @file dual_receiver_manager.h
 * @brief Dual CRSF receiver management and switching logic
 */

#ifndef __DUAL_RECEIVER_MANAGER_H
#define __DUAL_RECEIVER_MANAGER_H

#include "crsf_protocol.h"

/* ============================================================================
 * Configuration Constants
 * ========================================================================== */

#define RECEIVER_COUNT                  2
#define RECEIVER_TIMEOUT_MS             100     ///< Signal timeout (ms)
#define SIGNAL_LOSS_THRESHOLD_MS        500     ///< Complete loss threshold
#define RSSI_GOOD_THRESHOLD             -85     ///< dBm (negated in storage)
#define RSSI_POOR_THRESHOLD             -95     ///< dBm
#define LINK_QUALITY_GOOD_THRESHOLD     50      ///< %
#define LINK_QUALITY_POOR_THRESHOLD     30      ///< %
#define SWITCH_HYSTERESIS_MS            200     ///< Prevent rapid switching
#define RC_CHANNEL_COUNT                9       ///< Output channels

#define FAILSAFE_PWM_VALUE              1000    ///< Failsafe PWM (1000µs)

/* ============================================================================
 * Receiver Index Definitions
 * ========================================================================== */

typedef enum {
    RX_PRIMARY = 0,                     ///< Primary receiver (UART1)
    RX_SECONDARY = 1                   ///< Secondary receiver (UART2)
} receiver_index_t;

/* ============================================================================
 * Receiver State and Quality
 * ========================================================================== */

/**
 * @enum receiver_status
 * @brief Status of individual receiver
 */
typedef enum {
    RX_STATUS_NO_SIGNAL,                ///< No signal received
    RX_STATUS_WEAK_SIGNAL,              ///< Signal too weak
    RX_STATUS_SIGNAL_LOST,              ///< Signal previously had data, now lost
    RX_STATUS_GOOD,                     ///< Good signal quality
    RX_STATUS_EXCELLENT                 ///< Excellent signal quality
} receiver_status_t;

/**
 * @struct receiver_quality_metrics
 * @brief Signal quality metrics for a receiver
 */
typedef struct {
    int8_t rssi_dbm;                    ///< RSSI in dBm (stored as -value)
    uint8_t link_quality;               ///< Link quality percentage (0-100)
    receiver_status_t status;           ///< Current status
    uint32_t last_frame_time;           ///< Timestamp of last frame
    uint32_t frames_received;           ///< Total frames received
} receiver_quality_metrics_t;

/**
 * @struct dual_receiver_manager
 * @brief Manages dual receivers and switching logic
 */
typedef struct {
    crsf_receiver_t receivers[RECEIVER_COUNT];  ///< Receiver instances
    receiver_quality_metrics_t metrics[RECEIVER_COUNT];  ///< Quality metrics
    
    receiver_index_t active_receiver;   ///< Currently active receiver
    receiver_index_t last_active;       ///< Last active receiver
    uint32_t last_switch_time;          ///< Time of last switch
    uint32_t switch_count;              ///< Total switch count
    uint32_t last_decision_time;        ///< Last decision update time
    
    uint16_t output_channels[RC_CHANNEL_COUNT];  ///< Output PWM values (µs)
    uint8_t failsafe_active;            ///< Failsafe mode active
} dual_receiver_manager_t;

/* ============================================================================
 * Manager Functions
 * ========================================================================== */

/**
 * @brief Initialize dual receiver manager
 * @param manager Pointer to manager structure
 */
void drm_init(dual_receiver_manager_t *manager);

/**
 * @brief Process incoming byte for specified receiver
 * @param manager Pointer to manager structure
 * @param rx_index Receiver index (RX_PRIMARY or RX_SECONDARY)
 * @param byte Byte to process
 * @return 1 if complete frame received, 0 otherwise
 */
uint8_t drm_process_byte(dual_receiver_manager_t *manager, 
                         receiver_index_t rx_index, uint8_t byte);

/**
 * @brief Update signal quality metrics and make switching decision
 * @param manager Pointer to manager structure
 * @param current_time Current system time in milliseconds
 * @return 1 if receiver was switched, 0 otherwise
 * @details Should be called periodically (e.g., every 50-100ms)
 */
uint8_t drm_update(dual_receiver_manager_t *manager, uint32_t current_time);

/**
 * @brief Get output PWM channels from active receiver
 * @param manager Pointer to manager structure
 * @return Pointer to output channels array (9 values in microseconds)
 * @details Returns failsafe values if no valid receiver
 */
const uint16_t* drm_get_output_channels(dual_receiver_manager_t *manager);

/**
 * @brief Get active receiver index
 * @param manager Pointer to manager structure
 * @return RX_PRIMARY or RX_SECONDARY
 */
receiver_index_t drm_get_active_receiver(const dual_receiver_manager_t *manager);

/**
 * @brief Get receiver status for debugging
 * @param manager Pointer to manager structure
 * @param rx_index Receiver index
 * @return Status enumeration
 */
receiver_status_t drm_get_receiver_status(const dual_receiver_manager_t *manager,
                                          receiver_index_t rx_index);

/**
 * @brief Get RSSI of specified receiver
 * @param manager Pointer to manager structure
 * @param rx_index Receiver index
 * @return RSSI in dBm (negative value)
 */
int8_t drm_get_rssi(const dual_receiver_manager_t *manager, receiver_index_t rx_index);

/**
 * @brief Get link quality of specified receiver
 * @param manager Pointer to manager structure
 * @param rx_index Receiver index
 * @return Link quality percentage (0-100)
 */
uint8_t drm_get_link_quality(const dual_receiver_manager_t *manager, 
                             receiver_index_t rx_index);

/**
 * @brief Force manual receiver selection
 * @param manager Pointer to manager structure
 * @param rx_index Receiver index to switch to
 */
void drm_force_receiver(dual_receiver_manager_t *manager, receiver_index_t rx_index);

/**
 * @brief Reset switching logic to auto mode
 * @param manager Pointer to manager structure
 */
void drm_reset_to_auto(dual_receiver_manager_t *manager);

/**
 * @brief Get total switch count
 * @param manager Pointer to manager structure
 * @return Number of times receiver was switched
 */
uint32_t drm_get_switch_count(const dual_receiver_manager_t *manager);

/**
 * @brief Check if failsafe is currently active
 * @param manager Pointer to manager structure
 * @return 1 if failsafe, 0 if normal operation
 */
uint8_t drm_is_failsafe(const dual_receiver_manager_t *manager);

#endif /* __DUAL_RECEIVER_MANAGER_H */
