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

static uint8_t breathing_duty = 255;
static bool breathing_direction = 0; // 0 = decreasing duty (brightening), 1 = increasing duty (dimming)

// Example: ledBlinkpattern (3, 0b110, 250, 250, 750, 500, 32);
// This would give 3 yellow (0b110 RGB) blinks, with 250ms ontime and 250ms off time
// There would be a 750ms blank time before and 500ms blank time after the three blinks
// There is a PWM fade slope of positive 32, so each blink will fade in at a rate of +32 to the PWM level per loop iteration
void ledBlinkpattern (uint8_t num_blinks, uint8_t led_color_rgb,
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
        Set_LED_RGB(0b000);     //Turn off LED
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
            Set_LED_RGB(led_color_rgb);     //Turn on LED
        }
        next_step_time += blink_on_time_ms;
    }
    else if (step == max_steps-1 && timer_ms > next_step_time){         //ending blank time
        step++;
        Set_LED_RGB(0b000);     //Turn off LED
        next_step_time += ending_blank_time_ms;
    }
    else if (step == max_steps && timer_ms > next_step_time){           //cycle complete
        Set_LED_RGB(0b000);     //Turn off LED
        nonblocking_wait_counter.enable = 0;
        nonblocking_wait_counter.value = 0;
    }
    else if (step % 2 != 0 && timer_ms > next_step_time){        //Step number is odd
        step++;
        Set_LED_RGB(0b000);     //Turn off LED
        next_step_time += blink_off_time_ms;
    }
    else if (step % 2 == 0 && timer_ms > next_step_time){       //Step number is even
        step++;
        Set_LED_RGB(led_color_rgb);     //Turn on LED
        next_step_time += blink_on_time_ms;
    }
        
}

void errorBlinkpattern (uint8_t num_blinks){
    ledBlinkpattern(num_blinks, 0b100, 500, 500, 1000, 1000);
}

void ledBreathingUpdate(uint8_t RGB_en) {
    // Increment/decrement duty cycle every 32ms (called from main loop timer check)
    if (breathing_direction) {
        if (breathing_duty < (255 - LED_BREATHING_STEP)) breathing_duty += LED_BREATHING_STEP;
        else breathing_direction = 0;
    } else {
        if (breathing_duty > LED_BREATHING_STEP) breathing_duty -= LED_BREATHING_STEP;
        else breathing_direction = 1;
    }
    
    CCPR1L = breathing_duty;
    
    // Enable steering to the requested LED
    PSTR1CON = 0; // Clear first
    if (RGB_en & 0b001) blueLED = 1;
    if (RGB_en & 0b010) greenLED = 1;
    if (RGB_en & 0b100) redLED = 1;
}

void resetLEDBlinkPattern (void){
    PSTR1CON = 0;           // Disable PWM steering
    Set_LED_RGB(0b000);     // Turn off LED
    nonblocking_wait_counter.enable = false;
    nonblocking_wait_counter.value = 0;
    LED_code_cycle_counter.enable = false;
    LED_code_cycle_counter.value = 0;
    
    // Reset breathing state to start from zero light (255 duty) next time
    breathing_duty = 255;
    breathing_direction = 0; // Start by brightening
}
// Accepts binary input 0b000. Bit 2 = Red Enable. Bit 1 = Green Enable. Bit 0 = Red Enable. R.G.B.
// PWM_val sets PWM brightness level 0-1023
void Set_LED_RGB(uint8_t RGB_en){  
       
    if (RGB_en & 0b001){
        LATA = (LATA & 0b10111111); //Turns on blue LED
    }
    else{
        LATA = (LATA | 0b01000000); //Turns off blue LED
    }
    
    if (RGB_en & 0b010){
        LATB = (LATB & 0b11110111); //Turns on green LED
    }
    else{
        LATB = (LATB | 0b00001000); //Turns off green LED
    }
    
    if (RGB_en & 0b100){
        LATA = (LATA & 0b01111111); //Turns on red LED
    }
    else{
        LATA = (LATA | 0b10000000); //Turns off red LED
    }
}

bool cellDeltaLEDIndicator (void){
    uint8_t num_yellow_blinks = (uint8_t) ( (packdelta_end_of_charging_wait_mV / 50) + 1 );      //One blink 0mV - 50mV (so you always know it is there), Two blinks 50mV - 100mV
    LED_code_cycle_counter.enable = true;
	uint8_t mode_color = (MAX_CHARGE_CELL_VOLTAGE_mV == MAX_CHARGE_CELL_VOLTAGE_BOOST_mV) ? 0b001 : 0b010;
	ledBlinkpattern (num_yellow_blinks, mode_color, 250, 250, 750, 500);
    if (LED_code_cycle_counter.value > 1){
        resetLEDBlinkPattern();
        return true;
    }
    else {
        return false;
    }
}

bool cellVoltageLEDIndicator (void){
    static bool loaded_num_green_blinks = 0;
    static uint8_t wait_count = 0;
    static uint8_t num_green_blinks_res = 0;
    
    if (wait_count < 5){
        wait_count++;
        return false;   //Let three main loop iterations pass to give voltage measurements a chance to stabilize after a high current load which would cause lower than normal battery voltage readings.
    }
    else if (!loaded_num_green_blinks){
        num_green_blinks_res = CalculateChargeIndicator();
		loaded_num_green_blinks = 1;    //This reads and holds the number of blinks to be presented so the blink count won't change even if cell voltages do.
    }
    
    
    
    LED_code_cycle_counter.enable = true;
    ledBlinkpattern (num_green_blinks_res, 0b010, 250, 250, 500, 500);
    if (LED_code_cycle_counter.value > 1){
        resetLEDBlinkPattern();
        wait_count = 0;
        loaded_num_green_blinks = 0;
        return true;
    }
    else {
        return false;
    }
}