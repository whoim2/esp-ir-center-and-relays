///http://HOME.ddns.net:5008/relay?r1={value}

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

WiFiManager wifiManager;
ESP8266WebServer server(80);

String pin = "3363";

#define R1_PIN 16
#define R2_PIN 4
#define R3_PIN 3
#define R4_PIN 14

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
  uint8_t relay = 0;
  if(server.argName(0) == "r1") relay = R1_PIN;
  if(server.argName(0) == "r2") relay = R2_PIN;
  if(server.argName(0) == "r3") relay = R3_PIN;
  if(server.argName(0) == "r4") relay = R4_PIN;
  
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

void setup() {
  //pinmode
  //Serial.begin(115200);
  //Serial.println("Booting");

  pinMode(R1_PIN, OUTPUT);
  digitalWrite(R1_PIN, LOW);
  pinMode(R2_PIN, OUTPUT);
  digitalWrite(R2_PIN, LOW);
  pinMode(R3_PIN, OUTPUT);
  digitalWrite(R3_PIN, LOW);
  pinMode(R4_PIN, OUTPUT);
  digitalWrite(R4_PIN, LOW);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  digitalWrite(LED_BUILTIN, HIGH);


  wifiManager.autoConnect("ESPRELAY-4");
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, LOW);

  server.on("/", []() {
    server.send(200, "text/html", "ESP RELAY 4 / whoim@mail.ru<br>see instructions for use<br><br><a href=\"#\" onclick=\"location = location + 'rst?pin=' + prompt('enter pin'); return false;\">reset wifi settings<a>");
  });

  server.on("/relay", handle_r);
  server.on("/rst", []() {
    if(server.arg("pin") == pin) { 
      //Serial.println(server.arg(1));
      //Serial.println(pin);
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
  //Serial.println("HTTP started");
}

void loop() {
  server.handleClient();
}
