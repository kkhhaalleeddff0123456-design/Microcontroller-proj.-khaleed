#ifndef TACHOMETER_H
#define TACHOMETER_H

#include "../Service/STD_Types.h"

extern volatile uint16_h g_tac_count;
extern uint16_h g_rpm;

void TAC_Update250ms(void);

#endif // TACHOMETER_H
