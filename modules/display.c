#include "../Service/STD_Types.h"
#include <stdio.h>
#include "../MCL/GPIO/gpio_interface.h"
#include "../HAL/LCD_Hd44780/lcd_hd44780.h"
#include "display.h"
#include "odometer.h"
#include "warning_lamps.h"
#include "speedometer.h"
#include "tachometer.h"
#include "turns.h"
#include "gauges.h"

LCD_Hd44780_HandleType g_lcd;
PageType g_page = PG_MAIN;
uint8_h g_button_count = 0;
uint8_h g_button_state = 1;
uint8_h g_button_event = 0;

void DSP_Next(void)
{
    g_page = (PageType)((g_page + 1u) % PG_MAX);
}

void BTN_Update(void)
{
    uint8_h raw = GPIO_GetPinStatus(GPIO_PORTD, GPIO_PIN3);
    if (raw == 0)
    {
        if (g_button_count < 5u) g_button_count++;
    }
    else
    {
        if (g_button_count > 0u) g_button_count--;
    }

    if (g_button_count == 5u && g_button_state == 1)
    {
        g_button_state = 0;
        g_button_event = 1;
    }
    else if (g_button_count == 0u && g_button_state == 0)
    {
        g_button_state = 1;
    }
}

void DSP_Render(void)
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

        case PG_DIAG:
            snprintf(r0, 17, "PULSES:%4u SPD:%3u ", g_wheel_pulses, g_speed_kmh);
            snprintf(r1, 17, "RPM:%4u FUEL:%3u%% ", g_rpm, g_fuel_pct);
            break;

        default: break;
    }

    LCD_Hd44780_WriteStringAt(&g_lcd, 0, 0, (const uint8_h *)r0);
    LCD_Hd44780_WriteStringAt(&g_lcd, 1, 0, (const uint8_h *)r1);
}
