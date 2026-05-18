/*
* FU-Dyson-BMS	-	(unofficial) Firmware Upgrade for Dyson BMS - V6/V7 Vacuums
* Copyright (C) 2022 tinfever
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
* 
* The author can be contacted at tinfever6@(insert-everyone's-favorite-google-email-domain).com
* 
* NOTE: As an addendum to the GNU General Public License, any hardware using code or information from this project must also make publicly available complete electrical schematics and a bill of materials for such hardware.
*/

#include "main.h"
#include "LED.h"
#include "mcc_generated_files/epwm1.h"
#include "config.h"
#include "isl94208.h"

// Example: ledBlinkpattern (3, 0b110, 250, 250, 750, 500, 32);
// This would give 3 yellow (0b110 RGB) blinks, with 250ms ontime and 250ms off time
// There would be a 750ms blank time before and 500ms blank time after the three blinks
// There is a PWM fade slope of positive 32, so each blink will fade in at a rate of +32 to the PWM level per loop iteration
void ledBlinkpattern (uint8_t num_blinks, uint8_t led_color_rbbb,
                    uint16_t blink_on_time_ms, uint16_t blink_off_time_ms,
                    uint16_t starting_blank_time_ms, uint16_t ending_blank_time_ms){       
    uint16_t timer_ms = nonblocking_wait_counter.value*32;
    
    static uint8_t max_steps = 0; 
    static uint8_t step = 0;
    static uint16_t next_step_time = 0;
    
    /*Example step schedule for num_blinks = 3, blink_interval_ms = 500, starting_blank_time_ms = 1000, ending_blank_time_ms = 1000
    *Step #     End time    LED ON
    * 0          1000,      0     //starting blank interval
    * 1          1500,      1     //blink one
    * 2          2000,      0
    * 3          2500,      1     //blink two
    * 4          3000,      0
    * 5          3500,      1     //blink three
    * 6          4000,      0
    * 7          5000,      0     //ending blank interval
     */
        
    //Initialize
    if (!nonblocking_wait_counter.enable){
        Set_LED_RBBB(0b0000);     //Turn off LED
        nonblocking_wait_counter.value = 0;
        nonblocking_wait_counter.enable = 1;
        max_steps = (2*num_blinks+2)-1;     //Subtract one so it is zero indexed
        step = 0;
        next_step_time = starting_blank_time_ms;
        if (LED_code_cycle_counter.enable){
            LED_code_cycle_counter.value++;
        }
    }
    
    if (step == 0 && timer_ms > next_step_time){                        //starting blank time
        step++;
        if (num_blinks != 0){   // Leave LED off if num_blinks is zero. Next step will be cycle complete.
            Set_LED_RBBB(led_color_rbbb);     //Turn on LED
        }
        next_step_time += blink_on_time_ms;
    }
    else if (step == max_steps-1 && timer_ms > next_step_time){         //ending blank time
        step++;
        Set_LED_RBBB(0b0000);     //Turn off LED
        next_step_time += ending_blank_time_ms;
    }
    else if (step == max_steps && timer_ms > next_step_time){           //cycle complete
        Set_LED_RBBB(0b0000);     //Turn off LED
        nonblocking_wait_counter.enable = 0;
        nonblocking_wait_counter.value = 0;
    }
    else if (step % 2 != 0 && timer_ms > next_step_time){        //Step number is odd
        step++;
        Set_LED_RBBB(0b0000);     //Turn off LED
        next_step_time += blink_off_time_ms;
    }
    else if (step % 2 == 0 && timer_ms > next_step_time){       //Step number is even
        step++;
        Set_LED_RBBB(led_color_rbbb);     //Turn on LED
        next_step_time += blink_on_time_ms;
    }
       
}

