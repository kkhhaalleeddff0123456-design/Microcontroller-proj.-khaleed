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
 *  PD3      = LCD page button (active low)
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

/* Modules collected in modules/ */
#include "../modules/gauges.h"
#include "../modules/speedometer.h"
#include "../modules/tachometer.h"
#include "../modules/turns.h"
#include "../modules/warning_lamps.h"
#include "../modules/odometer.h"
#include "../modules/display.h"
#include "../modules/ignition.h"
#include "../modules/console.h"
#include "../modules/chime.h"

/* ── Raw registers ── */
#define MCUCR_REG  (*(volatile uint8_h *)0x55)
#define GICR_REG   (*(volatile uint8_h *)0x5B)
#define GIFR_REG   (*(volatile uint8_h *)0x5A)
#define TCCR2_REG  TIMER_TCCR2_REG
#define OCR2_REG   TIMER_OCR2_REG

#define SR_PORT    GPIO_PORTB
#define SR_DATA    GPIO_PIN2
#define SR_CLOCK   GPIO_PIN3


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

    /* PD3 = LCD page button input + pull-up */
    GPIO_SetPinDirection(GPIO_PORTD, GPIO_PIN3, GPIO_INPUT);
    GPIO_SetPinValue(GPIO_PORTD, GPIO_PIN3, 1);

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
        BTN_Update();
        if (g_button_event)
        {
            g_button_event = 0;
            DSP_Next();
        }
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
