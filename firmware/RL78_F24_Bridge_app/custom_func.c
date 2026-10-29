/*_____ I N C L U D E S ____________________________________________________*/
#include "inc_main.h"

#include "misc_config.h"
#include "custom_func.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

struct flag_32bit flag_PROJ_CTL;
#define FLAG_PROJ_TIMER_PERIOD_1000MS (flag_PROJ_CTL.bit0)
#define FLAG_PROJ_TIMER_PERIOD_SPECIFIC (flag_PROJ_CTL.bit1)
#define FLAG_PROJ_REVERSE2 (flag_PROJ_CTL.bit2)
#define FLAG_PROJ_REVERSE3 (flag_PROJ_CTL.bit3)
#define FLAG_PROJ_REVERSE4 (flag_PROJ_CTL.bit4)
#define FLAG_PROJ_REVERSE5 (flag_PROJ_CTL.bit5)
#define FLAG_PROJ_REVERSE6 (flag_PROJ_CTL.bit6)
#define FLAG_PROJ_REVERSE7 (flag_PROJ_CTL.bit7)

#define FLAG_PROJ_TRIG_1 (flag_PROJ_CTL.bit8)
#define FLAG_PROJ_TRIG_2 (flag_PROJ_CTL.bit9)
#define FLAG_PROJ_TRIG_3 (flag_PROJ_CTL.bit10)
#define FLAG_PROJ_TRIG_4 (flag_PROJ_CTL.bit11)
#define FLAG_PROJ_TRIG_5 (flag_PROJ_CTL.bit12)
#define FLAG_PROJ_REVERSE13 (flag_PROJ_CTL.bit13)
#define FLAG_PROJ_REVERSE14 (flag_PROJ_CTL.bit14)
#define FLAG_PROJ_REVERSE15 (flag_PROJ_CTL.bit15)

/*_____ D E F I N I T I O N S ______________________________________________*/

volatile unsigned int counter_tick = 0;

#define BUTTON_DEBOUNCE_MS (20U)
#define TIMER_TICK_PERIOD_MS (60000U)

#define LED1_MASK (0x01U)
#define LED2_MASK (0x02U)
#define LED3_MASK (0x04U)
#define LED_ALL_MASK (0x07U)

#define LED_IDLE_TOGGLE_MS (500U)
#define LED_TRANSFER_TOGGLE_MS (125U)
#define LED_ERROR_TOGGLE_MS (250U)

static volatile unsigned char g_button1_irq_pending = 0U;
static volatile unsigned char g_button2_irq_pending = 0U;
static unsigned char g_button1_seen = 0U;
static unsigned char g_button2_seen = 0U;
static unsigned int g_button1_last_tick = 0U;
static unsigned int g_button2_last_tick = 0U;

static f24_led_state_t g_led_state = F24_LED_STATE_IDLE;
static unsigned int g_led_last_tick = 0U;
static unsigned char g_led_phase = 0U;

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

void set_TIMER_PERIOD_1000MS(void)
{
    FLAG_PROJ_TIMER_PERIOD_1000MS = 1;
}

void reset_TIMER_PERIOD_1000MS(void)
{
    FLAG_PROJ_TIMER_PERIOD_1000MS = 0;
}

bool Is_TIMER_PERIOD_1000MS_Trig(void)
{
    return FLAG_PROJ_TIMER_PERIOD_1000MS;
}

unsigned int get_tick(void)
{
    return (counter_tick);
}

void set_tick(unsigned int t)
{
    counter_tick = t;
}

void tick_counter(void)
{
    counter_tick++;
    if (get_tick() >= 60000)
    {
        set_tick(0);
    }
}

static unsigned int elapsed_ms(unsigned int now, unsigned int before)
{
    unsigned int elapsed;

    if (now >= before)
    {
        elapsed = now - before;
    }
    else
    {
        elapsed = (TIMER_TICK_PERIOD_MS - before) + now;
    }

    return elapsed;
}

void delay_ms(unsigned int ms)
{
#if 1
    unsigned int tickstart = get_tick();
    unsigned int wait = ms;
    unsigned int tmp = 0;

    while (1)
    {
        if (get_tick() > tickstart) // tickstart = 59000 , tick_counter = 60000
        {
            tmp = get_tick() - tickstart;
        }
        else // tickstart = 59000 , tick_counter = 2048
        {
            tmp = 60000 - tickstart + get_tick();
        }

        if (tmp > wait)
        {
            break;
        }
    }

#else
    TIMER_Delay(TIMER0, 1000 * ms);
#endif
}

