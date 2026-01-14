
#include <Arduino.h>

#include <esp_system.h>
#include <string>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include <ElegantOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "WLAN_Credentials.h"
#include "config.h"
#include "wifi_mqtt.h"

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
long My_time = 0;
long Start_time;
long Up_time;
long U_days;
long U_hours;
long U_min;
long U_sec;

// Timers auxiliar variables
long now = millis();
int LEDblink = 0;
bool led = 1;

// Serial(2)
const uint8_t MSG_LEN = 50;
uint8_t buf[MSG_LEN];
uint8_t pos = 0;


    
// Create AsyncWebServer object on port 80
AsyncWebServer Asynserver(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Create an instance of the HardwareSerial class for Serial 2
HardwareSerial ld2460Serial(2);

// end of definitions -----------------------------------------------------

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin()) {
    log_i("An error has occurred while mounting LittleFS");
  }
  log_i("LittleFS mounted successfully");
}

String getOutputStates(){
  JSONVar myArray;
  
  U_days = Up_time / 86400;
  U_hours = (Up_time % 86400) / 3600;
  U_min = (Up_time % 3600) / 60;
  U_sec = (Up_time % 60);

  myArray["cards"][0]["c_text"] = Hostname;
  myArray["cards"][1]["c_text"] = WiFi.dnsIP().toString() + "   /   " + String(VERSION);
  myArray["cards"][2]["c_text"] = String(WiFi.RSSI());
  myArray["cards"][3]["c_text"] = String(MQTT_INTERVAL) + "ms";
  myArray["cards"][4]["c_text"] = String(U_days) + " days " + String(U_hours) + ":" + String(U_min) + ":" + String(U_sec);
  myArray["cards"][5]["c_text"] = "WiFi = " + String(WiFi_reconnect) + "   MQTT = " + String(Mqtt_reconnect);
  myArray["cards"][6]["c_text"] = "card6";
  myArray["cards"][7]["c_text"] = " to reboot click ok";
  myArray["cards"][8]["c_text"] = "8ms";
  myArray["cards"][9]["c_text"] = "9cm";
  myArray["cards"][10]["c_text"] = "10cm";
  
  String jsonString = JSON.stringify(myArray);
  return jsonString;
}

void notifyClients(String state) {
  ws.textAll(state);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;   // according to AsyncWebServer documentation this is ok
    int card;
    int value;

    log_i("Data received: ");
    log_i("%s\n",data);

    JSONVar myObject = JSON.parse((const char *)data);

    card =  myObject["card"];
    value =  myObject["value"];
    log_i("%d", card);
    log_i("%d",value);

    switch (card) {
      case 0:   // fresh connection
        notifyClients(getOutputStates());
        break;
      case 7:
        log_i("Reset..");
        ESP.restart();
        break;
      case 8:
        //SR04_scanInterval = value;
        notifyClients(getOutputStates());
        break;
      case 9:
        //SR04_cm_min = value;
        notifyClients(getOutputStates());
        break;
      case 10:
        //SR04_cm_max = value;
        notifyClients(getOutputStates());
        break;
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      log_i("WebSocket client connected");
      break;
    case WS_EVT_DISCONNECT:
      log_i("WebSocket client disconnected");
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}


void MQTTsend () {

  JSONVar mqtt_data, actuators;

  String mqtt_tag = Hostname + "/STATUS";
  log_i("%s\n", mqtt_tag.c_str());

  String s;
  s = "";
  for (int i = 0; i < pos; i++) {
    s += (char)buf[i];
  }

  mqtt_data["Time"] = My_time;
  mqtt_data["RSSI"] = WiFi.RSSI();
  mqtt_data["WIFIcon"] =WiFi_reconnect;
  mqtt_data["MQTTcon"] =Mqtt_reconnect;
  mqtt_data["Data"] = s;


  

  String mqtt_string = JSON.stringify(mqtt_data);

  log_i("%s\n", mqtt_string.c_str());

  mqttClient.publish(mqtt_tag.c_str(), mqtt_string.c_str());

  notifyClients(getOutputStates());
}

// receive MQTT messages
void MQTT_callback(String &topic, String &payload) {
  
  log_i("%s","Message arrived on topic: ");
  log_i("%s\n",topic);
  log_i("%s","Data : ");
  log_i("%s\n",payload);

  notifyClients(getOutputStates());
}


// setup 
void setup() {
  
  SERIALINIT                                 

  log_i("setup device\n");

  pinMode(GPIO_LED, OUTPUT);
  digitalWrite(GPIO_LED,led);

  log_i("setup WiFi\n");
  initWiFi();

  log_i("setup MQTT\n");
  initMQTT();
  mqttClient.onMessage(MQTT_callback);

  // Start Serial 2 with the defined RX and TX pins and a baud rate of 9600
  ld2460Serial.begin(LD2460_BAUD, SERIAL_8N1, RXD2, TXD2);
  log_i("Serial 2 started at 9600 baud rate");


  initSPIFFS();

    // init Websocket
  ws.onEvent(onEvent);
  Asynserver.addHandler(&ws);

  // Route for root / web page
  Asynserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html",false);
  });

  Asynserver.serveStatic("/", SPIFFS, "/");

  timeClient.begin();
  timeClient.setTimeOffset(0);
  // update UPCtime for Starttime
  timeClient.update();
  Start_time = timeClient.getEpochTime();

  // Start ElegantOTA
  ElegantOTA.begin(&Asynserver);
  
  // Start server
  Asynserver.begin();

}

void loop() {
  
  ws.cleanupClients();

  // update UPCtime
  timeClient.update();
  My_time = timeClient.getEpochTime();
  Up_time = My_time - Start_time;

  now = millis();

  // LED blinken
  if (now - LEDblink > 2000) {
    LEDblink = now;
    if(led == 0) {
      digitalWrite(GPIO_LED, 1);
      led = 1;
    }else{
      digitalWrite(GPIO_LED, 0);
      led = 0;
    }
  }

  pos = 0;
  while (ld2460Serial.available() > 0) {
    buf[pos++] = ld2460Serial.read();  // liest immer ein Byte aus dem RX-Buffer
    //log_i("%s\n",(char)buf[pos-1]);    gibt kernel panic ????
    ld2460Serial.print((char)buf[pos-1]);
  }

  if (pos > 0) {
    // Meldung empfangen
    MQTTsend();
    pos = 0;  // bereit für die nächste Meldung
  }


  // check WiFi
  if (WiFi.status() != WL_CONNECTED  ) {
    // try reconnect every 5 seconds
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;              // prevents mqtt reconnect running also
      // Attempt to reconnect
      reconnect_wifi();
    }
  }


  // check if MQTT broker is still connected
  if (!mqttClient.connected()) {
    // keinen scan ausführen
    //SR04_lastScan = now + 20000;
    // try reconnect every 5 seconds
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      reconnect_mqtt();
    }
  } else {
    // Client connected

    mqttClient.loop();

    // send data to MQTT broker
    if (now - Mqtt_lastSend > MQTT_INTERVAL) {
    Mqtt_lastSend = now;
    MQTTsend();
    } 
  }   
}