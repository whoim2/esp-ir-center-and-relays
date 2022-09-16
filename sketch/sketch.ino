#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>
#include <FS.h>
#include <LittleFS.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <IRac.h>
#include <IRtext.h>


WiFiManager wifiManager;
ESP8266WebServer server(80);

String pin = "1234"; //for reset
#define CMD_SIZE 24 //command slots size
#define IR_RECV_PIN 0 //pin for IR Reciever
#define IR_LED_PIN 4   //pin for IR Transmitter
#define DEBUG

IRrecv irrecv((uint16_t)IR_RECV_PIN);
IRsend irsend(IR_LED_PIN);
decode_results ir_ret;

//====================STORE struct and func
struct ir_data { //for store IR full packet
  uint64_t value;
  decode_type_t decode_type;
  uint8_t bits;
};
ir_data ir_inputs[CMD_SIZE], ir_last;
uint8_t ir_trigger_ids[CMD_SIZE];

typedef struct data { //struct for store one command to spiffs file
  char    _name[10];
  uint8_t _mode;
  ir_data _input_ir;
  ir_data _action_ir1;
  char    _action_url1[50];
  ir_data _action_ir2;
  char    _action_url2[50];
  uint16_t _actions_delay;
} data;
data _data_buff;

bool WriteLFS(const char * path, data * inStruct)  {
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  file.write((byte*)inStruct, sizeof(data) / sizeof(byte));
  file.close();
  return true;
}
bool ReadLFS(const char * path, data * outStruct)  {
  File file = LittleFS.open(path, "r");
  if (!file) return 0;
  while (file.available()) {
    byte tmp = file.read();
    memcpy((byte*)outStruct + file.position()-1, &tmp, sizeof(byte));
  }
  file.close();
  return true;
}

//====================get ir data from string as 0x88334455,NEC,32 and save to string
bool arg_from_string(String &from, String &to, uint8_t index, char separator) {
  uint16_t start = 0, idx = 0;
  uint8_t cur = 0;
  while (idx < from.length()) {
    if (from.charAt(idx) == separator) {
      if (cur == index) {
        to = from.substring(start, idx);
        return true;
      }
      cur++;
      while ((idx < from.length() - 1) && (from.charAt(idx + 1) == separator)) idx++;
      start = idx + 1;
    }
    idx++;
  }
  if ((cur == index) && (start < from.length())) {
    to = from.substring(start, from.length());
    return true;
  }
  return false;
}
ir_data ir_from_str(String str) {
  String result;
  char buff[20];
  ir_data data;
  uint8_t idx = 0;
  if(str.length() == 0) { //empty string
    data = {0, UNUSED, 0};
  } else { //not empty
      while (arg_from_string(str, result, idx, ',')) {
        #ifdef DEBUG
        Serial.print(idx);
        Serial.print("=");
        Serial.println(result);
        #endif
        if(idx == 0) {
            result.toCharArray(buff, 20);
            data.value = getUInt64fromHex(buff);        
        }
        if(idx == 1) {
          result.toCharArray(buff, 20);
          data.decode_type = strToDecodeType(buff);
        }
        if(idx == 2) data.bits = result.toInt();
        idx++;
        if(idx > 2) break;
      }
  } //not empty
  return data;
}

String ir_to_str(ir_data data) {//save ir full packet to string
  String str = String(data.value, HEX);
  str += ",";
  str += typeToString(data.decode_type, false);
  //(decode_type_t)atoi(str)).c_str()
  str += ",";
  str += String(data.bits, DEC);
  return str;
}

void load_ir_inputs() { //load all config ir-codes to array for check at events
  //load inputs ir-codes for events from spiffs
  for (uint8_t i = 0; i < CMD_SIZE; i++) {
    if (ReadLFS(config_filename(i), &_data_buff)) {
      if(_data_buff._input_ir.value != 0 && _data_buff._input_ir.decode_type != UNKNOWN) {
        ir_inputs[i] = _data_buff._input_ir;
        #ifdef DEBUG
        Serial.print("add to inputs: i");
        Serial.print(i, DEC);
        Serial.print(" v:");
        Serial.println(ir_inputs[i].value, HEX);
        #endif
      }
      else {
        ir_inputs[i] = {0, UNKNOWN, 0}; //fill nulls
      }
    }
  }
}

