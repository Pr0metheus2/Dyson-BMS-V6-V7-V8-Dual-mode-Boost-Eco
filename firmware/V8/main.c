/* FU-Dyson-BMS	-	(unofficial) Firmware Upgrade for Dyson BMS - V8 Vacuums
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

#include "mcc_generated_files/mcc.h"
#include "main.h"
#include "i2c.h"
#include "isl94208.h"
#include "config.h"
#include "thermistor.h"
#include "LED.h"
#include "FaultHandling.h"

/*
* Breakdown of cells internal resistance mapping:
*  - 0x20 to 0x23: Stores the current internal resistance (mincell_internal_resistence_last_charge_uOhms).
*  - 0x24 to 0x27: Stores the overall greatest internal resistance (mincell_internal_resistence_max).
* 
* Values in the macro:
*  * 0x20: 0
*  * 0x21: 0
*  * 0x22: 0x2E
*  * 0x23: 0xE0
*  * 0x24: 0
*  * 0x25: 0
*  * 0x26: 0x2E
*  * 0x27: 0x2E
* 
* The values are stored in Little-Endian format. This means the initial 32-bit value for
* mincell_internal_resistence_max (stored at 0x24) is actually represented by the bytes 0x2E, 0x2E, 0, 0.
* 
* When interpreted as a 32-bit integer:
* 0x00002E2E = 11,822 μΩ (or 11.8 mΩ).
* 
* The BMS starts with a "default" internal resistance record of approximately 11.8 mΩ. If cells have a
* higher resistance than this default, the BMS will eventually overwrite this initial value once the
* charge/discharge cycle is performed.
*/


//EEPROM Init during programming
__EEPROM_DATA(0x54, 0x69, 0x6E, 0x66, 0x65, 0x76, 0x65, 0x72);              //"Tinfever"    EEPROM addresses 0x00 - 0x07
__EEPROM_DATA(0x20, 0x46, 0x55, 0x2D, 0x44, 0x79, 0x73, 0x6F);              //" FU-Dyso"    EEPROM addresses 0x08 - 0x0F
__EEPROM_DATA(0x6E, 0x2D, 0x42, 0x4D, 0x53, 0x20, 0x56, ASCII_FIRMWARE_VERSION);  //"n-BMS V{insert firmware version here}"     EEPROM addresses 0x10 - 0x17
__EEPROM_DATA(0, EEPROM_START_OF_EVENT_LOGS_ADDR, 0, 0, 0, 0, 0, 0);                                     //Address of the next available space for recording error events       EEPROM addresses 0x18 - 0x1F
__EEPROM_DATA(0, 0, 0x2E, 0xE0, 0, 0, 0x2E, 0x2E);    // Space to store 32bit uOhm current internal resistence of min cell (0x20) [mincell_internal_resistence_last_charge_uOhms] and overall greatest current internal resistence of min cell (0x24) [mincell_internal_resistence_max], EEPROM addresses 0x20 - 0x27
__EEPROM_DATA(0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0);    // Space to store 32bit, minimal mincell_voltage_mV_last_trigger (0x28) and minimal CellVoltages[minimum_cell_last_trigger] (0x2A) , EEPROM addresses 0x28 - 0x2F
//                                    discharge current when overall maximal internal resistence was set (0x2C), thermistor temp max (0x2E) internal_ISL_temp max (0x2F))
__EEPROM_DATA(0, 0, 0, 0, 0, 0, 0, 0); //(0x30 - 0x37), uint16_t packdelta_end_of_charging_wait_mV @ 0x30-0x31
__EEPROM_DATA(0, 0, 0, 0, 0, 0, 0, 0); //(0x38 - 0x3F) Cell voltage to be saved on sleep: V1:0x38, V2:0x3A, V3:0x3C, V4:0x3E
__EEPROM_DATA(0, 0, 0, 0, 0x10, 0x36, 0, 0); //(0x40 - 0x47) Cell voltage to be saved on sleep: V5:0x40, V6:0x42; MAX_CHARGE_CELL_VOLTAGE_mV: 0x44
__EEPROM_DATA(0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x03, 0); //(0x48 - 0x4F) Cell voltage offsets to be set by user: Voff1: 0x48, Voff2: 0x49, Voff3: 0x4A, Voff4: 0x4B, Voff5: 0x4C, Voff6: 0x4D, charge_state_memory = 3: 0x4E

void __interrupt() ISR_high() {
    PORTB;  // Read prot b to update the interrupt flag in hidden reigster
    NOP();  // if no daly is used neccessary before interrupt flag clear
    __delay_ms(50); // falling edge slow when charger removed, may trigger 2x without delay. not neccessary if only used for wake up.
    IOCBF = 0b00000000;  // clear interrupt Flag
}


void ClearI2CBus(){
    uint8_t initialState[] = {TRIS_SDA, TRIS_SCL, ANS_SDA, ANS_SCL, LAT_SDA, LAT_SCL, SSP1CON1bits.SSPEN}; //Backup initial pin setup state
    SSP1CON1bits.SSPEN = 0;     //Disable MSSP if enabled
    TRIS_SDA = 1;     //SDA - Set Data as input
    TRIS_SCL = 1;     //SCL - Set Clock as input = high-impedance output
    ANS_SDA = 0;      //Set both pins as digital
    ANS_SCL = 0;
    LAT_SCL = 0;    //Set SCL PORT to be low, then we'll toggle TRIS to switch between low and high-impedance
    
    uint8_t validOnes = 0;
    
    //Clock the SCL pin until we get 10 valid ones in a row like we'd expect from an idle bus
    while (validOnes < 10){
        TRIS_SCL = 0; //Clock low
        __delay_us(5); //Wait out clock low period
        TRIS_SCL = 1; //Clock high
        __delay_us(2.5);  //Wait until we are in the mid point of clock high period
        if (PORT_SDA == 1 && PORT_SCL == 1){  //Read data and check if SDA is high (idle). Also make sure SCL isn't getting glitched low
            validOnes++;
        }
        else{
            validOnes = 0;  //if the data isn't a one, reset the counter so we get 10 in a row
        }
        __delay_us(2.5); //Wait remainder of clock high period
    }
    TRIS_SDA = (__bit) initialState[0];   //Restore initial pin I/O state
    TRIS_SCL = (__bit) initialState[1];
    ANS_SDA = (__bit) initialState[2];
    ANS_SCL = (__bit) initialState[3];
    LAT_SDA = (__bit) initialState[4];
    LAT_SCL = (__bit) initialState[5];
    SSP1CON1bits.SSPEN = (__bit) initialState[6];
    if (initialState[6]){
        ISL_Init();
    }
    I2C_ERROR_FLAGS = 0;
}

uint16_t static ConvertADCtoMV(uint16_t adcval){                //I included this function here and in isl94208 so that it could stand alone if needed. There is probably a better way to do this.
    return (uint16_t) ((uint32_t)adcval * VREF_VOLTAGE_mV / 1024);
}

