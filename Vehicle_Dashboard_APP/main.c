/*
 * Vehicle Dashboard - ATmega32A @ 8 MHz
 * All 10 modules from README implemented
 *
 * PIN MAP (from schematic):
 *  PA0/ADC0 = Fuel level pot
 *  PA1/ADC1 = Coolant temperature pot
 *  PA2/ADC2 = Battery voltage pot
 *  PA3/ADC3 = Oil pressure pot
 *  PB0      = Left turn switch  (active low, pull-up)
 *  PB1      = Right turn switch (active low, pull-up)
 *  PB2      = 74LS164 serial data (A,B tied together)
 *  PB3      = 74LS164 clock input
 *  PB4      = LCD E
 *  PB5      = LCD RW
 *  PB6      = LCD RS
 *  PC0-PC7  = LCD D0-D7
 *  PD0/TXD  = UART TX
 *  PD1/RXD  = UART RX
 *  PD2      = Tachometer pulse input
 *  PD6/ICP1 = Speed sensor (wheel, Timer1 Input Capture)
 *  PD7/OC2  = Buzzer (Timer2 PWM)
 *
 * 74LS164 lamp map:
 *  Q0=Low fuel  Q1=Oil pressure  Q2=Battery  Q3=Coolant
 *  Q4=Check engine  Q5=Left turn  Q6=Right turn  Q7=High beam
 */

#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

#include "../Service/STD_Types.h"
#include "../Service/Bit_Math.h"
#include "../MCL/GPIO/gpio_interface.h"
#include "../MCL/GPIO/gpio_registers.h"
#include "../MCL/ADC/adc_interface.h"
#include "../MCL/ADC/adc_registers.h"
#include "../MCL/Timer/timer_interface.h"
#include "../MCL/Timer/timer_registers.h"
#include "../MCL/UART/uart_interface.h"
#include "../HAL/LCD_Hd44780/lcd_hd44780.h"

/* ── Raw registers ── */
#define MCUCR_REG  (*(volatile uint8_h *)0x55)
#define GICR_REG   (*(volatile uint8_h *)0x5B)
#define GIFR_REG   (*(volatile uint8_h *)0x5A)
#define TCCR2_REG  TIMER_TCCR2_REG
#define OCR2_REG   TIMER_OCR2_REG

#define SR_PORT    GPIO_PORTB
#define SR_DATA    GPIO_PIN2
#define SR_CLOCK   GPIO_PIN3

static void SR_ShiftByte(uint8_h value)
{
    for (uint8_h bit = 0; bit < 8u; ++bit)
    {
        uint8_h data = (uint8_h)((value & (1u << (7u - bit))) ? 1u : 0u);
        GPIO_SetPinValue(SR_PORT, SR_DATA, data);
        GPIO_SetPinValue(SR_PORT, SR_CLOCK, 1);
        GPIO_SetPinValue(SR_PORT, SR_CLOCK, 0);
    }
}

/* ================================================================
 *  M1 — GAUGES
 * ================================================================ */
static uint16_h g_fuel_pct   = 0;   /* 0-100 %          */
static uint16_h g_coolant_c  = 0;   /* 40-130 °C        */
static uint32_h g_batt_mv    = 0;   /* 0-16000 mV       */
static uint16_h g_oil_barx10 = 0;   /* 0-100 (x10 bar)  */

static void GAU_Update(void)
{
    uint16_h raw;
    ADC_ReadChannelBlocking(ADC_CHANNEL0, &raw);
    g_fuel_pct   = (uint16_h)(((uint32_h)raw * 100u) / 1023u);

    ADC_ReadChannelBlocking(ADC_CHANNEL1, &raw);
    g_coolant_c  = (uint16_h)((((uint32_h)raw * 170u) / 1023u) + 40u);

    ADC_ReadChannelBlocking(ADC_CHANNEL2, &raw);
    g_batt_mv    = ((uint32_h)raw * 16000u) / 1023u;

    ADC_ReadChannelBlocking(ADC_CHANNEL3, &raw);
    g_oil_barx10 = (uint16_h)(((uint32_h)raw * 100u) / 1023u);
}

/* ================================================================
 *  M2 — SPEEDOMETER (Timer1 ICP)
 * ================================================================ */
static volatile uint16_h g_spd_ovf   = 0;
static volatile uint32_h g_spd_prev  = 0;
static volatile uint32_h g_spd_delta = 0;
static volatile uint8_h  g_spd_new   = 0;
static volatile uint32_h g_spd_last_cap = 0; /* for timeout */
static uint16_h g_speed_kmh = 0;

