
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


//LD2460
int PosX[4];
int PosY[4];
int Targets = 0;
    
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
  myArray["cards"][6]["c_text"] = String(Targets);
  myArray["cards"][7]["c_text"] = " to reboot click ok";
 
  
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



  mqtt_data["Time"] = My_time;
  mqtt_data["RSSI"] = WiFi.RSSI();
  mqtt_data["WIFIcon"] = WiFi_reconnect;
  mqtt_data["MQTTcon"] = Mqtt_reconnect;
  mqtt_data["Targets"] = Targets;
  mqtt_data["T1x"] = PosX[0];
  mqtt_data["T1y"] = PosY[0];
  mqtt_data["T2x"] = PosX[1];
  mqtt_data["T2y"] = PosY[1];
  mqtt_data["T3x"] = PosX[2];
  mqtt_data["T3y"] = PosY[2];
  mqtt_data["T4x"] = PosX[3];
  mqtt_data["T4y"] = PosY[4];
  mqtt_data["T5x"] = PosX[5];
  mqtt_data["T5y"] = PosY[5];


  

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
  log_i("Serial 2 started ");


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



  if (ld2460Serial.available() > 55 ) {
    byte rec_buf[256] = "";
    int len = ld2460Serial.readBytes(rec_buf, sizeof(rec_buf));

    log_i("daten gelesen");
    log_i("%d",len);

    for (int i = 0; i < len; i++) {
      if (rec_buf[i] == 0xF4 && rec_buf[i + 1] == 0xF3 && rec_buf[i + 2] == 0xF2 && rec_buf[i + 3] == 0xF1 && rec_buf[i + 4] == 0x04) {

        int index = i + 5;
        int len = (int16_t)(rec_buf[index] | (rec_buf[index + 1] << 8));

        if (len < 11) {
          break;
        }

        Targets = (len - 11) / 4;

        log_i("Anz. Targets =");
        log_i("%d",Targets);

        index = i + 7;
        for (int j = 0; j < Targets; j++) {
                
          PosX[j]= (int16_t)(rec_buf[index] | (rec_buf[index + 1] << 8));
          PosY[j] = (int16_t)(rec_buf[index + 2] | (rec_buf[index + 3] << 8));

          log_i("Target");
          log_i("%d",PosX[j]);
          log_i("%d",PosY[j]);

          index = index + 4;
          i = index;
        }
      }
    }
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

