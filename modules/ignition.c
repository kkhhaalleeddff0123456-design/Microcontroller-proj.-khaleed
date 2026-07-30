#include "../Service/STD_Types.h"
#include "ignition.h"
#include "tachometer.h"

IgnState g_ign = IGN_OFF;
uint16_h g_ign_timer = 0;

void FSM_Run(void)
{
    switch (g_ign)
    {
        case IGN_OFF:
            g_ign = IGN_ACC;
            break;
        case IGN_ACC:
            g_ign = IGN_ON;
            break;
        case IGN_ON:
            if (g_rpm > 500u) { g_ign_timer = 0; g_ign = IGN_RUNNING; }
            break;
        case IGN_RUNNING:
            if (g_rpm < 300u) { g_ign_timer++; if (g_ign_timer > 4u) g_ign = IGN_STALL; }
            else g_ign_timer = 0;
            break;
        case IGN_STALL:
            if (g_rpm > 500u) g_ign = IGN_RUNNING;
            break;
    }
}
