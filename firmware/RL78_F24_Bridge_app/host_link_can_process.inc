/* Nonblocking CAN progression, compiled by firmware and HAL fault tests. */
static void next_burst_frame(void)
{
    uint16_t id;
    uint8_t dlc;
    st_can_tx_frame_t *frame;
    frame = &g_can_transaction.tx_frame;
    memset(frame, 0, sizeof(*frame));
    (void)cb_next(&g_can_transaction.burst, &id, &dlc, frame->Data.DB);
    frame->Head.Bits.ID = id;
    frame->Head.Bits.DLC = dlc;
    frame->Head.Bits.FDCTR = CAN_FDCTR_CLASSICAL;
}

static void process_can_transaction(void)
{
    st_can_rx_frame_t rx_frame;
    can_length_t rx_length;
    e_can_txb_result_t tx_result;
    uint16_t tx_timestamp;
    uint16_t now;
    uint16_t timeout;
    uint16_t response_length;
    can_host_result_t can_result;
    uint32_t controller_state;
    can_host_tx_diagnostic_t tx_diagnostic;

    if (g_can_transaction.active == 0U)
    {
        return;
    }

    if (((g_can_transaction.command == HOST_LINK_CMD_CAN_TX_WAIT_RX) ||
         (g_can_transaction.command == CB_COMMAND && !g_can_transaction.burst.remaining)) &&
        (g_can_transaction.phase != HOST_LINK_CAN_PHASE_ABORT) &&
        (g_can_transaction.rx_matched == 0U))
    {
        while ((HostLinkUart_GetTxFree() >= HOST_LINK_MAX_FRAME_SIZE) &&
               (CAN_RxQueuePop(&rx_frame, &rx_length) != 0U))
        {
            if (rx_frame_matches(&rx_frame, rx_length) != 0U)
            {
                g_can_transaction.rx_frame = rx_frame;
                g_can_transaction.rx_length = rx_length;
                g_can_transaction.rx_matched = 1U;
                break;
            }
            response_length = encode_rx_can_record(&rx_frame, rx_length,
                                                    g_dispatch_payload);
            if ((response_length == 0U) ||
                (send_event(HOST_LINK_EVENT_CAN_RX, g_dispatch_payload,
                            response_length) == 0U))
            {
                increment_u32(&g_stats.host_command_errors);
            }
        }
    }

    if ((g_can_transaction.phase == HOST_LINK_CAN_PHASE_TX) ||
        (g_can_transaction.phase == HOST_LINK_CAN_PHASE_ABORT))
    {
        if (CAN_HostPollTransmit(&tx_result, &tx_timestamp) != 0U)
        {
            g_can_transaction.last_tx_result = tx_result;
            g_can_transaction.tx_timestamp = tx_timestamp;
            if (g_can_transaction.phase == HOST_LINK_CAN_PHASE_ABORT)
            {
                finish_can_timeout();
                return;
            }
            if (tx_result != CAN_TXB_END)
            {
                write_le32(g_transaction_response,
                           (uint32_t)(int32_t)tx_result);
                finish_can_transaction(HOST_LINK_STATUS_CAN_ERROR,
                                       g_transaction_response, 4U);
                return;
            }
            if (g_can_transaction.command == CB_COMMAND && g_can_transaction.burst.remaining)
            {
                g_can_transaction.phase = HOST_LINK_CAN_PHASE_GAP;
                g_can_transaction.phase_started_tick = (uint16_t)get_tick();
                return;
            }
            if (g_can_transaction.command == HOST_LINK_CMD_CAN_TX)
            {
                write_le32(&g_transaction_response[0],
                           (uint32_t)tx_timestamp);
                write_le32(&g_transaction_response[4],
                           (uint32_t)(int32_t)tx_result);
                CAN_HostGetLastTxDiagnostic(&tx_diagnostic);
                memcpy(&g_transaction_response[8],
                       tx_diagnostic.input_head, 8U);
                memcpy(&g_transaction_response[16],
                       tx_diagnostic.input_data, 8U);
                write_le16(&g_transaction_response[24],
                           tx_diagnostic.ram_idl);
                write_le16(&g_transaction_response[26],
                           tx_diagnostic.ram_idh);
                g_transaction_response[28] =
                    tx_diagnostic.ram_dlc_register;
                g_transaction_response[29] =
                    tx_diagnostic.ram_fd_control;
                memcpy(&g_transaction_response[30],
                       tx_diagnostic.ram_data, 8U);
                write_le16(&g_transaction_response[38],
                           tx_diagnostic.ram_window_before);
                finish_can_transaction(HOST_LINK_STATUS_OK,
                                       g_transaction_response, 40U);
                return;
            }
            if (g_can_transaction.rx_matched != 0U)
            {
                response_length = encode_rx_can_record(
                    &g_can_transaction.rx_frame,
                    g_can_transaction.rx_length,
                    g_transaction_response);
                if (response_length == 0U)
                {
                    finish_can_transaction(HOST_LINK_STATUS_CAN_ERROR,
                                           (const uint8_t *)0, 0U);
                }
                else
                {
                    finish_can_transaction(HOST_LINK_STATUS_OK,
                                           g_transaction_response,
                                           response_length);
                }
                return;
            }
            g_can_transaction.phase = HOST_LINK_CAN_PHASE_RX;
            g_can_transaction.phase_started_tick = (uint16_t)get_tick();
        }
    }

    if ((g_can_transaction.phase == HOST_LINK_CAN_PHASE_RX) &&
        (g_can_transaction.rx_matched != 0U))
    {
        response_length = encode_rx_can_record(
            &g_can_transaction.rx_frame,
            g_can_transaction.rx_length,
            g_transaction_response);
        if (response_length == 0U)
        {
            finish_can_transaction(HOST_LINK_STATUS_CAN_ERROR,
                                   (const uint8_t *)0, 0U);
        }
        else
        {
            finish_can_transaction(HOST_LINK_STATUS_OK,
                                   g_transaction_response,
                                   response_length);
        }
        return;
    }

    now = (uint16_t)get_tick();
    if (g_can_transaction.phase == HOST_LINK_CAN_PHASE_GAP)
    {
        if (elapsed_tick_ms(now, g_can_transaction.phase_started_tick) <
            cb_le16(g_can_transaction.burst.payload + 10)) return;
        next_burst_frame();
        can_result = CAN_HostTransmit(&g_can_transaction.tx_frame);
        if (can_result != CAN_HOST_RESULT_OK)
        {
            finish_can_transaction(can_result_to_status(can_result), (const uint8_t *)0, 0U);
            return;
        }
        g_can_transaction.phase = HOST_LINK_CAN_PHASE_TX;
        g_can_transaction.phase_started_tick = now;
        return;
    }
    timeout = (g_can_transaction.phase == HOST_LINK_CAN_PHASE_ABORT) ?
              HOST_LINK_CAN_ABORT_TIMEOUT_MS :
              g_can_transaction.timeout_ms;
    if (elapsed_tick_ms(now, g_can_transaction.phase_started_tick) < timeout)
    {
        return;
    }

    if (g_can_transaction.phase == HOST_LINK_CAN_PHASE_TX)
    {
        can_result = CAN_HostAbortTransmit();
        if (can_result != CAN_HOST_RESULT_OK)
        {
            finish_can_transaction(can_result_to_status(can_result),
                                   (const uint8_t *)0, 0U);
            return;
        }
        g_can_transaction.phase = HOST_LINK_CAN_PHASE_ABORT;
        g_can_transaction.phase_started_tick = now;
        return;
    }
    if (g_can_transaction.phase == HOST_LINK_CAN_PHASE_ABORT)
    {
        controller_state = 0UL;
        (void)CAN_HostStop(&controller_state);
        finish_can_timeout();
        return;
    }

    if (g_can_transaction.attempts <= g_can_transaction.retry_limit)
    {
        CAN_HostDiscardRxQueue();
        can_result = CAN_HostTransmit(&g_can_transaction.tx_frame);
        if (can_result != CAN_HOST_RESULT_OK)
        {
            finish_can_transaction(can_result_to_status(can_result),
                                   (const uint8_t *)0, 0U);
            return;
        }
        g_can_transaction.attempts++;
        g_can_transaction.phase = HOST_LINK_CAN_PHASE_TX;
        g_can_transaction.phase_started_tick = now;
        g_can_transaction.rx_matched = 0U;
        return;
    }

    finish_can_timeout();
}
