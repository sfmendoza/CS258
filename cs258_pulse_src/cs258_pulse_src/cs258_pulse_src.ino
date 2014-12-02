
/*

 Sensor wire goes to A0
 
 Pulse Sensor sample aquisition and processing happens in the background via Timer 2 interrupt. 2mS sample rate.
 PWM on pins 3 and 11 will not work when using this code, because we are using Timer 2!
 The following variables are automatically updated:
 Signal :      int that holds the analog signal data straight from the sensor. updated every 2mS.
 IBI  :        int that holds the time interval between beats. 2mS resolution.
 BPM  :        int that holds the heart rate value, derived every beat, from averaging previous 10 IBI values.
 hasPulse  :   boolean that is made true whenever Pulse is found and BPM is updated. User must reset.
 Pulse :       boolean that is true when a heartbeat is sensed then false in time with pin13 LED going out.
 
 */

// Include required libraries for WiFi
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

#include<stdlib.h>

// Define CC3000 chip pins
#define ADAFRUIT_CC3000_IRQ   3
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10

// WiFi network
#define WLAN_SSID       "HOME DL"
#define WLAN_PASS       "1mendoza99"
#define WLAN_SECURITY   WLAN_SEC_WPA2 

Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT, SPI_CLOCK_DIVIDER);
Adafruit_CC3000_Client client;

// server IP, port, and repository (change with your settings !)
uint32_t   t;
uint32_t ip = cc3000.IP2U32(192,168,0,104);
int port = 80;
String repository = "/webapp/";

const unsigned long
dhcpTimeout     = 60L * 1000L, // Max time to wait for address from DHCP
connectTimeout  = 15L * 1000L, // Max time to wait for server connection
responseTimeout = 15L * 1000L; // Max time to wait for data from server

// include the LCD library code:
#include <LiquidCrystal.h>

// initialize the LCD library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// make a heart custom character:
byte heart[8] = {
  0b00000,
  0b01010,
  0b11111,
  0b11111,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};

//  VARIABLES
int pulsePin = 0;                 // Sensor wire connected to analog pin 0
int blinkPin = 13;                // pin to blink led at each beat
int fadePin = 5;                  // pin to do fancy classy fading blink at each beat
int fadeRate = 0;                 // used to fade LED on with PWM on fadePin

// these variables are volatile because they are used during the interrupt service routine!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, must be seeded! 
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean hasPulse = false;  // becomes true when Arduoino finds a beat.

void setup(){
  Serial.begin(115200);              // we agree to talk fast!

  setupLCD();                        // sets up the LCD
  //setupWifi();

  pinMode(blinkPin, OUTPUT);         // pin that will blink to your heartbeat!
  pinMode(fadePin, OUTPUT);          // pin that will fade to your heartbeat!

  interruptSetup();                  // sets up to read Sensor signal every 2mS 
}


void loop(){
  sendDataToProcessing('S', Signal);     // send Processing the raw Pulse Sensor data
  if (hasPulse == true){                 // hasPulse flag is true when arduino finds a heartbeat
    fadeRate = 255;                      // Set 'fadeRate' Variable to 255 to fade LED with pulse
    sendDataToProcessing('B', BPM);      // send heart rate with a 'B' prefix
    sendDataToProcessing('Q', IBI);      // send time between beats with a 'Q' prefix
    hasPulse = false;                    // reset the hasPulse flag for next time    
  }

  ledFadeToBeat();

  delay(20);                             //  take a break
}


void ledFadeToBeat(){
  fadeRate -= 15;                         //  set LED fade value
  fadeRate = constrain(fadeRate,0,255);   //  keep LED fade value from going into negative numbers!
  analogWrite(fadePin,fadeRate);          //  fade LED
}


void sendDataToProcessing(char symbol, int data ){
  Serial.print(symbol);                // symbol prefix tells Processing what type of data is coming
  Serial.println(data);                // the data to send culminating in a carriage return
  //Serial.print("  - ");
  //Serial.println(BPM);  
  writeToLCD();
  //sendToWifi();
}


void setupLCD() {

  analogWrite(6, 127);      // set up the LCD's contrast

  lcd.begin(16, 2);         // set up the LCD's number of columns and rows
  //lcd.createChar(0, heart); // create the new character (heart)
  lcd.setCursor(0, 0);
  //lcd.print("hello world");      // Print a message to the LCD.
  lcd.print("CS 258");
}

void writeToLCD() {

  lcd.setCursor(0, 1); // set the cursor to the second line

  // this displays BPM xx (heart)
  lcd.print("BPM ");
  lcd.print(BPM);
  lcd.print("                ");
  //lcd.write(byte(0));
}


void setupWifi() {
  /* Initialise the module */
  Serial.println(F("\nInitialising the CC3000 ..."));
  if (!cc3000.begin()) {
    Serial.println(F("Unable to initialise the CC3000! Check your wiring?"));
    while(1);
  }

  /* Delete any old connection data on the module */
  Serial.println(F("\nDeleting old connection profiles"));
  if (!cc3000.deleteProfiles()) {
    Serial.println(F("Failed!"));
    while(1);
  }

  /* Attempt to connect to an access point */
  char *ssid = WLAN_SSID;
  Serial.print(F("\nAttempting to connect to ")); 
  Serial.println(ssid);

  /* NOTE: Secure connections are not available in 'Tiny' mode! */
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }

  /* Wait for DHCP to complete */
  Serial.println(F("\nRequest DHCP"));
  while (!cc3000.checkDHCP())  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  
  
   Serial.println(F("...Connecting to server"));
  client = cc3000.connectTCP(ip, port);
  if (client.connected()) {
    Serial.println(F("Connected")); 
  } 
  else {
    Serial.println(F("Connection failed"));    
  }
}

void sendToWifi() {
//  //Open Socket
//  if (client.connected()) {
//    Serial.println("Connected"); 
//    String request = "GET "+ repository + "data/sendData?val=" + BPM + " HTTP/1.1\r\nConnection: close\r\n\r\n";
//
//    Serial.print("...Sending request:");
//    Serial.println(request);
//    send_request(request);
//  } 
//  else {
//    Serial.println(F("Connection failed"));    
//    return;
//  }
}

/*******************************************************************************
 * send_request
 ********************************************************************************/
bool send_request (String request) {
  // Transform to char
  char requestBuf[request.length()+1];
  request.toCharArray(requestBuf,request.length()); 
  // Send request
  if (client.connected()) {
    client.fastrprintln(requestBuf); 
  } 
  else {
    Serial.println(F("Connection failed"));    
    return false;
  }
  return true;
  free(requestBuf);
}



