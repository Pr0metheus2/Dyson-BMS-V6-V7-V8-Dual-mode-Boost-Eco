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

#ifndef CONFIG_H
#define	CONFIG_H

#include <xc.h>
#include "mcc_generated_files/epwm1.h"

//Common Configuration Options////////////////////////////
const uint8_t MIN_CHARGE_START_TEMP_C = 15; //When connected to charger will wait until temperature is reached before charge start
const uint8_t MIN_CHARGE_TEMP_C = 12; //Fallig below this value during charging will generate an under temperature errer flag
const uint8_t MAX_CHARGE_START_TEMP_C = 40; //When connected to charger will wait until temperature is below this temperature to start charging
const uint8_t MAX_CHARGE_TEMP_C = 50;           //Celsius. MAX_DISCHARGE_TEMP_C must be greater than MAX_CHARGE_TEMP_C for it to work correctly.
const uint8_t MAX_DISCHARGE_TEMP_C = 60;        //Celsius. 70C max per LG 18650 HD2C datasheet. 60C limit was hit when doing full current 20A discharge test. 70C limit was hit during 20A discharge test too. These things run hot. Vacuum running in max mode consumes ~17A. Need to do full discharge test with vacuum in max mode. Might reduce temp limit since 70C scares me.
const uint8_t MIN_TEMP_C = 7; //Celsius. Charging and discharging will not work below this temperature. 7 degrees C is the lowest value in SV11 thermistor LUT. Must be > HYSTERESIS_TEMP_C to avoid potential overflow issues in getThermistorTemp.
const uint16_t MAX_DISCHARGE_CURRENT_mA = 30000;        //Current limit for the PIC measurement of current through the output shunt. (DISCHARGE_OC_SHUNT_PICREAD)
const uint16_t MIN_DISCHARGE_CELL_VOLTAGE_mV = 3000;    //The output will be disabled when the min cell voltage goes below this value. full_discharge_flag will be set.
const uint16_t CRITICAL_MIN_CELL_VOLTAGE_mV = 2000;   // no charging if one cell is below.
const uint16_t CELL_CUTOUT_VOLTAGE_DISCHARGE_mV = 2500; // no no cell is allowed to go below while under load
const uint16_t MAX_CHARGE_CELL_VOLTAGE_BOOST_mV = 4150;
const uint16_t MAX_CHARGE_CELL_VOLTAGE_ECO_mV = 4000;
const uint16_t PACK_CHARGE_NOT_COMPLETE_BOOST_THRESH_mV = 4050;
const uint16_t PACK_CHARGE_NOT_COMPLETE_ECO_THRESH_mV = 3900;
const uint8_t PACK_CHARGE_MAX_NUM_OF_CHARGE_WAIT_REPEATS = 20; //Limit the Number of WaitCharge -> Charge repetations


/* Comment this line out to have the pack always stay awake while the charger is connected.
 * This will increase idle power draw and cause charging to be enabled occasionally to keep the battery topped off.
 * I expect this repeated slow discharge (due to staying awake) and repeated top off cycles to decrease battery life.
 * I'm not sure how bad this will be though.
 * In theory, this option should let you keep a pack with a high level of self-discharge fully charged.
 * It won't help you if your cells are going out of balance though. */
#define SLEEP_AFTER_CHARGE_COMPLETE


///////////////////////////////////////////////////////////



#define FIRMWARE_VERSION 3.6
#define ASCII_FIRMWARE_VERSION 0x35;      //ASCII '5' = 0x35. This must be manually determined so the EEPROM section is human readable.

//Uncomment these lines while in debug mode to disable certain temperature checks for testing.
//#ifdef __DEBUG  //Make sure these only work in debug mode
//#define __DEBUG_DISABLE_PIC_THERMISTOR_READ       //Uncomment for testing
//#define __DEBUG_DISABLE_PIC_ISL_INT_READ          //Uncomment for testing
//#define __DEBUG_DONT_SLEEP
//#endif

//EEPROM Formatting Parameters
#define EEPROM_START_OF_EVENT_LOGS_ADDR 0x50
#define EEPROM_NEXT_BYTE_AVAIL_STORAGE_ADDR 0x19
#define EEPROM_RUNTIME_TOTAL_STARTING_ADDR 0x1C    //32-bit runtime counter to be held in 0x1C, 0x1D, 0x1E, 0x1F

