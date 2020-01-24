/*
 *  Application note: Thermostat with MODBUS via RS485 for ArduiTouch ESP  
 *  Version 1.0
 *  Copyright (C) 2018  Hartmut Wendt  www.zihatec.de
 *  
 *  (based on sources of https://github.com/angeloc/simplemodbusng)
 *  
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/   


/*______Import Libraries_______*/
#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <XPT2046_Touchscreen.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <SimpleModbusSlave.h> 
#include "usergraphics.h"

/*______End of Libraries_______*/


/*______Define LCD pins for ArduiTouch _______*/
#define TFT_CS   D1
#define TFT_DC   D2
#define TFT_LED  15  


#define HAVE_TOUCHPAD
#define TOUCH_CS 0
#define TOUCH_IRQ 2
// #define touch_yellow_header  // enable this line for TFTs with yellow header
/*_______End of defanitions______*/

/*______Assign pressure_______*/
#define ILI9341_ULTRA_DARKGREY    0x632C      
#define MINPRESSURE 10
#define MAXPRESSURE 2000
/*_______Assigned______*/

/*____Calibrate TFT LCD_____*/
#define TS_MINX 370
#define TS_MINY 470
#define TS_MAXX 3700
#define TS_MAXY 3600
/*______End of Calibration______*/


/*____Modbus_____*/
#define BAUDRATE 9600
#define DEFAULT_ID 1
enum 
{     
  // just add or remove registers 
  // The first register starts at address 0
  ROOM_TEMP,  // measured room temp from external sensor
  SET_TEMP,   // set-point temperature by user, 
  FAN_LEVEL,  // level for ventilation (values 0 - 5)
  BEEPER,     // any value between 400 and 4000 will set the beeper with the given frequency for 100ms
  DISP_ONOFF, // timer for display automatic off function (0 switch backlight off, >0 set timer for automatic off)
  TOTAL_ERRORS,
  // leave this one
  TOTAL_REGS_SIZE 
  // total number of registers for function 3 and 16 share the same register array
}; 
/*______End of Modbus______*/


/*____Program specific constants_____*/
#define MAX_TEMPERATURE 28  
#define MIN_TEMPERATURE 18
enum { PM_MAIN, PM_OPTION, PM_CLEANING};  // Program modes
enum { BOOT, COOLING, TEMP_OK, HEATING};        // Thermostat modes
/*______End of specific constants______*/


Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);


#define _debug
 int X,Y;
 uint8_t Thermostat_mode = BOOT;
 
 uint8_t iFan_level = 0;
 uint8_t iRoom_temperature = 21;
 uint8_t iSet_temperature = 20;

 uint8_t PMode = PM_MAIN;         // program mode
 uint8_t Modbus_ID = DEFAULT_ID;  // ID / address for modbus
 bool Touch_pressed = false;
 uint8_t Timer_Cleaning=0;
 
 unsigned int holdingRegs[TOTAL_REGS_SIZE]; // function 3 and 16 register array 


 

void setup() {
  #ifdef _debug
  Serial.begin(9600); 
  #endif

  // Init MODBUS registers
  holdingRegs[ROOM_TEMP] = iRoom_temperature;
  holdingRegs[SET_TEMP] = iSet_temperature;
  holdingRegs[ROOM_TEMP] = iFan_level;
  holdingRegs[BEEPER] = 0;
  holdingRegs[DISP_ONOFF] = 255;
  
  #ifndef _debug 
  /* parameters(long baudrate, 
                unsigned char ID, 
                unsigned char transmit enable pin, 
                unsigned int holding registers size,
                unsigned char low latency)
                
     The transmit enable pin is used in half duplex communication to activate a MAX485 or similar
     to deactivate this mode use any value < 2 because 0 & 1 is reserved for Rx & Tx.
     Low latency delays makes the implementation non-standard
     but practically it works with all major modbus master implementations.
  */
  modbus_configure(BAUDRATE, Modbus_ID, 0, TOTAL_REGS_SIZE, 0); 
  #endif

  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);   // HIGH to Turn on;
  //digitalWrite(TFT_LED, LOW);    // LOW to Turn on with assembled T1 and R2


  tft.begin();
  touch.begin();
  #ifdef _debug
  Serial.print("tftx ="); Serial.print(tft.width()); Serial.print(" tfty ="); Serial.println(tft.height());
  #endif

  draw_main_screen();
  
  //Sound
  noTone(D0);
}


TS_Point p;