void ADCPrepare(void){
    DAC_SetOutput(0);   //Make sure DAC is set to 0V
    ADC_SelectChannel(ADC_PIC_DAC); //Connect ADC to 0V to empty internal ADC sample/hold capacitor
    __delay_us(1);  //Wait a little bit
}

uint16_t readADCmV(adc_channel_t channel){        //Adds routine to switch to DAC VSS output to clear sample/hold capacitor before taking real reading
    ADCPrepare();
    return ConvertADCtoMV( ADC_GetConversion(channel) ); //Finally run the conversion and store the result
}

uint16_t dischargeIsense_mA(void){
    ADCPrepare();
    uint16_t adcval = ADC_GetConversion(ADC_DISCHARGE_ISENSE);
    return (uint16_t) ((uint32_t)adcval * VREF_VOLTAGE_mV * 1000 / 1024 / 2);  //This better maintains precision by doing the multiplication for 2500mV VREF and 1000mA/A in one step as a uint32_t. Then we divide by 1024 ADC steps and the 2mOhm shunt resistor.
}

detect_t checkDetect(void){
    uint16_t result = readADCmV(ADC_CHRG_TRIG_DETECT);
    adc_chrg_trig_detect_voltage_history[4]=adc_chrg_trig_detect_voltage_history[3];
    adc_chrg_trig_detect_voltage_history[3]=adc_chrg_trig_detect_voltage_history[2];
    adc_chrg_trig_detect_voltage_history[2]=adc_chrg_trig_detect_voltage_history[1];
    adc_chrg_trig_detect_voltage_history[1]=adc_chrg_trig_detect_voltage_history[0];
    adc_chrg_trig_detect_voltage_history[0] = result;
  
    uint16_t min_adc_chrg_trig_detect_voltage = 65535;
    uint16_t max_adc_chrg_trig_detect_voltage = 0;
    
    uint8_t i = 0;
    while (i <= 4){
        if(adc_chrg_trig_detect_voltage_history[i] < min_adc_chrg_trig_detect_voltage){
            min_adc_chrg_trig_detect_voltage = adc_chrg_trig_detect_voltage_history[i];
        }
        if(adc_chrg_trig_detect_voltage_history[i] > max_adc_chrg_trig_detect_voltage){
            max_adc_chrg_trig_detect_voltage = adc_chrg_trig_detect_voltage_history[i];
        }
        i++;
    }
        
    if (result > DETECT_CHARGER_THRESH_mV){
        return CHARGER;
    }
    else if (result < DETECT_CHARGER_THRESH_mV
            && result > DETECT_TRIGGER_THRESH_mV
            && ((max_adc_chrg_trig_detect_voltage - min_adc_chrg_trig_detect_voltage) < 8) || state == OUTPUT_EN){ //If dischargeFET is off, Volatage needs to be stable to trigger. IF discharge FET is on it is sufficient to be within the thresholds
        pack_charge_wait_repeats_counter = 0;
        return TRIGGER;
    }
    else{
        pack_charge_wait_repeats_counter = 0;
        return NONE;
    }
}

modelnum_t checkModelNum (void){
/* This function assumes that if the reading of the thermistor from the ISL is
 * > 100mV above the reading from the PIC, we must be using an SV09 board 
 * which has an opamp driving the ISL thermistor input to ~3.3V until the thermistor voltage
 * goes below ~820mV, then it drives the ISL input to ~0V.
 * The SV11 has the thermistor input to the ISL and PIC tied together since
 * there is just the thermistor with one pull-up resistor. It uses a different voltage scale though. */
    uint16_t isl_thermistor_reading = ISL_GetAnalogOutmV(AO_EXTTEMP);
    uint16_t pic_thermistor_reading = readADCmV(ADC_THERMISTOR);
    int16_t delta = (int16_t)isl_thermistor_reading - (int16_t)pic_thermistor_reading;
    if (delta > 100){
        return SV09;
    }
    else{
        return SV11;
    }
}



////////////////////////////////////////////////////////////////////



void init(void){
//INIT STEPS
    
    I2C_ERROR_FLAGS = 0;
    
    /* Initialize the device */
    SYSTEM_Initialize();
    TMR4_StartTimer();   //Keep timer running
    DAC_SetOutput(0);   //Make sure DAC output is 0V = VSS

    //* Set up I2C pins */
    TRIS_SDA = 1;     //SDA - Make sure both pins are inputs
    TRIS_SCL = 1;     //SCL
    ANS_SDA = 0;    //Set both pins as digital
    ANS_SCL = 0;
    I2C1_Init();
    ClearI2CBus();  //Clear I2C bus once on startup just in case
    while (PORT_SDA == 0 || PORT_SCL == 0){   //If bus is still not idle (meaning pins aren't high), which shouldn't be possible, then keep trying to clear bus. Do not pass go. Do not collect $200.
    //    __debug_break();
        ClearI2CBus();
    }
    
    modelnum = checkModelNum();    
    
    //Load 32-bit total runtime counter from EEPROM
    total_runtime_counter.value = Read32BitUintVariableFromEEPROM(EEPROM_RUNTIME_TOTAL_STARTING_ADDR);  
    mincell_internal_resistence_last_charge_uOhms = Read32BitUintVariableFromEEPROM(0x20);
    packdelta_end_of_charging_wait_mV = Read16BitUintVariableFromEEPROM(0x30);
    mincell_internal_resistence_max = Read32BitUintVariableFromEEPROM(0x24);
    thermistor_temp_max = DATAEE_ReadByte(0x2E);
    isl_int_temp_max = (uint16_t) DATAEE_ReadByte(0x2F);
    cell_offset_voltage_1 = (int8_t) DATAEE_ReadByte(0x48);
    cell_offset_voltage_2 = (int8_t) DATAEE_ReadByte(0x49);
    cell_offset_voltage_3 = (int8_t) DATAEE_ReadByte(0x4A);
    cell_offset_voltage_4 = (int8_t) DATAEE_ReadByte(0x4B);
    cell_offset_voltage_5 = (int8_t) DATAEE_ReadByte(0x4C);
    cell_offset_voltage_6 = (int8_t) DATAEE_ReadByte(0x4D);
    charge_state_memory = (int8_t) DATAEE_ReadByte(0x4E);
    //Load MAX_CHARGE_CELL_VOLTAGE_mV from EEPROM
    MAX_CHARGE_CELL_VOLTAGE_mV = Read16BitUintVariableFromEEPROM(EEPROM_ADDR_MAX_CHARGE_VOLT);
    // Default to Boost mode if EEPROM is uninitialized or value is invalid
    if (MAX_CHARGE_CELL_VOLTAGE_mV != MAX_CHARGE_CELL_VOLTAGE_BOOST_mV && MAX_CHARGE_CELL_VOLTAGE_mV != MAX_CHARGE_CELL_VOLTAGE_ECO_mV){
        MAX_CHARGE_CELL_VOLTAGE_mV = MAX_CHARGE_CELL_VOLTAGE_BOOST_mV;
        Write16BitUintVariableToEEPROM(EEPROM_ADDR_MAX_CHARGE_VOLT, MAX_CHARGE_CELL_VOLTAGE_mV);
    }
    //INIT END
    
    if (modelnum == SV09){
        state = ERROR;
    }
    else{
        state = IDLE;
    }
}