//====================web handlers
void handle_root() {
  String msg;
  irrecv.disableIRIn();
  //if execute cmd
  if(server.arg("cmd") != "") {
    if(server.arg(0).toInt() >= 0 && server.arg(0).toInt() < CMD_SIZE) {
      #ifdef DEBUG
      Serial.print("http event for cmd: ");
      Serial.println(server.arg(0));
      #endif
      command_execute(server.arg(0).toInt());
      server.send(200, "text/plain", "{\"status\":\"ok\",\"text\":\"Готово\",\"value\":\""+server.arg(0)+"\"}");
      irrecv.enableIRIn();
      return;
    }
  }
  //if save
  if(server.arg("id") != "") {
    #ifdef DEBUG
    Serial.print("save data for id: ");
    Serial.println(server.arg("id"));
    #endif
    //stored data to buff
    server.arg("n").toCharArray(_data_buff._name, 10);
    _data_buff._mode = server.arg("m").toInt();
    _data_buff._input_ir = ir_from_str(server.arg("e1")); 
    _data_buff._action_ir1 = ir_from_str(server.arg("a1"));
    _data_buff._action_ir2 = ir_from_str(server.arg("a2"));
    server.arg("u1").toCharArray(_data_buff._action_url1, 50);
    server.arg("u2").toCharArray(_data_buff._action_url2, 50);
    _data_buff._actions_delay = server.arg("d").toInt();

    if (WriteLFS(config_filename(server.arg("id").toInt()), &_data_buff)) {
      #ifdef DEBUG
      Serial.println("WriteLFS Ok");
      #endif
      load_ir_inputs(); //reload inputs to array
      led_blink();
    }
    msg = "<font color=red><b>Settings for command " + server.arg("id") + " saved</b></font>";
  }
  String web = "<!DOCTYPE html> <html>\n";
  web += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  web += "<title>IR Control</title>\n";
  web += "<script>function httpGet(theUrl){var xmlHttp = new XMLHttpRequest();xmlHttp.open(\"GET\",theUrl,false);xmlHttp.send(null);return xmlHttp.responseText;}</script>";
  web += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}\n";
  web += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h4 {color: #444444;margin-bottom: 50px;}\n";
  web += ".button {background-color: #1abc9c;border: none;color: white;padding: 4px 4px;text-decoration: none;font-size: 10px;margin: 2px;cursor: pointer;border-radius: 4px;}\n";
  web += ".button-on {background-color: #1abc9c;}\n";
  web += ".button-on:active {background-color: #16a085;}\n";
  web += ".button-off {background-color: #34495e;}\n";
  web += ".button-off:active {background-color: #2c3e50;}\n";
  web += "p {font-size: 12px;color: #888;margin-bottom: 5px;}\n";
  web += "div {width: 500px; text-align: right;}\n";
  web += "</style>\n</head>\n<body>\n";
  web += "<h4>ESP-IR / whoim@mail.ru<br>see instructions for use<br><br><a href=\"#\" onclick=\"location = location + 'rst?pin=' + prompt('enter pin'); return false;\">reset wifi settings<a></h4>\n";
  web += msg + "\n";
  
  for (uint8_t i = 0; i < CMD_SIZE; i++) {
    if (ReadLFS(config_filename(i), &_data_buff)) {
    } else {
      _data_buff = (const struct data){ 0 };
    }
    web += "<form  method=\"post\">\n";
    web += "<p><div><hr><b>CMD&nbsp;" + String(i) + "&nbsp;</b><input name=\"n\" type=\"text\" size=\"10\" maxlength=\"10\" value=\"" + _data_buff._name + "\"> name&nbsp;<br><small>http://"+WiFi.localIP().toString().c_str()+"?cmd=" + String(i) + "</small><br><br>\n";
    web += "<input type=\"hidden\" name=\"id\" value=\"" + String(i) + "\">\n";
    web += "mode: <select name=\"m\" id=\"" + String(i) + "_m\"><option" + (_data_buff._mode == 0 ? " selected" : "") + " value=\"0\">Linear</option><option" + (_data_buff._mode == 1 ? " selected" : "") + " value=\"1\">Trigger</option></select><br>\n";
    web += "event IR code:  <a class=\"button button-off\" href=\"#\" onclick=\"document.getElementById('"+String(i)+"_e1').value=httpGet('/lastir');return false;\">READ</a>&nbsp;<input id=\"" + String(i) + "_e1\" name=\"e1\" type=\"text\" size=\"40\" value=\"" + ir_to_str(_data_buff._input_ir) + "\"><br>\n";
    web += "action1 IR code: <a class=\"button button-off\" href=\"#\" onclick=\"document.getElementById('"+String(i)+"_a1').value=httpGet('/lastir');return false;\">READ</a>&nbsp;<input id=\"" + String(i) + "_a1\" name=\"a1\" type=\"text\" size=\"40\" value=\"" + ir_to_str(_data_buff._action_ir1) + "\"><br>\n";
    web += "action1 URL get: <input name=\"u1\" type=\"text\" size=\"40\" maxlength=\"50\" value=\"" + String(_data_buff._action_url1) + "\"><br>\n";
    web += "actions delay: <input name=\"d\" type=\"text\" size=\"40\" value=\"" + String(_data_buff._actions_delay) + "\"><br>\n";
    web += "action2 IR code: <a class=\"button button-off\" href=\"#\" onclick=\"document.getElementById('"+String(i)+"_a2').value=httpGet('/lastir');return false;\">READ</a>&nbsp;<input id=\"" + String(i) + "_a2\" name=\"a2\" type=\"text\" size=\"40\" value=\"" + ir_to_str(_data_buff._action_ir2) + "\"><br>\n";
    web += "action2 URL get: <input name=\"u2\" type=\"text\" size=\"40\" maxlength=\"50\" value=\"" + String(_data_buff._action_url2) + "\"><br>\n";
    web += "<br><button type=\"submit\" class=\"button button-off\">SAVE</button>\n";
  web += "</div></p></form>\n";
  }
  web += "</body>\n</html>\n";
  
  server.send(200, "text/html", web);
  irrecv.enableIRIn();
}

