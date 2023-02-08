/***************************************************************************
  @matlen67 - 08.02.2023

  Wiring
  NodeMCU D1 - RS485 RO
  NodeMCU D3 - RS485 DE/RE
  NodeMCU D4 - RS485 DI

****************************************************************************/
#include <FS.h>  
#include "Arduino.h"
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsync_WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include "html.h"
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AsyncElegantOTA.h>

#define DEBUG true

#define DEBUG_SERIAL \
  if (DEBUG) Serial


#define RXPin        D1  // Serial Receive pin (D1)
#define TXPin        D4  // Serial Transmit pin (D4)
 
//RS485 control
#define SERIAL_COMMUNICATION_CONTROL_PIN D3 // Transmission set pin (D3)
#define RS485_TX_PIN_VALUE HIGH
#define RS485_RX_PIN_VALUE LOW
 
SoftwareSerial RS485Serial(RXPin, TXPin); // RX, TX

WiFiClient espClient;
PubSubClient client(espClient);

//Timer
unsigned long lastTime1 = 0;  
unsigned long timerDelay1 = 500;  // send readings timer

unsigned long lastTime2 = 0;  
unsigned long timerDelay2 = 2000;  // send readings timer

String msg = "";
char msgData[20];

//mqtt
char mqtt_server[40] = "192.168.178.10";
char mqtt_port[6] = "1889";
//default custom static IP
char static_ip[16] = "192.168.178.250";
char static_gw[16] = "192.168.178.1";
char static_sn[16] = "255.255.255.0";


String dataReceived;
int data;
bool isDataReceived = false;
uint8_t byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7; 
int byteSend;
int data_array[8];
int soyo_hello_data[8] = {0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}; // bit7 org 0x00, CRC 0xFF
int soyo_power_data[8] = {0x24, 0x56, 0x00, 0x21, 0x00, 0x00, 0x80, 0x08}; // 0 Watt

int cur_watt;
int cur_power;

bool newData = false;
int value_power = 0;
unsigned char mac[6];
String clientId;
String topic_alive;
String topic_power;

long rssi;


AsyncWebServer server(80);
AsyncEventSource events("/events");
AsyncDNSServer dns;


//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  DEBUG_SERIAL.println("Should save config");
  shouldSaveConfig = true;
}


void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}


String processor(const String& var){
   
   DEBUG_SERIAL.println(var);

    //STATICPOWER
    if(var == "STATICPOWER"){
      return String(value_power);
    }
    else if(var == "WIFIRSSI"){
      return String(rssi);      
    }
    else if(var == "CLIENTID"){
      return String(clientId);
    }
  
  return String();
}


void reconnect() {
  while (!client.connected()) {
    DEBUG_SERIAL.println("Attempting MQTT connection...");
   
    if (client.connect(clientId.c_str())) {
      DEBUG_SERIAL.println("connected");
      client.publish(topic_alive.c_str(), "1");
      client.publish(topic_power.c_str(), "0"); //power 0 watt
      client.subscribe(topic_power.c_str());    // subscripe topic
    } else {
      delay(3000);
    }
  }
}


//callback from mqtt
void callback(char* topic, byte* payload, unsigned int length) {
  DEBUG_SERIAL.print("Message arrived: ");
  DEBUG_SERIAL.print(topic);
  DEBUG_SERIAL.print(" ");
  bool isNumber = true;
  String topicString(topic);

  if(topicString == topic_power){

    for (int i=0;i<length;i++) {
      DEBUG_SERIAL.print((char)payload[i]);
    
      if(!isDigit(payload[i])){
        isNumber = false;
      }
    }
    
    if(isNumber && !newData){
      payload[length] = '\0'; 
      int aNumber = atoi((char *)payload);
      value_power = aNumber;
      newData = true;    
    }
  }

  DEBUG_SERIAL.println();
}


void setSoyoPowerData(int power){
  soyo_power_data[0] = 0x24;
  soyo_power_data[1] = 0x56;
  soyo_power_data[2] = 0x00;
  soyo_power_data[3] = 0x21;
  soyo_power_data[4] = power >> 0x08;
  soyo_power_data[5] = power & 0xFF;
  soyo_power_data[6] = 0x80;
  soyo_power_data[7] = calc_checksumme(soyo_power_data[1], soyo_power_data[2], soyo_power_data[3], soyo_power_data[4], soyo_power_data[5], soyo_power_data[6]);
}


