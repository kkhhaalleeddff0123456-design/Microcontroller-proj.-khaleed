#include "../Service/STD_Types.h"
#include <stdio.h>
#include "../MCL/UART/uart_interface.h"

extern uint16_h g_speed_kmh;
extern uint16_h g_rpm;
extern uint16_h g_fuel_pct;
extern uint16_h g_coolant_c;
extern uint32_h g_batt_mv;
extern uint16_h g_oil_barx10;

void CON_Report(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf),
             "SPD=%u RPM=%u FUEL=%u COOL=%u BATT=%lu OIL=%u\r\n",
             g_speed_kmh, g_rpm, g_fuel_pct,
             g_coolant_c, g_batt_mv, g_oil_barx10);
    UART_SendString((const uint8_h *)buf);
}