void CAN_FD_Process_in_polling(void)
{
    if (FLAG_PROJ_TRIG_1)
    {
        FLAG_PROJ_TRIG_1 = 0;
        CAN_Send_TX_Data_Message_Buffer();
    }

    if ((FLAG_PROJ_TRIG_2 == 1) && (FLAG_PROJ_TIMER_PERIOD_SPECIFIC == 1))
    {
        FLAG_PROJ_TIMER_PERIOD_SPECIFIC = 0;
        CAN_Send_TX_Data_Message_Buffer_polling1();
    }

    if ((FLAG_PROJ_TRIG_3 == 1) && (FLAG_PROJ_TIMER_PERIOD_SPECIFIC == 1))
    {
        FLAG_PROJ_TIMER_PERIOD_SPECIFIC = 0;
        CAN_Send_TX_Data_Message_Buffer_polling2();
    }

    if (FLAG_PROJ_TRIG_4)
    {
        FLAG_PROJ_TRIG_4 = 0;
        CAN_FD_Send_TX_Data_Message_Buffer1();
    }
    if (FLAG_PROJ_TRIG_5)
    {
        FLAG_PROJ_TRIG_5 = 0;
        CAN_FD_Send_TX_Data_Message_Buffer2();
    }

    // CAN_Read_RX_Data_FIFO();// check isr:r_can_glb_rxmb_isr
    // CAN_Get_RX_Data_Message_Buffer();
}

void Timer_1ms_IRQ(void)
{
    tick_counter();

    if ((get_tick() % 1000) == 0)
    {
        set_TIMER_PERIOD_1000MS();
    }

    if ((get_tick() % 250) == 0)
    {
        FLAG_PROJ_TIMER_PERIOD_SPECIFIC = 1;
    }

    if ((get_tick() % 50) == 0)
    {
    }
}

static void LED_Write(unsigned char mask)
{
    P8 = (uint8_t)((P8 & (uint8_t)~LED_ALL_MASK) | (mask & LED_ALL_MASK));
}

void LED_SetState(f24_led_state_t state)
{
    g_led_state = state;
    g_led_phase = 0U;
    g_led_last_tick = get_tick();

    switch (state)
    {
    case F24_LED_STATE_CONNECTED:
        LED_Write(LED1_MASK);
        break;

    case F24_LED_STATE_TRANSFER_START:
        LED_Write(LED1_MASK | LED2_MASK);
        break;

    case F24_LED_STATE_TRANSFER_ACTIVE:
        LED_Write(LED1_MASK);
        break;

    case F24_LED_STATE_ERROR:
        LED_Write(0U);
        break;

    case F24_LED_STATE_IDLE:
    default:
        g_led_state = F24_LED_STATE_IDLE;
        LED_Write(0U);
        break;
    }
}

void LED_Process(void)
{
    unsigned int now;
    unsigned int period;

    now = get_tick();
    period = 0U;

    if (g_led_state == F24_LED_STATE_IDLE)
    {
        period = LED_IDLE_TOGGLE_MS;
    }
    else if (g_led_state == F24_LED_STATE_TRANSFER_ACTIVE)
    {
        period = LED_TRANSFER_TOGGLE_MS;
    }
    else if (g_led_state == F24_LED_STATE_ERROR)
    {
        period = LED_ERROR_TOGGLE_MS;
    }

    if ((period != 0U) && (elapsed_ms(now, g_led_last_tick) >= period))
    {
        g_led_last_tick = now;
        g_led_phase ^= 1U;

        if (g_led_state == F24_LED_STATE_IDLE)
        {
            LED_Write((g_led_phase != 0U) ? LED1_MASK : 0U);
        }
        else if (g_led_state == F24_LED_STATE_TRANSFER_ACTIVE)
        {
            LED_Write((g_led_phase != 0U) ? (LED1_MASK | LED2_MASK) : LED1_MASK);
        }
        else
        {
            LED_Write((g_led_phase != 0U) ? LED3_MASK : 0U);
        }
    }
}

void loop(void)
{
    if (Is_TIMER_PERIOD_1000MS_Trig())
    {
        reset_TIMER_PERIOD_1000MS();
    }

    Button_Process_in_polling();
    LED_Process();
    HostLink_Process();
    HostLinkUart_Process();
    CAN_FD_Process_in_polling();
}