void loop() {
  //TS_Point p = waitTouch();
  //X = p.y; Y = p.x;
  if (Touch_Event()== true) { 
    X = p.y; Y = p.x;
    if (Touch_pressed == false) {
      if (holdingRegs[DISP_ONOFF]) DetectButtons();
      holdingRegs[DISP_ONOFF] = 255; // reset BL timer       
    }
    Touch_pressed = true;
    
  } else {
    Touch_pressed = false;
  }

  //automatic display BL timeout
  if (holdingRegs[DISP_ONOFF]) {
    holdingRegs[DISP_ONOFF]--;
    digitalWrite(TFT_LED, HIGH); // Backlight on 
    //digitalWrite(TFT_LED, LOW); // Backlight on  - with assembled T1 and R2
  } else {
    digitalWrite(TFT_LED, LOW); // Backlight off
    //digitalWrite(TFT_LED, HIGH); // Backlight off  - with assembled T1 and R2
  }

  // screen cleaning
  Cleaning_processing();
  
  // Modbus
  Modbus_processing(); 
  delay(100);
  
}


/********************************************************************//**
 * @brief     detects a touch event and converts touch data 
 * @param[in] None
 * @return    boolean (true = touch pressed, false = touch unpressed) 
 *********************************************************************/
bool Touch_Event() {
  p = touch.getPoint(); 
  delay(1);
  #ifdef touch_yellow_header
    p.x = map(p.x, TS_MINX, TS_MAXX, 320, 0); // yellow header
  #else
    p.x = map(p.x, TS_MINX, TS_MAXX, 0, 320); // black header
  #endif
  p.y = map(p.y, TS_MINY, TS_MAXY, 240, 0);
  if (p.z > MINPRESSURE) return true;  
  return false;  
}


/********************************************************************//**
 * @brief     Processing for screen cleaning function
 * @param[in] None
 * @return    None
 *********************************************************************/
void Cleaning_processing()
{
  // idle timer for screen cleaning
  if (PMode == PM_CLEANING) {
      if ((Timer_Cleaning % 10) == 0) {
        tft.fillRect(0,0, 100, 60, ILI9341_BLACK);
        tft.setCursor(10, 50);
        tft.print(Timer_Cleaning / 10);
        
      }
      if (Timer_Cleaning) {
        Timer_Cleaning--;
      } else {
        draw_option_screen();
        PMode = PM_OPTION;
      }
  }  
}


/********************************************************************//**
 * @brief     Processing for MODBUS function / checking all registers
 * @param[in] None
 * @return    None
 *********************************************************************/
void Modbus_processing()
{
  holdingRegs[TOTAL_ERRORS] = modbus_update(holdingRegs); 

  // update of variables by Modbus
  if (holdingRegs[ROOM_TEMP] != iRoom_temperature) {
     if ((holdingRegs[ROOM_TEMP] > 50) || (holdingRegs[ROOM_TEMP] < 5)) {
       holdingRegs[ROOM_TEMP] = iRoom_temperature;
     } else {
       iRoom_temperature = holdingRegs[ROOM_TEMP];
       update_Room_temp();
       update_circle_color();
     }
  }

  if (holdingRegs[SET_TEMP] != iSet_temperature) {
     if ((holdingRegs[SET_TEMP] > MAX_TEMPERATURE) || (holdingRegs[SET_TEMP] < MIN_TEMPERATURE)) {
       holdingRegs[SET_TEMP] = iSet_temperature;
     } else {
       iSet_temperature = holdingRegs[SET_TEMP];
       update_SET_temp();
       update_circle_color();
     }
  }


  if (holdingRegs[FAN_LEVEL] != iFan_level) {
     if ((holdingRegs[FAN_LEVEL] > 5) || (holdingRegs[FAN_LEVEL] < 0)) {
       holdingRegs[FAN_LEVEL] = iFan_level;
     } else {
       iFan_level = holdingRegs[FAN_LEVEL];
       draw_fan_level(50,312,iFan_level);
     }
  }

  if ((holdingRegs[BEEPER] > 500) && (holdingRegs[BEEPER] < 4000)) {
    tone(D0,holdingRegs[BEEPER],100);
  } 
  holdingRegs[BEEPER] = 0;    
  
}


/********************************************************************//**
 * @brief     detecting pressed buttons with the given touchscreen values
 * @param[in] None
 * @return    None
 *********************************************************************/
