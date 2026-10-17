#include <string.h>

#include "inc_main.h"
#include "host_link_protocol.h"
#ifndef BRIDGE_SPLIT_APP
#error Build the host runtime only as RL78_F24_Bridge_app.
#endif
#include "f24_bridge_support.h"
#include "f24_app_identity.generated.h"
#include "app_image_config.h"
#if APP_IMAGE_VERSION != F24_APP_IMAGE_VERSION || APP_IMAGE_BUILD != F24_APP_IMAGE_BUILD
#error App release header and generated identity must match before compilation.
#endif
/* Post-build compares this linked record with the editable App header.
 * Volatile reads keep the live Host Link identity tied to compiled data.
 * These native words are not the serialized BUR1 metadata structure.
 */
const volatile uint32_t __far g_host_app_image_contract[5] = {
    APP_IMAGE_VERSION, APP_IMAGE_BUILD, APP_IMAGE_TARGET_ID,
    APP_IMAGE_ROLE, APP_IMAGE_UPDATE_TRANSPORTS
};
#define F24_HOST_FW_MAJOR ((uint8_t)(g_host_app_image_contract[0] >> 16))
#define F24_HOST_FW_MINOR ((uint8_t)(g_host_app_image_contract[0] >> 8))
#define F24_HOST_FW_PATCH ((uint8_t)g_host_app_image_contract[0])
#define F24_HOST_FW_BUILD g_host_app_image_contract[1]
#include "f24_can_clock.h"
#include "f24_can_timing.h"
#include "bridge_i2c.h"
#include "../common/can_burst.h"
#include "../common/can_burst.inc"
#include "../common/diagnostic_clock.h"
#define BRIDGE_SELF_UPDATE_FEATURE BU_FEATURE

#define HOST_LINK_SOF0                         (0x55U)
#define HOST_LINK_SOF1                         (0xAAU)
#define HOST_LINK_HEADER_SIZE                  (12U)
#define HOST_LINK_PAYLOAD_CRC_SIZE             (2U)
#define HOST_LINK_MIN_FRAME_SIZE               (14U)

#define HOST_LINK_TYPE_REQUEST                 (0x10U)
#define HOST_LINK_TYPE_RESPONSE                (0x20U)
#define HOST_LINK_TYPE_EVENT                   (0x30U)

#define HOST_LINK_STATUS_OK                    (0x00U)
#define HOST_LINK_STATUS_BUSY                  (0x01U)
#define HOST_LINK_STATUS_BAD_COMMAND           (0x02U)
#define HOST_LINK_STATUS_BAD_ARGUMENT          (0x03U)
#define HOST_LINK_STATUS_CRC_ERROR             (0x04U)
#define HOST_LINK_STATUS_SEQUENCE_ERROR        (0x05U)
#define HOST_LINK_STATUS_TIMEOUT               (0x06U)
#define HOST_LINK_STATUS_CAN_ERROR             (0x07U)
#define HOST_LINK_STATUS_OVERFLOW              (0x08U)
#define HOST_LINK_STATUS_BAD_STATE             (0x0AU)

#define HOST_LINK_CMD_PING                     (0x00U)
#define HOST_LINK_CMD_GET_HOST_INFO            (0x01U)
#define HOST_LINK_CMD_GET_CAPABILITIES         (0x02U)
#define HOST_LINK_CMD_RESET_HOST               (0x03U)
#define HOST_LINK_CMD_GET_PORT_INFO            (0x04U)
#define HOST_LINK_CMD_SELECT_CAN_PORT          (0x14U)
#define HOST_LINK_CMD_SET_I2C_CONFIG           (0x40U)
#define HOST_LINK_CMD_I2C_TRANSACTION          (0x41U)
#define HOST_LINK_CMD_SET_CAN_CONFIG           (0x10U)
#define HOST_LINK_CMD_SET_CAN_FILTER           (0x11U)
#define HOST_LINK_CMD_CAN_START                (0x12U)
#define HOST_LINK_CMD_CAN_STOP                 (0x13U)
#define HOST_LINK_CMD_CAN_TX                   (0x20U)
#define HOST_LINK_CMD_CAN_TX_WAIT_RX           (0x21U)
#define HOST_LINK_CMD_GET_STATS                (0x30U)
#define HOST_LINK_CMD_CLEAR_STATS              (0x31U)
#define HOST_LINK_EVENT_CAN_RX                 (0x80U)
#define HOST_LINK_EVENT_CAN_STATE              (0x81U)
#define HOST_LINK_EVENT_HOST_ERROR             (0x82U)

#define HOST_LINK_BOARD_ID                     F24_BRIDGE_BOARD_ID
#define HOST_LINK_FEATURE_CLASSICAL_CAN        (0x00000001UL)
#define HOST_LINK_FEATURE_CAN_FD               (0x00000002UL)
#define HOST_LINK_FEATURE_STANDARD_ID          (0x00000004UL)
#define HOST_LINK_FEATURE_EXTENDED_ID          (0x00000008UL)
#define HOST_LINK_FEATURE_TX_WAIT_RX           (0x00000010UL)
#define HOST_LINK_FEATURE_ASYNC_RX             (0x00000020UL)
#define HOST_LINK_FEATURE_AUTO_BUS_OFF         (0x00000080UL)
#define HOST_LINK_FEATURES                     (HOST_LINK_FEATURE_CLASSICAL_CAN | \
                                                HOST_LINK_FEATURE_CAN_FD | \
                                                HOST_LINK_FEATURE_STANDARD_ID | \
                                                HOST_LINK_FEATURE_EXTENDED_ID | \
                                                HOST_LINK_FEATURE_TX_WAIT_RX | \
                                                HOST_LINK_FEATURE_ASYNC_RX | \
                                                HOST_LINK_FEATURE_AUTO_BUS_OFF | \
                                                0x00000300UL | BRIDGE_SELF_UPDATE_FEATURE | HL_BAUD_FEATURE | CB_FEATURE)

#define HOST_LINK_NOMINAL_BITRATE_MASK         F24_CAN_NOMINAL_MASK
#define HOST_LINK_DATA_BITRATE_MASK            (0x00000002UL)

#define HOST_LINK_PROCESS_BUDGET               (64U)
#define HOST_LINK_READ_CHUNK                   (16U)

#define HOST_LINK_CAN_MODE_CLASSICAL           (0U)
#define HOST_LINK_CAN_FLAG_IDE                 (0x01U)
#define HOST_LINK_CAN_FLAG_RTR                 (0x02U)
#define HOST_LINK_CAN_FLAG_FDF                 (0x04U)
#define HOST_LINK_CAN_FLAG_BRS                 (0x08U)
#define HOST_LINK_CAN_FLAG_ESI                 (0x10U)
#define HOST_LINK_CAN_FLAG_MASK                (0x1FU)
#define HOST_LINK_CAN_RECORD_HEADER_SIZE       (12U)
#define HOST_LINK_CAN_TX_TIMEOUT_MS            (1000U)
#define HOST_LINK_CAN_ABORT_TIMEOUT_MS         (100U)
#define HOST_LINK_TICK_WRAP_MS                 (60000U)

#define HOST_LINK_CAN_PHASE_TX                 (1U)
#define HOST_LINK_CAN_PHASE_RX                 (2U)
#define HOST_LINK_CAN_PHASE_ABORT              (3U)
#define HOST_LINK_CAN_PHASE_GAP                (4U)
#define HOST_LINK_CAN_TRANSACTION_QUEUE_CAPACITY (2U)

#define HOST_LINK_CAN_STATE_SOURCE_MASK        (CAN_SOURCE_CH_ERR_WARNING | \
                                                CAN_SOURCE_CH_ERR_PASSIVE | \
                                                CAN_SOURCE_CH_ERR_BO_ENTRY | \
                                                CAN_SOURCE_CH_ERR_BO_RCVR)