int calc_checksumme(int b1, int b2, int b3, int b4, int b5, int b6 ){
  int calc = (0xFF - b1 - b2 - b3 - b4 - b5 -b6) % 256;
  return calc & 0xFF;
}


void setup()  {
  DEBUG_SERIAL.begin(115200);
  delay(250);

  WiFi.macAddress(mac);
  
  DEBUG_SERIAL.println("Start");
  DEBUG_SERIAL.printf("ESP_%02X%02X%02x", mac[3], mac[4], mac[5]);
  DEBUG_SERIAL.println();
  
  //clientId = "soyo_%02X%02X%02x", mac[3], mac[4], mac[5];
  clientId = "soyo_"+ String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  topic_alive = "SoyoSource/soyo_" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX) + "/alive";
  topic_power = "SoyoSource/soyo_" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX) + "/power";

  DEBUG_SERIAL.println(topic_alive);
  
  pinMode(SERIAL_COMMUNICATION_CONTROL_PIN, OUTPUT);
  digitalWrite(SERIAL_COMMUNICATION_CONTROL_PIN, RS485_RX_PIN_VALUE);
  RS485Serial.begin(4800);   // set RS485 baud

  //read configuration from FS json
  DEBUG_SERIAL.println("mounting FS...");

  if (SPIFFS.begin()) {
    DEBUG_SERIAL.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      DEBUG_SERIAL.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        DEBUG_SERIAL.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
          DynamicJsonDocument json(1024);
          auto deserializeError = deserializeJson(json, buf.get());
          serializeJson(json, Serial);
          if ( ! deserializeError ) {
        #else
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success()) {
        #endif
          DEBUG_SERIAL.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);

        } else {
          DEBUG_SERIAL.println("failed to load json config");
        }
      }
    }
  } else {
    DEBUG_SERIAL.println("failed to mount FS");
  }
  //end read

  
  WiFi.persistent(true); // sonst verlieret er nach einem Neustart die IP !!!

  ESPAsync_WMParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  ESPAsync_WMParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5); 

  ESPAsync_WiFiManager wifiManager(&server,&dns);

  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.setConfigPortalTimeout(180);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);

  String apName = "soyo_esp_" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  bool res;
  res = wifiManager.autoConnect(apName.c_str());

  if(!res) {
    DEBUG_SERIAL.println("Failed to connect");
    ESP.restart();
  }
  else {
    //if you get here you have connected to the WiFi    
    DEBUG_SERIAL.print("WiFi connected to ");
    DEBUG_SERIAL.println(String(WiFi.SSID()));
    DEBUG_SERIAL.print("RSSI = ");
    DEBUG_SERIAL.print(String(WiFi.RSSI()));
    DEBUG_SERIAL.println(" dB");
    DEBUG_SERIAL.print("IP address  ");
    DEBUG_SERIAL.println(WiFi.localIP());
    DEBUG_SERIAL.println();

    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    
    //save the custom parameters to FS
    if (shouldSaveConfig) {
      DEBUG_SERIAL.println("saving config");
      #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
       DynamicJsonDocument json(1024);
      #else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.createObject();
      #endif
      json["mqtt_server"] = mqtt_server;
      json["mqtt_port"] = mqtt_port;
  
      json["ip"] = WiFi.localIP().toString();
      json["gateway"] = WiFi.gatewayIP().toString();
      json["subnet"] = WiFi.subnetMask().toString();

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        DEBUG_SERIAL.println("failed to open config file for writing");
      }

      #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
        serializeJson(json, Serial);
        serializeJson(json, configFile);
      #else
        json.printTo(Serial);
        json.printTo(configFile);
      #endif
      configFile.close();
    }

    DEBUG_SERIAL.println("set mqtt server!");
    DEBUG_SERIAL.println(String("mqtt_server: ") + mqtt_server);
    DEBUG_SERIAL.println(String("mqtt_port: ") + mqtt_port);

    client.setServer(mqtt_server, atoi(mqtt_port));
    client.setCallback(callback);
    client.publish(topic_alive.c_str(), "0");

    // Handle Web Server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", index_html, processor);
    });

    // Handle Web Server Events
    events.onConnect([](AsyncEventSourceClient *client){
      if(client->lastId()){
        DEBUG_SERIAL.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
      }
      
      client->send("hello!", NULL, millis(), 10000);
    });

    // set soyo power to 0
    server.on("/set_0", HTTP_GET, [](AsyncWebServerRequest *request) {
      value_power = 0;
      sprintf(msgData, "%d",value_power);
      client.publish(topic_power.c_str(), msgData);
      newData = true;
      request->send_P(200, "text/html", index_html, processor);
    });


    // current soyp power +1
    server.on("/plus1", HTTP_GET, [](AsyncWebServerRequest *request) {
      value_power +=1;
      sprintf(msgData, "%d",value_power);
      client.publish(topic_power.c_str(), msgData);
      newData = true;
      request->send_P(200, "text/html", index_html, processor);
    });

    // current soyp power +10
    server.on("/plus10", HTTP_GET, [](AsyncWebServerRequest *request) {
      value_power +=10;
      sprintf(msgData, "%d",value_power);
      client.publish(topic_power.c_str(), msgData);
      newData = true;
      request->send_P(200, "text/html", index_html, processor);
    });

    // current soyp power -1
    server.on("/minus1", HTTP_GET, [](AsyncWebServerRequest *request) {
      value_power -=1;
      if(value_power < 0){
        value_power = 0;
      }
      sprintf(msgData, "%d",value_power);
      client.publish(topic_power.c_str(), msgData);
      newData = true;
      request->send_P(200, "text/html", index_html, processor);
      
    });

    // current soyp power -10
    server.on("/minus10", HTTP_GET, [](AsyncWebServerRequest *request) {
      value_power -=10;
      if(value_power < 0){
        value_power = 0;
      }
      sprintf(msgData, "%d",value_power);
      client.publish(topic_power.c_str(), msgData);
      newData = true;
      request->send_P(200, "text/html", index_html, processor);
    });

    // send wifi rssi
    server.on("/wifi_rssi", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send_P(200, "text/html", index_html, processor);
    });

    AsyncElegantOTA.begin(&server);    // Start AsyncElegantOTA

    server.onNotFound(notFound);
    server.addHandler(&events);
    server.begin();
    DEBUG_SERIAL.println("Server start");
    
  }

  // end setup()  
}