static void SPD_OnOverflow(void) { g_spd_ovf++; }

ISR(TIMER1_CAPT_vect)
{
    uint16_h icr = TIMER_ICR1_REG;
    uint16_h ovf = g_spd_ovf;
    if (GET_BIT(TIMER_TIFR_REG, TIMER_TOV1_BIT) && icr < 0x8000u) ovf++;
    uint32_h now    = ((uint32_h)ovf << 16) | icr;
    g_spd_delta     = now - g_spd_prev;
    g_spd_prev      = now;
    g_spd_last_cap  = now;
    g_spd_new       = 1;
}

static void SPD_Update(void)
{
    /* timeout: no pulse for ~1 s (125kHz/65536 ~ 1.9 ovf/s, use 2 ovf) */
    uint32_h now = ((uint32_h)g_spd_ovf << 16) | TIMER_TCNT1_REG;
    if ((now - g_spd_last_cap) > 125000UL) { g_speed_kmh = 0; return; }

    if (g_spd_new)
    {
        g_spd_new = 0;
        if (g_spd_delta > 0)
        {
            /* period_us = delta * 8; speed = 1800000 / period_us */
            uint32_h period_us = g_spd_delta * 8UL;
            uint16_h kmh = (uint16_h)(1800000UL / period_us);
            g_speed_kmh = (kmh > 250u) ? 0u : kmh; /* reject noise */
        }
    }
}

/* ================================================================
 *  M3 — TACHOMETER (INT0 pulse counting, 250 ms window)
 * ================================================================ */
static volatile uint16_h g_tac_count = 0;
static uint16_h g_rpm = 0;

ISR(INT0_vect) { g_tac_count++; }

static void TAC_Update250ms(void)
{
    uint16_h cnt;
    cli(); cnt = g_tac_count; g_tac_count = 0; sei();
    g_rpm = cnt * 120u; /* RPM = count * 120 */
}

/* ================================================================
 *  M4 — TURN INDICATORS (PB0=left, PB1=right, blink via scheduler)
 * ================================================================ */
static uint8_h g_turn_left  = 0;
static uint8_h g_turn_right = 0;
static uint8_h g_hazard     = 0;
static uint8_h g_blink_state= 0;

static void TRN_Update(void)
{
    uint8_h sw_left  = (GPIO_GetPinStatus(GPIO_PORTB, GPIO_PIN0) == 0);
    uint8_h sw_right = (GPIO_GetPinStatus(GPIO_PORTB, GPIO_PIN1) == 0);

    if (sw_left && sw_right) { g_hazard = 1; g_turn_left = 0; g_turn_right = 0; }
    else if (sw_left)        { g_hazard = 0; g_turn_left = 1; g_turn_right = 0; }
    else if (sw_right)       { g_hazard = 0; g_turn_left = 0; g_turn_right = 1; }
    else                     { g_hazard = 0; g_turn_left = 0; g_turn_right = 0; }
}

/* ================================================================
 *  M5 — WARNING LAMPS (74LS164 serial shift register on PB2/PB3)
 * ================================================================ */
#define LAMP_LOW_FUEL    0x01
#define LAMP_OIL         0x02
#define LAMP_BATTERY     0x04
#define LAMP_COOLANT     0x08
#define LAMP_CHECK_ENG   0x10
#define LAMP_LEFT_TURN   0x20
#define LAMP_RIGHT_TURN  0x40
#define LAMP_HIGH_BEAM   0x80

static uint8_h g_lamp_mask = 0;

static void LMP_Refresh(uint8_h mask)
{
    g_lamp_mask = mask;
    SR_ShiftByte(mask);
}

static void WRN_Update(void)
{
    uint8_h mask = 0;
    if (g_fuel_pct   < 10u)                          mask |= LAMP_LOW_FUEL;
    if (g_oil_barx10 < 10u && g_rpm > 500u)          mask |= LAMP_OIL;
    if (g_batt_mv    < 12000u || g_batt_mv > 15000u) mask |= LAMP_BATTERY;
    if (g_coolant_c  > 110u)                         mask |= LAMP_COOLANT;

    /* turn lamps */
    if ((g_turn_left  || g_hazard) && g_blink_state) mask |= LAMP_LEFT_TURN;
    if ((g_turn_right || g_hazard) && g_blink_state) mask |= LAMP_RIGHT_TURN;

    LMP_Refresh(mask);
}