void sleep_ISL_or_PIC(void){
    
    ISL_Write_Register(FETControl, 0b00000000);     //Make sure all FETs are disabled
     
    if (detect == CHARGER){
        state = CHARGING_WAIT;    // in case PIC somehow did not go to sleep
        ANSELB = 0b11010110; // Set RB5 to digital input to allow WAKEUP from removing the charger
        INTCON = 0b10001000; //Set bit 7 and bit 3 to enable global interrupts and enable interrupts on change
        IOCBP =  0b00100000;  //PIN RB5 IOC for rising edge, just in case
        IOCBN =  0b00100000;  //PIN RB5 IOC for falling edge 
        SLEEP();            // Put PIC to sleep, ISL stays awaye to avoid uneven current draw from cells. Expect approx. 1,2mA even power draw in this state
        NOP(); // PIC will be woken up by possibly empty interrupt routine and then excecution will continue from here
        ANSELB = 0b11110110; // Clear RB5 for analog input for regular operation as initialised
        INTCON = 0b00000000;  //Clear bit 7 and bit 3 to disable global interrupts and enable interrupts on change
        IOCBP = 0b00000000;  //PIN RB5 IOC off for rising edge
        IOCBN = 0b00000000;  //PIN RB5 IOC off for falling edge
    }
    else {
        //going to fully sleep. ISL will turn of PIC by cutting power. write some EEPROM Data first:
        Write32BitUintVariableToEEPROM(0x20, mincell_internal_resistence_last_charge_uOhms);
        if (mincell_internal_resistence_max > Read32BitUintVariableFromEEPROM(0x24)){
            Write32BitUintVariableToEEPROM(0x24, mincell_internal_resistence_max);
            Write16BitUintVariableToEEPROM(0x28, mincell_voltage_mV_last_trigger_eeprom);
            Write16BitUintVariableToEEPROM(0x2A, mincell_voltage_mV_last_trigger_recover_eeprom);
            Write16BitUintVariableToEEPROM(0x2C, discharge_current_mA_last_trigger_eeprom);
        }
        if (thermistor_temp_max > DATAEE_ReadByte(0x2E)){
            DATAEE_WriteByte(0x2E, thermistor_temp_max);
        }
        if ((uint8_t) isl_int_temp_max > DATAEE_ReadByte(0x2F)){
            DATAEE_WriteByte(0x2F, (uint8_t) isl_int_temp_max);
        }     
        Write16BitUintVariableToEEPROM(0x30, packdelta_end_of_charging_wait_mV);
        
        //Write current cell voltages (including applied offsets) to be compared with multimeter readings
        Write16BitUintVariableToEEPROM(0x38, CellVoltages[1]);
        Write16BitUintVariableToEEPROM(0x3A, CellVoltages[2]);
        Write16BitUintVariableToEEPROM(0x3C, CellVoltages[3]);
        Write16BitUintVariableToEEPROM(0x3E, CellVoltages[4]);
        Write16BitUintVariableToEEPROM(0x40, CellVoltages[5]);
        Write16BitUintVariableToEEPROM(0x42, CellVoltages[6]);
        
        //Write current charge_state_memory to EEPROM
        DATAEE_WriteByte(0x4E, charge_state_memory);
        
        //Write latest runtime counter value to EEPROM
        Write32BitUintVariableToEEPROM(EEPROM_RUNTIME_TOTAL_STARTING_ADDR, total_runtime_counter.value);
              
        state = IDLE;    // in case ISL does somehow not go to sleep       
        ISL_SetSpecificBits(ISL.SLEEP, 1);   // only one sleep command to ISL is necessary because we do not let the ISL sleep while on charger. It is stated in the data sheet that ISL will only go to sleep in another changer of sleep bit if WKUP is pulled high. We don�t need this anymore
        __delay_ms(250);                    // ISL will kill PIC when it goes to sleep because it shuts down VCC. PIC will wakeup together with ISL in reset state. ISL wakes up when charger is connected or Trigger is pulled/released.
        ClearI2CBus(); //If the ISL didn't actually sleep when we just told it to, something is seriously wrong. The best we can do is to try to reset the ISL.
        ISL_Init();    //This includes a POR reset of the ISL
        //this should leave the current draw at 2,5�A on cell 1 and 0,2�A on the other cells
    }
}

void idle(void){
    static bool previous_detect_was_charger = 0;
    static bool show_cell_delta_LEDs = 1;
    
    if (detect == TRIGGER                       //Trigger is pulled
        && minCellOK()          //Min cell is not below low voltage cut out of 3V
        && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS) //Make sure WKUP = 1 meaning charger connected or trigger pressed
        && full_discharge_flag == false             //Make sure pack hasn't just been fully discharged
        && safetyChecks()
        ){
            state = OUTPUT_EN;
    }
    else if (detect == TRIGGER
        && full_discharge_flag == true
        ){
            state = ERROR;
    }
    else if (detect == CHARGER                       //Charger is connected
            && charge_complete_flag == false         //We haven't already done a complete charge cycle
            && maxCellOK()          //Max cell < 4.20V
            && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS) //Make sure WKUP = 1 meaning charger connected or trigger pressed
            && safetyChecks()
        ){
        sleep_timeout_counter.enable = false;
        if ((show_cell_delta_LEDs == 1 && cellDeltaLEDIndicator())
            || !show_cell_delta_LEDs
            ) {
            if (CHARGE_BOOST_ACTIVE == 0){
                MAX_CHARGE_CELL_VOLTAGE_mV = MAX_CHARGE_CELL_VOLTAGE_ECO_mV;
                Write16BitUintVariableToEEPROM(EEPROM_ADDR_MAX_CHARGE_VOLT, MAX_CHARGE_CELL_VOLTAGE_ECO_mV);
            }
            // CHARGE_BOOST_ACTIVE = 0;
            if (maxCellOK()){
                charge_wait_counter.enable = true; 
                charge_wait_counter.value = CHARGE_WAIT_TIMEOUT;
                state = CHARGING_WAIT;
            }
            else {
                charge_complete_flag =true;
                show_cell_delta_LEDs = 0;
            }
        }   
    }
    else if ((detect == NONE                         //Start sleep counter if we are idle with no charger or trigger, but no errors
#ifdef SLEEP_AFTER_CHARGE_COMPLETE
            || (detect == CHARGER && charge_complete_flag) //Also sleep after charge is complete while we are on the charger, if configured.
#endif
            )
#ifndef SLEEP_AFTER_CHARGE_COMPLETE
            && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS) == 0