typedef struct
{
    uint8_t buffer[HOST_LINK_MAX_FRAME_SIZE];
    uint16_t count;
    uint16_t expected_length;
} host_link_parser_t;

typedef struct
{
    uint32_t uart_rx_bytes;
    uint32_t uart_tx_bytes;
    uint32_t uart_crc_errors;
    uint32_t uart_invalid_errors;
    uint32_t uart_rx_overflows;
    uint32_t can_rx_frames;
    uint32_t can_tx_frames;
    uint32_t can_rx_lost_frames;
    uint32_t can_tx_failures;
    uint32_t can_warning_entries;
    uint32_t can_error_passive_entries;
    uint32_t can_bus_off_entries;
    uint32_t can_recovery_count;
    uint32_t host_command_errors;
} host_link_stats_t;

typedef struct
{
    uint8_t valid;
    uint16_t sequence;
    uint8_t command;
    uint16_t payload_crc;
    uint8_t response[HOST_LINK_MAX_FRAME_SIZE];
    uint16_t response_length;
} host_link_replay_cache_t;

typedef struct
{
    uint8_t active;
    uint8_t command;
    uint8_t phase;
    uint8_t attempts;
    uint8_t retry_limit;
    uint8_t expected_dlc;
    uint8_t expected_flags;
    uint8_t expected_flags_mask;
    uint8_t rx_matched;
    can_length_t rx_length;
    uint16_t sequence;
    uint16_t request_payload_crc;
    uint16_t timeout_ms;
    uint16_t phase_started_tick;
    uint16_t tx_timestamp;
    uint32_t queued_tick;
    uint32_t started_tick;
    uint32_t expected_id;
    uint32_t expected_id_mask;
    e_can_txb_result_t last_tx_result;
    st_can_tx_frame_t tx_frame;
    st_can_rx_frame_t rx_frame;
    cb_cursor_t burst;
} host_link_can_transaction_t;

static host_link_parser_t g_parser;
static host_link_stats_t g_stats;
static host_link_replay_cache_t g_replay;
static uint8_t g_response[HOST_LINK_MAX_FRAME_SIZE];
static uint8_t g_dispatch_payload[HOST_LINK_MAX_PAYLOAD];
static uint8_t g_transaction_response[HOST_LINK_MAX_PAYLOAD];
static host_link_can_transaction_t g_can_transaction;
static host_link_can_transaction_t
    g_can_transaction_queue[HOST_LINK_CAN_TRANSACTION_QUEUE_CAPACITY];
static uint8_t g_can_transaction_queue_head;
static uint8_t g_can_transaction_queue_tail;
static uint8_t g_can_transaction_queue_count;
static uint8_t g_transaction_high_water;
static uint32_t g_queue_wait_max_ms, g_transaction_max_ms, g_process_max_ms;
static hl_diagnostic_clock_t g_diagnostic_clock;
static can_host_error_event_t g_pending_state_event;
static can_host_error_event_t g_pending_host_error_event;
static uint8_t g_pending_state_event_valid;
static uint8_t g_pending_host_error_event_valid;
static uint8_t g_reset_pending;
static uint8_t g_i2c_request_active;
static uint16_t g_i2c_sequence;
static uint16_t g_i2c_payload_crc;
static uint8_t g_i2c_result_waiting;
static uint8_t g_i2c_result_status;
static uint16_t g_i2c_result_received;

static uint16_t read_le16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] |
                      ((uint16_t)data[1] << 8));
}

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void write_le16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xFFUL);
    data[1] = (uint8_t)((value >> 8) & 0xFFUL);
    data[2] = (uint8_t)((value >> 16) & 0xFFUL);
    data[3] = (uint8_t)((value >> 24) & 0xFFUL);
}

static uint8_t can_dlc_to_length(uint8_t dlc, uint8_t *length)
{
    static const uint8_t fd_lengths[8] = {
        12U, 16U, 20U, 24U, 32U, 48U, 64U, 0U
    };

    if (length == (uint8_t *)0)
    {
        return 0U;
    }
    if (dlc <= 8U)
    {
        *length = dlc;
        return 1U;
    }
    if (dlc <= 15U)
    {
        *length = fd_lengths[dlc - 9U];
        return 1U;
    }
    return 0U;
}

static uint16_t elapsed_tick_ms(uint16_t now, uint16_t before)
{
    if (now >= before)
    {
        return (uint16_t)(now - before);
    }
    return (uint16_t)((HOST_LINK_TICK_WRAP_MS - before) + now);
}

static uint8_t decode_tx_can_record(const uint8_t *record,
                                    uint16_t record_length,
                                    st_can_tx_frame_t *frame)
{
    can_host_config_t config;
    uint8_t flags;
    uint8_t dlc;
    uint8_t data_length;
    uint8_t dlc_length;
    uint8_t fdctr;
    uint32_t id;
    uint32_t maximum_id;

    if ((record == (const uint8_t *)0) ||
        (frame == (st_can_tx_frame_t *)0) ||
        (record_length < HOST_LINK_CAN_RECORD_HEADER_SIZE))
    {
        return 0U;
    }

    flags = record[1];
    dlc = record[2];
    data_length = record[3];
    id = read_le32(&record[4]);
    if ((record[0] != 0U) ||
        ((flags & (uint8_t)~HOST_LINK_CAN_FLAG_MASK) != 0U) ||
        ((flags & HOST_LINK_CAN_FLAG_ESI) != 0U) ||
        (read_le32(&record[8]) != 0UL) ||
        (can_dlc_to_length(dlc, &dlc_length) == 0U) ||
        (data_length != dlc_length) ||
        (record_length != (uint16_t)(HOST_LINK_CAN_RECORD_HEADER_SIZE +
                                     data_length)))
    {
        return 0U;
    }

    maximum_id = ((flags & HOST_LINK_CAN_FLAG_IDE) != 0U) ?
                 0x1FFFFFFFUL : 0x7FFUL;
    if (id > maximum_id)
    {
        return 0U;
    }
    if (((flags & HOST_LINK_CAN_FLAG_BRS) != 0U) &&
        ((flags & HOST_LINK_CAN_FLAG_FDF) == 0U))
    {
        return 0U;
    }
    if (((flags & HOST_LINK_CAN_FLAG_RTR) != 0U) &&
        (((flags & (HOST_LINK_CAN_FLAG_FDF | HOST_LINK_CAN_FLAG_BRS)) != 0U) ||
         (data_length != 0U)))
    {
        return 0U;
    }
    if (((flags & HOST_LINK_CAN_FLAG_FDF) == 0U) &&
        (data_length > 8U))
    {
        return 0U;
    }

    CAN_HostGetConfig(&config);
    if ((config.mode == HOST_LINK_CAN_MODE_CLASSICAL) &&
        ((flags & HOST_LINK_CAN_FLAG_FDF) != 0U))
    {
        return 0U;
    }

    if ((flags & HOST_LINK_CAN_FLAG_FDF) == 0U)
    {
        fdctr = CAN_FDCTR_CLASSICAL;
    }
    else if ((flags & HOST_LINK_CAN_FLAG_BRS) != 0U)
    {
        fdctr = CAN_FDCTR_FD;
    }
    else
    {
        fdctr = CAN_FDCTR_FD_NMNL;
    }

    /* Follow the Renesas F24 CAN/CAN FD examples and populate the public
     * bit-field interface consumed by R_CAN_SendByTXMB. */
    memset(frame, 0, sizeof(*frame));
    frame->Head.Bits.ID = id;
    frame->Head.Bits.THLEN = CAN_THL_DISABLE;
    frame->Head.Bits.RTR =
        ((flags & HOST_LINK_CAN_FLAG_RTR) != 0U) ?
        CAN_RTR_CLASSICAL_RMT : CAN_RTR_DATA_FRAME;
    frame->Head.Bits.IDE =
        ((flags & HOST_LINK_CAN_FLAG_IDE) != 0U) ?
        CAN_IDE_EXT_FORMAT : CAN_IDE_STD_FORMAT;
    frame->Head.Bits.FDCTR = fdctr;
    frame->Head.Bits.DLC = dlc;
    frame->Head.Bits.IFL = 0U;
    frame->Head.Bits.LBL = 0U;

    /* Reject an unexpected compiler/layout result before using the driver. */
    if ((frame->Head.Bits.ID != id) ||
        (frame->Head.Bits.DLC != dlc) ||
        (frame->Head.Bits.FDCTR != fdctr) ||
        (frame->Head.Bits.THLEN != CAN_THL_DISABLE) ||
        (frame->Head.Bits.IDE !=
            (((flags & HOST_LINK_CAN_FLAG_IDE) != 0U) ?
             CAN_IDE_EXT_FORMAT : CAN_IDE_STD_FORMAT)) ||
        (frame->Head.Bits.RTR !=
            (((flags & HOST_LINK_CAN_FLAG_RTR) != 0U) ?
             CAN_RTR_CLASSICAL_RMT : CAN_RTR_DATA_FRAME)))
    {
        return 0U;
    }
    if (data_length != 0U)
    {
        memcpy(frame->Data.DB, &record[HOST_LINK_CAN_RECORD_HEADER_SIZE],
               data_length);
    }
    return 1U;
}

