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

#ifndef MAIN_H
#define	MAIN_H

#include "mcc_generated_files/adc.h"

enum {
    INIT = 0,
    SLEEP,
    IDLE,
    CHARGING,
    CHARGING_WAIT,
    CELL_BALANCE,
    OUTPUT_EN,
    ERROR,
} state;

typedef enum{
    NONE = 0,       //0b00
    TRIGGER = 1,    //0b01
    CHARGER = 2     //0b10
} detect_t;

detect_t detect = 0;
//#define DETECT_HISTORY_SIZE 4
//detect_t detect_history[DETECT_HISTORY_SIZE] = 0;
//uint8_t oldest_detect_index = 0;

uint8_t detect_history = 0; //Bits 0-1 = position 0; Bits 2-3 = position 1; Bits 4-5 = position 2; Bits 6-7 = position 3

uint16_t adc_chrg_trig_detect_voltage_history[5];

uint8_t pack_charge_wait_repeats_counter = 0; // Counter for WaitCharge -> Charge repetations

uint8_t enable_slow_charge = 0; // for slow charging if mincell voltage below 3000mV but no Error 

typedef enum {
    SV09 = 0,
    SV11 = 1,
    NUM_OF_MODELS,
} modelnum_t;
modelnum_t modelnum;

int16_t isl_int_temp;
int16_t isl_int_temp_max = 0;
uint8_t thermistor_temp;
uint8_t thermistor_temp_max = 0;
bool charge_complete_flag = false;
bool full_discharge_flag = false;
uint16_t discharge_current_mA = 0;
uint16_t discharge_current_mA_last_trigger = 0;  //last discharge current while on trigger to calculate internal resistance
uint16_t discharge_current_mA_last_trigger_eeprom = 0; //record for eeprom discharge current for highest internal resistence
uint16_t mincell_voltage_mV_last_trigger = 0xFFFF;  //last discharge mincell voltage while on trigger to calculate internal resistance
uint8_t minimum_cell_last_trigger = 0; // record which wass the cell with the miniumum voltage when last output was enabled
uint32_t mincell_internal_resistence_last_charge_uOhms = 0;  //will be calculated from difference in voltage mincell between when trigger last pulled to charge start
uint32_t mincell_internal_resistence_max = 0; //record highest recorded internal resistence
uint8_t led_charge_indicator = 0; //to control the thee blue LEDs indicating charge state 0b111 = full charge, 0b100 = low charge
uint16_t packdelta_end_of_charging_wait_mV = 0;
uint16_t mincell_voltage_mV_last_trigger_eeprom = 0;
uint16_t mincell_voltage_mV_last_trigger_recover_eeprom = 0;
uint8_t I2C_error_counter = 0;
int8_t cell_offset_voltage_1 = 0;
int8_t cell_offset_voltage_2 = 0;
int8_t cell_offset_voltage_3 = 0;
int8_t cell_offset_voltage_4 = 0;
int8_t cell_offset_voltage_5 = 0;
int8_t cell_offset_voltage_6 = 0;
uint16_t MAX_CHARGE_CELL_VOLTAGE_mV = 4150;       //Charging will stop when max cell voltage goes above this value. Dynamic Value, either MAX_CHARGE_CELL_VOLTAGE_ECO_mV or MAX_CHARGE_CELL_VOLTAGE_BOOST_mV
uint8_t CHARGE_BOOST_ACTIVE = 1;  //Charge to Boost Voltage: 0 = No; 1 = Yes
uint8_t charge_state_memory = 3; //Charge state to keep in memory, charge states 1-3;
uint8_t charge_state_modify_flag = 0; // 0: no modification, 1: free modification, 2: only decrease, 3: only increase
uint8_t allow_override_charge_state_modify_flag_after_charging = 0;

typedef struct {
    uint16_t value;
    bool enable;
} counter_t;

typedef struct {
    uint32_t value;
    bool enable;
} big_counter_t;

counter_t charge_wait_counter = {0,0};
counter_t sleep_timeout_counter = {0,0};
counter_t nonblocking_wait_counter = {0,0};
counter_t error_timeout_wait_counter = {0,0};
big_counter_t charge_duration_counter = {0,0};
counter_t LED_code_cycle_counter = {0,0};
big_counter_t total_runtime_counter = {0,0};
big_counter_t onetime_runtime_counter = {0,0};

detect_t GetDetectHistory(uint8_t position);
bool CheckStateInDetectHistory(detect_t detect_val);

uint16_t readADCmV(adc_channel_t channel);
void Write32BitUintVariableToEEPROM(uint8_t starting_addr, uint32_t variable_to_write);
uint32_t Read32BitUintVariableFromEEPROM(uint8_t starting_addr);
void Write16BitUintVariableToEEPROM(uint8_t starting_addr, uint16_t variable_to_write);
uint16_t Read16BitUintVariableFromEEPROM(uint8_t starting_addr);
void ClearI2CBus(void);
uint8_t CalculateChargeIndicator(void);
void blinkError(uint8_t count);






#endif	/* MAIN_H */