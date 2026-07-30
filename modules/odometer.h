#ifndef ODOMETER_H
#define ODOMETER_H

#include "../Service/STD_Types.h"

extern uint32_h g_odo_mm;
extern uint32_h g_trip_mm;

void ODO_AddPulse(void);

#endif // ODOMETER_H