static uint8_t rx_frame_flags(const st_can_rx_frame_t *frame)
{
    uint8_t flags;

    flags = 0U;
    if (frame->Head.Bits.IDE != 0U)
    {
        flags |= HOST_LINK_CAN_FLAG_IDE;
    }
    if (frame->Head.Bits.RTR != 0U)
    {
        flags |= HOST_LINK_CAN_FLAG_RTR;
    }
    if (frame->Head.Bits.FDF != 0U)
    {
        flags |= HOST_LINK_CAN_FLAG_FDF;
    }
    if (frame->Head.Bits.BRS != 0U)
    {
        flags |= HOST_LINK_CAN_FLAG_BRS;
    }
    if (frame->Head.Bits.ESI != 0U)
    {
        flags |= HOST_LINK_CAN_FLAG_ESI;
    }
    return flags;
}

static uint16_t encode_rx_can_record(const st_can_rx_frame_t *frame,
                                     can_length_t length,
                                     uint8_t *record)
{
    if ((frame == (const st_can_rx_frame_t *)0) ||
        (record == (uint8_t *)0) || (length > 64U))
    {
        return 0U;
    }

    record[0] = 0U;
    record[1] = rx_frame_flags(frame);
    record[2] = frame->Head.Bits.DLC;
    record[3] = length;
    write_le32(&record[4], frame->Head.Bits.ID);
    write_le32(&record[8], (uint32_t)frame->Head.Bits.TS);
    if (length != 0U)
    {
        memcpy(&record[HOST_LINK_CAN_RECORD_HEADER_SIZE],
               frame->Data.DB, length);
    }
    return (uint16_t)(HOST_LINK_CAN_RECORD_HEADER_SIZE + length);
}

static uint8_t rx_frame_matches(const st_can_rx_frame_t *frame,
                                can_length_t length)
{
    uint8_t flags;

    (void)length;
    flags = rx_frame_flags(frame);
    if ((((frame->Head.Bits.ID ^ g_can_transaction.expected_id) &
          g_can_transaction.expected_id_mask) != 0UL) ||
        (((flags ^ g_can_transaction.expected_flags) &
          g_can_transaction.expected_flags_mask) != 0U))
    {
        return 0U;
    }
    if ((g_can_transaction.expected_dlc != 0xFFU) &&
        (frame->Head.Bits.DLC != g_can_transaction.expected_dlc))
    {
        return 0U;
    }
    return 1U;
}

static void increment_u32(uint32_t *value)
{
    if (*value != 0xFFFFFFFFUL)
    {
        (*value)++;
    }
}

static void add_u32(uint32_t *value, uint16_t amount)
{
    if ((0xFFFFFFFFUL - *value) < (uint32_t)amount)
    {
        *value = 0xFFFFFFFFUL;
    }
    else
    {
        *value += amount;
    }
}

