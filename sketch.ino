#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "config.h" //설정 파일

#ifndef SENSOR_NAME
#define SENSOR_NAME "센서 이름 설정"
#endif
#define ATTR_NUM 3

/*************** ESP8266 HTTP Client ***********************/
#include <ESP8266HTTPClient.h>
#ifndef SERVER_IP
#define SERVER_IP "프로토콜://서버 주소"  // test server
#endif


/*************** Temp&Hum Sensor am2320 *********************/
#include <Wire.h>
#include <AM2320.h>
// AM2320 pinout: 1-VCC, 2-SDA, 3-GND, 4-SCL.
// punched side is upside, pin1 from left pin
// nodemcu_v1~ Wire default pin: SDA-D2(GPIO4), SCL-D1(GPIO5)
AM2320 am2320(&Wire);

/*************** WebSerial ***************************/
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

AsyncWebServer server(55050);
void callback(unsigned char* data, unsigned int length){
  data[length] = '\0';
  WebSerial.println((char*) data);
}


/************** WIFI ssid&password *******************/
#ifndef STASSID
#define STASSID "ssid" // ssid of your WIFI AP
#define STAPSK  "password" // password of your WIFI AP
#endif
const char* ssid = STASSID;
const char* password = STAPSK;

/*********** flash for debug ************************/
void led_flash(int pin, int times, int length){
  for(int i=0; i<times; i++){
    digitalWrite(pin, HIGH);
    delay(length/2);
    digitalWrite(pin, LOW);
    delay(length/2);    
  }
}

/************************ millis() timer setting ******************/
unsigned long startTime;
unsigned long interval = 10000; // interval in millisecond

void setup() {
  
  Serial.begin(115200);
  Serial.println("Booting");

  /************************** Wifi Setup ***************************/
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();  // restart if wifi is not connected
  }

  /*************************** Arduino OTA *************************/

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /***************************** WebSerial Setup ***************************/
  WebSerial.begin(&server);
  WebSerial.msgCallback(callback);
  server.begin();  

  /***************************** AM2320 Setup **********************/
  Wire.begin();

  /***************************** User setup code ***************************/
  pinMode(BUILTIN_LED, OUTPUT);
  led_flash(BUILTIN_LED, 5, 200);
  startTime = millis(); // set initial startTime for millis timer
}

void loop() {
  /************ ArduinoOTA handler ************/
  ArduinoOTA.handle();

  /************ User Loop Code ****************/
  if(millis() - startTime >= interval){
    startTime = millis(); // reset timer;
    /* temp&humid read */
    const int l = ATTR_NUM;
    char* attr_name_buffer[l]={ "name", "temp", "humid" };
    char attr_data_buffer[l][20]={ SENSOR_NAME, "", ""};
    
    if(am2320.Read()==0){
      dtostrf(am2320.cTemp, 5, 2, attr_data_buffer[1]);
      dtostrf(am2320.Humidity, 5, 2, attr_data_buffer[2]);
    } else {
      WebSerial.println(am2320.Read());
      strcpy(attr_data_buffer[1], "crc error");
      strcpy(attr_data_buffer[2], "crc error");
    }
  
    /* http post loop */
    WiFiClient client;
    HTTPClient http;
    http.begin(client, "http://" SERVER_IP "/temphumid-write");  // http client module begin
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // http post header
    String httpRequestData = "";
    for(int i=0;i<l;i++){
      if(i!=0){
        httpRequestData.concat("&");
      }
      httpRequestData.concat(attr_name_buffer[i]);
      httpRequestData.concat("=");
      httpRequestData.concat(attr_data_buffer[i]);
    }   
    int httpCode = http.POST(httpRequestData);
  
    /* webserial print for debug */
    WebSerial.print("Http Response Code: ");
    WebSerial.println(httpCode);
    WebSerial.println(httpRequestData);
  }
  
  /*
  WebSerial.print("Temp: "); 
  dtostrf(temperature, 5, 2, print_buffer); WebSerial.println(print_buffer);
  WebSerial.print("Hum:  ");
  dtostrf(humidity, 5, 2, print_buffer); WebSerial.println(print_buffer);
  */
  
}
