#include "../Service/STD_Types.h"

uint32_h g_odo_mm   = 0;
uint32_h g_trip_mm  = 0;

void ODO_AddPulse(void)
{
    g_odo_mm  += 500u;
    g_trip_mm += 500u;
}