#define EEPROM_ADDR_MIN_CELL_IR_LAST            0x20
#define EEPROM_ADDR_MIN_CELL_IR_MAX             0x24
#define EEPROM_ADDR_MIN_CELL_V_LAST_TRIG        0x28
#define EEPROM_ADDR_MIN_CELL_V_LAST_TRIG_RECOV  0x2A
#define EEPROM_ADDR_DISCHARGE_CURR_LAST_TRIG    0x2C
#define EEPROM_ADDR_THERMISTOR_TEMP_MAX         0x2E
#define EEPROM_ADDR_ISL_INT_TEMP_MAX            0x2F
#define EEPROM_ADDR_PACK_DELTA_END_CHARGE       0x30
#define EEPROM_ADDR_CELL_V1_SLEEP               0x38
#define EEPROM_ADDR_CELL_V2_SLEEP               0x3A
#define EEPROM_ADDR_CELL_V3_SLEEP               0x3C
#define EEPROM_ADDR_CELL_V4_SLEEP               0x3E
#define EEPROM_ADDR_CELL_V5_SLEEP               0x40
#define EEPROM_ADDR_CELL_V6_SLEEP               0x42
#define EEPROM_ADDR_MAX_CHARGE_VOLT             0x44
#define EEPROM_ADDR_CELL_OFFSET_1               0x48
#define EEPROM_ADDR_CELL_OFFSET_2               0x49
#define EEPROM_ADDR_CELL_OFFSET_3               0x4A
#define EEPROM_ADDR_CELL_OFFSET_4               0x4B
#define EEPROM_ADDR_CELL_OFFSET_5               0x4C
#define EEPROM_ADDR_CELL_OFFSET_6               0x4D

#define redLED PSTR1CONbits.STR1C
#define greenLED PSTR1CONbits.STR1B
#define blueLED PSTR1CONbits.STR1D

#define PORT_SDA PORTBbits.RB1
#define PORT_SCL PORTBbits.RB4
#define LAT_SDA LATBbits.LATB1
#define LAT_SCL LATBbits.LATB4
#define TRIS_SDA TRISBbits.TRISB1
#define TRIS_SCL TRISBbits.TRISB4
#define ANS_SDA ANSELBbits.ANSB1
#define ANS_SCL ANSELBbits.ANSB4

#define ISL_I2C_ADDR 0x50

#define ADC_DISCHARGE_ISENSE 0x0
#define ADC_THERMISTOR 0x1
#define ADC_ISL_OUT 0x4
#define ADC_PIC_INT_TEMP 0x1D
#define ADC_PIC_DAC 0x1E
#define ADC_PIC_FVR 0x1F
#define ADC_CHRG_TRIG_DETECT 0x07
#define ADC_SV09CHECK 0x0A

const uint16_t VREF_VOLTAGE_mV = 2500;

const uint16_t DETECT_CHARGER_THRESH_mV = 1500;
const uint16_t DETECT_TRIGGER_THRESH_mV = 200;

const uint8_t HYSTERESIS_TEMP_C = 3;

/* Mark charge complete if a charging cycle takes less than this amount of time.
313 * 32ms = 10.016s, if it took less than 10 seconds for max cell voltage to be > 4.20v, mark charge complete */
const uint16_t CHARGE_COMPELTE_TIMEOUT = 313;   


/* Length of time to wait between charge cycles
 2188 * 32ms = 70.016 seconds*/
const uint16_t CHARGE_WAIT_TIMEOUT = 2188;

/* Length of time to wait while idle before going to sleep
 1876*32ms = 60.032s */
const uint16_t IDLE_SLEEP_TIMEOUT = 1876;

/* Length of time to wait while in an error state before going to sleep
 1876*32ms = 60.032s */
const uint16_t ERROR_SLEEP_TIMEOUT = 1876;

/* Length of time there must be no errors before exiting error state
 94 * 32ms = 3.008s */
const uint8_t ERROR_EXIT_TIMEOUT = 94;

/* Number of times the LED error code will be presented after the fault is cleared and trigger/charger removed.
 Ensures the error code won't immediately go away once the user stops pulling the trigger.*/
const uint8_t NUM_OF_LED_CODES_AFTER_FAULT_CLEAR = 3;

/* Comment this line out to disable the cell voltage rolling average */
#define ENABLE_CELL_VOLTAGE_ROLLING_AVERAGE

/* Number of samples to include in the cell voltage rolling averaging*/
#define CELLVOLTAGE_AVERAGE_WINDOW_SIZE 15 // max. 15 otherwise risk of overflow and wrong voltage readings

const uint8_t CRITICAL_I2C_ERROR_THRESH = 2;    //If there are 2 consecutive I2C error results, even after attempting to recover, set flag for critical I2C error, show blink code, and then RESET.

/* LED Breathing speed step. Smaller = Slower. 
 * Current value 5 = ~3.2s full cycle. */
const uint8_t LED_BREATHING_STEP = 8; // higher the number, faster breathing effect












#ifdef	__cplusplus
extern "C" {
#endif




#ifdef	__cplusplus
}
#endif

#endif	/* CONFIG_H */