void DetectButtons()
{
  // in main program
  if (PMode == PM_MAIN){

   // button UP
   if ((X>190) && (Y<50)) {
    if (iSet_temperature < MAX_TEMPERATURE) iSet_temperature++;
    tone(D0,2000,100);
    holdingRegs[SET_TEMP] = iSet_temperature;
    update_SET_temp();
    update_circle_color();
   }
    
   // button DWN
   if ((X>190) && (Y>200 && Y<250)) {
    if (iSet_temperature > MIN_TEMPERATURE) iSet_temperature--;
    tone(D0,2000,100);
    holdingRegs[SET_TEMP] = iSet_temperature;
    update_SET_temp();
    update_circle_color();
   }

   // button FAN MAX
   if ((X>180) && (Y>270)) {
    tone(D0,2000,100);
    if (iFan_level < 5) iFan_level++;
    draw_fan_level(50,312,iFan_level);
    holdingRegs[FAN_LEVEL] = iFan_level;
   }

   // button FAN MIN
   if ((X<60) && (Y>270)) {
    tone(D0,2000,100);
    if (iFan_level > 0) iFan_level--;
    draw_fan_level(50,312,iFan_level);
    holdingRegs[FAN_LEVEL] = iFan_level;
   }

   // button gearwheel
   if ((X<60) && (Y<50)) {
    draw_option_screen();
    PMode = PM_OPTION;
   }

 
  } else if (PMode == PM_OPTION){ 

   // button -
   if ((X<110) && (Y<75)) {
    if (Modbus_ID > 0) Modbus_ID--;
    update_Modbus_addr();
   }

   // button +
   if ((X>130) && (Y<75)) {
    if (Modbus_ID < 255) Modbus_ID++;
    update_Modbus_addr();
   }
   
   // button screen cleaning
   if ((Y>85) && (Y<155)) {
     tft.fillScreen(ILI9341_BLACK);
     tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
     tft.setFont(&FreeSansBold24pt7b);  
     PMode = PM_CLEANING;    
     Timer_Cleaning = 255;
   }


   // button OK
   if (Y>265) {
     Thermostat_mode = BOOT;
     draw_main_screen();
     modbus_configure(BAUDRATE, Modbus_ID, 0, TOTAL_REGS_SIZE, 0);      
     PMode = PM_MAIN;    
   }
    
    
  }
}



/********************************************************************//**
 * @brief     Drawing of the main program screen
 * @param[in] None
 * @return    None
 *********************************************************************/
void draw_main_screen()
{
  tft.fillScreen(ILI9341_BLACK);
  
  // draw circles
  update_circle_color();

  // draw temperature up/dwn buttons
  draw_up_down_button();

  // draw icons
  tft.drawRGBBitmap(10,290, fan_blue_24,24,24);
  tft.drawRGBBitmap(200,282, fan_blue_32,32,32);
  tft.drawRGBBitmap(10,10, wrench, 24,24);  
  
  // draw default fan level
  draw_fan_level(50,312,iFan_level);

  update_SET_temp();
 
}



/********************************************************************//**
 * @brief     Drawing of the screen for Options menu
 * @param[in] None
 * @return    None
 *********************************************************************/
void draw_option_screen()
{
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&FreeSansBold9pt7b);  
  
  // Modbus Address adjustment
  tft.setCursor(10, 20);
  tft.print("MODBUS address");
  tft.setFont(&FreeSansBold24pt7b);
  tft.setCursor(30, 65);
  tft.print("-");
  tft.setCursor(190, 65);
  tft.print("+");
  tft.drawLine(5,80,235,80, ILI9341_WHITE);

  // Screen cleaning idle timer
  tft.setFont(&FreeSansBold12pt7b);  
  tft.setCursor(26, 130);
  tft.print("Screen cleaning");
  tft.drawLine(5,160,235,160, ILI9341_WHITE);

  // OK Button
  tft.setFont(&FreeSansBold24pt7b);
  tft.drawLine(5,260,235,260, ILI9341_WHITE);
  tft.setCursor(90, 310);
  tft.print("OK");  
  update_Modbus_addr();
 
}


