#ifndef EPWM1_H
#define EPWM1_H

#include <stdint.h>

void EPWM1_Initialize(void);
void EPWM1_DisableSteering(void);
void EPWM1_SetActiveLowBrightness(uint8_t brightness);
void EPWM1_SteerToChargingBlueLED(uint8_t blue_led_mask);

#endif /* EPWM1_H */