void handleNotFound() {
  irrecv.disableIRIn();
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
  irrecv.enableIRIn();
}

void handle_lastir() {
  irrecv.disableIRIn();
  server.send(200, "text/plain", ir_to_str(ir_last));
  led_blink();
  irrecv.enableIRIn();
}

void handle_rst() {
  irrecv.disableIRIn();
  if(server.arg("pin") == pin) { 
    #ifdef DEBUG
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
  irrecv.enableIRIn();
}

//=====================wifi client
void get_url(String url) {
  irrecv.disableIRIn();
  #ifdef DEBUG 
  Serial.print("http request to: ");
  Serial.println(url);
  #endif
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url.c_str());
  int httpResponseCode = http.GET();
  if (httpResponseCode>0) {
    #ifdef DEBUG
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
    #endif
  } else {
    #ifdef DEBUG
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    #endif
  }
  http.end();
  irrecv.enableIRIn();
}

//====================other func
char * config_filename(uint8_t i) {
  static char filename[10];
  String filename_s = "/c_";
  filename_s += i;
  filename_s += ".cfg";
  filename_s.toCharArray(filename, 10);
  return filename;
}

uint64_t getUInt64fromHex(char const *str) {
    uint64_t accumulator = 0;
    for (size_t i = 0 ; isxdigit((unsigned char)str[i]) ; ++i) {
        char c = str[i];
        accumulator *= 16;
        if (isdigit(c)) /* '0' .. '9'*/
            accumulator += c - '0';
        else if (isupper(c)) /* 'A' .. 'F'*/
            accumulator += c - 'A' + 10;
        else /* 'a' .. 'f'*/
            accumulator += c - 'a' + 10;
    }
    return accumulator;
}

void led_blink(){
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
}

