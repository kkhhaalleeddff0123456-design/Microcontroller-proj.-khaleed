#include "../Service/STD_Types.h"
#include "../Service/Bit_Math.h"
#include "../MCL/ADC/adc_interface.h"

uint16_h g_fuel_pct   = 0;
uint16_h g_coolant_c  = 0;
uint32_h g_batt_mv    = 0;
uint16_h g_oil_barx10 = 0;

void GAU_Update(void)
{
    uint16_h raw;
    ADC_ReadChannelBlocking(ADC_CHANNEL0, &raw);
    g_fuel_pct   = (uint16_h)(((uint32_h)raw * 100u) / 1023u);

    ADC_ReadChannelBlocking(ADC_CHANNEL1, &raw);
    g_coolant_c  = (uint16_h)((((uint32_h)raw * 170u) / 1023u) + 40u);

    ADC_ReadChannelBlocking(ADC_CHANNEL2, &raw);
    g_batt_mv    = ((uint32_h)raw * 16000u) / 1023u;

    ADC_ReadChannelBlocking(ADC_CHANNEL3, &raw);
    g_oil_barx10 = (uint16_h)(((uint32_h)raw * 100u) / 1023u);
}
