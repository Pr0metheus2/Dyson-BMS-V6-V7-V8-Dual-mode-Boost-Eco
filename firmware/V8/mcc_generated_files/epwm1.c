#include <xc.h>
#include "epwm1.h"

void EPWM1_Initialize(void)
{
    EPWM1_DisableSteering();
    CCP1AS = 0x00;
    CCP1CON = 0x0C;
    PWM1CON = 0x00;
    EPWM1_SetActiveLowBrightness(0);
}

void EPWM1_DisableSteering(void)
{
    PSTR1CONbits.STR1A = 0;
    PSTR1CONbits.STR1B = 0;
    PSTR1CONbits.STR1C = 0;
    PSTR1CONbits.STR1D = 0;
}

void EPWM1_SetActiveLowBrightness(uint8_t brightness)
{
    uint16_t duty;
    
    if (brightness > 31){
        brightness = 31;
    }
    
    duty = 1023 - (((uint16_t)brightness * 1023) / 31);
    CCPR1L = (uint8_t)(duty >> 2);
    CCP1CONbits.DC1B = (uint8_t)(duty & 0x03);
}

void EPWM1_SteerToChargingBlueLED(uint8_t blue_led_mask)
{
    EPWM1_DisableSteering();
    
    if (blue_led_mask & 0b00000001){
        APFCON0bits.P1DSEL = 1;
        PSTR1CONbits.STR1D = 1;
    }
    else if (blue_led_mask & 0b00000010){
        APFCON0bits.CCP1SEL = 1;
        PSTR1CONbits.STR1A = 1;
    }
    else if (blue_led_mask & 0b00000100){
        APFCON0bits.CCP1SEL = 0;
        PSTR1CONbits.STR1A = 1;
    }
}