#endif
            && sleep_timeout_counter.enable == false
            && safetyChecks()
            ){
                sleep_timeout_counter.value = 0;        //Clear and start sleep counter
                sleep_timeout_counter.enable = true;
    }
    else if (!safetyChecks()){                        //Somehow there was an error  
        state = ERROR;
    }
    else if (detect == CHARGER                  //Set charge_complete_flag if pack is put on charger but max cell is already at maximum voltage
            && charge_complete_flag == false
            && !maxCellOK()
            ){
                charge_complete_flag = true;
				sleep_timeout_counter.value = 0;	// Reset timer when charger connected
            }
    else if ((detect == CHARGER && charge_complete_flag) && ((show_cell_delta_LEDs == 1 && cellDeltaLEDIndicator())
            || !show_cell_delta_LEDs)){    //Pack on charger and charge is complete = Green LED
		Set_LED_RBBB(0b0111); //LED Full
        show_cell_delta_LEDs = 0;
    }
    //else if (detect == CHARGER && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)) {                           //Pack on charger but charge isn't complete and we aren't in a charging or error state = Yellow LED, undefined state
    //    Set_LED_RBBB(0b1000); //LED Red
    //}
    else if (detect == NONE){                               
        if (CheckStateInDetectHistory(CHARGER)){                                //Show cell delta code when charger removed after complete charge
            previous_detect_was_charger = true;                             //This flag is set if we are transitioning from detect == CHARGER to detect == NONE
            allow_override_charge_state_modify_flag_after_charging = 1;
            
            // Force reset of sleep timeout to ensure full 15s interval
            sleep_timeout_counter.value = 0;
            sleep_timeout_counter.enable = true;
        }
          
        
         if ((previous_detect_was_charger && cellDeltaLEDIndicator()) || !previous_detect_was_charger)    
        {       //If the Charger -> None transition was detected, keep checking/running the cellDeltaLEDIndicator function until it is complete. 
            charge_state_modify_flag = 0; // disallow decrease of charge state (if idle for less than one second)
            if (sleep_timeout_counter.value >= 160 || allow_override_charge_state_modify_flag_after_charging == 1){
                charge_state_modify_flag = 2; // allow decrease of charge state
            }
            Set_LED_RBBB(CalculateChargeIndicator()); //Idle, show charge state 
            previous_detect_was_charger = false;                            //Then remove flag
            show_cell_delta_LEDs = 1; //Set this just in case
        }
    }
    //else if (detect == TRIGGER && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS) && !full_discharge_flag){    //trigger is pulled but we didn't enable output, and it isn't because there was an error or the pack is fully discharged = Yellow LED
            //There is a delay inside ISL IC between when detect == TRIGGER and when WKUP == 1. adding && WKUP_STATUS make sure both are in the same state to avoid brief yellow LED flash.
    //    Set_LED_RBBB(0b1000); //LED Red
    //}
    
    //There is no handling for WKUP_STATUS and DETECT to be in different states. If WKUP_STATUS == 0 (don't wakeup) but DETECT == TRIGGER, God help us all.
    
    //If the charger is connected, we are in a fully charged idle state, the user holds down the trigger, and then disconnects the charger while holding the trigger, the cell balance LED indicator is not shown. Not worth it to implement.
    
    // If the max cell voltage goes under this threshold while the charger is connected but idle, the charge_complete_flag will be cleared and charging will restart.
    uint16_t current_resume_thresh = (MAX_CHARGE_CELL_VOLTAGE_mV == MAX_CHARGE_CELL_VOLTAGE_BOOST_mV) ? PACK_CHARGE_NOT_COMPLETE_THRESH_BOOST_mV : PACK_CHARGE_NOT_COMPLETE_THRESH_ECO_mV;
    if (charge_complete_flag == true && cellstats.maxcell_mV < current_resume_thresh && detect != CHARGER){
        charge_complete_flag = false;
        show_cell_delta_LEDs = 1;
    }
    
    if (sleep_timeout_counter.value >= 64 && sleep_timeout_counter.enable == true){ //to use cell recovery of 5000ms for internal resistence calculation, and 2000ms for recovery over full discharge flag
        if (full_discharge_flag == false && !minCellOK() && detect != CHARGER){         //If min cell voltage is too low and we aren't on the charger, set full_discharge_flag so we can reference it for LED codes.
            full_discharge_flag = true;
            mincell_internal_resistence_max = mincell_internal_resistence_last_charge_uOhms; // This mean due to some reason mincell_internal_resistence_max is set wrong
            charge_state_memory = 1; //overwrite, disgarding charge_state_modify_flag
        }     
        if (sleep_timeout_counter.value >= 160 && mincell_voltage_mV_last_trigger < CellVoltages[minimum_cell_last_trigger] && discharge_current_mA_last_trigger >= 4000 && onetime_runtime_counter.value > 156){ // to avoid negative overflow and at least5 seconds of 5A of discharge current recorded
            mincell_internal_resistence_last_charge_uOhms = 1000000 * (CellVoltages[minimum_cell_last_trigger] - mincell_voltage_mV_last_trigger) / discharge_current_mA_last_trigger; // estimate internal resistence of the lowest cell
            if (mincell_internal_resistence_last_charge_uOhms > mincell_internal_resistence_max){
                mincell_internal_resistence_max = mincell_internal_resistence_last_charge_uOhms;
                mincell_voltage_mV_last_trigger_eeprom = mincell_voltage_mV_last_trigger;
                mincell_voltage_mV_last_trigger_recover_eeprom = CellVoltages[minimum_cell_last_trigger];
                discharge_current_mA_last_trigger_eeprom = discharge_current_mA_last_trigger;
            }
			mincell_voltage_mV_last_trigger = 0xFFFF; //to only do it once 
        }
    }
  
    if (sleep_timeout_counter.value >  IDLE_SLEEP_TIMEOUT && sleep_timeout_counter.enable == true){    //938*32ms = 30.016s //If we are in IDLE state for 30 seconds and not on the charger, go to sleep. We will stay awake on the charger since we have power to spare and can then make sure battery voltages don't drop over time.
        sleep_timeout_counter.enable = false;
        sleep_timeout_counter.value = 0;
        state = SLEEP;
    }
    
    //Clean up before going to different state
    if (state != IDLE) {
        sleep_timeout_counter.enable = false; //We aren't going to be sleeping soon
        resetLEDBlinkPattern();
        previous_detect_was_charger = false;
        allow_override_charge_state_modify_flag_after_charging = 0;
        show_cell_delta_LEDs = 1;
    }
    
}