uint16_t HostLink_Crc16CcittFalse(const uint8_t *data, uint16_t length)
{
    uint16_t crc;
    uint16_t index;
    uint8_t bit;

    crc = 0xFFFFU;
    if ((data == (const uint8_t *)0) && (length != 0U))
    {
        return 0U;
    }

    for (index = 0U; index < length; index++)
    {
        crc ^= (uint16_t)((uint16_t)data[index] << 8);
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static void parser_reset(void)
{
    g_parser.count = 0U;
    g_parser.expected_length = 0U;
}

static void parser_resynchronize(void)
{
    uint16_t index;
    uint16_t remaining;

    for (index = 1U; (index + 1U) < g_parser.count; index++)
    {
        if ((g_parser.buffer[index] == HOST_LINK_SOF0) &&
            (g_parser.buffer[index + 1U] == HOST_LINK_SOF1))
        {
            remaining = (uint16_t)(g_parser.count - index);
            memmove(g_parser.buffer, &g_parser.buffer[index], remaining);
            g_parser.count = remaining;
            g_parser.expected_length = 0U;
            return;
        }
    }

    if ((g_parser.count != 0U) &&
        (g_parser.buffer[g_parser.count - 1U] == HOST_LINK_SOF0))
    {
        g_parser.buffer[0] = HOST_LINK_SOF0;
        g_parser.count = 1U;
        g_parser.expected_length = 0U;
    }
    else
    {
        parser_reset();
    }
}

static uint8_t send_response(uint16_t sequence,
                             uint8_t command,
                             uint8_t status,
                             const uint8_t *payload,
                             uint16_t payload_length,
                             uint8_t update_cache,
                             uint16_t request_payload_crc)
{
    uint16_t response_length;
    uint16_t crc;

    if ((payload_length > HOST_LINK_MAX_PAYLOAD) ||
        ((payload == (const uint8_t *)0) && (payload_length != 0U)))
    {
        return 0U;
    }

    response_length = (uint16_t)(HOST_LINK_HEADER_SIZE + payload_length +
                                 HOST_LINK_PAYLOAD_CRC_SIZE);
    g_response[0] = HOST_LINK_SOF0;
    g_response[1] = HOST_LINK_SOF1;
    g_response[2] = HOST_LINK_VERSION;
    g_response[3] = HOST_LINK_TYPE_RESPONSE;
    write_le16(&g_response[4], sequence);
    g_response[6] = command;
    g_response[7] = status;
    write_le16(&g_response[8], payload_length);
    crc = HostLink_Crc16CcittFalse(&g_response[2], 8U);
    write_le16(&g_response[10], crc);

    if (payload_length != 0U)
    {
        memcpy(&g_response[HOST_LINK_HEADER_SIZE], payload, payload_length);
    }
    crc = HostLink_Crc16CcittFalse(payload, payload_length);
    write_le16(&g_response[HOST_LINK_HEADER_SIZE + payload_length], crc);

    if (update_cache != 0U)
    {
        g_replay.valid = 1U;
        g_replay.sequence = sequence;
        g_replay.command = command;
        g_replay.payload_crc = request_payload_crc;
        g_replay.response_length = response_length;
        memcpy(g_replay.response, g_response, response_length);
    }
    if (HostLinkUart_Write(g_response, response_length) == 0U)
    {
        increment_u32(&g_stats.host_command_errors);
        return 0U;
    }
    add_u32(&g_stats.uart_tx_bytes, response_length);
    return 1U;
}

static uint8_t send_event(uint8_t event_id,
                          const uint8_t *payload,
                          uint16_t payload_length)
{
    uint16_t frame_length;
    uint16_t crc;

    if ((payload_length > HOST_LINK_MAX_PAYLOAD) ||
        ((payload == (const uint8_t *)0) && (payload_length != 0U)))
    {
        return 0U;
    }

    frame_length = (uint16_t)(HOST_LINK_HEADER_SIZE + payload_length +
                              HOST_LINK_PAYLOAD_CRC_SIZE);
    if (HostLinkUart_GetTxFree() < frame_length)
    {
        return 0U;
    }

    g_response[0] = HOST_LINK_SOF0;
    g_response[1] = HOST_LINK_SOF1;
    g_response[2] = HOST_LINK_VERSION;
    g_response[3] = HOST_LINK_TYPE_EVENT;
    write_le16(&g_response[4], 0U);
    g_response[6] = event_id;
    g_response[7] = HOST_LINK_STATUS_OK;
    write_le16(&g_response[8], payload_length);
    crc = HostLink_Crc16CcittFalse(&g_response[2], 8U);
    write_le16(&g_response[10], crc);
    if (payload_length != 0U)
    {
        memcpy(&g_response[HOST_LINK_HEADER_SIZE], payload, payload_length);
    }
    crc = HostLink_Crc16CcittFalse(payload, payload_length);
    write_le16(&g_response[HOST_LINK_HEADER_SIZE + payload_length], crc);

    if (HostLinkUart_Write(g_response, frame_length) == 0U)
    {
        increment_u32(&g_stats.host_command_errors);
        return 0U;
    }
    add_u32(&g_stats.uart_tx_bytes, frame_length);
    return 1U;
}

static void encode_error_event(const can_host_error_event_t *event,
                               uint8_t *payload)
{
    write_le16(&payload[0], event->channel_sources);
    write_le16(&payload[2], event->global_sources);
    write_le32(&payload[4], event->controller_state);
    write_le32(&payload[8], event->arbitration_lost_entries);
}

static void capture_can_error_events(void)
{
    can_host_error_event_t event;
    uint16_t non_state_sources;

    if (CAN_HostTakeErrorEvent(&event) == 0U)
    {
        return;
    }

    if ((event.channel_sources & HOST_LINK_CAN_STATE_SOURCE_MASK) != 0U)
    {
        g_pending_state_event = event;
        g_pending_state_event_valid = 1U;
    }
    non_state_sources = (uint16_t)(event.channel_sources &
        (uint16_t)~HOST_LINK_CAN_STATE_SOURCE_MASK);
    if ((non_state_sources != 0U) || (event.global_sources != 0U))
    {
        g_pending_host_error_event = event;
        g_pending_host_error_event_valid = 1U;
    }
}

static void process_can_error_events(void)
{
    uint8_t payload[12];

    capture_can_error_events();
    if (g_pending_state_event_valid != 0U)
    {
        encode_error_event(&g_pending_state_event, payload);
        if (send_event(HOST_LINK_EVENT_CAN_STATE, payload, 12U) != 0U)
        {
            g_pending_state_event_valid = 0U;
        }
        return;
    }
    if (g_pending_host_error_event_valid != 0U)
    {
        encode_error_event(&g_pending_host_error_event, payload);
        if (send_event(HOST_LINK_EVENT_HOST_ERROR, payload, 12U) != 0U)
        {
            g_pending_host_error_event_valid = 0U;
        }
    }
}

static void process_can_rx_event(void)
{
    st_can_rx_frame_t frame;
    can_length_t length;
    uint16_t payload_length;

    if ((g_can_transaction.active != 0U) ||
        (HostLinkUart_GetTxFree() < HOST_LINK_MAX_FRAME_SIZE))
    {
        return;
    }
    if (CAN_RxQueuePop(&frame, &length) == 0U)
    {
        return;
    }
    payload_length = encode_rx_can_record(&frame, length,
                                           g_dispatch_payload);
    if ((payload_length == 0U) ||
        (send_event(HOST_LINK_EVENT_CAN_RX, g_dispatch_payload,
                    payload_length) == 0U))
    {
        increment_u32(&g_stats.host_command_errors);
    }
}

static void clear_statistics(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    g_transaction_high_water=0U;
    g_queue_wait_max_ms=0U; g_transaction_max_ms=0U; g_process_max_ms=0U;
    HostLinkUart_ClearStats();
    CAN_ClearHostStats();
}

static void make_host_info(uint8_t *payload)
{
    write_le16(&payload[0], HOST_LINK_BOARD_ID);
    payload[2] = F24_HOST_FW_MAJOR;
    payload[3] = F24_HOST_FW_MINOR;
    payload[4] = F24_HOST_FW_PATCH;
    payload[5] = HOST_LINK_VERSION;
    payload[6] = HOST_LINK_VERSION;
    payload[7] = 0U;
    write_le32(&payload[8], F24_HOST_FW_BUILD);
    write_le32(&payload[12], HOST_LINK_FEATURES);
}

static void make_capabilities(uint8_t *payload)
{
    write_le32(&payload[0], HOST_LINK_FEATURES);
    write_le16(&payload[4], HOST_LINK_MAX_PAYLOAD);
    payload[6] = 64U;
    payload[7] = (uint8_t)(HOST_LINK_CAN_TRANSACTION_QUEUE_CAPACITY + 1U);
    payload[8] = 1U;
    payload[9] = 4U;
    payload[10] = 1U;
    payload[11] = 0U;
    write_le32(&payload[12], HOST_LINK_NOMINAL_BITRATE_MASK);
    write_le32(&payload[16], HOST_LINK_DATA_BITRATE_MASK);
    write_le32(&payload[20], 0UL);
}

static uint8_t can_result_to_status(can_host_result_t result)
{
    if (result == CAN_HOST_RESULT_OK)
    {
        return HOST_LINK_STATUS_OK;
    }
    if (result == CAN_HOST_RESULT_BAD_ARGUMENT)
    {
        return HOST_LINK_STATUS_BAD_ARGUMENT;
    }
    if (result == CAN_HOST_RESULT_BAD_STATE)
    {
        return HOST_LINK_STATUS_BAD_STATE;
    }
    return HOST_LINK_STATUS_CAN_ERROR;
}


static uint32_t diagnostic_now(void)
{
    return Hl_DiagnosticNow(&g_diagnostic_clock,(uint16_t)get_tick());
}

static void next_burst_frame(void);
static can_host_result_t begin_can_transaction(void)
{
    can_host_result_t result;
    uint32_t wait_ms;

    g_can_transaction.started_tick=diagnostic_now();
    wait_ms=g_can_transaction.started_tick-g_can_transaction.queued_tick;
    if (wait_ms>g_queue_wait_max_ms) g_queue_wait_max_ms=wait_ms;

    CAN_HostDiscardRxQueue();
    if (g_can_transaction.command == CB_COMMAND) next_burst_frame();
    result = CAN_HostTransmit(&g_can_transaction.tx_frame);
    if (result != CAN_HOST_RESULT_OK)
    {
        return result;
    }

    g_can_transaction.active = 1U;
    g_can_transaction.phase = HOST_LINK_CAN_PHASE_TX;
    g_can_transaction.attempts = 1U;
    g_can_transaction.phase_started_tick = (uint16_t)get_tick();
    g_can_transaction.last_tx_result = CAN_TXB_TRANSMITTING;
    LED_SetState(F24_LED_STATE_TRANSFER_ACTIVE);
    return CAN_HOST_RESULT_OK;
}

static uint8_t queued_transaction_matches(uint16_t sequence,
                                          uint8_t command,
                                          uint16_t payload_crc,
                                          uint8_t *sequence_conflict)
{
    uint8_t index;
    uint8_t slot;
    const host_link_can_transaction_t *transaction;

    *sequence_conflict = 0U;
    slot = g_can_transaction_queue_head;
    for (index = 0U; index < g_can_transaction_queue_count; index++)
    {
        transaction = &g_can_transaction_queue[slot];
        if (transaction->sequence == sequence)
        {
            if ((transaction->command == command) &&
                (transaction->request_payload_crc == payload_crc))
            {
                return 1U;
            }
            *sequence_conflict = 1U;
            return 0U;
        }
        slot++;
        if (slot >= HOST_LINK_CAN_TRANSACTION_QUEUE_CAPACITY)
        {
            slot = 0U;
        }
    }
    return 0U;
}

static uint8_t schedule_can_transaction(
    const host_link_can_transaction_t *transaction)
{
    can_host_result_t can_result;

    if (g_can_transaction.active != 0U)
    {
        if (g_can_transaction_queue_count >=
            HOST_LINK_CAN_TRANSACTION_QUEUE_CAPACITY)
        {
            return HOST_LINK_STATUS_OVERFLOW;
        }
        memcpy(&g_can_transaction_queue[g_can_transaction_queue_tail],
               transaction, sizeof(*transaction));
        g_can_transaction_queue_tail++;
        if (g_can_transaction_queue_tail >=
            HOST_LINK_CAN_TRANSACTION_QUEUE_CAPACITY)
        {
            g_can_transaction_queue_tail = 0U;
        }
        g_can_transaction_queue_count++;
        g_can_transaction_queue[(g_can_transaction_queue_tail==0U) ?
            HOST_LINK_CAN_TRANSACTION_QUEUE_CAPACITY-1U : g_can_transaction_queue_tail-1U].queued_tick=diagnostic_now();
        if (g_can_transaction_queue_count>g_transaction_high_water) g_transaction_high_water=g_can_transaction_queue_count;
        return HOST_LINK_STATUS_OK;
    }

    memcpy(&g_can_transaction, transaction, sizeof(g_can_transaction));
    g_can_transaction.queued_tick=diagnostic_now();
    can_result = begin_can_transaction();
    return can_result_to_status(can_result);
}

static void start_next_can_transaction(void)
{
    can_host_result_t can_result;
    uint8_t status;

    while ((g_can_transaction.active == 0U) &&
           (g_can_transaction_queue_count != 0U))
    {
        memcpy(&g_can_transaction,
               &g_can_transaction_queue[g_can_transaction_queue_head],
               sizeof(g_can_transaction));
        g_can_transaction_queue_head++;
        if (g_can_transaction_queue_head >=
            HOST_LINK_CAN_TRANSACTION_QUEUE_CAPACITY)
        {
            g_can_transaction_queue_head = 0U;
        }
        g_can_transaction_queue_count--;
        can_result = begin_can_transaction();
        if (can_result == CAN_HOST_RESULT_OK)
        {
            return;
        }

        status = can_result_to_status(can_result);
        increment_u32(&g_stats.host_command_errors);
        send_response(g_can_transaction.sequence,
                      g_can_transaction.command,
                      status, (const uint8_t *)0, 0U, 1U,
                      g_can_transaction.request_payload_crc);
        g_can_transaction.active = 0U;
        LED_SetState(F24_LED_STATE_ERROR);
    }
}

static void finish_can_transaction(uint8_t status,
                                   const uint8_t *payload,
                                   uint16_t payload_length)
{
    uint32_t duration;
    duration=diagnostic_now()-g_can_transaction.started_tick;
    if (duration>g_transaction_max_ms) g_transaction_max_ms=duration;
    if (status != HOST_LINK_STATUS_OK)
    {
        increment_u32(&g_stats.host_command_errors);
    }
    send_response(g_can_transaction.sequence,
                  g_can_transaction.command,
                  status,
                  payload,
                  payload_length,
                  1U,
                  g_can_transaction.request_payload_crc);
    g_can_transaction.active = 0U;
    if (status == HOST_LINK_STATUS_OK)
    {
        LED_SetState(F24_LED_STATE_CONNECTED);
    }
    else
    {
        LED_SetState(F24_LED_STATE_ERROR);
    }
    start_next_can_transaction();
}

static void finish_can_timeout(void)
{
    g_transaction_response[0] = g_can_transaction.attempts;
    g_transaction_response[1] = 0U;
    g_transaction_response[2] = 0U;
    g_transaction_response[3] = 0U;
    write_le32(&g_transaction_response[4],
               (uint32_t)(int32_t)g_can_transaction.last_tx_result);
    finish_can_transaction(HOST_LINK_STATUS_TIMEOUT,
                           g_transaction_response, 8U);
}

#include "host_link_can_process.inc"

static void make_statistics(uint8_t *payload)
{
    host_link_uart_stats_t uart_stats;
    can_host_stats_t can_stats;
    const uint32_t *values;
    uint8_t index;

    HostLinkUart_GetStats(&uart_stats);
    CAN_GetHostStats(&can_stats);
    g_stats.uart_rx_overflows = uart_stats.rx_overflow_count;
    g_stats.can_rx_frames = can_stats.rx_frames;
    g_stats.can_tx_frames = can_stats.tx_frames;
    g_stats.can_rx_lost_frames = can_stats.rx_lost_frames;
    g_stats.can_tx_failures = can_stats.tx_failures;
    g_stats.can_warning_entries = can_stats.warning_entries;
    g_stats.can_error_passive_entries = can_stats.error_passive_entries;
    g_stats.can_bus_off_entries = can_stats.bus_off_entries;
    g_stats.can_recovery_count = can_stats.recovery_count;
    values = (const uint32_t *)&g_stats;
    for (index = 0U; index < 14U; index++)
    {
        write_le32(&payload[(uint16_t)index * 4U], values[index]);
    }
    write_le32(payload+56U,uart_stats.rx_high_water);
    write_le32(payload+60U,uart_stats.tx_high_water);
    write_le32(payload+64U,CAN_HostGetRxHighWater());
    write_le32(payload+68U,g_transaction_high_water);
    write_le32(payload+72U,g_queue_wait_max_ms);
    write_le32(payload+76U,g_transaction_max_ms);
    write_le32(payload+80U,g_process_max_ms);
}

static void dispatch_request(void)
{
    uint16_t sequence;
    uint8_t command;
    uint16_t payload_length;
    uint16_t payload_crc;
    const uint8_t *payload;
    uint8_t *response_payload;
    uint8_t status;
    uint16_t response_payload_length;
    uint8_t clear_after_response;
    uint8_t defer_response;
    uint8_t reset_after_response;
    can_host_config_t can_config;
    can_host_filter_t can_filter;
    can_host_result_t can_result;
    uint32_t controller_state;
    st_can_tx_frame_t tx_frame;
    uint16_t response_timeout;
    uint8_t retry_limit;
    uint8_t expected_dlc;
    uint32_t expected_id;
    uint32_t expected_id_mask;
    uint8_t expected_flags;
    uint8_t expected_flags_mask;
    uint32_t maximum_id;
    uint8_t queued_sequence_conflict;
    host_link_can_transaction_t pending_transaction;

    sequence = read_le16(&g_parser.buffer[4]);
    command = g_parser.buffer[6];
    payload_length = read_le16(&g_parser.buffer[8]);
    payload = &g_parser.buffer[HOST_LINK_HEADER_SIZE];
    payload_crc = HostLink_Crc16CcittFalse(payload, payload_length);
    response_payload = g_dispatch_payload;

    if (g_i2c_request_active != 0U)
    {
        if (sequence == g_i2c_sequence)
        {
            if ((command == HOST_LINK_CMD_I2C_TRANSACTION) &&
                (payload_crc == g_i2c_payload_crc)) return;
            send_response(sequence, command, HOST_LINK_STATUS_SEQUENCE_ERROR,
                          (const uint8_t *)0, 0U, 0U, payload_crc);
            return;
        }
        if ((command == HOST_LINK_CMD_I2C_TRANSACTION) &&
            (payload_length == 1U) && (payload[0] == 4U))
        {
            Bridge_I2cAbort();
            send_response(g_i2c_sequence, HOST_LINK_CMD_I2C_TRANSACTION,
                          HOST_LINK_STATUS_BAD_STATE, (const uint8_t *)0,
                          0U, 0U, g_i2c_payload_crc);
            g_i2c_request_active = 0U;
            g_i2c_result_waiting = 0U;
        }
        else
        {
            send_response(sequence, command, HOST_LINK_STATUS_BUSY,
                          (const uint8_t *)0, 0U, 0U, payload_crc);
            return;
        }
    }

    if (g_can_transaction.active != 0U)
    {
        if ((sequence == g_can_transaction.sequence) &&
            (command == g_can_transaction.command) &&
            (payload_crc == g_can_transaction.request_payload_crc))
        {
            return;
        }
        if (queued_transaction_matches(sequence, command, payload_crc,
                                       &queued_sequence_conflict) != 0U)
        {
            return;
        }
        if ((sequence == 0U) ||
            (sequence == g_can_transaction.sequence) ||
            (queued_sequence_conflict != 0U) ||
            ((command != HOST_LINK_CMD_CAN_TX) &&
             (command != HOST_LINK_CMD_CAN_TX_WAIT_RX) && (command != CB_COMMAND)))
        {
            increment_u32(&g_stats.host_command_errors);
            send_response(sequence, command,
                          (sequence == 0U) ?
                              HOST_LINK_STATUS_BAD_ARGUMENT :
                          (((sequence == g_can_transaction.sequence) ||
                            (queued_sequence_conflict != 0U)) ?
                              HOST_LINK_STATUS_SEQUENCE_ERROR :
                              HOST_LINK_STATUS_BUSY),
                          (const uint8_t *)0, 0U, 0U, payload_crc);
            return;
        }
    }

    if ((g_replay.valid != 0U) && (sequence == g_replay.sequence))
    {
        if ((command == g_replay.command) &&
            (payload_crc == g_replay.payload_crc))
        {
            if (HostLinkUart_Write(g_replay.response,
                                   g_replay.response_length) != 0U)
            {
                if (command == HL_BAUD_SET && g_replay.response[7] == 0U)
                    HostLinkUart_ArmBaud();
                add_u32(&g_stats.uart_tx_bytes, g_replay.response_length);
            }
            else
            {
                increment_u32(&g_stats.host_command_errors);
            }
        }
        else
        {
            increment_u32(&g_stats.host_command_errors);
            send_response(sequence, command, HOST_LINK_STATUS_SEQUENCE_ERROR,
                          (const uint8_t *)0, 0U, 0U, payload_crc);
        }
        return;
    }

    LED_SetState(F24_LED_STATE_CONNECTED);

    status = HOST_LINK_STATUS_OK;
    response_payload_length = 0U;
    clear_after_response = 0U;
    defer_response = 0U;
    reset_after_response = 0U;

    if ((Bridge_I2cReserved() != 0U) &&
        (command >= HOST_LINK_CMD_SET_CAN_CONFIG) &&
        (command <= CB_COMMAND))
    {
        send_response(sequence, command, HOST_LINK_STATUS_BUSY,
                      (const uint8_t *)0, 0U, 1U, payload_crc);
        return;
    }

    if (sequence == 0U)
    {
        status = HOST_LINK_STATUS_BAD_ARGUMENT;
    }
    else if (HostLinkUart_BaudPending() && command != HL_BAUD_INFO && command != HL_BAUD_CONFIRM)
    {
        status = HOST_LINK_STATUS_BUSY;
    }
    else
    {
        switch (command)
        {
            case HL_BAUD_INFO:
                if (payload_length) status = HOST_LINK_STATUS_BAD_ARGUMENT;
                else
                {
                    uint32_t nominal, actual;
                    HostLinkUart_BaudInfo(&nominal, &actual);
                    memset(response_payload, 0, HL_BAUD_INFO_SIZE);
                    response_payload[0] = 1U;
                    response_payload[1] = 1U;
                    write_le32(response_payload + 4, HL_BAUD_DEFAULT);
                    write_le32(response_payload + 8, nominal);
                    write_le32(response_payload + 12, HL_BAUD_MASK);
                    write_le32(response_payload + 16, actual);
                    response_payload_length = HL_BAUD_INFO_SIZE;
                }
                break;
            case HL_BAUD_SET:
                if (payload_length != 4U) status = HOST_LINK_STATUS_BAD_ARGUMENT;
                else if (CAN_HostIsRunning() || Bridge_I2cReserved()) status = HOST_LINK_STATUS_BUSY;
                else
                {
                    status = HostLinkUart_PrepareBaud(read_le32(payload));
                    if (!status) { memcpy(response_payload, payload, 4U); response_payload_length = 4U; }
                }
                break;
            case HL_BAUD_CONFIRM:
                if (payload_length != 4U) status = HOST_LINK_STATUS_BAD_ARGUMENT;
                else
                {
                    status = HostLinkUart_ConfirmBaud(read_le32(payload));
                    if (!status) { memcpy(response_payload, payload, 4U); response_payload_length = 4U; }
                }
                break;
            case HOST_LINK_CMD_GET_PORT_INFO:
                if ((payload_length != 2U) || (payload[0] > 1U) ||
                    (payload[1] != 0U))
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    memset(response_payload, 0, 32U);
                    response_payload[0] = payload[0];
                    response_payload[1] = 1U;
                    response_payload[2] = 0U;
                    response_payload[3] = (payload[0] == 0U) ? 5U : 3U;
                    write_le32(&response_payload[4], (payload[0] == 0U) ?
                               HOST_LINK_NOMINAL_BITRATE_MASK : 1UL);
                    write_le16(&response_payload[8], (payload[0] == 0U) ?
                               64U : BRIDGE_I2C_LIMIT);
                    write_le16(&response_payload[10], (payload[0] == 0U) ?
                               64U : BRIDGE_I2C_LIMIT);
                    memcpy(&response_payload[12], (payload[0] == 0U) ?
                           "CAN0 P10/P11" : "IICA0 SCL62/SDA63",
                           (payload[0] == 0U) ? 12U : 17U);
                    response_payload_length = 32U;
                }
                break;

            case HOST_LINK_CMD_SELECT_CAN_PORT:
                if ((payload_length != 1U) || (payload[0] != 0U))
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                else if (CAN_HostIsRunning() != 0U)
                    status = HOST_LINK_STATUS_BAD_STATE;
                else
                {
                    response_payload[0] = 0U;
                    response_payload_length = 1U;
                }
                break;

            case HOST_LINK_CMD_SET_I2C_CONFIG:
                if ((payload_length != 4U) || (payload[2] != 0U) ||
                    (payload[3] != 0U))
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                else if (CAN_HostIsRunning() != 0U)
                    status = HOST_LINK_STATUS_BAD_STATE;
                else
                {
                    status = Bridge_I2cConfigure(payload[0], payload[1]);
                    if (status == 0U)
                    {
                        memcpy(response_payload, payload, 4U);
                        response_payload_length = 4U;
                    }
                }
                break;

            case HOST_LINK_CMD_I2C_TRANSACTION:
                if (CAN_HostIsRunning() != 0U)
                    status = HOST_LINK_STATUS_BAD_STATE;
                else
                {
                    status = Bridge_I2cCommand(payload, payload_length,
                        response_payload, &response_payload_length, get_tick());
                    if (status == BRIDGE_I2C_PENDING)
                    {
                        g_i2c_request_active = 1U;
                        g_i2c_sequence = sequence;
                        g_i2c_payload_crc = payload_crc;
                        g_i2c_result_waiting = 0U;
                        LED_SetState(F24_LED_STATE_TRANSFER_ACTIVE);
                        status = 0U;
                        defer_response = 1U;
                    }
                }
                break;

#ifdef BRIDGE_SPLIT_APP
            case BU_INFO_COMMAND:
            case BU_ENTER_COMMAND:
            case BU_READ_METADATA_COMMAND:
            case BU_DIAGNOSTICS_COMMAND:
                F24Bridge_AppCommand(command, payload, payload_length,
                    response_payload, &response_payload_length, &status);
                break;
#endif
            case HOST_LINK_CMD_PING:
                if (payload_length > HOST_LINK_MAX_PAYLOAD)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    memcpy(response_payload, payload, payload_length);
                    response_payload_length = payload_length;
                }
                break;

            case HOST_LINK_CMD_GET_HOST_INFO:
                if (payload_length != 0U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    make_host_info(response_payload);
                    response_payload_length = 16U;
                }
                break;

            case HOST_LINK_CMD_GET_CAPABILITIES:
                if (payload_length != 0U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    make_capabilities(response_payload);
                    response_payload_length = 24U;
                }
                break;

            case HOST_LINK_CMD_RESET_HOST:
                if (payload_length != 0U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    Bridge_I2cAbort();
                    reset_after_response = 1U;
                }
                break;

            case HOST_LINK_CMD_SET_CAN_CONFIG:
                if (payload_length != 4U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    can_config.mode = payload[0];
                    can_config.nominal_bitrate_preset = payload[1];
                    can_config.data_bitrate_preset = payload[2];
                    can_config.flags = payload[3];
                    can_result = CAN_HostSetConfig(&can_config);
                    status = can_result_to_status(can_result);
                    if (status == HOST_LINK_STATUS_OK)
                    {
                        memcpy(response_payload, payload, payload_length);
                        response_payload_length = payload_length;
                    }
                    else if (F24_CanClockIsReady() == 0U)
                    {
                        status = HOST_LINK_STATUS_CAN_ERROR;
                        F24_CanClockGetDiagnostic(response_payload);
                        response_payload_length = 8U;
                    }
                }
                break;

            case HOST_LINK_CMD_SET_CAN_FILTER:
                if (payload_length != 12U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    can_filter.enabled = payload[1];
                    can_filter.ide = payload[2];
                    can_filter.frame_type = payload[3];
                    can_filter.id = read_le32(&payload[4]);
                    can_filter.mask = read_le32(&payload[8]);
                    can_result = CAN_HostSetFilter(payload[0], &can_filter);
                    status = can_result_to_status(can_result);
                    if (status == HOST_LINK_STATUS_OK)
                    {
                        memcpy(response_payload, payload, payload_length);
                        response_payload_length = payload_length;
                    }
                }
                break;

            case HOST_LINK_CMD_CAN_START:
                if (payload_length != 0U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    controller_state = 0UL;
                    can_result = CAN_HostStart(&controller_state);
                    status = can_result_to_status(can_result);
                    if (status == HOST_LINK_STATUS_OK)
                    {
                        write_le32(response_payload, controller_state);
                        response_payload_length = 4U;
                    }
                }
                break;

            case HOST_LINK_CMD_CAN_STOP:
                if (payload_length != 0U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    controller_state = 0UL;
                    can_result = CAN_HostStop(&controller_state);
                    status = can_result_to_status(can_result);
                    if (status == HOST_LINK_STATUS_OK)
                    {
                        write_le32(response_payload, controller_state);
                        response_payload_length = 4U;
                    }
                }
                break;

            case HOST_LINK_CMD_CAN_TX:
                if (decode_tx_can_record(payload, payload_length,
                                         &tx_frame) == 0U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    memset(&pending_transaction, 0,
                           sizeof(pending_transaction));
                    pending_transaction.command = command;
                    pending_transaction.sequence = sequence;
                    pending_transaction.request_payload_crc = payload_crc;
                    pending_transaction.timeout_ms =
                        HOST_LINK_CAN_TX_TIMEOUT_MS;
                    memcpy(&pending_transaction.tx_frame, &tx_frame,
                           sizeof(tx_frame));
                    status = schedule_can_transaction(&pending_transaction);
                    if (status == HOST_LINK_STATUS_OK)
                    {
                        defer_response = 1U;
                    }
                }
                break;

            case CB_COMMAND:
                memset(&pending_transaction, 0, sizeof(pending_transaction));
                if (!cb_decode(&pending_transaction.burst, payload, payload_length))
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                    break;
                }
                pending_transaction.command = command;
                pending_transaction.sequence = sequence;
                pending_transaction.request_payload_crc = payload_crc;
                pending_transaction.timeout_ms = cb_le16(payload+2);
                pending_transaction.expected_id = cb_le16(payload+4);
                pending_transaction.expected_id_mask = cb_le16(payload+6);
                pending_transaction.expected_dlc = payload[8];
                pending_transaction.expected_flags_mask = 0x0FU;
                status = schedule_can_transaction(&pending_transaction);
                if (status == HOST_LINK_STATUS_OK) defer_response = 1U;
                break;

            case HOST_LINK_CMD_CAN_TX_WAIT_RX:
                if (payload_length <
                    (16U + HOST_LINK_CAN_RECORD_HEADER_SIZE))
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                    break;
                }

                response_timeout = read_le16(&payload[0]);
                retry_limit = payload[2];
                expected_dlc = payload[3];
                expected_id = read_le32(&payload[4]);
                expected_id_mask = read_le32(&payload[8]);
                expected_flags = payload[12];
                expected_flags_mask = payload[13];
                maximum_id = ((expected_flags & HOST_LINK_CAN_FLAG_IDE) != 0U) ?
                             0x1FFFFFFFUL : 0x7FFUL;
                if ((response_timeout == 0U) ||
                    (response_timeout > 30000U) ||
                    (retry_limit > 3U) ||
                    ((expected_dlc != 0xFFU) && (expected_dlc > 15U)) ||
                    (expected_id_mask == 0UL) ||
                    (expected_id > maximum_id) ||
                    (expected_id_mask > maximum_id) ||
                    ((expected_flags &
                      (uint8_t)~HOST_LINK_CAN_FLAG_MASK) != 0U) ||
                    ((expected_flags_mask &
                      (uint8_t)~HOST_LINK_CAN_FLAG_MASK) != 0U) ||
                    (payload[14] != 0U) || (payload[15] != 0U) ||
                    (decode_tx_can_record(&payload[16],
                        (uint16_t)(payload_length - 16U), &tx_frame) == 0U))
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    memset(&pending_transaction, 0,
                           sizeof(pending_transaction));
                    pending_transaction.command = command;
                    pending_transaction.sequence = sequence;
                    pending_transaction.request_payload_crc = payload_crc;
                    pending_transaction.timeout_ms = response_timeout;
                    pending_transaction.retry_limit = retry_limit;
                    pending_transaction.expected_dlc = expected_dlc;
                    pending_transaction.expected_id = expected_id;
                    pending_transaction.expected_id_mask = expected_id_mask;
                    pending_transaction.expected_flags = expected_flags;
                    pending_transaction.expected_flags_mask =
                        expected_flags_mask;
                    memcpy(&pending_transaction.tx_frame, &tx_frame,
                           sizeof(tx_frame));
                    status = schedule_can_transaction(&pending_transaction);
                    if (status == HOST_LINK_STATUS_OK)
                    {
                        defer_response = 1U;
                    }
                }
                break;

            case HOST_LINK_CMD_GET_STATS:
                if (payload_length != 0U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    make_statistics(response_payload);
                    response_payload_length = 84U;
                }
                break;

            case HOST_LINK_CMD_CLEAR_STATS:
                if (payload_length != 0U)
                {
                    status = HOST_LINK_STATUS_BAD_ARGUMENT;
                }
                else
                {
                    clear_after_response = 1U;
                }
                break;

            default:
                status = HOST_LINK_STATUS_BAD_COMMAND;
                break;
        }
    }

    if (status != HOST_LINK_STATUS_OK)
    {
        increment_u32(&g_stats.host_command_errors);
        if (status == HOST_LINK_STATUS_CAN_ERROR)
        {
            LED_SetState(F24_LED_STATE_ERROR);
        }
    }
    if (defer_response != 0U)
    {
        return;
    }
    if (send_response(sequence, command, status, response_payload,
                      response_payload_length, 1U, payload_crc) != 0U)
    {
        if (command == HL_BAUD_SET && status == HOST_LINK_STATUS_OK)
            HostLinkUart_ArmBaud();
#ifdef BRIDGE_SPLIT_APP
        if (command == BU_ENTER_COMMAND && status == HOST_LINK_STATUS_OK)
            F24Bridge_AppResponseSent();
#endif
        if (clear_after_response != 0U)
        {
            clear_statistics();
        }
        if ((reset_after_response != 0U) &&
            (status == HOST_LINK_STATUS_OK))
        {
            g_reset_pending = 1U;
        }
    }
}

