#ifndef TURNS_H
#define TURNS_H

#include "../Service/STD_Types.h"

extern uint8_h g_turn_left;
extern uint8_h g_turn_right;
extern uint8_h g_hazard;
extern uint8_h g_blink_state;

void TRN_Update(void);

#endif // TURNS_H
