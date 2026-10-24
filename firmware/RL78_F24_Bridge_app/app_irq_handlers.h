#ifndef APP_IRQ_HANDLERS_H
#define APP_IRQ_HANDLERS_H

/* X(vector_byte_address, global_ISR_symbol).
 * Addresses come from this MCU's iodefine.h interrupt constants and the
 * hardware manual Interrupt Functions vector table, not a CAN port number.
 * ExampleINTST0=0x16: App slot=0x5000+0x16*2=0x502C.
 * Add/register the ISR C source in this App MTPJ; use a no-vect interrupt
 * pragma and RETI. Configure the peripheral/pending flag/mask separately.
 * Pre-build generates all64 slots. Unlisted normal IRQs use the default ISR;
 * reset, reserved addresses and BRK0x7E are not ordinary IRQ mapping entries.
 */
#define APP_IRQ_HANDLERS(X) \
    X(0x16, r_Config_UART0_interrupt_send) \
    X(0x18, r_Config_UART0_interrupt_receive) \
    X(0x28, r_Config_IICA0_interrupt) \
    X(0x2A, r_Config_INTC_intp8_interrupt) \
    X(0x2E, r_Config_TAU0_1_interrupt) \
    X(0x38, r_Config_INTC_intp7_interrupt) \
    X(0x3E, Bridge_UartTxIsr) \
    X(0x40, Bridge_UartRxIsr) \
    X(0x4C, r_can_glb_rxmb_isr) \
    X(0x4E, r_can_ch0_error_isr) \
    X(0x52, r_can_ch0_cfifo_rx_isr) \
    X(0x54, r_can_ch0_transmit_isr) \
    X(0x58, r_can_glb_error_isr)
#endif
