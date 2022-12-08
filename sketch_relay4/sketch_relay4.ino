///http://ip_addr/relay?r1={value}

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>       //https://github.com/knolleary/pubsubclient
#include <FS.h>
#include <LittleFS.h>

WiFiManager wifiManager;
ESP8266WebServer server(80);

#define SERIAL_DEBUG

//reset pin
String pin = "3363";

#define R1_PIN 16
#define R2_PIN 4
#define R3_PIN 3
#define R4_PIN 14

#define USE_MQTT //comment for disable MQTT functional

typedef struct gcfg { //struct for store global config to spiffs file
  #ifdef USE_MQTT
  char mqtt_host[50];
  uint16_t mqtt_port;
  char mqtt_user[20];
  char mqtt_pwd[20];
  char topic_r1[20];
  char topic_r2[20];
  char topic_r3[20];
  char topic_r4[20];
  #endif
} gcfg;
gcfg _gconfig;

//LFS
bool WriteLFS_config(const char * path, gcfg * inStruct)  {
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  file.write((byte*)inStruct, sizeof(gcfg) / sizeof(byte));
  file.close();
  return true;
}

bool ReadLFS_config(const char * path, gcfg * outStruct)  {
  File file = LittleFS.open(path, "r");
  if (!file) return 0;
  while (file.available()) {
    byte tmp = file.read();
    memcpy((byte*)outStruct + file.position()-1, &tmp, sizeof(byte));
  }
  file.close();
  return true;
}

// MQTT
#ifdef USE_MQTT
uint32_t mqtt_connect_timer;

WiFiClient espClient;
PubSubClient client(espClient);

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    
  String data_pay;
  for (int i = 0; i < length; i++) {
    data_pay += String((char)payload[i]);
  }
  #ifdef SERIAL_DEBUG
  Serial.println(data_pay);
  #endif
  
  uint8_t relay = 0;
  if(String(topic) == String(_gconfig.topic_r1)) relay = R1_PIN;
  #ifdef R2_PIN
  if(String(topic) == String(_gconfig.topic_r2)) relay = R2_PIN;
  #endif
  #ifdef R3_PIN
  if(String(topic) == String(_gconfig.topic_r3)) relay = R3_PIN;
  #endif
  #ifdef R4_PIN
  if(String(topic) == String(_gconfig.topic_r4)) relay = R4_PIN;
  #endif

  if( relay != 0 ) digitalWrite(relay, data_pay.toInt());

  led_blink(200);
}
#endif

