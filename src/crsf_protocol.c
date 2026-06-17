/**
 * @file crsf_protocol.c
 * @brief CRSF Protocol implementation
 */

#include "crsf_protocol.h"
#include <string.h>

/* ============================================================================
 * CRC8 Lookup Table
 * ========================================================================== */

static const uint8_t crsf_crc8tab[256] = {
    0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54, 0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
    0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06, 0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
    0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0, 0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
    0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2, 0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
    0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9, 0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
    0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B, 0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
    0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D, 0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
    0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F, 0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
    0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB, 0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
    0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9, 0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
    0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F, 0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
    0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D, 0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
    0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26, 0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
    0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74, 0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
    0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82, 0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
    0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0, 0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9
};

/* ============================================================================
 * CRC Functions
 * ========================================================================== */

uint8_t crsf_crc8(const uint8_t *ptr, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc = crsf_crc8tab[crc ^ ptr[i]];
    }
    return crc;
}

/* ============================================================================
 * CRSF Receiver Functions
 * ========================================================================== */

void crsf_receiver_init(crsf_receiver_t *rx)
{
    if (rx == NULL) return;
    
    memset(rx, 0, sizeof(crsf_receiver_t));
    rx->state = CRSF_RX_STATE_SYNC;
}

/**
 * @brief Validate and process a complete CRSF frame
 */
static uint8_t crsf_process_frame(crsf_receiver_t *rx)
{
    uint8_t frame_len = rx->frame.length;
    
    // Validate length
    if (frame_len < CRSF_FRAME_MIN_LENGTH || frame_len > CRSF_MAX_PAYLOAD_LENGTH) {
        rx->error_count++;
        return 0;
    }
    
    // Calculate CRC on Type + Payload (excludes Sync and Length bytes)
    uint8_t calculated_crc = crsf_crc8(&rx->frame.type, frame_len - 1);
    uint8_t received_crc = rx->frame.payload[frame_len - 2];
    
    if (calculated_crc != received_crc) {
        rx->error_count++;
        return 0;
    }
    
    // Process frame based on type
    switch (rx->frame.type) {
        case CRSF_FRAMETYPE_LINK_STATISTICS:
            if (frame_len >= sizeof(crsf_link_statistics_t) + 2) {
                memcpy(&rx->link_stats, &rx->frame.payload[0], sizeof(crsf_link_statistics_t));
                rx->has_link_stats = 1;
            }
            break;
            
        case CRSF_FRAMETYPE_RC_CHANNELS:
            if (frame_len >= sizeof(crsf_rc_channels_t) + 2) {
                memcpy(&rx->rc_channels, &rx->frame.payload[0], sizeof(crsf_rc_channels_t));
                rx->has_rc_channels = 1;
            }
            break;
            
        default:
            // Unknown frame type - ignore
            break;
    }
    
    rx->frame_count++;
    return 1;
}

uint8_t crsf_receiver_process_byte(crsf_receiver_t *rx, uint8_t byte)
{
    if (rx == NULL) return 0;
    
    switch (rx->state) {
        case CRSF_RX_STATE_SYNC:
            if (byte == CRSF_SYNC_BYTE) {
                rx->frame.sync = byte;
                rx->state = CRSF_RX_STATE_LENGTH;
            }
            break;
            
        case CRSF_RX_STATE_LENGTH:
            rx->frame.length = byte;
            if (rx->frame.length >= CRSF_FRAME_MIN_LENGTH && 
                rx->frame.length <= CRSF_MAX_PAYLOAD_LENGTH) {
                rx->payload_index = 0;
                rx->state = CRSF_RX_STATE_TYPE;
            } else {
                rx->state = CRSF_RX_STATE_SYNC;
                rx->error_count++;
            }
            break;
            
        case CRSF_RX_STATE_TYPE:
            rx->frame.type = byte;
            rx->state = CRSF_RX_STATE_PAYLOAD;
            break;
            
        case CRSF_RX_STATE_PAYLOAD:
            rx->frame.payload[rx->payload_index++] = byte;
            if (rx->payload_index >= (rx->frame.length - 1)) {
                rx->state = CRSF_RX_STATE_CRC;
            }
            break;
            
        case CRSF_RX_STATE_CRC:
            rx->frame.crc = byte;
            rx->frame.payload[rx->payload_index] = byte;  // Store CRC in payload
            rx->state = CRSF_RX_STATE_SYNC;
            
            // Process complete frame
            if (crsf_process_frame(rx)) {
                return 1;  // Complete valid frame received
            }
            break;
            
        default:
            rx->state = CRSF_RX_STATE_SYNC;
            break;
    }
    
    return 0;  // No complete frame yet
}

uint8_t crsf_receiver_is_valid(crsf_receiver_t *rx, uint32_t timeout_ms)
{
    if (rx == NULL) return 0;
    if (rx->frame_count == 0) return 0;  // Never received a frame
    
    // Check timeout - this requires external time tracking
    // Implementation depends on your system timer
    // For now, just return frame_count > 0 as indicator
    return (rx->frame_count > 0);
}

/* ============================================================================
 * Channel Conversion Functions
 * ========================================================================== */

uint16_t crsf_channel_to_us(uint16_t value)
{
    // Clamp to valid CRSF range
    if (value < CRSF_RC_MIN_VALUE) value = CRSF_RC_MIN_VALUE;
    if (value > CRSF_RC_MAX_VALUE) value = CRSF_RC_MAX_VALUE;
    
    // Convert using CRSF formula: us = (value - 992) * 5 / 8 + 1500
    return CRSF_TICKS_TO_US(value);
}

uint16_t crsf_us_to_channel(uint16_t us)
{
    // Clamp to standard RC range
    if (us < 1000) us = 1000;
    if (us > 2000) us = 2000;
    
    // Convert using inverse CRSF formula: value = (us - 1500) * 8 / 5 + 992
    return CRSF_US_TO_TICKS(us);
}
