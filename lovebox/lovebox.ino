#include <Reactduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <Servo.h>
#include <SSD1306Wire.h>

#include "settings.h"
const int fetchIntervalMillis = _fetchIntervalSeconds * 1000;
const char* ssid = _ssid;
const char* password = _password;
const String url = _url;
const int lightValueThreshold = _lightValueThreshold;

SSD1306Wire oled(0x3C, D2, D1);
Servo heartServo; 
int pos = 90;
int increment = -1;
int lightValue;
String line;
String mode;
char idSaved = '0'; 
bool wasRead = true;
reaction process;

/*
 *  Make sure WiFi is connected and load credentials
 */
void wifiConnect() {
  Serial.printf("Connecting to WiFi '%s'...", ssid);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
  
    // Waiting for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  }
  Serial.print("..done. IP ");
  Serial.println(WiFi.localIP());
}

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
    serial.println("connection failed.");
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
  String id = client.readStringUntil('\n');
  Serial.printf("\tid: '%s', last processed id: '%c'\n", id.c_str(), idSaved);
  if(id[0] != idSaved){ // new message
    wasRead = 0;
    idSaved = id[0];
    EEPROM.write(142, idSaved);
    EEPROM.write(144, wasRead);
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

  // Unterscheide zwischen Text und Bild
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
  heartServo.write(pos);      
  delay(50);    // wait 50ms to turn servo

  if(pos == 75 || pos == 105){ // 75°-105° range
    increment *= -1;
  }
  pos += increment;
}

/*
 *  Turn screen on/off based on light value
 */
void checkScreen() {
  if(lightValue > lightValueThreshold) {
    oled.on();
  } else {
    oled.off();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
  }
  
  if(wasRead){
    getGistMessage();   
  }
  
  while(!wasRead){   
    spinServo();    // Turn the heart
    lightValue = analogRead(0);   // Read light value
    if(lightValue > lightValueThreshold) {
      Serial.printf("Analog read value (LDR) %d above threshold of %d -> consider message read.\n", lightValue, lightValueThreshold);
      wasRead = 1;
      EEPROM.write(144, wasRead);
      EEPROM.commit();
    }
  }
}

Reactduino app([] () {
  // Setup serial
  Serial.begin(9600);
  Serial.println("\n\n");
  
  Serial.print("Attatching servo...");
  heartServo.attach(16);       // Servo on D0
  Serial.println("done.");
  
  Serial.print("Initializing display...");
  oled.init();
  oled.flipScreenVertically();
  oled.setColor(WHITE);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.setFont(ArialMT_Plain_10);
     
  oled.clear();
  oled.drawString(30, 30, "<3 LOVEBOX <3");
  oled.display();
  Serial.println("done.");
  
  wifiConnect();

  EEPROM.begin(512);
  idSaved = EEPROM.get(142, idSaved);
  wasRead = EEPROM.get(144, wasRead);

  // Turn screen on and off based on light value
  app.repeat(100, checkScreen);

  if wasRead {
    // Check for and display new messages
    process = app.repeat(fetchIntervalMillis, getGistMessage);
  } else {
    // Wait for message to be read
    process = app.repeat(50, spinServo);
  }
});