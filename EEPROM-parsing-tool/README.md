# EEPROM-parsing-tool.py

EEPROM-parsing-tool.py converts EEPROM diagnostic data from the FW-Dyson-BMS firmware into a human-readable JSON file. It reads the current EEPROM layout: firmware version, total runtime, cell-resistance records, trigger measurements, maximum temperatures, saved cell voltages, charge-voltage setting, and cell-voltage offsets.

usage: EEPROM-parsing-tool.py file_path

For example, parsing `v6.txt` writes `v6_output.txt` in the same directory. The console only displays the output-file path.

Example output:

```json
    {
        "Firmware": "Tinfever FU-Dyson-BMS v3.8",
        "Total_Runtime_Seconds": 19.872,
        "Min_Cell_Resistance_uOhm": {
            "Last_Charge": 12000,
            "Maximum_Recorded": 11822
        },
        "Cell_Voltages_Last_Sleep_mV": {
            "Cell_1": 4056
        }
    }
```

Fault logging is no longer stored in EEPROM, so the tool does not report a `Faults` section.

You can use either the PICKit 3 Standalone Programmer App, or you can use MPLABX to read the EEPROM data.


PICKit 3 Standalone Programmer App
-----------------------------------

Short Instructions:
Read the device using the [PICkit 3 Programmer App and Scripting Tool v3.10](https://www.microchip.com/en-us/tools-resources/archives/mplab-ecosystem). In the EEPROM data window make sure the data is shown as "Hex Only". Right-click anywhere in the EEPROM data and click Select All, then right-click and Copy. Paste the data in to a .txt file. Run EEPROM-parsing-tool.py with the path to the saved data as the single argument.

Detailed Instructions:
1) Connect PICKit to computer
2) Connect PITKit to BMS board
3) Briefly press button on battery to wake up BMS board (you may also need to place a magnet over the reed switch if using a V7 (SV11) battery). The LED will light up when board is awake.
4) Open the Standalone Programmer App (Probably shown as PICkit 3 v3.01 in the start-menu)
5) The PIC should be automatically detected. The program should show Device: PIC16LF1847. If it doesn't, check your connections, then select Device Family > Midrange > 1.8V Min. Then select Tools > Check Communication
6) Click the Read button
7) Under the EEPROM Data section, make sure the drop down is set to "Hex Only".
8) Right-click anywhere in the EEPROM hex data at the bottom. Click Select All. Right-click again and click copy.
9) Open Notepad and paste the data in to a blank file. Save the file in the same directory as this script.
10) Run this script by running EEPROM-parsing-tool.py your-exported-file-name-here



MPLABX
--------------
Short instructions :
Read the EEPROM data using MPLAB X. View the EE Data Memory window, right click on the data and do "Export Table". Verify it will export address range 0x00 to 0xFF and save the data somewhere. Run EEPROM-parsing-tool.py with the path to the saved data as the single argument.

Detailed Instructions:
1) Connect PICKit to computer
2) Open MPLAB X IDE
3) Confirm PICKit is configured to not power the target device
4) Connect PICKit to BMS board
5) Briefly press button on battery to wake up BMS board (you may also need to place a magnet over the reed switch if using a V7 (SV11) battery). The LED will light up when board is awake.
6) Click the toolbar icon for "Read Device Memory" in MPLAB X (The icon looks like an up arrow. It is near the "Play" icon)
7) Navigate to Window > Target Memory Views > EE Data Memory
8) Right click anywhere in the hex data and select "Export table"
9) Make sure it will export address ranged 0x00 to 0xFF. Do not select "Single Column Ouput". Save it in the same directory as this script.
10) Run this script by running EEPROM-parsing-tool.py your-exported-file-name-here


