#include "../Service/STD_Types.h"
#include "warning_lamps.h"
#include "../MCL/GPIO/gpio_interface.h"

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

uint8_h g_lamp_mask = 0;

void LMP_Refresh(uint8_h mask)
{
    g_lamp_mask = mask;
    SR_ShiftByte(mask);
}

void WRN_Update(void)
{
    extern uint16_h g_fuel_pct;
    extern uint16_h g_oil_barx10;
    extern uint32_h g_batt_mv;
    extern uint16_h g_coolant_c;
    extern uint16_h g_rpm;
    extern uint8_h g_blink_state;
    extern uint8_h g_turn_left;
    extern uint8_h g_turn_right;
    extern uint8_h g_hazard;

    uint8_h mask = 0;
    if (g_fuel_pct   < 10u)                          mask |= LAMP_LOW_FUEL;
    if (g_oil_barx10 < 10u && g_rpm > 500u)          mask |= LAMP_OIL;
    if (g_batt_mv    < 12000u || g_batt_mv > 15000u) mask |= LAMP_BATTERY;
    if (g_coolant_c  > 110u)                         mask |= LAMP_COOLANT;

    if ((g_turn_left  || g_hazard) && g_blink_state) mask |= LAMP_LEFT_TURN;
    if ((g_turn_right || g_hazard) && g_blink_state) mask |= LAMP_RIGHT_TURN;

    LMP_Refresh(mask);
}