void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  digitalWrite(SERIAL_COMMUNICATION_CONTROL_PIN, RS485_RX_PIN_VALUE); // Init receive
 
  if (RS485Serial.available() >= 8){
    for(int i=0; i<8; i++){
      data_array[i] = RS485Serial.read();
      if(data_array[0] == 0x24 || data_array[0] == 0x21){
        DEBUG_SERIAL.print(char(data_array[i]),HEX);
        DEBUG_SERIAL.print(" ");
      } else {
        DEBUG_SERIAL.print("data error !!!!");
        DEBUG_SERIAL.print("data_array[0] = ");
        DEBUG_SERIAL.print(data_array[0],HEX);
        DEBUG_SERIAL.println();
        break;
      } 
    }
    DEBUG_SERIAL.println();
  }
      
    
  //send power to SoyoSource every 500ms
  if ((millis() - lastTime1) > timerDelay1) {
    digitalWrite(SERIAL_COMMUNICATION_CONTROL_PIN, RS485_TX_PIN_VALUE); // Init transmit

    if(newData == true && value_power >=0){
      setSoyoPowerData(value_power);
      newData = false;
    }
    

    for(int i=0; i<8; i++){
      RS485Serial.write(soyo_power_data[i]);
      DEBUG_SERIAL.print(soyo_power_data[i], HEX);
      DEBUG_SERIAL.print(" ");
    }
    DEBUG_SERIAL.println();
    events.send(String(value_power).c_str(),"static_Power",millis());
    lastTime1 = millis();
  }


  //send hello to SoyoSource every 2000ms
  if ((millis() - lastTime2) > timerDelay2) {
    digitalWrite(SERIAL_COMMUNICATION_CONTROL_PIN, RS485_TX_PIN_VALUE); // Init transmit

    for(int i=0; i<8; i++){
      RS485Serial.write(soyo_hello_data[i]);
    }
    //DEBUG_SERIAL.print(String(WiFi.RSSI()));
    rssi = WiFi.RSSI();
    events.send(String(rssi).c_str(), "wifi_RSSI", millis());
    client.publish(topic_alive.c_str(), "1");
    
    lastTime2 = millis();
  }
 
   
}