#ifndef GAUGES_H
#define GAUGES_H

#include "../Service/STD_Types.h"

extern uint16_h g_fuel_pct;
extern uint16_h g_coolant_c;
extern uint32_h g_batt_mv;
extern uint16_h g_oil_barx10;

void GAU_Update(void);

#endif // GAUGES_H