void resetLEDBlinkPattern (void){
    Set_LED_RBBB(0b0000);     //Turn off LED
    nonblocking_wait_counter.enable = false;
    nonblocking_wait_counter.value = 0;
    LED_code_cycle_counter.enable = false;
    LED_code_cycle_counter.value = 0;
    
}
// Accepts binary input 0b000. Bit 2 = Red Enable. Bit 1 = Green Enable. Bit 0 = Red Enable. R.G.B.
// PWM_val sets PWM brightness level 0-1023
void Set_LED_RBBB(uint8_t RBBB_en){  
    EPWM1_DisableSteering();
        
    if (RBBB_en & 0b1000){
        LATA = (LATA & 0b01111111); //Turns on red LED
    }
    else{
        LATA = (LATA | 0b10000000); //Turns off red LED
    }
    
    if (RBBB_en & 0b0001){
        LATA = (LATA & 0b10111111); //Turns on blue LED 1
    }
    else{
        LATA = (LATA | 0b01000000); //Turns off blue LED 1
    }
    
    if (RBBB_en & 0b0010){
        LATB = (LATB & 0b11111110); //Turns on blue LED 2
    }
    else{
        LATB = (LATB | 0b00000001); //Turns off blue LED 2
    }
    
    if (RBBB_en & 0b0100){
        LATB = (LATB & 0b11110111); //Turns on blue LED 3
    }
    else{
        LATB = (LATB | 0b00001000); //Turns off blue LED 3
    }
}

void SetChargingBreathLED(uint8_t charge_indicator){
    static bool epwm_initialized = false;
    uint8_t steady_leds = charge_indicator >> 1;
    uint8_t breathing_led = charge_indicator & ~steady_leds;
    uint8_t breath_phase = (uint8_t)(charge_duration_counter.value & 0x3F);
    uint8_t breath_level;
    
    if (breath_phase > 31){
        breath_phase = 63 - breath_phase;
    }
    
    if (!epwm_initialized){
        EPWM1_Initialize();
        epwm_initialized = true;
    }
    
    breath_level = breath_phase;
    Set_LED_RBBB(steady_leds);
    EPWM1_SetActiveLowBrightness(breath_level);
    EPWM1_SteerToChargingBlueLED(breathing_led);
}

void SetDischargeTriggerLED(uint8_t charge_indicator){
    uint8_t steady_leds = 0;
    uint8_t flashing_led = 0;
    uint8_t flash_phase;

    if (charge_indicator == 0b00001000){
        Set_LED_RBBB(0b0000);
        return;
    }

    if (charge_indicator == 0b00000001){
        steady_leds = 0b0000;
        flashing_led = 0b0001;
    }
    else if (charge_indicator == 0b00000011){
        steady_leds = 0b0001;
        flashing_led = 0b0010;
    }
    else if (charge_indicator == 0b00000111){
        steady_leds = 0b0011;
        flashing_led = 0b0100;
    }
    else{
        Set_LED_RBBB(charge_indicator);
        return;
    }
	// Flash phase speed
	// >> 3 = faster, about 256 ms per phase, that gives a full on/off cycle of about 0.5s
	// >> 4 = current, about 512 ms per phase, that gives a full on/off cycle of about 1s
	// >> 5 = slower, about 1024 ms per phase, that gives a full on/off cycle of about 2s
	flash_phase = (uint8_t)((onetime_runtime_counter.value >> 4) & 1);

    Set_LED_RBBB(steady_leds | (flash_phase ? flashing_led : 0));
}

bool cellDeltaLEDIndicator(void){
    uint8_t num_yellow_blinks = (uint8_t) ( (packdelta_end_of_charging_wait_mV / 50) + 1 );      //One blink 0mV - 50mV (so you always know it is there), Two blinks 50mV - 100mV
    LED_code_cycle_counter.enable = true;
	// Voltage balance indication according to mode selected: Boost = Red+B2+B3 (0b1110), Eco = Red+B3 (0b1100)
	uint8_t mode_color = (MAX_CHARGE_CELL_VOLTAGE_mV == MAX_CHARGE_CELL_VOLTAGE_BOOST_mV) ? 0b1110 : 0b1010;
	ledBlinkpattern (num_yellow_blinks, mode_color, 250, 250, 750, 500);
    if (LED_code_cycle_counter.value > 1){
        resetLEDBlinkPattern();
        return true;
    }
    else {
        return false;
    }
}