static uint8_t validate_header(void)
{
    uint16_t received_crc;
    uint16_t calculated_crc;
    uint16_t payload_length;

    received_crc = read_le16(&g_parser.buffer[10]);
    calculated_crc = HostLink_Crc16CcittFalse(&g_parser.buffer[2], 8U);
    if (received_crc != calculated_crc)
    {
        increment_u32(&g_stats.uart_crc_errors);
        return 0U;
    }
    if ((g_parser.buffer[2] != HOST_LINK_VERSION) ||
        (g_parser.buffer[3] != HOST_LINK_TYPE_REQUEST) ||
        (g_parser.buffer[7] != HOST_LINK_STATUS_OK))
    {
        increment_u32(&g_stats.uart_invalid_errors);
        return 0U;
    }

    payload_length = read_le16(&g_parser.buffer[8]);
    if (payload_length > HOST_LINK_MAX_PAYLOAD)
    {
        increment_u32(&g_stats.uart_invalid_errors);
        return 0U;
    }
    g_parser.expected_length = (uint16_t)(HOST_LINK_HEADER_SIZE +
                                          payload_length +
                                          HOST_LINK_PAYLOAD_CRC_SIZE);
    return 1U;
}

static void evaluate_parser(void)
{
    uint16_t payload_length;
    uint16_t received_crc;
    uint16_t calculated_crc;
    uint16_t sequence;
    uint8_t command;

    while ((g_parser.count >= HOST_LINK_HEADER_SIZE) &&
           (g_parser.expected_length == 0U))
    {
        if (validate_header() != 0U)
        {
            break;
        }
        parser_resynchronize();
    }

    if ((g_parser.expected_length == 0U) ||
        (g_parser.count < g_parser.expected_length))
    {
        return;
    }

    payload_length = read_le16(&g_parser.buffer[8]);
    received_crc = read_le16(&g_parser.buffer[HOST_LINK_HEADER_SIZE +
                                                payload_length]);
    calculated_crc = HostLink_Crc16CcittFalse(
        &g_parser.buffer[HOST_LINK_HEADER_SIZE], payload_length);
    if (received_crc != calculated_crc)
    {
        increment_u32(&g_stats.uart_crc_errors);
        sequence = read_le16(&g_parser.buffer[4]);
        command = g_parser.buffer[6];
        send_response(sequence, command, HOST_LINK_STATUS_CRC_ERROR,
                      (const uint8_t *)0, 0U, 0U, calculated_crc);
        parser_resynchronize();
        evaluate_parser();
        return;
    }

    dispatch_request();
    parser_reset();
}

