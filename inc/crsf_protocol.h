/**
 * @file crsf_protocol.h
 * @brief CRSF Protocol definitions and structures
 * @details Implements CRSF v3 protocol parsing for ELRS receivers
 */

#ifndef __CRSF_PROTOCOL_H
#define __CRSF_PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ============================================================================
 * CRSF Protocol Constants
 * ========================================================================== */

#define CRSF_SYNC_BYTE                  0xC8
#define CRSF_FRAMETYPE_LINK_STATISTICS  0x14
#define CRSF_FRAMETYPE_RC_CHANNELS      0x16
#define CRSF_MAX_FRAME_LENGTH           64
#define CRSF_MAX_PAYLOAD_LENGTH         62  // 64 - sync - length
#define CRSF_FRAME_CRC_SIZE             1
#define CRSF_FRAME_MIN_LENGTH           3   // Type + min_payload + CRC

#define CRSF_RC_CHANNEL_COUNT           16
#define CRSF_RC_CHANNEL_BITS            11
#define CRSF_RC_CENTER_VALUE            992
#define CRSF_RC_MIN_VALUE               800
#define CRSF_RC_MAX_VALUE               2200

/* CRSF RC Channel to µs conversion macros */
#define CRSF_TICKS_TO_US(x)             (((x - 992) * 5) / 8 + 1500)
#define CRSF_US_TO_TICKS(x)             (((x - 1500) * 8) / 5 + 992)

/* ============================================================================
 * CRSF Frame Type Definitions
 * ========================================================================== */

/**
 * @struct crsf_frame_header
 * @brief CRSF frame header structure
 */
typedef struct {
    uint8_t sync;                       ///< Sync byte (0xC8)
    uint8_t length;                     ///< Frame length excluding sync and length bytes
    uint8_t type;                       ///< Frame type
} crsf_frame_header_t;

/**
 * @struct crsf_link_statistics
 * @brief Link Statistics frame (0x14)
 * @details Contains RSSI, SNR, Link Quality information
 */
typedef struct {
    uint8_t uplink_rssi_1;              ///< Uplink RSSI antenna 1 (dBm * -1)
    uint8_t uplink_rssi_2;              ///< Uplink RSSI antenna 2 (dBm * -1)
    uint8_t uplink_link_quality;        ///< Uplink link quality (%)
    int8_t  uplink_snr;                 ///< Uplink SNR (dB)
    uint8_t active_antenna;             ///< Currently best antenna number
    uint8_t rf_profile;                 ///< RF profile (4fps/50fps/150fps)
    uint8_t uplink_tx_power;            ///< Uplink TX power (mW enum)
    uint8_t downlink_rssi;              ///< Downlink RSSI (dBm * -1)
    uint8_t downlink_link_quality;      ///< Downlink link quality (%)
    int8_t  downlink_snr;               ///< Downlink SNR (dB)
} crsf_link_statistics_t;

/**
 * @struct crsf_rc_channels
 * @brief RC Channels Packed frame (0x16)
 * @details 16 channels packed into 22 bytes, 11-bit resolution each
 * @note This is a packed structure - bit fields follow CRSF specification
 */
typedef struct {
    int16_t ch1  : 11;
    int16_t ch2  : 11;
    int16_t ch3  : 11;
    int16_t ch4  : 11;
    int16_t ch5  : 11;
    int16_t ch6  : 11;
    int16_t ch7  : 11;
    int16_t ch8  : 11;
    int16_t ch9  : 11;
    int16_t ch10 : 11;
    int16_t ch11 : 11;
    int16_t ch12 : 11;
    int16_t ch13 : 11;
    int16_t ch14 : 11;
    int16_t ch15 : 11;
    int16_t ch16 : 11;
} crsf_rc_channels_t;

/**
 * @struct crsf_frame
 * @brief Complete CRSF frame structure
 */
typedef struct {
    uint8_t sync;                       ///< Sync byte
    uint8_t length;                     ///< Frame length
    uint8_t type;                       ///< Frame type
    uint8_t payload[CRSF_MAX_PAYLOAD_LENGTH];  ///< Payload data
    uint8_t crc;                        ///< Frame CRC
} crsf_frame_t;

/* ============================================================================
 * CRSF Receiver State
 * ========================================================================== */

/**
 * @enum crsf_rx_state
 * @brief CRSF receiver state machine
 */
typedef enum {
    CRSF_RX_STATE_SYNC,                 ///< Waiting for sync byte
    CRSF_RX_STATE_LENGTH,               ///< Reading frame length
    CRSF_RX_STATE_TYPE,                 ///< Reading frame type
    CRSF_RX_STATE_PAYLOAD,              ///< Reading payload data
    CRSF_RX_STATE_CRC                   ///< Reading CRC byte
} crsf_rx_state_t;

/**
 * @struct crsf_receiver
 * @brief CRSF receiver instance state
 */
typedef struct {
    crsf_rx_state_t state;              ///< Current parser state
    crsf_frame_t frame;                 ///< Current frame being built
    uint8_t payload_index;              ///< Payload write index
    uint32_t last_frame_time;           ///< Timestamp of last valid frame (ms)
    uint32_t frame_count;               ///< Total frames received
    uint32_t error_count;               ///< CRC/format error count
    
    // Extracted data
    crsf_link_statistics_t link_stats;  ///< Last link statistics
    crsf_rc_channels_t rc_channels;     ///< Last RC channel values
    uint8_t has_link_stats;             ///< Flag: valid link stats available
    uint8_t has_rc_channels;            ///< Flag: valid RC channels available
} crsf_receiver_t;

/* ============================================================================
 * CRC Functions
 * ========================================================================== */

/**
 * @brief Calculate CRC8 for CRSF frame
 * @param ptr Pointer to data buffer
 * @param len Length of data
 * @return Calculated CRC8 value
 * @details Uses polynomial 0xD5 (x7 + x6 + x4 + x2 + x0)
 */
uint8_t crsf_crc8(const uint8_t *ptr, uint8_t len);

/* ============================================================================
 * CRSF Receiver Functions
 * ========================================================================== */

/**
 * @brief Initialize CRSF receiver
 * @param rx Pointer to receiver structure
 */
void crsf_receiver_init(crsf_receiver_t *rx);

/**
 * @brief Process a single byte from CRSF stream
 * @param rx Pointer to receiver structure
 * @param byte Byte to process
 * @return 1 if complete frame received and valid, 0 otherwise
 */
uint8_t crsf_receiver_process_byte(crsf_receiver_t *rx, uint8_t byte);

/**
 * @brief Check if receiver has valid data (not timed out)
 * @param rx Pointer to receiver structure
 * @param timeout_ms Timeout value in milliseconds
 * @return 1 if valid, 0 if timed out
 */
uint8_t crsf_receiver_is_valid(crsf_receiver_t *rx, uint32_t timeout_ms);

/**
 * @brief Extract RC channel value in microseconds
 * @param value Raw 11-bit channel value
 * @return Channel value in microseconds (1000-2000)
 */
uint16_t crsf_channel_to_us(uint16_t value);

/**
 * @brief Convert microsecond value to CRSF channel format
 * @param us Microsecond value (1000-2000)
 * @return 11-bit CRSF channel value
 */
uint16_t crsf_us_to_channel(uint16_t us);

#endif /* __CRSF_PROTOCOL_H */