void charging(void){
    
    mincell_voltage_mV_last_trigger = 0xFFFF; //to avoid wrong calculation of internal resistence (in case not going from discharge to void directly)
    
    if (cellstats.mincell_mV < CRITICAL_MIN_CELL_VOLTAGE_mV){ // No Error handling implemented. Will result in 20 times blink pattern. Will need to disassamble pack and check the cells manually
        state = ERROR;
        return;
    }
    if (cellstats.mincell_mV < MIN_DISCHARGE_CELL_VOLTAGE_mV){ // enable slow charging, max 10 seconds at a time, then charge wait.
        enable_slow_charge = 1;
    }
    if (!ISL_GetSpecificBits_cached(ISL.ENABLE_CHARGE_FET) && cellstats.mincell_mV >= MIN_DISCHARGE_CELL_VOLTAGE_mV){
        enable_slow_charge = 0;
    }
    
    if (!ISL_GetSpecificBits_cached(ISL.ENABLE_CHARGE_FET)     //if we aren't already charging
        && detect == CHARGER
        && maxCellOK()
        && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)
        && safetyChecks()
        && chargeTempCheck()
        ){      
        charge_duration_counter.value = 0;
        charge_duration_counter.enable = true;          //Start charge timer
        ISL_SetSpecificBits(ISL.ENABLE_CHARGE_FET, 1);  //Enable Charging
        full_discharge_flag = false;                    //Clear full discharge flag once we start charging
        resetLEDBlinkPattern();
    }
    else if (ISL_GetSpecificBits_cached(ISL.ENABLE_CHARGE_FET)     //same as above but we are already charging and all conditions are good
        && detect == CHARGER
        && maxCellOK()
        && ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)
        && safetyChecks()
        && chargeTempCheck()
        && (enable_slow_charge == 0 || charge_duration_counter.value < CHARGE_COMPELTE_TIMEOUT)  // allow 10 seconds of charging if slow charging is activated
            ){
        charge_state_modify_flag = 3; //Allow increase of charge level
        SetChargingBreathLED(CalculateChargeIndicator());
    }
    else if (!maxCellOK() //Target voltage reached
            || (enable_slow_charge == 1 && charge_duration_counter.value >= CHARGE_COMPELTE_TIMEOUT)){    // Or slow charge and 10 seconds of charge reached     
        ISL_SetSpecificBits(ISL.ENABLE_CHARGE_FET, 0); //Disable Charging
        charge_duration_counter.enable = false;         //Stop charge timer
        if (charge_duration_counter.value < CHARGE_COMPELTE_TIMEOUT                             //313 * 32ms = 10.016s, if it took less than 10 seconds for max cell voltage to be > 4.20v, mark charge complete
            || pack_charge_wait_repeats_counter >= PACK_CHARGE_MAX_NUM_OF_CHARGE_WAIT_REPEATS){  // too many charge wait repetations
            charge_complete_flag = true;
            state = CHARGING_WAIT;
        }
        else if (enable_slow_charge == 1){
            state = CHARGING_WAIT;
        }
        else{       //Go to charge wait state and wait 70 seconds before starting next charge cycle
            state = CHARGING_WAIT;
            pack_charge_wait_repeats_counter++;
        }
    }
    else if (!safetyChecks() || !chargeTempCheck()){     //There was an error
        ISL_SetSpecificBits(ISL.ENABLE_CHARGE_FET, 0); //Disable Charging
        charge_duration_counter.enable = false;         //Stop charge timer
        state = ERROR;
    }
    else{                                   //charger removed before complete charge
        ISL_SetSpecificBits(ISL.ENABLE_CHARGE_FET, 0); //Disable Charging
        charge_duration_counter.enable = false;         //Stop charge timer
        state = IDLE;
    }
    //Clean up before state change
    if (state != CHARGING){
        resetLEDBlinkPattern();
    }
    
}

void chargingWait(void){
    if(!(isl_int_temp > MIN_CHARGE_START_TEMP_C && thermistor_temp > MIN_CHARGE_START_TEMP_C && isl_int_temp < MAX_CHARGE_START_TEMP_C && thermistor_temp < MAX_CHARGE_START_TEMP_C)){
        //do nothing, wait until temperatures have stabilised
        Set_LED_RBBB(0b0010); //Separated blue LED indicating wait for temperature to normalise
    }
    else if (!charge_wait_counter.enable){   //if counter isn't enabled, clear and enable it.
        charge_wait_counter.value = 0;
        charge_wait_counter.enable = true;  //Clear and start charge wait counter
    }
    else if (charge_wait_counter.value >= CHARGE_WAIT_TIMEOUT){         //2188 * 32ms = 70.016 seconds
        charge_wait_counter.enable = false;
        if (charge_complete_flag){
            packdelta_end_of_charging_wait_mV = cellstats.packdelta_mV; // for delta cell indication next time battery is put on charger
            state = IDLE;
        }
        else {
        state = CHARGING;
        }
    }
    else{
        Set_LED_RBBB(0b0101); //Separated blue LEDs indicating charing wait
    }
    
    if (detect != CHARGER){                    //Charger removed
        charge_wait_counter.enable = false;
        state = IDLE;
    }
    
    if (!safetyChecks()){  //Somehow there was an error
        charge_wait_counter.enable = false;
        state = ERROR;
    }
    
}