void Button_Process_in_polling(void)
{
    unsigned int now;

    now = get_tick();

    if (g_button1_irq_pending != 0U)
    {
        g_button1_irq_pending = 0U;
        if ((g_button1_seen == 0U) || (elapsed_ms(now, g_button1_last_tick) >= BUTTON_DEBOUNCE_MS))
        {
            g_button1_seen = 1U;
            g_button1_last_tick = now;
        }
    }

    if (g_button2_irq_pending != 0U)
    {
        g_button2_irq_pending = 0U;
        if ((g_button2_seen == 0U) || (elapsed_ms(now, g_button2_last_tick) >= BUTTON_DEBOUNCE_MS))
        {
            g_button2_seen = 1U;
            g_button2_last_tick = now;
        }
    }
}

void Button1_Process_in_IRQ(void)
{
    g_button1_irq_pending = 1U;
}

void Button2_Process_in_IRQ(void)
{
    g_button2_irq_pending = 1U;
}

/*
    Reset Control Flag Register (RESF)
    BIT7 : TRAP
    BIT6 : 0
    BIT5 : 0
    BIT4 : WDCLRF
    BIT3 : 0
    BIT2 : 0
    BIT1 : IAWRF
    BIT0 : LVIRF
*/
void check_reset_source(void)
{
    uint8_t src;

    src = RESF;
    (void)src;
}

/*
    7:Internal reset by execution of illegal instruction
    1:Internal reset by illegal-memory access
*/
// perform sofware reset
void _reset_by_illegal_instruction(void)
{
    static const unsigned char illegal_Instruction = 0xFF;
    void (*dummy)(void) = (void (*)(void))&illegal_Instruction;
    dummy();
}
void _reset_by_illegal_memory_access(void)
{
#if 1
    const unsigned char ILLEGAL_ACCESS_ON = 0x80;
    IAWCTL |= ILLEGAL_ACCESS_ON;            // switch IAWEN on (default off)
    *(__far volatile char *)0x00000 = 0x00; // write illegal address 0x00000(RESET VECTOR)
#else
    signed char __far *a;                // Create a far-Pointer
    IAWCTL |= _80_CGC_ILLEGAL_ACCESS_ON; // switch IAWEN on (default off)
    a = (signed char __far *)0x0000;     // Point to 0x000000 (FLASH-ROM area)
    *a = 0;
#endif
}

void RL78_soft_reset(unsigned char flag)
{
    switch (flag)
    {
    case 7: // do not use under debug mode
        _reset_by_illegal_instruction();
        break;
    case 1:
        _reset_by_illegal_memory_access();
        break;
    }
}

void F24_BoardIo_Init(void)
{
    /* Set active-high LEDs P80, P81, and P82 off before enabling outputs. */
    P8 &= (uint8_t)~LED_ALL_MASK;
    PM8 &= (uint8_t)~LED_ALL_MASK;

    /* Keep the CAN transceiver in standby until Host Link CAN_START. */
    P1 |= 0x04U;
    PM1 &= (uint8_t)~0x04U;

    /* IICA0 is unused. Disable it and release externally pulled-up P62/P63. */
    R_Config_IICA0_Stop();
    IICAMK0 = 1U;
    IICAIF0 = 0U;
    IICA0EN = 0U;
    P6 |= 0x0CU;
    PM6 |= 0x0CU;

    LED_SetState(F24_LED_STATE_IDLE);
}

void hardware_init(void)
{
    e_can_err_t can_result;

    F24_BoardIo_Init();
    HostLinkUart_Init();
    HostLink_Init();
    /* Clock changes are bounded and precede UART/timer start. Both a valid
     * PLL clock and the HOCO fallback keep the CPU at 40 MHz. */
    can_result = CAN_Bus_Config(); // CAN , P10:CTXD0 , P11:CRXD0
    R_Config_UART0_Start();        // UART , P15 , P16
    R_Config_TAU0_1_Start();       // TIMER
    R_Config_INTC_INTP7_Start();   // SW1 , P32
    R_Config_INTC_INTP8_Start();   // SW2 , P70

    if (can_result != CAN_SUCCESS)
    {
        LED_SetState(F24_LED_STATE_ERROR);
    }
    BSP_EI();
}