/* ================================================================
 *  M6 — ODOMETER (pulse counting, no EEPROM in this sim build)
 * ================================================================ */
static uint32_h g_odo_mm   = 0;  /* lifetime mm  */
static uint32_h g_trip_mm  = 0;  /* trip mm      */

static void ODO_AddPulse(void)
{
    g_odo_mm  += 500u;
    g_trip_mm += 500u;
}

/* ================================================================
 *  M7 — DISPLAY (LCD 16x2 parallel)
 * ================================================================ */
static LCD_Hd44780_HandleType g_lcd;

typedef enum { PG_MAIN=0, PG_ENGINE, PG_ELECTRICAL, PG_TRIP, PG_MAX } PageType;
static PageType g_page = PG_MAIN;

static void DSP_Render(void)
{
    char r0[17], r1[17];

    switch (g_page)
    {
        case PG_MAIN:
            snprintf(r0, 17, "SPD:%3u RPM:%4u", g_speed_kmh, g_rpm);
            if      (g_lamp_mask & LAMP_OIL)     snprintf(r1,17,"!! OIL PRESS !! ");
            else if (g_lamp_mask & LAMP_COOLANT)  snprintf(r1,17,"!! COOLANT !!   ");
            else if (g_lamp_mask & LAMP_BATTERY)  snprintf(r1,17,"!! BATTERY !!   ");
            else if (g_lamp_mask & LAMP_LOW_FUEL) snprintf(r1,17,"  LOW FUEL      ");
            else if (g_hazard)                    snprintf(r1,17,"  HAZARD ON     ");
            else if (g_turn_left)                 snprintf(r1,17,"<< LEFT TURN    ");
            else if (g_turn_right)                snprintf(r1,17,"   RIGHT TURN >>");
            else                                  snprintf(r1,17,"                ");
            break;

        case PG_ENGINE:
            snprintf(r0, 17, "RPM:  %4u      ", g_rpm);
            snprintf(r1, 17, "COOL: %3u C     ", g_coolant_c);
            break;

        case PG_ELECTRICAL:
            snprintf(r0, 17, "BATT:%2u.%uV     ",
                     (uint16_h)(g_batt_mv/1000u),
                     (uint16_h)((g_batt_mv%1000u)/100u));
            snprintf(r1, 17, "FUEL: %3u %%     ", g_fuel_pct);
            break;

        case PG_TRIP:
            snprintf(r0, 17, "TRIP:%5lu m   ", g_trip_mm/1000u);
            snprintf(r1, 17, "ODO: %5lu km  ", g_odo_mm/1000000u);
            break;

        default: break;
    }

    LCD_Hd44780_WriteStringAt(&g_lcd, 0, 0, (const uint8_h *)r0);
    LCD_Hd44780_WriteStringAt(&g_lcd, 1, 0, (const uint8_h *)r1);
}

/* ================================================================
 *  M8 — IGNITION FSM
 * ================================================================ */
typedef enum { IGN_OFF, IGN_ACC, IGN_ON, IGN_RUNNING, IGN_STALL } IgnState;
static IgnState g_ign = IGN_OFF;
static uint16_h g_ign_timer = 0;

static void FSM_Run(void)
{
    switch (g_ign)
    {
        case IGN_OFF:
            g_ign = IGN_ACC;
            break;
        case IGN_ACC:
            g_ign = IGN_ON;
            break;
        case IGN_ON:
            if (g_rpm > 500u) { g_ign_timer = 0; g_ign = IGN_RUNNING; }
            break;
        case IGN_RUNNING:
            if (g_rpm < 300u) { g_ign_timer++; if (g_ign_timer > 4u) g_ign = IGN_STALL; }
            else g_ign_timer = 0;
            break;
        case IGN_STALL:
            if (g_rpm > 500u) g_ign = IGN_RUNNING;
            break;
    }
}

/* ================================================================
 *  M9 — CONSOLE (UART 9600 8N1)
 * ================================================================ */
static void CON_Report(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf),
             "SPD=%u RPM=%u FUEL=%u COOL=%u BATT=%lu OIL=%u\r\n",
             g_speed_kmh, g_rpm, g_fuel_pct,
             g_coolant_c, g_batt_mv, g_oil_barx10);
    UART_SendString((const uint8_h *)buf);
}