/********************************************************************//**
 * @brief     update of the value for set temperature on the screen
 *            (in the big colored circle)
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_SET_temp()
{
  int16_t x1, y1;
  uint16_t w, h;
  String curValue = String(iSet_temperature);
  int str_len =  curValue.length() + 1; 
  char char_array[str_len];
  curValue.toCharArray(char_array, str_len);
  tft.fillRect(70, 96, 60, 50, ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&FreeSansBold24pt7b);
  tft.getTextBounds(char_array, 80, 130, &x1, &y1, &w, &h);
  tft.setCursor(123 - w, 130);
  tft.print(char_array);
}



/********************************************************************//**
 * @brief     update of the value for room temperature on the screen
 *            (in the small grey circle)
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_Room_temp()
{
  int16_t x1, y1;
  uint16_t w, h;
  String curValue = String(iRoom_temperature);
  int str_len =  curValue.length() + 1; 
  char char_array[str_len];
  curValue.toCharArray(char_array, str_len);
  tft.fillRect(36, 200, 30, 21, ILI9341_ULTRA_DARKGREY);
  tft.setTextColor(ILI9341_WHITE, ILI9341_ULTRA_DARKGREY);
  tft.setFont(&FreeSansBold12pt7b);
  tft.getTextBounds(char_array, 40, 220, &x1, &y1, &w, &h);
  tft.setCursor(61 - w, 220);
  tft.print(char_array);
}



/********************************************************************//**
 * @brief     update of the color of the big circle according the 
 *            difference between set and room temperature 
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_circle_color()
{
  // HEATING 
  if ((iRoom_temperature < iSet_temperature) && (Thermostat_mode != HEATING)) {
    Thermostat_mode = HEATING;
    draw_circles();
  }

  // COOLING 
  if ((iRoom_temperature > iSet_temperature) && (Thermostat_mode != COOLING)) {
    Thermostat_mode = COOLING;
    draw_circles();
  }

  // Temperature ok 
  if ((iRoom_temperature == iSet_temperature) && (Thermostat_mode != TEMP_OK)) {
    Thermostat_mode = TEMP_OK;
    draw_circles();
  }
}


/********************************************************************//**
 * @brief     update of the value for MODBUS ID in the options menu on  
 *            the screen
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_Modbus_addr()
{
  tft.fillRect(110, 30, 60, 45, ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&FreeSansBold24pt7b);
  tft.setCursor(115, 65);
  tft.print(Modbus_ID);
}



/********************************************************************//**
 * @brief     drawing of the circles in main screen including the value 
 *            of room temperature
 * @param[in] None
 * @return    None
 *********************************************************************/
void draw_circles()
{

  //draw big circle 
  unsigned char i;
  if (iRoom_temperature < iSet_temperature) {
    // heating - red
    for(i=0; i < 10; i++) tft.drawCircle(120, 120, 80 + i, ILI9341_RED);
  } else if (iRoom_temperature > iSet_temperature) {
    // cooling - blue
    for(i=0; i < 10; i++) tft.drawCircle(120, 120, 80 + i, ILI9341_BLUE);    
  } else {
    // Temperature ok
    for(i=0; i < 10; i++) tft.drawCircle(120, 120, 80 + i, ILI9341_GREEN);       
  }

  //draw small 
  tft.fillCircle(60, 200, 40, ILI9341_ULTRA_DARKGREY);

  //draw °C in big circle
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(130, 100);
  tft.print("o");
  tft.setFont(&FreeSansBold24pt7b);
  tft.setCursor(140, 130);
  tft.print("C");

  // draw room and °C in small circle
  tft.setTextColor(ILI9341_WHITE, ILI9341_ULTRA_DARKGREY);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setCursor(75, 220);
  tft.print("C");
  tft.drawCircle(69,204, 2, ILI9341_WHITE);
  tft.drawCircle(69,204, 3, ILI9341_WHITE);
  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(35, 190);
  tft.print("Room");
  update_Room_temp();

}



/********************************************************************//**
 * @brief     drawing of the both buttons for setting temperature up 
 *            and down
 * @param[in] None
 * @return    None
 *********************************************************************/
void draw_up_down_button()
{
  //up button 
  tft.fillTriangle(215,10,230,30,200,30, ILI9341_WHITE);
    
  //down button
  tft.fillTriangle(215,230,230,210,200,210, ILI9341_WHITE);
}



/********************************************************************//**
 * @brief     drawing of the fan level in main screen
 * @param[in] None
 * @return    None
 *********************************************************************/
void draw_fan_level(uint16_t x0, uint16_t y0,  uint8_t ilevel)
{
  unsigned char i;
  if (ilevel >= 5)  ilevel = 5;
  for(i=0; i < 5; i++) {
    if (i < ilevel)  {
      tft.fillRect(x0 + (30*i), y0- 10 -(i*8), 20, 10 + (i*8), ILI9341_WHITE);  
    } else {
      tft.fillRect(x0 + (30*i), y0- 10 -(i*8), 20, 10 + (i*8), ILI9341_BLACK);
      tft.drawRect(x0 + (30*i), y0- 10 -(i*8), 20, 10 + (i*8), ILI9341_WHITE); 
      
    }
  }
}
