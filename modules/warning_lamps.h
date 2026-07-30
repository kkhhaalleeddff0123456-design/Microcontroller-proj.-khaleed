#ifndef WARNING_LAMPS_H
#define WARNING_LAMPS_H

#include "../Service/STD_Types.h"

#define LAMP_LOW_FUEL    0x01
#define LAMP_OIL         0x02
#define LAMP_BATTERY     0x04
#define LAMP_COOLANT     0x08
#define LAMP_CHECK_ENG   0x10
#define LAMP_LEFT_TURN   0x20
#define LAMP_RIGHT_TURN  0x40
#define LAMP_HIGH_BEAM   0x80

extern uint8_h g_lamp_mask;

void LMP_Refresh(uint8_h mask);
void WRN_Update(void);

#endif // WARNING_LAMPS_H