static void consume_byte(uint8_t data)
{
    if (g_parser.count == 0U)
    {
        if (data == HOST_LINK_SOF0)
        {
            g_parser.buffer[0] = data;
            g_parser.count = 1U;
        }
        return;
    }

    if (g_parser.count == 1U)
    {
        if (data == HOST_LINK_SOF1)
        {
            g_parser.buffer[1] = data;
            g_parser.count = 2U;
        }
        else if (data != HOST_LINK_SOF0)
        {
            parser_reset();
        }
        return;
    }

    if (g_parser.count >= HOST_LINK_MAX_FRAME_SIZE)
    {
        increment_u32(&g_stats.uart_invalid_errors);
        parser_resynchronize();
        return;
    }

    g_parser.buffer[g_parser.count] = data;
    g_parser.count++;
    evaluate_parser();
}

void HostLink_Init(void)
{
    memset(&g_parser, 0, sizeof(g_parser));
    memset(&g_stats, 0, sizeof(g_stats));
    memset(&g_replay, 0, sizeof(g_replay));
    memset(&g_can_transaction, 0, sizeof(g_can_transaction));
    memset(g_can_transaction_queue, 0, sizeof(g_can_transaction_queue));
    g_can_transaction_queue_head = 0U;
    g_can_transaction_queue_tail = 0U;
    g_can_transaction_queue_count = 0U;
    memset(&g_pending_state_event, 0, sizeof(g_pending_state_event));
    memset(&g_pending_host_error_event, 0,
           sizeof(g_pending_host_error_event));
    g_pending_state_event_valid = 0U;
    g_pending_host_error_event_valid = 0U;
    g_reset_pending = 0U;
    g_i2c_request_active = 0U;
    g_i2c_result_waiting = 0U;
    Bridge_I2cInit();
}

