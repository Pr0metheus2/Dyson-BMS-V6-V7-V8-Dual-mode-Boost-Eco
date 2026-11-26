import json
import argparse
from pathlib import Path
from textwrap import wrap

parser = argparse.ArgumentParser()
parser.add_argument("file_path", type=Path)

p = parser.parse_args()

if p.file_path.exists():
    filepath = p.file_path
else:
    print("Error: file not found")
    programPause = input('Press <ENTER> to quit')
    exit()

temp_list = []
combined_list = []

with open(filepath) as tsv:
    for line in tsv:
        if line.split():    #Make sure line isn't empty
            temp_list.append(wrap(line, 2))      #split removes all white space characters and converts remaining to a list of two characters per item            

if all(line[0] == format(i, 'X').ljust(2, '0') for i, line in enumerate(temp_list)):  #if all of the lines have a value at the beginning that matches the line number, they are probably line numbers so drop them
    for line in temp_list:
        line.pop(0)     #Drop first value that contains the line number


for line in temp_list:
    combined_list += line

falsecount = 0
for item in combined_list[1::2]:        #Check if all odd items are zero, indicating hex data has zeros between each byte
    if bool(int(item, 16)) == False:
        falsecount += 1

if falsecount == len(combined_list[1::2]):
    print("all odd values are zero")
    for i, item in enumerate(combined_list[1::2]):
        combined_list.pop(i+1)          #This is kind of weird but it works. Deletes all the odd items after confirming they are all zero

if len(combined_list) < 32:
    parser.error("EEPROM dump is incomplete; at least 32 bytes are required.")

eeprom = [int(value, 16) for value in combined_list]


def read_u8(address):
    return eeprom[address]


def read_i8(address):
    value = read_u8(address)
    return value - 0x100 if value >= 0x80 else value


def read_u16(address):
    return (read_u8(address) << 8) | read_u8(address + 1)


def read_u32(address):
    return ((read_u8(address) << 24) | (read_u8(address + 1) << 16) |
            (read_u8(address + 2) << 8) | read_u8(address + 3))


def read_u16_or_none(address):
    value = read_u16(address)
    return None if value == 0xFFFF else value


firmware_version_hex = ''.join(combined_list[:24]) 
firmware_version = bytearray.fromhex(firmware_version_hex).decode()

# New firmware images store a full version suffix (for example, ".7") at
# EEPROM addresses 0x18-0x19. Older images do not contain this ".<digit>"
# suffix and continue to decode as their original "... V7" identifier.
firmware_version_suffix = bytearray.fromhex(''.join(combined_list[24:26]))
if firmware_version_suffix[0] == ord('.') and chr(firmware_version_suffix[1]).isdigit():
    firmware_version += firmware_version_suffix.decode()

output_data = {}
output_data["Firmware"] = firmware_version

output_data["Total_Runtime_Seconds"] = round(read_u32(0x1C) * 0.032, 3)  # 32 ms per tick
output_data["Min_Cell_Resistance_uOhm"] = {
    "Last_Charge": read_u32(0x20),
    "Maximum_Recorded": read_u32(0x24)
}
minimum_cell_voltage_last_trigger = read_u16_or_none(0x28)
recovered_cell_voltage_last_trigger = read_u16_or_none(0x2A)
output_data["Trigger_At_Maximum_Resistance"] = {
    "Minimum_Cell_Voltage_mV": minimum_cell_voltage_last_trigger,
    "Recovered_Cell_Voltage_mV": recovered_cell_voltage_last_trigger,
    "Discharge_Current_mA": (
        read_u16(0x2C)
        if minimum_cell_voltage_last_trigger is not None
        else None
    )
}
output_data["Maximum_Temperatures_C"] = {
    "Thermistor": read_u8(0x2E),
    "ISL94208_Internal": read_u8(0x2F)
}
output_data["Pack_Delta_End_Charge_mV"] = read_u16(0x30)
output_data["Cell_Voltages_Last_Sleep_mV"] = {
    f"Cell_{cell}": read_u16(0x38 + (cell - 1) * 2)
    for cell in range(1, 7)
}
output_data["Maximum_Charge_Cell_Voltage_mV"] = read_u16(0x44)
output_data["Cell_Voltage_Offsets_mV"] = {
    f"Cell_{cell}": read_i8(0x48 + cell - 1)
    for cell in range(1, 7)
}

output_path = filepath.with_name(f"{filepath.stem}_output{filepath.suffix}")
with output_path.open("w", encoding="utf-8") as output_file:
    json.dump(output_data, output_file, indent=4)
    output_file.write("\n")

print(f"Output saved to: {output_path}")

programPause = input('Press <ENTER> to continue')
