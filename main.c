/*
 * Vehicle Dashboard - ATmega32A @ 8 MHz
 * PA0=Gas pedal, PB0=Left LED, PB1=Right LED, PB2=Overspeed LED
 * PB3=Buzzer, PC0-7=LCD data, PA1=LCD RS, PA2=LCD EN
 * PD2/INT0=Left switch, PD3/INT1=Right switch, PD4=Hazard
 * PD6/ICP1=Wheel speed, PD0/PD1=UART
 */

#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

#include "../Service/STD_Types.h"
#include "../Service/Bit_Math.h"
#include "../MCL/GPIO/gpio_interface.h"
#include "../MCL/GPIO/gpio_registers.h"
#include "../MCL/ADC/adc_interface.h"
#include "../MCL/Timer/timer_interface.h"
#include "../MCL/Timer/timer_registers.h"
#include "../MCL/UART/uart_interface.h"
#include "../HAL/LCD_Hd44780/lcd_hd44780.h"

/* Raw AVR registers not exposed by drivers */
#define MCUCR_REG  (*(volatile uint8_h *)0x55)
#define GICR_REG   (*(volatile uint8_h *)0x5B)
#define GIFR_REG   (*(volatile uint8_h *)0x5A)

#define SPEED_LIMIT  120u
#define BLINK_MS     500u

/* Shared state */
static volatile uint16_h g_tick    = 0;
static volatile uint16_h g_ovf_cnt = 0;
static volatile uint32_h g_icr_prev= 0;
static volatile uint32_h g_delta   = 0;
static volatile uint8_h  g_new_cap = 0;
static volatile uint8_h  g_left    = 0;
static volatile uint8_h  g_right   = 0;

static LCD_Hd44780_HandleType g_lcd;

/* Timer callbacks — called from timer.c ISRs (no duplicate vectors) */
static void on_tick(void)  { g_tick++;    }
static void on_ovf(void)   { g_ovf_cnt++; }

/* ICP1, INT0, INT1 — defined here because interrupt.c is a stub */
ISR(TIMER1_CAPT_vect)
{
    uint16_h icr = TIMER_ICR1_REG;
    uint16_h ovf = g_ovf_cnt;
    if (GET_BIT(TIMER_TIFR_REG, TIMER_TOV1_BIT) && icr < 0x8000u) ovf++;
    uint32_h now = ((uint32_h)ovf << 16) | icr;
    g_delta      = now - g_icr_prev;
    g_icr_prev   = now;
    g_new_cap    = 1;
}

ISR(INT0_vect) { g_left  ^= 1; if (g_left)  g_right = 0; }
ISR(INT1_vect) { g_right ^= 1; if (g_right) g_left  = 0; }