void outputEN(void){
    static uint8_t startup_led_step = 0;
	static bool runonce = 0;
	charge_complete_flag = false;
	if (!ISL_GetSpecificBits_cached(ISL.ENABLE_DISCHARGE_FET)  //If discharge isn't already enabled
		&& detect == TRIGGER                       //Trigger is pulled
		&& ISL_GetSpecificBits_cached(ISL.WKUP_STATUS) //Make sure WKUP = 1 meaning charger connected or trigger pressed   
		&& minCellOK()          //Min cell is not below low voltage cut out of 3V
		&& safetyChecks()
			){
			ISL_SetSpecificBits(ISL.ENABLE_DISCHARGE_FET, 1);
                startup_led_step = 0;
			resetLEDBlinkPattern(); 
			runonce = 0;
			total_runtime_counter.enable = true;
			onetime_runtime_counter.enable = true;
                onetime_runtime_counter.value = 0; //Start discharge timer from zero so trigger-start LED phase is deterministic
			LED_code_cycle_counter.value = 0;
			
	}
	else if (ISL_GetSpecificBits_cached(ISL.ENABLE_DISCHARGE_FET)  //Same as above but we are already discharging and all conditions are good
		&& detect == TRIGGER
		&& ISL_GetSpecificBits_cached(ISL.WKUP_STATUS)
		&& (minCellOK() || (onetime_runtime_counter.enable && onetime_runtime_counter.value < 64)) //allow 2 seconds, discharge currrent measurement to stabilise
		&& safetyChecks()
			){
			if (onetime_runtime_counter.value > 156 && discharge_current_mA >= 4000){ //output must run at least 5 seconds
				discharge_current_mA_last_trigger = discharge_current_mA; //record last discharge current when trigger was pulled
				mincell_voltage_mV_last_trigger = cellstats.mincell_mV;   //record last min cell voltage before cell recovery
				minimum_cell_last_trigger = cellstats.mincellnum;        //record which was the cell with minimum voltage
			}

			//Fancy start up LEDs
			runonce = 0;
			charge_state_modify_flag = 0; // disallow decrease of charge state
			if (onetime_runtime_counter.value >= 64){ // 2000ms delay
				charge_state_modify_flag = 2; // allow decrease of charge state
			}
			SetDischargeTriggerLED(CalculateChargeIndicator());   //LED charge indicator on discharge
		}
	else if (!minCellOK() && onetime_runtime_counter.enable && onetime_runtime_counter.value >= 64){   //If we hit the min cell voltage cut off, prevent discharging battery further until it is put on charger, but allow 2 seconds, discharge currrent measurement to stabilise
		full_discharge_flag = true;
		charge_state_memory = 1; //overwrite, disgarding charge_state_modify_flag
		state = IDLE;
	}     
	else if (!safetyChecks()){
			state = ERROR;
	}
	else if (detect == CHARGER){    //Charger attached while trigger was pulled
		static uint8_t switch_led_step = 0;
		ISL_SetSpecificBits(ISL.ENABLE_DISCHARGE_FET, 0);   //Disable discharging
		if (!runonce){
			resetLEDBlinkPattern();
			LED_code_cycle_counter.enable = true;
			runonce = true;
		} 
		
		uint8_t num_blinks = 10; //Mode switch (Boost/Eco) visual feedback
		ledBlinkpattern (num_blinks, 0b1111, 100, 100, 500, 0);
		if (LED_code_cycle_counter.value > 1){	//One LED cycle completed
			// Toggle mode between Boost and Eco
			MAX_CHARGE_CELL_VOLTAGE_mV = (MAX_CHARGE_CELL_VOLTAGE_mV == MAX_CHARGE_CELL_VOLTAGE_BOOST_mV) ? MAX_CHARGE_CELL_VOLTAGE_ECO_mV : MAX_CHARGE_CELL_VOLTAGE_BOOST_mV;
			Write16BitUintVariableToEEPROM(EEPROM_ADDR_MAX_CHARGE_VOLT, MAX_CHARGE_CELL_VOLTAGE_mV);
			resetLEDBlinkPattern();
			state = IDLE;
		}
	} else {                                                  //Trigger released; WKUP status = 1
		runonce = 0;
		state = IDLE;
	}

	//State change cleanup
	if (state != OUTPUT_EN){
		ISL_SetSpecificBits(ISL.ENABLE_DISCHARGE_FET, 0);   //Disable discharging
		total_runtime_counter.enable = false;
		onetime_runtime_counter.enable = false;
		startup_led_step = 0;
		runonce = 0;
		resetLEDBlinkPattern();
	}
}

void blinkError(uint8_t count) {
    ledBlinkpattern(count, 0b1000, 500, 500, 1000, 1000);
}

