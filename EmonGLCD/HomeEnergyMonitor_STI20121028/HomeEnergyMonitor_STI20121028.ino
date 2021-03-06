//------------------------------------------------------------------------------------------------------------------------------------------------
// emonGLCD Home Energy Monitor example
// to be used with nanode Home Energy Monitor example

// Uses power1 variable - change as required if your using different ports

// emonGLCD documentation http://openEnergyMonitor.org/emon/emonglcd

// RTC to reset Kwh counters at midnight is implemented is software. 
// Correct time is updated via NanodeRF which gets time from internet
// Temperature recorded on the emonglcd is also sent to the NanodeRF for online graphing

// GLCD library by Jean-Claude Wippler: JeeLabs.org
// 2010-05-28 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php
//
// Authors: Glyn Hudson and Trystan Lea
// Part of the: openenergymonitor.org project
// Licenced under GNU GPL V3
// http://openenergymonitor.org/emon/license

// THIS SKETCH REQUIRES:

// Libraries in the standard arduino libraries folder:
//
//	- OneWire library	http://www.pjrc.com/teensy/td_libs_OneWire.html
//	- DallasTemperature	http://download.milesburton.com/Arduino/MaximTemperature
//                           or https://github.com/milesburton/Arduino-Temperature-Control-Library
//	- JeeLib		https://github.com/jcw/jeelib
//	- RTClib		https://github.com/jcw/rtclib
//	- GLCD_ST7565		https://github.com/jcw/glcdlib
//
// Other files in project directory (should appear in the arduino tabs above)
//	- icons.ino
//	- templates.ino
//
//-------------------------------------------------------------------------------------------------------------------------------------------------

#include <JeeLib.h>
#include <GLCD_ST7565.h>
#include <avr/pgmspace.h>
//#include "utility/font_clR6x8.h"
GLCD_ST7565 glcd;

#include <OneWire.h>		    // http://www.pjrc.com/teensy/td_libs_OneWire.html
#include <DallasTemperature.h>      // http://download.milesburton.com/Arduino/MaximTemperature/ (3.7.2 Beta needed for Arduino 1.0)

#include <RTClib.h>                 // Real time clock (RTC) - used for software RTC to reset kWh counters at midnight
#include <Wire.h>                   // Part of Arduino libraries - needed for RTClib
RTC_Millis RTC;

//--------------------------------------------------------------------------------------------
// RFM12B Settings
//--------------------------------------------------------------------------------------------
#define MYNODE 3             // Should be unique on network, node ID 30 reserved for base station
#define freq RF12_868MHZ     // frequency - match to same frequency as RFM12B module (change to 868Mhz or 915Mhz if appropriate)
#define group 212 

#define ONE_WIRE_BUS 5              // temperature sensor connection - hard wired 

unsigned long fast_update, slow_update;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
double temp,maxtemp,mintemp;

//---------------------------------------------------
// Data structures for transfering data between units
//---------------------------------------------------
typedef struct { int power1, power2, power3, Vrms; } PayloadTX;         // neat way of packaging data for RF comms
PayloadTX emontx;

typedef struct { int temperature; } PayloadGLCD;
PayloadGLCD emonglcd;

int hour = 0, minute = 0;
double usekwh = 0;

const int greenLED=6;               // Green tri-color LED
const int redLED=9;                 // Red tri-color LED
const int LDRpin=4;    		    // analog pin of onboard lightsensor 
int cval_use;

const int enterswitchpin=15;        // digital pin of enter switch - low when pressed
const int upswitchpin=16;           // digital pin of up switch - low when pressed
const int downswitchpin=19;         // digital pin of down switch - low when pressed

int ScreenNumber=0;                 // Number of screen
char outBuf [25];
//-------------------------------------------------------------------------------------------- 
// Flow control
//-------------------------------------------------------------------------------------------- 
unsigned long last_emontx;                   // Used to count time from last emontx update
unsigned long last_emonbase;                   // Used to count time from last emontx update

//--------------------------------------------------------------------------------------------
// Setup
//--------------------------------------------------------------------------------------------
void setup()
{
  Serial.begin(57600);
  rf12_initialize(MYNODE, freq,group);
  glcd.begin(0x20);
  glcd.backLight(200);
  //glcd.setFont(font_clR6x8);
  
  sensors.begin();                         // start up the DS18B20 temp sensor onboard  
  sensors.requestTemperatures();
  temp = (sensors.getTempCByIndex(0));     // get inital temperture reading
  mintemp = temp; maxtemp = temp;          // reset min and max

  pinMode(greenLED, OUTPUT); 
  pinMode(redLED, OUTPUT); 
}