static void hw_init(void)
{
    /* Outputs */
    GPIO_SetPinDirection(GPIO_PORTB, GPIO_PIN0, GPIO_OUTPUT); /* left LED  */
    GPIO_SetPinDirection(GPIO_PORTB, GPIO_PIN1, GPIO_OUTPUT); /* right LED */
    GPIO_SetPinDirection(GPIO_PORTB, GPIO_PIN2, GPIO_OUTPUT); /* overspeed */
    GPIO_SetPinDirection(GPIO_PORTB, GPIO_PIN3, GPIO_OUTPUT); /* buzzer    */

    /* Inputs + pull-ups */
    GPIO_SetPinDirection(GPIO_PORTD, GPIO_PIN2, GPIO_INPUT);
    GPIO_SetPinDirection(GPIO_PORTD, GPIO_PIN3, GPIO_INPUT);
    GPIO_SetPinDirection(GPIO_PORTD, GPIO_PIN4, GPIO_INPUT);
    GPIO_SetPinValue(GPIO_PORTD, GPIO_PIN2, 1);
    GPIO_SetPinValue(GPIO_PORTD, GPIO_PIN3, 1);
    GPIO_SetPinValue(GPIO_PORTD, GPIO_PIN4, 1);

    /* ADC */
    ADC_ConfigType adc = { ADC_REF_AVCC, ADC_PRESCALER_64 };
    ADC_Init(&adc);

    /* Timer0 CTC 10 ms tick: prescaler=1024, OCR0=77 @ 8MHz */
    Timer_ConfigType t0 = { TIMER_CHANNEL_0, TIMER_MODE_CTC,
                            TIMER_CLOCK_DIV_1024, 0, 77 };
    Timer_Init(&t0);
    Timer_SetCallBack(TIMER_CHANNEL_0, TIMER_INT_COMPARE_MATCH, on_tick);
    Timer_EnableInterrupt(TIMER_CHANNEL_0, TIMER_INT_COMPARE_MATCH);

    /* Timer1 Normal prescaler=64 for ICP1 */
    Timer_ConfigType t1 = { TIMER_CHANNEL_1, TIMER_MODE_NORMAL,
                            TIMER_CLOCK_DIV_64, 0, 0 };
    Timer_Init(&t1);
    SET_BIT(TIMER_TCCR1B_REG, TIMER_ICNC1_BIT); /* noise cancel */
    SET_BIT(TIMER_TCCR1B_REG, TIMER_ICES1_BIT); /* rising edge  */
    Timer_SetCallBack(TIMER_CHANNEL_1, TIMER_INT_OVERFLOW, on_ovf);
    Timer_EnableInterrupt(TIMER_CHANNEL_1, TIMER_INT_OVERFLOW);
    SET_BIT(TIMER_TIMSK_REG, TIMER_TICIE1_BIT);  /* ICP1 enable  */

    /* INT0/INT1 falling edge */
    MCUCR_REG |=  (1<<3)|(1<<1);   /* ISC11=1, ISC01=1 */
    MCUCR_REG &= ~((1<<2)|(1<<0)); /* ISC10=0, ISC00=0 */
    GIFR_REG  |=  (1<<7)|(1<<6);   /* clear flags      */
    GICR_REG  |=  (1<<7)|(1<<6);   /* enable INT1,INT0 */

    /* UART 9600 8N1 */
    UART_ConfigType uart = { UART_BAUD_9600, UART_DATA_8BITS,
                             UART_PARITY_NONE, UART_STOP_1BIT };
    UART_Init(&uart);

    /* LCD 8-bit on PORTC, RS=PA1, EN=PA2 */
    g_lcd.bus          = LCD_HD44780_BUS_8BIT;
    g_lcd.dataPort     = GPIO_PORTC;
    g_lcd.dataStartPin = GPIO_PIN0;
    g_lcd.controlPort  = GPIO_PORTA;
    g_lcd.rsPin        = GPIO_PIN1;
    g_lcd.enPin        = GPIO_PIN2;
    g_lcd.useRwPin     = 0;
    g_lcd.rows         = 2;
    g_lcd.cols         = 16;
    LCD_Hd44780_Init(&g_lcd);

    sei();
}

int main(void)
{
    hw_init();
    LCD_Hd44780_WriteStringAt(&g_lcd, 0, 0, (const uint8_h *)"Vehicle Dashboard");
    LCD_Hd44780_WriteStringAt(&g_lcd, 1, 0, (const uint8_h *)"  Initializing..");
    _delay_ms(800);
    LCD_Hd44780_Clear(&g_lcd);
    UART_SendString((const uint8_h *)"Dashboard ready\r\n");

    uint16_h blink_t = 0, uart_t = 0, last = 0;
    uint8_h  blink   = 0;

    while (1)
    {
        uint16_h now  = g_tick;
        uint16_h dt   = now - last;
        last          = now;
        blink_t      += dt;
        uart_t       += dt;

        /* Speed from gas pedal */
        uint16_h raw;
        ADC_ReadChannelBlocking(ADC_CHANNEL0, &raw);
        uint16_h spd = (uint16_h)(((uint32_h)raw * 200u) / 1023u);

        /* Hazard */
        uint8_h haz = (GPIO_GetPinStatus(GPIO_PORTD, GPIO_PIN4) == 0);
        if (haz) { g_left = 0; g_right = 0; }

        /* Blink */
        if (blink_t >= BLINK_MS) { blink_t = 0; blink ^= 1; }
        GPIO_SetPinValue(GPIO_PORTB, GPIO_PIN0, blink && (g_left  || haz));
        GPIO_SetPinValue(GPIO_PORTB, GPIO_PIN1, blink && (g_right || haz));

        /* Overspeed */
        uint8_h ovs = (spd >= SPEED_LIMIT);
        GPIO_SetPinValue(GPIO_PORTB, GPIO_PIN2, ovs);
        GPIO_SetPinValue(GPIO_PORTB, GPIO_PIN3, ovs);

        /* LCD */
        char buf[17];
        snprintf(buf, sizeof(buf), "SPD: %3u km/h   ", spd);
        LCD_Hd44780_WriteStringAt(&g_lcd, 0, 0, (const uint8_h *)buf);

        const char *status =
            ovs     ? "!! OVERSPEED !! " :
            haz     ? "  HAZARD ON     " :
            g_left  ? "<< LEFT TURN    " :
            g_right ? "   RIGHT TURN >>" :
                      "                ";
        LCD_Hd44780_WriteStringAt(&g_lcd, 1, 0, (const uint8_h *)status);

        /* UART every 500 ms */
        if (uart_t >= 500u)
        {
            uart_t = 0;
            snprintf(buf, sizeof(buf), "SPD=%u OVS=%u\r\n", spd, ovs);
            UART_SendString((const uint8_h *)buf);
        }
    }
    return 0;
}
