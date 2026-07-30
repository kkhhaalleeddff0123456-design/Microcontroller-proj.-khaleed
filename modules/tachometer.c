#include "../Service/STD_Types.h"
#include <avr/interrupt.h>

volatile uint16_h g_tac_count = 0;
uint16_h g_rpm = 0;

ISR(INT0_vect) { g_tac_count++; }

void TAC_Update250ms(void)
{
    uint16_h cnt;
    cli(); cnt = g_tac_count; g_tac_count = 0; sei();
    g_rpm = cnt * 120u;
}