void led_blink(uint16_t pause){
  digitalWrite(LED_BUILTIN, HIGH);
  delay(pause);
  digitalWrite(LED_BUILTIN, LOW);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handle_r() {

  #ifdef SERIAL_DEBUG
  String message = "Arguments: ";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  Serial.println(message);
  #endif
  
  uint8_t relay = 0;
  if(server.argName(0) == "r1") relay = R1_PIN;
  #ifdef R2_PIN
  if(server.argName(0) == "r2") relay = R2_PIN;
  #endif
  #ifdef R3_PIN
  if(server.argName(0) == "r3") relay = R3_PIN;
  #endif
  #ifdef R4_PIN
  if(server.argName(0) == "r4") relay = R4_PIN;
  #endif

  #ifdef USE_MQTT
  String topic;
  if(server.argName(0) == "r1") topic = String(_gconfig.topic_r1);
  #ifdef R2_PIN
  if(server.argName(0) == "r2") topic = String(_gconfig.topic_r2);
  #endif
  #ifdef R3_PIN
  if(server.argName(0) == "r3") topic = String(_gconfig.topic_r3);
  #endif
  #ifdef R4_PIN
  if(server.argName(0) == "r4") topic = String(_gconfig.topic_r4);
  #endif
  #endif
  
  //set state
  if(server.arg(0) != "{value}") //not status query from Kyzia
    if( relay != 0 ) { 
      digitalWrite(relay, server.arg(0).toInt());
      #ifdef USE_MQTT
      if(!topic.isEmpty()) client.publish(topic.c_str(), server.arg(0).c_str(), true);
      #endif
    }

  //return status
  server.send(200, "text/plain", "{\"status\":\"ok\",\"text\":\"Готово\",\"value\":\""+String(digitalRead(relay))+"\"}");

  led_blink(200);
}

#ifdef USE_MQTT

void mqtt_reconnect() {
    if (client.connected()) client.disconnect();
    #ifdef SERIAL_DEBUG
    Serial.print("Attempting MQTT connection...");
    #endif
    client.setServer(_gconfig.mqtt_host, _gconfig.mqtt_port);
    client.setCallback(mqtt_callback);
    String clientId = "ESPRELAY4-" + WiFi.macAddress();
    if (ReadLFS_config("/config", &_gconfig)) {
        if (client.connect(clientId.c_str(), _gconfig.mqtt_user, _gconfig.mqtt_pwd) ) {
          #ifdef SERIAL_DEBUG
          Serial.println("connected");      
          #endif
          client.subscribe( (String(_gconfig.topic_r1) + "/#").c_str() );
          #ifdef R2_PIN
          client.subscribe( (String(_gconfig.topic_r2) + "/#").c_str() );
          #endif
          #ifdef R3_PIN
          client.subscribe( (String(_gconfig.topic_r3) + "/#").c_str() );
          #endif
          #ifdef R4_PIN
          client.subscribe( (String(_gconfig.topic_r4) + "/#").c_str() );
          #endif
        } else {
          mqtt_connect_timer = millis();
          #ifdef SERIAL_DEBUG
          Serial.print("failed, rc=");
          Serial.println(client.state());
          #endif
        }
   } else { //ReadLFS_config
    mqtt_connect_timer = millis();
    #ifdef SERIAL_DEBUG
    Serial.println("Global config not found, skip MQTT connection");
    #endif
  }
}
#endif

void setup() {
  #ifdef SERIAL_DEBUG
  Serial.begin(115200);
  Serial.println("Booting");
  #endif
  //pinmode
  pinMode(R1_PIN, OUTPUT);
  digitalWrite(R1_PIN, LOW);
  #ifdef R2_PIN
  pinMode(R2_PIN, OUTPUT);
  digitalWrite(R2_PIN, LOW);
  #endif
  #ifdef R3_PIN
  pinMode(R3_PIN, OUTPUT);
  digitalWrite(R3_PIN, LOW);
  #endif
  #ifdef R4_PIN
  pinMode(R4_PIN, OUTPUT);
  digitalWrite(R4_PIN, LOW);
  #endif

  pinMode(LED_BUILTIN, OUTPUT);

  //fs
  if(!LittleFS.begin()){
   #ifdef SERIAL_DEBUG
   Serial.println("An Error has occurred while mounting LittleFS");
   #endif
   LittleFS.format();
   ESP.restart();
  }

  wifiManager.autoConnect("ESPRELAY-4");
  #ifdef SERIAL_DEBUG
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  #endif
  digitalWrite(LED_BUILTIN, LOW);

  server.on("/", []() {

    //if save config
    if(server.arg("config") == "1") {
     #ifdef USE_MQTT
      server.arg("mqtt_host").toCharArray(_gconfig.mqtt_host, 50);
      server.arg("mqtt_user").toCharArray(_gconfig.mqtt_user, 20);
      server.arg("mqtt_pwd"). toCharArray(_gconfig.mqtt_pwd,  20);
      _gconfig.mqtt_port = server.arg("mqtt_port").toInt();
      server.arg("topic_r1"). toCharArray(_gconfig.topic_r1,  20);
      #ifdef R2_PIN
      server.arg("topic_r2"). toCharArray(_gconfig.topic_r2,  20);
      #endif      
      #ifdef R3_PIN
      server.arg("topic_r3"). toCharArray(_gconfig.topic_r3,  20);
      #endif      
      #ifdef R4_PIN
      server.arg("topic_r4"). toCharArray(_gconfig.topic_r4,  20);
      #endif      
      #endif //USE_MQTT
      if (WriteLFS_config("/config", &_gconfig)) {
        #ifdef SERIAL_DEBUG
        Serial.println("WriteLFS_config Ok");
        #endif
        #ifdef USE_MQTT
        mqtt_reconnect();
        #endif
        led_blink(200);
      } else {
        #ifdef SERIAL_DEBUG
        Serial.println("WriteLFS_config ERROR");
        #endif
      }
    }
    
    String web = "<!DOCTYPE html> <html>\n";
    web += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
    web += "<title>RELAY 4</title>\n";
    web += "<script>function httpGet(theUrl){var xmlHttp = new XMLHttpRequest();xmlHttp.open(\"GET\",theUrl,false);xmlHttp.send(null);return xmlHttp.responseText;}</script>\n";
    web += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}\n";
    web += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h4 {color: #444444;margin-bottom: 50px;}\n";
    web += ".button {background-color: #1abc9c;border: none;color: white;padding: 4px 4px;text-decoration: none;font-size: 14px;margin: 2px;cursor: pointer;border-radius: 4px;}\n";
    web += ".button-on {background-color: #1abc9c;}\n";
    web += ".button-on:active {background-color: #16a085;}\n";
    web += ".button-off {background-color: #34495e;}\n";
    web += ".button-off:active {background-color: #2c3e50;}\n";
    web += "p {font-size: 12px;color: #888;margin-bottom: 5px;}\n";
    web += "div {width: 500px; text-align: right;}\n";
    web += "</style>\n</head>\n<body>\n";
    web += "<div><h4>ESP-RELAY-4</h4>\n";
    web += "<small>http://"+WiFi.localIP().toString()+"/relay?rX=n</small><br>\n";
    //web control
    web += "<p><div><h5>CONTROL:</h5>\n";
    web += "Relay 1 <a class=\"button button-on\" href=\"#\" onclick=\"httpGet('/relay?r1=1');return false;\">ON</a>&nbsp;<a class=\"button button-off\" href=\"#\" onclick=\"httpGet('/relay?r1=0');return false;\">OFF</a><br>\n";
    #ifdef R2_PIN
    web += "Relay 2 <a class=\"button button-on\" href=\"#\" onclick=\"httpGet('/relay?r2=1');return false;\">ON</a>&nbsp;<a class=\"button button-off\" href=\"#\" onclick=\"httpGet('/relay?r2=0');return false;\">OFF</a><br>\n";
    #endif
    #ifdef R3_PIN
    web += "Relay 3 <a class=\"button button-on\" href=\"#\" onclick=\"httpGet('/relay?r3=1');return false;\">ON</a>&nbsp;<a class=\"button button-off\" href=\"#\" onclick=\"httpGet('/relay?r3=0');return false;\">OFF</a><br>\n";
    #endif
    #ifdef R4_PIN
    web += "Relay 4 <a class=\"button button-on\" href=\"#\" onclick=\"httpGet('/relay?r4=1');return false;\">ON</a>&nbsp;<a class=\"button button-off\" href=\"#\" onclick=\"httpGet('/relay?r4=0');return false;\">OFF</a><br>\n";
    #endif
    web += "</div></p>\n";
    
    #ifdef USE_MQTT
    web += "<h5><a href=\"#\" onclick=\"if(getElementById('mqtt_div').style.display == 'none') {getElementById('mqtt_div').style.display = 'block';} else {getElementById('mqtt_div').style.display = 'none';} return false;\">mqtt settings >>></a></h5>\n<div id=\"mqtt_div\" style=\"display: none\">";
    web += "<p><form  method=\"post\">\n";
    web += "<input type=\"hidden\" name=\"config\" value=\"1\">\n";
    web += "host: <input name=\"mqtt_host\" type=\"text\" size=\"40\" maxlength=\"50\" value=\"" + String(_gconfig.mqtt_host) + "\"><br>\n";
    web += "port: <input name=\"mqtt_port\" type=\"text\" size=\"40\" maxlength=\"10\" value=\"" + String(_gconfig.mqtt_port) + "\"> <br>\n";
    web += "user: <input name=\"mqtt_user\" type=\"text\" size=\"40\" maxlength=\"20\" value=\"" + String(_gconfig.mqtt_user) + "\"> <br>\n";
    web += "password: <input name=\"mqtt_pwd\" type=\"password\" size=\"40\" maxlength=\"20\" value=\"" + String(_gconfig.mqtt_pwd) + "\"><br>\n";
    web += "<br><br>\n";
    web += "topic r1: <input name=\"topic_r1\" type=\"text\" size=\"40\" maxlength=\"20\" value=\"" + String(_gconfig.topic_r1) + "\"><br>\n";
    #ifdef R2_PIN
    web += "topic r2: <input name=\"topic_r2\" type=\"text\" size=\"40\" maxlength=\"20\" value=\"" + String(_gconfig.topic_r2) + "\"><br>\n";
    #endif
    #ifdef R3_PIN
    web += "topic r3: <input name=\"topic_r3\" type=\"text\" size=\"40\" maxlength=\"20\" value=\"" + String(_gconfig.topic_r3) + "\"><br>\n";
    #endif
    #ifdef R4_PIN
    web += "topic r4: <input name=\"topic_r4\" type=\"text\" size=\"40\" maxlength=\"20\" value=\"" + String(_gconfig.topic_r4) + "\"><br>\n";
    #endif
    web += "<br><button type=\"submit\" class=\"button button-off\">SAVE CONFIG</button>\n";
    web += "</div></form></p>\n";
    #endif //USE_MQTT
    web += "<h5><a href=\"#\" onclick=\"location = location + 'rst?pin=' + prompt('enter pin'); return false;\">reset wifi settings<a></h5>\n";
    web += "</div></body>\n</html>\n";
    server.send(200, "text/html", web);
  });

  server.on("/relay", handle_r);
  server.on("/rst", []() {
    if(server.arg("pin") == pin) { 
      #ifdef SERIAL_DEBUG
      Serial.println(server.arg(1));
      Serial.println(pin);
      #endif
      server.send(200, "text/html", "WIFI settings erased, rebooting device");
      delay(500);
      wifiManager.resetSettings();
      ESP.restart();
    } else {
      server.send(200, "text/html", "invalid pin, go back and try again");
    }
  });
  server.onNotFound(handleNotFound);
  
  server.begin();
  
  #ifdef SERIAL_DEBUG
  Serial.println("HTTP started");
  #endif
}

void loop() {
  #ifdef USE_MQTT
  if(mqtt_connect_timer + 15000 < millis())
      if (!client.connected()) mqtt_reconnect();

  if (client.connected()) client.loop();
  #endif
  
  server.handleClient();
}