static void process_i2c_transaction(void)
{
    uint8_t payload[2];

    Bridge_I2cPoll(get_tick());
    if (g_i2c_request_active == 0U) return;
    if (g_i2c_result_waiting == 0U)
    {
        if (Bridge_I2cTakeResult(&g_i2c_result_status,
                                 &g_i2c_result_received) == 0U) return;
        g_i2c_result_waiting = 1U;
    }
    write_le16(payload, g_i2c_result_received);
    if (send_response(g_i2c_sequence, HOST_LINK_CMD_I2C_TRANSACTION,
                      g_i2c_result_status, payload, 2U, 1U,
                      g_i2c_payload_crc) != 0U)
    {
        g_i2c_request_active = 0U;
        g_i2c_result_waiting = 0U;
        LED_SetState((g_i2c_result_status == 0U) ?
                     F24_LED_STATE_CONNECTED : F24_LED_STATE_ERROR);
    }
}

static void host_link_process_once(void)
{
    uint8_t bytes[HOST_LINK_READ_CHUNK];
    uint16_t count;
    uint16_t index;
    uint16_t processed;

    if (HostLinkUart_TakeRateChange())
    {
        parser_reset();
        memset(&g_replay, 0, sizeof(g_replay));
    }

#ifdef BRIDGE_SPLIT_APP
    F24Bridge_AppProcess();
#endif

    if (g_reset_pending != 0U)
    {
        if (HostLinkUart_IsTxIdle() != 0U)
        {
            RL78_soft_reset(1U);
        }
        return;
    }

    process_can_transaction();
    process_i2c_transaction();
    process_can_error_events();
    processed = 0U;
    while (processed < HOST_LINK_PROCESS_BUDGET)
    {
        count = HostLinkUart_Read(bytes, HOST_LINK_READ_CHUNK);
        if (count == 0U)
        {
            break;
        }
        for (index = 0U; index < count; index++)
        {
            increment_u32(&g_stats.uart_rx_bytes);
            consume_byte(bytes[index]);
        }
        processed = (uint16_t)(processed + count);
    }
    process_can_transaction();
    process_i2c_transaction();
    process_can_error_events();
    process_can_rx_event();
}

void HostLink_Process(void)
{
    uint32_t started, duration;
    started=diagnostic_now();
    host_link_process_once();
    duration=diagnostic_now()-started;
    if (duration>g_process_max_ms) g_process_max_ms=duration;
}
