# ESP8266-Modbus-Thermostat-Example
ESP8266 based wall thermostat with ILI9341 touchscreen and Modbus support for ArduiTouch ESP  

![My image](https://i.pinimg.com/564x/f1/7e/0f/f17e0fb9de3552c1c893d19d422fd41a.jpg)

## Usage

Install the following libraries through Arduino Library Manager

Adafruit GFX Library https://github.com/adafruit/Adafruit-GFX-Library/archive/master.zip
Adafruit ILI9341 Library https://github.com/adafruit/Adafruit_ILI9341
XPT2046_Touchscreen by Paul Stoffregen https://github.com/PaulStoffregen/XPT2046_Touchscreen/blob/master/XPT2046_Touchscreen.h 
SimpleModbus NG https://github.com/angeloc/simplemodbusng

You can also download the library also directly as ZIP file and uncompress the folder under yourarduinosketchfolder/libraries/   

After installing the Adafruit libraries, restart the Arduino IDE. 

## MODBUS

Please see the table below about the register function:
400001  ROOM_TEMP     measured room temperatur from external sensor (values 5 - 50) 
400002  SET_TEMP      set-point temperature by user (values 18 - 28) 
400003  FAN_LEVEL     level for ventilation (values 0 - 5) 
400004  BEEPER        any value between 500 and 4000 will set the beeper with the given frequency for 100ms 
400005  DISP_ONOFF    timer for display automatic off function (0 switch backlight off, >0 set timer for automatic) 
400006  TOTAL_ERRORS  Counted communication errors


## Website

You can find the latest version of the code and the description of the hardware at
https://www.hwhardsoft.de/english/projects/arduitouch-esp/

# License

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA 