/* ================================================================
 *  M10 — CHIME (Buzzer on PD7/OC2)
 * ================================================================ */
static void CHM_Beep(uint8_h on)
{
    if (on)
    {
        /* Fast PWM, prescaler 8, OCR2=127 -> ~3.9 kHz tone */
        TCCR2_REG = (1<<6)|(1<<3)|(1<<1)|(1<<0); /* COM21,WGM21,WGM20,CS20 */
        OCR2_REG  = 127;
    }
    else
    {
        TCCR2_REG = 0;
        GPIO_SetPinValue(GPIO_PORTD, GPIO_PIN7, 0);
    }
}

/* ================================================================
 *  SCHEDULER TICK (Timer0 CTC, 10 ms)
 * ================================================================ */
static volatile uint16_h g_tick = 0;
static void on_tick(void) { g_tick++; }

/* ================================================================
 *  HARDWARE INIT
 * ================================================================ */
static void hw_init(void)
{
    /* PB0/PB1 inputs + pull-ups (turn switches) */
    GPIO_SetPinDirection(GPIO_PORTB, GPIO_PIN0, GPIO_INPUT);
    GPIO_SetPinDirection(GPIO_PORTB, GPIO_PIN1, GPIO_INPUT);
    GPIO_SetPinValue(GPIO_PORTB, GPIO_PIN0, 1);
    GPIO_SetPinValue(GPIO_PORTB, GPIO_PIN1, 1);

    /* PB2/PB3 = 74LS164 serial data and clock */
    GPIO_SetPinDirection(SR_PORT, SR_DATA, GPIO_OUTPUT);
    GPIO_SetPinDirection(SR_PORT, SR_CLOCK, GPIO_OUTPUT);
    GPIO_SetPinValue(SR_PORT, SR_DATA, 0);
    GPIO_SetPinValue(SR_PORT, SR_CLOCK, 0);

    /* PB4/PB5/PB6 = LCD E/RW/RS */
    GPIO_SetPinDirection(GPIO_PORTB, GPIO_PIN4, GPIO_OUTPUT);
    GPIO_SetPinDirection(GPIO_PORTB, GPIO_PIN5, GPIO_OUTPUT);
    GPIO_SetPinDirection(GPIO_PORTB, GPIO_PIN6, GPIO_OUTPUT);
    GPIO_SetPinValue(GPIO_PORTB, GPIO_PIN4, 0);
    GPIO_SetPinValue(GPIO_PORTB, GPIO_PIN5, 0);
    GPIO_SetPinValue(GPIO_PORTB, GPIO_PIN6, 0);

    /* PD7 = buzzer output */
    GPIO_SetPinDirection(GPIO_PORTD, GPIO_PIN7, GPIO_OUTPUT);

    /* Timer0 CTC 10 ms */
    Timer_ConfigType t0 = { TIMER_CHANNEL_0, TIMER_MODE_CTC,
                            TIMER_CLOCK_DIV_1024, 0, 77 };
    Timer_Init(&t0);
    Timer_SetCallBack(TIMER_CHANNEL_0, TIMER_INT_COMPARE_MATCH, on_tick);
    Timer_EnableInterrupt(TIMER_CHANNEL_0, TIMER_INT_COMPARE_MATCH);

    /* Timer1 Normal prescaler=64 for ICP1 (speed sensor) */
    Timer_ConfigType t1 = { TIMER_CHANNEL_1, TIMER_MODE_NORMAL,
                            TIMER_CLOCK_DIV_64, 0, 0 };
    Timer_Init(&t1);
    SET_BIT(TIMER_TCCR1B_REG, TIMER_ICNC1_BIT);
    SET_BIT(TIMER_TCCR1B_REG, TIMER_ICES1_BIT);
    Timer_SetCallBack(TIMER_CHANNEL_1, TIMER_INT_OVERFLOW, SPD_OnOverflow);
    Timer_EnableInterrupt(TIMER_CHANNEL_1, TIMER_INT_OVERFLOW);
    SET_BIT(TIMER_TIMSK_REG, TIMER_TICIE1_BIT);

    /* INT0 rising edge for tachometer */
    MCUCR_REG |=  (1<<1);   /* ISC01=1 */
    MCUCR_REG &= ~(1<<0);   /* ISC00=0 -> wait, rising = ISC01=1,ISC00=1 */
    MCUCR_REG |=  (1<<1)|(1<<0); /* ISC01=1,ISC00=1 = rising edge */
    GIFR_REG  |=  (1<<6);
    GICR_REG  |=  (1<<6);   /* enable INT0 */

    /* UART 9600 8N1 */
    UART_ConfigType uart = { UART_BAUD_9600, UART_DATA_8BITS,
                             UART_PARITY_NONE, UART_STOP_1BIT };
    UART_Init(&uart);

    /* LCD 8-bit on PORTC, RS=PB6, RW=PB5, E=PB4 */
    g_lcd.bus          = LCD_HD44780_BUS_8BIT;
    g_lcd.dataPort     = GPIO_PORTC;
    g_lcd.dataStartPin = GPIO_PIN0;
    g_lcd.controlPort  = GPIO_PORTB;
    g_lcd.rsPin        = GPIO_PIN6;
    g_lcd.rwPin        = GPIO_PIN5;
    g_lcd.enPin        = GPIO_PIN4;
    g_lcd.useRwPin     = 1;
    g_lcd.rows         = 2;
    g_lcd.cols         = 16;
    LCD_Hd44780_Init(&g_lcd);

    /* ADC after LCD — restore PA0-PA3 as analog inputs */
    ADC_ConfigType adc = { ADC_REF_AVCC, ADC_PRESCALER_64 };
    ADC_Init(&adc);
    GPIO_DDRA &= ~0x0Fu;    /* PA0-PA3 inputs */
    GPIO_PORTA_REG &= ~0x0Fu; /* no pull-ups   */

    sei();
}