void command_execute(uint8_t i) {
  if(ReadLFS(config_filename(i), &_data_buff)) {
    #ifdef DEBUG
    Serial.println("config readed");
    #endif          
    if(_data_buff._mode == 0) {//linear mode
        #ifdef DEBUG
        Serial.println("action1 started");
        #endif
        if(_data_buff._action_ir1.decode_type > 0 && _data_buff._action_ir1.value != 0) irsend.send(_data_buff._action_ir1.decode_type, _data_buff._action_ir1.value, _data_buff._action_ir1.bits);
        if(strlen(_data_buff._action_url1) != 0) get_url(_data_buff._action_url1);
        delay(_data_buff._actions_delay);
        #ifdef DEBUG
        Serial.println("action2 started");
        #endif
        if(_data_buff._action_ir2.decode_type > 0 && _data_buff._action_ir2.value != 0) irsend.send(_data_buff._action_ir2.decode_type, _data_buff._action_ir2.value, _data_buff._action_ir2.bits);
        if(strlen(_data_buff._action_url2) != 0) get_url(_data_buff._action_url2);
    }//linear mode
    
    if(_data_buff._mode == 1) {//trigger mode
      if(ir_trigger_ids[i] == 0) {
        #ifdef DEBUG
        Serial.println("trigger state 1 started");
        #endif
        if(_data_buff._action_ir1.decode_type > 0 && _data_buff._action_ir1.value != 0) irsend.send(_data_buff._action_ir1.decode_type, _data_buff._action_ir1.value, _data_buff._action_ir1.bits);
        if(strlen(_data_buff._action_url1) != 0) get_url(_data_buff._action_url1);
        ir_trigger_ids[i] = 1;
      } else {
        #ifdef DEBUG
        Serial.println("trigger state 2 started");
        #endif
        if(_data_buff._action_ir2.decode_type > 0 && _data_buff._action_ir2.value != 0) irsend.send(_data_buff._action_ir2.decode_type, _data_buff._action_ir2.value, _data_buff._action_ir2.bits);
        if(strlen(_data_buff._action_url2) != 0) get_url(_data_buff._action_url2);
        ir_trigger_ids[i] = 0;
      }
    }//trigger mode
  }//ReadLFS
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  #ifdef DEBUG
  Serial.println("Booting...");
  #endif
  //ir
  irrecv.enableIRIn();
  delay(300);
  irsend.begin();
  delay(300);
  //fs
  if(!LittleFS.begin()){
   #ifdef DEBUG
   Serial.println("An Error has occurred while mounting LittleFS");
   #endif
   LittleFS.format();
   ESP.restart();
  }
  load_ir_inputs();
  //pins
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
  //wifi
  wifiManager.autoConnect("ESP-IR");
  #ifdef DEBUG
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  #endif
  digitalWrite(LED_BUILTIN, LOW);
  //http
  server.on("/", handle_root);
  server.on("/lastir", handle_lastir);
  server.on("/rst", handle_rst);
  server.onNotFound(handleNotFound);
  server.begin();
  #ifdef DEBUG
  Serial.println("HTTP started");
  #endif
}

void loop() {
  if (irrecv.decode(&ir_ret)) {
    
    serialPrintUint64(ir_ret.value, HEX);
    #ifdef DEBUG
    Serial.print(",");
    Serial.print(typeToString(ir_ret.decode_type));
    Serial.print(",");
    Serial.println(ir_ret.bits, DEC);
    #endif
    
    //store to global for use in web page
    if(ir_ret.decode_type != UNKNOWN) {
      ir_last.value = ir_ret.value;
      ir_last.decode_type = ir_ret.decode_type;
      ir_last.bits = ir_ret.bits;
    }
    
    //check in events
    for (uint8_t i = 0; i < CMD_SIZE; i++) {
      if(ir_ret.value == ir_inputs[i].value) {
        #ifdef DEBUG
        Serial.print("Detect command from IR: ");
        Serial.println(i, DEC);
        #endif
        delay(500);
        command_execute(i);
      }//ir_ret.value == ir_inputs
    }//for
    
    led_blink();
    irrecv.resume();
  }
  server.handleClient();
}