void error(void){
    ISL_Write_Register(FETControl, 0b00000000);     //Make sure all FETs are disabled
    
    mincell_voltage_mV_last_trigger = 0xFFFF; //to avoid wrong calculation of internal resistence (in case not going from discharge to void directly)
    
    if (total_runtime_counter.enable == true){  // In case normal Output enable clean up was bypassed due to brown out or I2C error
        total_runtime_counter.enable = false;
        onetime_runtime_counter.enable = false;
        //Write latest runtime counter value to EEPROM
        Write32BitUintVariableToEEPROM(EEPROM_RUNTIME_TOTAL_STARTING_ADDR, total_runtime_counter.value);
    }
    
    static bool EEPROM_Event_Logged = false;

    current_error_reason = (error_reason_t){0};
    setErrorReasonFlags(&current_error_reason);
        
    static bool critical_i2c_error = false;
    if (!(I2C_error_counter < CRITICAL_I2C_ERROR_THRESH)){
        critical_i2c_error = true;
    }
    
    /* This is a dirty hack to handle a possible hardware bug where:
         * 0) Directly short the output (or connect it to multiple electronic loads that appear as a short until they start to limit the current)
         * 1) The trigger is pulled and output is enabled as usual
         * 2) There is a massive current spike, but not so huge as to trip the short circuit protection (which has to be set to 175A because the next lowest step, 100A, is insufficient for vacuum startup).
         * 3) The output is disabled, but probably not actually because short-circuit protection kicks in. It takes about 400us, vs the 190us for short circuit protection.
         * 4) The ISL94208 RESETS ITSELF AND DOES NOT PROVIDE ANY ERROR FLAG. Thus it is not obvious what happened.
         *      - This is likely because during a hard short, there can be a current spike of 140A+.
         *      - This causes the voltage on VBACK (attached to cell 1) to drop as low as 1.46V (vs 3-4.2V normally).
         *      - VCC for the ISL94208, which is connected to Cell 6 drops as low as 10.5V but that's still above the POR voltage
         *      - This is below the listed typical POR voltage (no minimum provided) for VBACK and is likely causing the reset of the ISL, which wipes all registers.
         *      - All of this is likely caused because Dyson omitted the 10uF capacitor for VBACK shown in the ISL94208 datasheet (page 32).
         * 5) The normal I2C commands to the ISL fail while it is resetting, causing I2C errors.
         * 6) Previously, I2C errors were handled by resetting the PIC. So the PIC is reset.
         * 7) The PIC starts up and sees the trigger is pulled and the ISL is presenting no error flags.
         * 8 ) Wash, rinse, repeat. Go to step 1.
         * 
         * Now, we are setting user flag bits 0 and 1 during setup and routinely checking to make sure those are still set.
         * We also check to make sure WKPOL is still set correctly, because when checking the AnalogOut voltages, we have to write to that register which also contains the user flag bits.
         * Occasionally, depending on the timing of the I2C errors, the code will write to that register and inadvertently set the User Flag bits again, even though there was an error.
         * If those are ever cleared, that means the ISL reset itself. This causes the ISL_BROWN_OUT error code.
         * 
         * This could probably be integrated in to the normal fault handling system much more cleanly,
         * and without having to create an infinite loop and duplicate some of the main loop routines.
         * However, at this point I can't be bothered and so I've just hacked this in.
         * This is the simplest way of handling a situation where I'm not sure I can trust anything is initialized the way it should be.
         *  
        */
    if (critical_i2c_error || past_error_reason.ISL_BROWN_OUT){         
        resetLEDBlinkPattern();
        while(1){                                               //It's called critical for a reason
            ISL_Write_Register(FETControl, 0b00000000);     //Make sure all FETs are disabled
            if (I2C_ERROR_FLAGS != 0){
                I2C1_Init();    //Attempting to recover I2C bus as a last ditch effort to turn off MOSFETs before erroring out. Might not be useful.
                ClearI2CBus();
            }
            
            if (!detect){
                LED_code_cycle_counter.enable = true;       //Start led error code sequence after trigger released.
            } 
            else{
                LED_code_cycle_counter.value = 0;           //Attaching charger or pulling trigger will reset LED code count
                LED_code_cycle_counter.enable = false;
            }

            if (LED_code_cycle_counter.value > NUM_OF_LED_CODES_AFTER_FAULT_CLEAR){
                RESET();        //Once required number of error codes are shown, use the nuclear option.
            }
            
            
            if (past_error_reason.ISL_BROWN_OUT){
                ledBlinkpattern(16, 0b1000, 500, 500, 1000, 1000);    //ISL brown out
            }
            else{
                ledBlinkpattern(15, 0b1000, 500, 500, 1000, 1000);    //critical i2c error
            }
            
            if (TMR4_HasOverflowOccured()){         //Every 32ms //Since we aren't going in to main loop again, we still have to service this counter for the LED code to work
                if (nonblocking_wait_counter.enable){
                    nonblocking_wait_counter.value++;
                }
            }
            
            CLRWDT();   //We also have to clear the WDT
            
            detect = checkDetect();     //And check the latest detect value
            
        }
    }
    
    if (!current_error_reason.ISL_INT_OVERTEMP_FLAG
        && !current_error_reason.ISL_EXT_OVERTEMP_FLAG 
        && !current_error_reason.ISL_INT_OVERTEMP_PICREAD 
        && !current_error_reason.THERMISTOR_OVERTEMP_PICREAD 
        && !current_error_reason.UNDERTEMP_FLAG
        && !current_error_reason.CHARGE_OC_FLAG 
        && !current_error_reason.DISCHARGE_OC_FLAG 
        && !current_error_reason.DISCHARGE_SC_FLAG 
        && !current_error_reason.DISCHARGE_OC_SHUNT_PICREAD 
        && !current_error_reason.CHARGE_ISL_INT_OVERTEMP_PICREAD 
        && !current_error_reason.CHARGE_THERMISTOR_OVERTEMP_PICREAD 
        && !current_error_reason.TEMP_HYSTERESIS 
        && ((detect == NONE) || detect == CHARGER)    //if the error reason was being fully discharged, allow exit loop if device is connected to charger
        && discharge_current_mA == 0
            ){
            if (!LED_code_cycle_counter.enable){
                LED_code_cycle_counter.value = 0;
                LED_code_cycle_counter.enable = true;
            }

            /* Error wait timeout is necessary because the while the ISL94208 datasheet claims:
             * "If the over-temperature condition has cleared, this bit is reset when the register is read."
             * regarding the external over temperature (XOT) bit, this does not appear to be true.
             * If we are continuously reading the status register, the XOT bit will only be read as asserted roughly once every 560ms.
             * It acts as if reading it is clearing the bit each time, and then it is re-asserted the next time the ISL94208
             * does it's automatic temperature scan. Oddly, occasionally we can read the XOT bit as asserted twice in a row.
             * Most of the time, even under a continuous low voltage (high temperature) on the temp. input,
             * the XOT bit will only read as asserted roughly 1 in 49 reads. */
            if (!error_timeout_wait_counter.enable){
                error_timeout_wait_counter.value = 0;
                error_timeout_wait_counter.enable = true;
            }
            else if (error_timeout_wait_counter.enable
                    && error_timeout_wait_counter.value > ERROR_EXIT_TIMEOUT
                    && LED_code_cycle_counter.enable
                    && LED_code_cycle_counter.value >= NUM_OF_LED_CODES_AFTER_FAULT_CLEAR
                    ){       //three seconds must pass with no errors before error state can be exited. Also, LED code must be presented the configured number of times after fault/detect clear
                error_timeout_wait_counter.enable = false;
                sleep_timeout_counter.enable = false;
                past_error_reason = (error_reason_t){0};    //Clear error reason value for future usage
                current_error_reason = (error_reason_t){0};
                resetLEDBlinkPattern();
                EEPROM_Event_Logged = false;
                full_discharge_flag = false;
                state = IDLE;
                return;
            }
        }
    else {
        error_timeout_wait_counter.enable = false;      //If there are any errors, stop the error exit timeout counter. The next time through the loop there are no errors, the counter will be reset to zero and restarted.
        LED_code_cycle_counter.enable = false;
    }

    if (past_error_reason.ISL_INT_OVERTEMP_FLAG) blinkError(4);
    else if (past_error_reason.ISL_EXT_OVERTEMP_FLAG) blinkError(5);
    else if (past_error_reason.ISL_INT_OVERTEMP_PICREAD) blinkError(6);
    else if (past_error_reason.THERMISTOR_OVERTEMP_PICREAD) blinkError(7);
    else if (past_error_reason.CHARGE_OC_FLAG) blinkError(8);
    else if (past_error_reason.DISCHARGE_OC_FLAG) blinkError(9);
    else if (past_error_reason.DISCHARGE_SC_FLAG) blinkError(10);
    else if (past_error_reason.DISCHARGE_OC_SHUNT_PICREAD) blinkError(11);
    else if (past_error_reason.CHARGE_ISL_INT_OVERTEMP_PICREAD) blinkError(12);
    else if (past_error_reason.CHARGE_THERMISTOR_OVERTEMP_PICREAD) blinkError(13);
    else if (past_error_reason.UNDERTEMP_FLAG) blinkError(14);
    else if (full_discharge_flag) ledBlinkpattern (3, 0b0001, 150, 150, 450, 450);       //trigger is pulled but battery is low
    else blinkError(20);                                                                  //Unidentified Error
    
    
    
    
    
    if (sleep_timeout_counter.enable == false   //If there is an error, start sleep counter (if it isn't already started), so we sleep if in error state for too long
            && detect != CHARGER){                 //Also, don't start sleep sequence if we are connected to charger
        sleep_timeout_counter.value = 0;
        sleep_timeout_counter.enable = true;
    }
    else if (detect == CHARGER){                   //If at any point the charger is connected, abort sleep sequence. I'm not quite sure why we'd want to though.
        sleep_timeout_counter.enable = false;
    }
    else if (sleep_timeout_counter.value >  ERROR_SLEEP_TIMEOUT //1876*32ms = 60.032s //If we are in ERROR state for 60 seconds, just go to sleep.
            && sleep_timeout_counter.enable == true
            && nonblocking_wait_counter.enable == false     //Don't sleep in the middle of an LED blink code cycle
            && nonblocking_wait_counter.value == 0
            && detect != CHARGER
            ){    
        sleep_timeout_counter.enable = false;
        state = SLEEP;
        
    }
    
}
#ifdef __DEBUG
    volatile uint16_t loop_counter = 0;
#endif

void RecordDetectHistory(void){
    detect_history = (uint8_t) ( (uint8_t) (detect_history << 2) | (detect & 0b00000011) );
}
    
detect_t GetDetectHistory(uint8_t position){
    return (detect_t) ((detect_history >> (2*position)) & 0b00000011);
}

