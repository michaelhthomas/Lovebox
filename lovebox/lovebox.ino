#include <Reactduino.h>

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <Servo.h>
#include <SSD1306Wire.h>

// WiFi Credential Manager
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager WiFi Configuration Magic

// Helper functions & includes
#include "includes/EEPROM_int.h"
#include "includes/logo.h" // Lovebox logo for when there is no message to be displayed or when initializing

// Pull in and adapt settings
#include "settings.h"
const int fetchIntervalMillis = fetchIntervalSeconds * 1000;
const int brightnessCheckMillis = brightnessCheckSeconds * 1000;

// Global variable definitions
SSD1306Wire oled(0x3C, D3, D2);
Servo heartServo;
int increment = -1;
String line;
String mode;
int idSaved = 0;
bool wasRead = true;
bool screenOn = true;
reaction box_process;
reaction display_process;
WiFiManager wifiManager;

/*
 *  Connect to the server and download message from gist
 */
void getGistMessage() {
  Serial.print("Fetching message...");
  const int httpsPort = 443;
  const char* host = "gist.githubusercontent.com";
  const char fingerprint[] = "70 94 DE DD E6 C4 69 48 3A 92 70 A1 48 56 78 2D 18 64 E0 B7";
  
  WiFiClientSecure client;
  client.setFingerprint(fingerprint);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed.");
    return; // Connection failed
  }

  // current millis used as a cache-busting means
  client.print(String("GET ") + url + "?" + millis() + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String temp = client.readStringUntil('\n');
    if (temp == "\r") {
      break;
    }
  }
  int id = atoi(client.readStringUntil('\n').c_str());
  Serial.printf("\tid: '%d', last processed id: '%d'\n", id, idSaved);
  if(id != idSaved){ // new message
    switchProcess(0);
    idSaved = id;
    writeIntIntoEEPROM(142, idSaved);
    EEPROM.commit(); 

    mode = client.readStringUntil('\n');
    Serial.println("\tmode: " + mode);
    line = client.readStringUntil(0);
    Serial.println("\tmessage: " + line);
    drawMessage(line);
  } else {
    Serial.println("\t-> message id wasn't updated");
  }
}

/*
 *  Display message on screen
 */
void drawMessage(const String& message) {
  Serial.print("Drawing message....");
  oled.clear();

  // Differentiates between text and image modes
  if(mode[0] == 't'){
    oled.drawStringMaxWidth(0, 0, 128, message);    
  } 
  else {
    for(int i = 0; i <= message.length(); i++){
      int x = i % 129;
      int y = i / 129;
    
      if(message[i] == '1'){
        oled.setPixel(x, y);
      }
    } 
  }    
  oled.display();
  Serial.println("done.");
}

/*
 *  Spin the servo when a new message is available
 */
void spinServo() {
  static int pos = initialServoPosition;
  static int increment = -1;

  heartServo.write(pos);      
  delay(50);    // wait 50ms to turn servo

  if(pos == (initialServoPosition - 15) || pos == (initialServoPosition + 15)){ // 15° rotation range
    increment *= -1;
  }
  pos += increment;
}

/*
 *  Reset the serco to the middle
 */
void resetServo() {
  heartServo.write(initialServoPosition);
}

/*
 *  Turn screen on/off based on light value
 */
void checkScreen() {
  int lightValue;
  // If the screen is on, turn it off briefly so that the light sensor can get an accurate reading
  if(screenOn) {
    oled.displayOff();
    delay(500); // Wait for display to turn off before checking
    lightValue = analogRead(0);   // Read light value
    oled.displayOn();
  } else {
    lightValue = analogRead(0);   // Read light value
  }
  
  if(lightValue > lightValueThreshold) {
    if(!screenOn) {
      oled.displayOn();
      screenOn = true;
      app.free(display_process);
      display_process = app.repeat(brightnessCheckMillis, checkScreen); // Only check the screen periodically, as the flicker to proplerly check brightness would be a little annoying otherwise
      Serial.printf("Analog read value (LDR) %d above threshold of %d -> turning screen on.\n", lightValue, lightValueThreshold);
    }
    if(!wasRead) { 
      switchProcess(1);
    }
  } else {
    if(screenOn) {
      oled.displayOff();
      screenOn = false;
      app.free(display_process);
      display_process = app.repeat(100, checkScreen); // Check the screen every 100ms to ensure that it will turn on immediately when the box is opened
      Serial.printf("Analog read value (LDR) %d below threshold of %d -> turning screen off.\n", lightValue, lightValueThreshold);
    } else {
      // Serial.printf("Analog read value (LDR) %d below threshold of %d -> keeping screen off.\n", lightValue, lightValueThreshold);
    }
  }
}

/*
 *  Switch between two processes: 
 *   - Spin servo when there is a new message
 *   - Once the message is read, stop spinning the servo and check for new messages
 */
void switchProcess(bool s) {
  switch (s) {
    case 0: 
      wasRead = false;
      app.free(box_process);
      box_process = app.repeat(50, spinServo);
      break;
    case 1: 
      wasRead = true;
      app.free(box_process);
      box_process = app.repeat(fetchIntervalMillis, getGistMessage);
      resetServo();
      break;
  }
}

Reactduino app([] () {
  // Setup serial
  Serial.begin(9600);
  Serial.println("\n\n");
  
  // Setup servo
  Serial.print("Attatching servo...");
  heartServo.attach(16);       // Servo on D0
  Serial.println("done.");
  resetServo(); // set servo to starting position
  
  // Setup display
  Serial.print("Initializing display...");
  oled.init();
  oled.flipScreenVertically();
  oled.setColor(WHITE);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.setFont(ArialMT_Plain_10);
  oled.clear();
  oled.drawXbm(0, 0, Lovebox_Logo_width, Lovebox_Logo_height, Lovebox_Logo_bits);
  oled.display();
  Serial.println("done.");
  
  // Load last message id from EEPROM
  EEPROM.begin(512);
  idSaved = readIntFromEEPROM(142);
  Serial.println(idSaved);

  // Setup wifi using wifiManager
  WiFi.mode(WIFI_STA); // Disables AP mode unless needed
  wifiManager.autoConnect("Lovebox");
  getGistMessage();

  // Disable the built-in LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  app.free(box_process);
  if(wasRead) {
    // Check for and display new messages
    box_process = app.repeat(fetchIntervalMillis, getGistMessage);
  } else {
    // Wait for message to be read
    box_process = app.repeat(50, spinServo);
  }

  // Turn screen on and off based on light value
  display_process = app.repeat(brightnessCheckMillis, checkScreen);
});