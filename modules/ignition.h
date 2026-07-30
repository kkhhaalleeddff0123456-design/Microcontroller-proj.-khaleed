#ifndef IGNITION_H
#define IGNITION_H

#include "../Service/STD_Types.h"

typedef enum { IGN_OFF, IGN_ACC, IGN_ON, IGN_RUNNING, IGN_STALL } IgnState;
extern IgnState g_ign;
extern uint16_h g_ign_timer;

void FSM_Run(void);

#endif // IGNITION_H
