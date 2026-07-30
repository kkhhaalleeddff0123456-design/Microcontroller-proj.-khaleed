#ifndef SPEEDOMETER_H
#define SPEEDOMETER_H

#include "../Service/STD_Types.h"

extern volatile uint16_h g_spd_ovf;
extern volatile uint32_h g_spd_prev;
extern volatile uint32_h g_spd_delta;
extern volatile uint8_h  g_spd_new;
extern volatile uint32_h g_spd_last_cap;
extern volatile uint16_h g_wheel_pulses;
extern uint16_h g_speed_kmh;

void SPD_OnOverflow(void);
void SPD_Update(void);

#endif // SPEEDOMETER_H
