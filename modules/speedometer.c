#include "../Service/STD_Types.h"
#include "../Service/Bit_Math.h"
#include <avr/interrupt.h>
#include "../MCL/Timer/timer_registers.h"
#include "../MCL/Timer/timer_interface.h"
#include "odometer.h"

volatile uint16_h g_spd_ovf   = 0;
volatile uint32_h g_spd_prev  = 0;
volatile uint32_h g_spd_delta = 0;
volatile uint8_h  g_spd_new   = 0;
volatile uint32_h g_spd_last_cap = 0;
volatile uint16_h g_wheel_pulses = 0;
uint16_h g_speed_kmh = 0;

void SPD_OnOverflow(void) { g_spd_ovf++; }

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
    g_wheel_pulses++;
}

void SPD_Update(void)
{
    uint16_h pulses;
    cli();
    pulses = g_wheel_pulses;
    g_wheel_pulses = 0;
    sei();

    while (pulses--)
    {
        ODO_AddPulse();
    }

    uint32_h now = ((uint32_h)g_spd_ovf << 16) | TIMER_TCNT1_REG;
    if ((now - g_spd_last_cap) > 125000UL) { g_speed_kmh = 0; return; }

    if (g_spd_new)
    {
        g_spd_new = 0;
        if (g_spd_delta > 0)
        {
            uint32_h period_us = g_spd_delta * 8UL;
            uint16_h kmh = (uint16_h)(1800000UL / period_us);
            g_speed_kmh = (kmh > 250u) ? 0u : kmh;
        }
    }
}