/* ================================================================
 *  MAIN
 * ================================================================ */
int main(void)
{
    hw_init();

    /* Bulb check: all lamps on for 3 s */
    LMP_Refresh(0xFF);
    LCD_Hd44780_WriteStringAt(&g_lcd, 0, 0, (const uint8_h *)"Vehicle Dashboard");
    LCD_Hd44780_WriteStringAt(&g_lcd, 1, 0, (const uint8_h *)"  Bulb Check... ");
    _delay_ms(3000);
    LMP_Refresh(0x00);
    LCD_Hd44780_Clear(&g_lcd);
    UART_SendString((const uint8_h *)"Dashboard ready\r\n");

    uint16_h last       = 0;
    uint16_h t_lamps    = 0;  /* 50 ms  */
    uint16_h t_speed    = 0;  /* 100 ms */
    uint16_h t_tacho    = 0;  /* 250 ms */
    uint16_h t_lcd      = 0;  /* 250 ms */
    uint16_h t_gauges   = 0;  /* 500 ms */
    uint16_h t_report   = 0;  /* 5000 ms */
    uint16_h t_blink    = 0;  /* 450 ms */

    g_ign = IGN_ON; /* auto-start for simulation */

    while (1)
    {
        uint16_h now = g_tick;
        uint16_h dt  = now - last;
        last         = now;

        t_lamps  += dt; t_speed  += dt; t_tacho += dt;
        t_lcd    += dt; t_gauges += dt; t_report+= dt;
        t_blink  += dt;

        /* 10 ms — inputs + FSM */
        TRN_Update();
        FSM_Run();

        /* 50 ms — lamps */
        if (t_lamps >= 50u)  { t_lamps = 0; WRN_Update(); }

        /* 100 ms — speed + odometer */
        if (t_speed >= 100u)
        {
            t_speed = 0;
            SPD_Update();
            if (g_spd_new == 0 && g_speed_kmh > 0) ODO_AddPulse();
        }

        /* 250 ms — tachometer */
        if (t_tacho >= 250u) { t_tacho = 0; TAC_Update250ms(); }

        /* 250 ms — LCD */
        if (t_lcd   >= 250u) { t_lcd   = 0; DSP_Render(); }

        /* 450 ms — blink toggle */
        if (t_blink >= 450u)
        {
            t_blink = 0;
            g_blink_state ^= 1;
            /* chime tick with turn signal */
            if (g_blink_state && (g_turn_left || g_turn_right || g_hazard))
            { CHM_Beep(1); _delay_ms(20); CHM_Beep(0); }
        }

        /* 500 ms — gauges */
        if (t_gauges >= 500u) { t_gauges = 0; GAU_Update(); }

        /* 5000 ms — UART report */
        if (t_report >= 5000u) { t_report = 0; CON_Report(); }

        /* Overspeed chime */
        if (g_speed_kmh >= 120u)
        { CHM_Beep(1); _delay_ms(50); CHM_Beep(0); }
    }
    return 0;
}