bool CheckStateInDetectHistory(detect_t detect_val){
    for (uint8_t i = 0; i < 4; i++){
        if(GetDetectHistory(i) == detect_val){
            return true;
        }
    }
    return false;
}

void Write32BitUintVariableToEEPROM(uint8_t starting_addr, uint32_t variable_to_write){       //Make sure there are four bytes available with the provided starting address
    DATAEE_WriteByte(starting_addr, (uint8_t) ((variable_to_write & 0xFF000000) >> 24)     );
    DATAEE_WriteByte(starting_addr+1, (uint8_t) ((variable_to_write & 0xFF0000) >> 16) );
    DATAEE_WriteByte(starting_addr+2, (uint8_t) ((variable_to_write & 0xFF00) >> 8)    );
    DATAEE_WriteByte(starting_addr+3, (uint8_t) (variable_to_write & 0xFF)   );
}

void Write16BitUintVariableToEEPROM(uint8_t starting_addr, uint16_t variable_to_write){       //Make sure there are four bytes available with the provided starting address
    DATAEE_WriteByte(starting_addr, (uint8_t) ((variable_to_write & 0xFF00) >> 8)    );
    DATAEE_WriteByte(starting_addr+1, (uint8_t) (variable_to_write & 0xFF)   );
}

uint32_t Read32BitUintVariableFromEEPROM(uint8_t starting_addr){     
    uint32_t variable_to_read = 0;
    variable_to_read = (uint32_t) DATAEE_ReadByte(starting_addr) << 24;
    variable_to_read |= (uint32_t) DATAEE_ReadByte(starting_addr+1) << 16;
    variable_to_read |= (uint32_t) DATAEE_ReadByte(starting_addr+2) << 8;
    variable_to_read |= (uint32_t) DATAEE_ReadByte(starting_addr+3);
	return variable_to_read;
}

uint16_t Read16BitUintVariableFromEEPROM(uint8_t starting_addr){     
    uint16_t variable_to_read = 0;
    variable_to_read |= (uint16_t) DATAEE_ReadByte(starting_addr) << 8;
    variable_to_read |= (uint16_t) DATAEE_ReadByte(starting_addr+1);
	return variable_to_read;
}

uint8_t CalculateChargeIndicator(void){
    uint16_t dynamic_offset_mV_load = 0;
    uint8_t charge_level = 0;
    dynamic_offset_mV_load = MIN_DISCHARGE_CELL_VOLTAGE_mV - ((mincell_internal_resistence_max / 100) * discharge_current_mA) / 10000; // to avoid overflow, reducec accuracy by x100
    if (dynamic_offset_mV_load < CELL_CUTOUT_VOLTAGE_DISCHARGE_mV){
        dynamic_offset_mV_load = CELL_CUTOUT_VOLTAGE_DISCHARGE_mV;
    }
    charge_level = (uint8_t)((cellstats.mincell_mV-dynamic_offset_mV_load) / ((MAX_CHARGE_CELL_VOLTAGE_mV-MIN_DISCHARGE_CELL_VOLTAGE_mV)/3) + 1);
    
    if (cellstats.mincell_mV <= dynamic_offset_mV_load){
        charge_level = 0;
    }
    
    if (charge_state_modify_flag == 1
        || (charge_state_modify_flag == 2 && charge_level < charge_state_memory)
        || (charge_state_modify_flag == 3 && charge_level > charge_state_memory))
    {    
    charge_state_memory = charge_level;
    }

    if (charge_state_memory >= 3){
        return 0b00000111;
    }
    else if (charge_state_memory == 2){
        return 0b00000011;
    }
    else if (charge_state_memory <= 1){
        return 0b00000001;
    }
    else {
        return 0b00001000;
    }
}

void main(void)
{    
    init();
    
    while (1)
    {
        CLRWDT();
        //__delay_ms(5);
        #ifdef __DEBUG
        loop_counter++;
        #endif  

        ISL_Read_Register(AnalogOut); //Get Analog Out register that contains user bits
        ISL_Read_Register(FeatureSet); //Get Feature Set reg to check WKPOL
        ISL_BrownOutHandler();
        
        ISL_ReadAllCellVoltages();
        ISL_calcCellStats();
        RecordDetectHistory();
        detect = checkDetect();
        isl_int_temp = ISL_GetInternalTemp();
        if (isl_int_temp > isl_int_temp_max) {
            isl_int_temp_max = isl_int_temp;
        }
        thermistor_temp = getThermistorTemp(modelnum);
        if (thermistor_temp > thermistor_temp_max) {
            thermistor_temp_max = thermistor_temp;
        }
        
        ISL_Read_Register(Config);      //Get config register so we can check WKUP status later on
        ISL_Read_Register(Status);      //Get Status register to check for error flags
        ISL_Read_Register(FETControl);  //Get current FET status
        ISL_Read_Register(AnalogOut); //Get Analog Out register that contains user bits
        ISL_Read_Register(FeatureSet); //Get Feature Set reg to check WKPOL
        discharge_current_mA = dischargeIsense_mA();
        
        if (ISL_BrownOutHandler()){
            //do nothing
        }
        else if (I2C_ERROR_FLAGS != 0){     //I2C error handling
            I2C_error_counter++;

            if (I2C_error_counter < CRITICAL_I2C_ERROR_THRESH) {
                I2C1_Init();
                ClearI2CBus(); //Clear error flags  //First try just clearing I2C bus (which will also POR reset ISL94208)  
                continue; //Then go again from the top.
            } else {
                I2C1_Init();    //Attempting to recover I2C bus as a last ditch effort to turn off MOSFETs before erroring out. Might not be useful.
                ClearI2CBus();
                state = ERROR;
            }
        } else {        //If we were successful this time, clear the error counter.
            I2C_error_counter = 0;
        }
        
        
        switch(state){
            case INIT:
                init();
                break;
                
            case SLEEP:
                sleep_ISL_or_PIC();
                break;
            
            case IDLE:
                idle();
                break;
                
            case CHARGING:
                charging();
                break;
                
            case CHARGING_WAIT:
                chargingWait();
                break;
                
            case OUTPUT_EN:
                outputEN();
                break;

            case ERROR:
                error();
                break;
                
        }
        
        if (TMR4_HasOverflowOccured()){         //Every 32ms 
            if (charge_wait_counter.enable){
                charge_wait_counter.value++;
            }
            
            if (charge_duration_counter.enable){
                charge_duration_counter.value++;
            }
            
            if (sleep_timeout_counter.enable){
                sleep_timeout_counter.value++;
            }
            
            if (nonblocking_wait_counter.enable){
                nonblocking_wait_counter.value++;
            }
            
            if (error_timeout_wait_counter.enable){
                error_timeout_wait_counter.value++;
            }
            
            if (total_runtime_counter.enable){
                total_runtime_counter.value++;
            }
            if (onetime_runtime_counter.enable){
                onetime_runtime_counter.value++;
            }
            
        
        
        }
        
        
    }
}