//--------------------------------------------------------------------------------------------
// Loop
//--------------------------------------------------------------------------------------------
void loop()
{
    // Check switches S1, S2, S3
    int S1=digitalRead(enterswitchpin);
    int S2=digitalRead(upswitchpin);
    int S3=digitalRead(downswitchpin);
    
    if (S2) ScreenNumber=1;
    else if (S3) ScreenNumber=0;
          
  if (rf12_recvDone())
  {
    if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)  // and no rf errors
    {
      int node_id = (rf12_hdr & 0x1F);
      if (node_id == 2) {emontx = *(PayloadTX*) rf12_data; last_emontx = millis();}
      Serial.print(emontx.power1);
      Serial.print(" ");
      Serial.print(emontx.power2);
      Serial.print(" ");      
      Serial.print(emontx.power3);      
      Serial.print(" ");      
      Serial.print(emontx.Vrms); 
      Serial.println();    

      if (node_id == 15)
      {
        RTC.adjust(DateTime(2012, 1, 1, rf12_data[1], rf12_data[2], rf12_data[3]));
        last_emonbase = millis();
      } 
    }
  }

  //--------------------------------------------------------------------------------------------
  // Display update every 200ms
  //--------------------------------------------------------------------------------------------
  if ((millis()-fast_update)>200)
  {
    fast_update = millis();
    
    DateTime now = RTC.now();
    int last_hour = hour;
    hour = now.hour();
    minute = now.minute();

    usekwh += ((emontx.power1+emontx.power2+emontx.power3) * 0.2) / 3600000;
    cval_use = cval_use + ((emontx.power1+emontx.power2+emontx.power3) - cval_use)*0.50;
    Serial.println(cval_use);
    Serial.println(usekwh);    
    
    if (ScreenNumber==0)
    {
      draw_power_page( "POWER" ,cval_use, "USE", usekwh);
      draw_temperature_time_footer(temp, mintemp, maxtemp, hour,minute);
      glcd.refresh();
    }
    else if (ScreenNumber==1)
    {
      glcd.clear();

      dtostrf((float)emontx.power1/240,0,1,outBuf); 
      strcat(outBuf," A");
      glcd.drawString(0,0, "L1:");
      glcd.drawString(50,0, outBuf);          
      
      dtostrf((float)emontx.power2/240,0,1,outBuf); 
      strcat(outBuf," A");
      glcd.drawString(0,16, "L2:");
      glcd.drawString(50,16, outBuf);                
      
      dtostrf((float)emontx.power3/240,0,1,outBuf); 
      strcat(outBuf," A");
      glcd.drawString(0,32, "L3:");
      glcd.drawString(50,32, outBuf);                
      
//      sprintf(outBuf, "L1: %5d mA", (long int)emontx.power1*100/24); 
//      glcd.drawString(0,0, outBuf);
      
      sprintf(outBuf, "Total:   %4d W", cval_use);
      glcd.drawString(0,48, outBuf); 
      
      Serial.print((float)emontx.power1/240);
      Serial.print(" ");
      Serial.print((float)emontx.power2/240);
      Serial.print(" ");      
      Serial.print((float)emontx.power3/240);      
      Serial.println();         
            
      //glcd.drawString_P(0,0, PSTR("Phase-L1:"));
      //glcd.drawString_P(0,16, PSTR("Phase-L2:"));
      //glcd.drawString_P(0,32, PSTR("Phase-L3:"));
                  
      //glcd.drawString_P(0,48, PSTR("Zunanja:"));
      //glcd.drawString_P(0,64, PSTR("Notranja A:"));      
      //glcd.drawString_P(0,0, PSTR("Notranja B:"));        
      //glcd.drawString_P(0,0, PSTR("Peč:"));  
 
      glcd.refresh();
    }  

    int LDR = analogRead(LDRpin);                     // Read the LDR Value so we can work out the light level in the room.
    int LDRbacklight = map(LDR, 0, 1023, 50, 250);    // Map the data from the LDR from 0-1023 (Max seen 1000) to var GLCDbrightness min/max
    LDRbacklight = 255;  //constrain(LDRbacklight, 0, 255);   // Constrain the value to make sure its a PWM value 0-255
    if (S1) 
      glcd.backLight(LDRbacklight); 
    else 
      glcd.backLight(0);
      
    //if ((hour > 22) ||  (hour < 5)) glcd.backLight(0); else glcd.backLight(LDRbacklight);  
  } 
  
  if ((millis()-slow_update)>10000)
  {
    slow_update = millis();

    sensors.requestTemperatures();
    temp = (sensors.getTempCByIndex(0));
    if (temp > maxtemp) maxtemp = temp;
    if (temp < mintemp) mintemp = temp;
   
    emonglcd.temperature = (int) (temp * 100);                          // set emonglcd payload
    //int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}  // if ready to send + exit loop if it gets stuck as it seems too
    //rf12_sendStart(0, &emonglcd, sizeof emonglcd);                      // send emonglcd data
    //rf12_sendWait(0);    
  }
}
