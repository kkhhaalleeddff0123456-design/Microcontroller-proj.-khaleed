#include "../Service/STD_Types.h"
#include "../MCL/Timer/timer_registers.h"
#include "../MCL/GPIO/gpio_interface.h"

void CHM_Beep(uint8_h on)
{
    if (on)
    {
        TIMER_TCCR2_REG = (1<<6)|(1<<3)|(1<<1)|(1<<0);
        TIMER_OCR2_REG  = 127;
    }
    else
    {
        TIMER_TCCR2_REG = 0;
        GPIO_SetPinValue(GPIO_PORTD, GPIO_PIN7, 0);
    }
}
