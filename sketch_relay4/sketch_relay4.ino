///http://HOME.ddns.net:5008/relay?r1={value}

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>       //https://github.com/knolleary/pubsubclient

WiFiManager wifiManager;
ESP8266WebServer server(80);

//#define SERIAL_DEBUG

//reset pin
String pin = "1234";

#define R1_PIN 16
#define R2_PIN 4
#define R3_PIN 3
#define R4_PIN 14

#define USE_HTTP //comment for disable HTTP functional
#define USE_MQTT //comment for disable MQTT functional
// MQTT
#ifdef USE_MQTT
const char* mqtt_server = "xx.wqtt.ru"; //wqtt.ru broker 300rub / YEAR
const int mqtt_port = 1111;
const char* mqtt_user = "xxxx";
const char* mqtt_password = "xxxx";
const String  R1_TOPIC = "r1";
const String  R2_TOPIC = "r2";
const String  R3_TOPIC = "r3";
const String  R4_TOPIC = "r4";
WiFiClient espClient;
PubSubClient client(espClient);
#endif

#ifdef USE_MQTT
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    
  String data_pay;
  for (int i = 0; i < length; i++) {
    data_pay += String((char)payload[i]);
  }
  #ifdef SERIAL_DEBUG
  Serial.println(data_pay);
  #endif
  
  uint8_t relay = 0;
  #ifdef R1_PIN
  if(String(topic) == R1_TOPIC) relay = R1_PIN;
  #endif
  #ifdef R2_PIN
  if(String(topic) == R2_TOPIC) relay = R2_PIN;
  #endif
  #ifdef R3_PIN
  if(String(topic) == R3_TOPIC) relay = R3_PIN;
  #endif
  #ifdef R4_PIN
  if(String(topic) == R4_TOPIC) relay = R4_PIN;
  #endif

  if( relay != 0 ) digitalWrite(relay, data_pay.toInt());

  //led blink
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
}
#endif

#ifdef USE_HTTP
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
  #ifdef R1_PIN
  if(server.argName(0) == "r1") relay = R1_PIN;
  #endif
  #ifdef R2_PIN
  if(server.argName(0) == "r2") relay = R2_PIN;
  #endif
  #ifdef R3_PIN
  if(server.argName(0) == "r3") relay = R3_PIN;
  #endif
  #ifdef R4_PIN
  if(server.argName(0) == "r4") relay = R4_PIN;
  #endif
  
  //set state
  if(server.arg(0) != "{value}") //not status query from Kyzia
    if( relay != 0 ) digitalWrite(relay, server.arg(0).toInt());

  //return status
  server.send(200, "text/plain", "{\"status\":\"ok\",\"text\":\"Готово\",\"value\":\""+String(digitalRead(relay))+"\"}");

  //led blink
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
}
#endif

#ifdef USE_MQTT
void mqtt_reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESPRELAY4-" + WiFi.macAddress();
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password) ) {
      #ifdef SERIAL_DEBUG
      Serial.println("connected");      
      #endif
      #ifdef R1_PIN
      client.subscribe( (R1_TOPIC + "/#").c_str() );
      #endif
      #ifdef R2_PIN
      client.subscribe( (R2_TOPIC + "/#").c_str() );
      #endif
      #ifdef R3_PIN
      client.subscribe( (R3_TOPIC + "/#").c_str() );
      #endif
      #ifdef R4_PIN
      client.subscribe( (R4_TOPIC + "/#").c_str() );
      #endif
    } else {
      #ifdef SERIAL_DEBUG
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      #endif
      delay(5000);
    }
  }
}
#endif

void setup() {
  #ifdef SERIAL_DEBUG
  Serial.begin(115200);
  Serial.println("Booting");
  #endif
  //pinmode

  #ifdef R1_PIN
  pinMode(R1_PIN, OUTPUT);
  digitalWrite(R1_PIN, LOW);
  #endif
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
  digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  digitalWrite(LED_BUILTIN, HIGH);


  wifiManager.autoConnect("ESPRELAY-4");
  #ifdef SERIAL_DEBUG
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  #endif
  digitalWrite(LED_BUILTIN, LOW);

  #ifdef USE_HTTP
  server.on("/", []() {
    server.send(200, "text/html", "ESP RELAY 4<br>see instructions for use at https://github.com/whoim2/esp-ir-center-and-relays<br><br><a href=\"#\" onclick=\"location = location + 'rst?pin=' + prompt('enter pin'); return false;\">reset wifi settings<a>");
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
  #endif
  


  //mqtt
  #ifdef USE_MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);
  #endif
}

void loop() {
  #ifdef USE_MQTT
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();
  #endif
  
  #ifdef USE_HTTP
  server.handleClient();
  #endif
}
