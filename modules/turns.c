#include "../Service/STD_Types.h"
#include "../MCL/GPIO/gpio_interface.h"

uint8_h g_turn_left  = 0;
uint8_h g_turn_right = 0;
uint8_h g_hazard     = 0;
uint8_h g_blink_state= 0;

void TRN_Update(void)
{
    uint8_h sw_left  = (GPIO_GetPinStatus(GPIO_PORTB, GPIO_PIN0) == 0);
    uint8_h sw_right = (GPIO_GetPinStatus(GPIO_PORTB, GPIO_PIN1) == 0);

    if (sw_left && sw_right) { g_hazard = 1; g_turn_left = 0; g_turn_right = 0; }
    else if (sw_left)        { g_hazard = 0; g_turn_left = 1; g_turn_right = 0; }
    else if (sw_right)       { g_hazard = 0; g_turn_left = 0; g_turn_right = 1; }
    else                     { g_hazard = 0; g_turn_left = 0; g_turn_right = 0; }
}
