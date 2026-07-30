#ifndef DISPLAY_H
#define DISPLAY_H

#include "../Service/STD_Types.h"
#include "../HAL/LCD_Hd44780/lcd_hd44780.h"

extern LCD_Hd44780_HandleType g_lcd;
typedef enum { PG_MAIN=0, PG_ENGINE, PG_ELECTRICAL, PG_TRIP, PG_DIAG, PG_MAX } PageType;
extern PageType g_page;
extern uint8_h g_button_event;

void DSP_Next(void);
void BTN_Update(void);
void DSP_Render(void);

#endif // DISPLAY_H
