/*
  Andreas Wirooks

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <Arduino_JSON.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"
#include <time.h>
#include <PubSubClient.h>

//define Screen width heigth
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// OLED connect
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT Pin assignment
#define DHTPIN 14
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// Wifi Connection
const char* ssid = "Vodafone-F493";
const char* password = "GnpPRCGbcELBhGR3";

// MQTT Broker Server
#define mqtt_server "192.168.0.200"
#define mqtt_user "Andario"
#define mqtt_password "Cindy1986"

//MQTT Switch
const char* ID = "Schlafzimmer_Dim";
const char* topic = "Schlafzimmer";
const char* state_topic = "Schlafzimmer/state";
const char* command = "Schlafzimmer/command";
String dim;

#define humidity "Schlafzimmer/humidity"
#define temperature "Schlafzimmer/temperature"

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received messages;");
  Serial.println(topic);
  String topicStr = command;
  Serial.println("Callback update");
  Serial.print(topic);
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
  }
  payload[length] = '\0';
  //strTopic = String((char*)topic;
  dim = String((char*)payload);
  Serial.println();
  if (dim == "ON") {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    display.display();
  }
  else if (dim == "OFF") {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.display();
  }
}

// Wifi Client Setup
ESP8266WiFiMulti WiFiMulti;
WiFiClient espclient;
PubSubClient mqttClient(mqtt_server, 1883, callback, espclient);

//long currentTime, lastTime;
char messages[50];
#define MSG_BUFFER_SIZE (100)
char msg[MSG_BUFFER_SIZE];
int value = 0;
int count = 0;




// URL from Open Weather MAP
const String url = "http://api.openweathermap.org/data/2.5/weather?q=Koeln,de&appid=54a15c2b5c31e84d3f8ffe733c3a6cc4&units=metric";

const char* serverName = "http://api.openweathermap.org/data/2.5/weather?q=Koeln,de&appid=54a15c2b5c31e84d3f8ffe733c3a6cc4&units=metric";
// Your Domain name with URL path or IP address with path
String openWeatherMapApiKey = "54a15c2b5c31e84d3f8ffe733c3a6cc4";

// country code and city
String city = "Koeln";
String countryCode = "de";

// declarations
char buff[1024];
char tmp[200];
char time_value[20];

// Time
struct tm tm;
const uint32_t SYNC_INTERVAL = 24;
const char* const PROGMEM NTP_SERVER[] = {"fritz.box", "de.pool.ntp.org", "at.pool.ntp.org", "ch.pool.ntp.org", "ptbtime1.ptb.de", "europe.pool.ntp.org"};
extern "C" uint8_t sntp_getreachability(uint8_t);

bool getNtpServer(bool reply = false) {
  uint32_t timeout {millis()};
  configTime("CET-1CEST,M3.5.0/02,M10.5.0/03", NTP_SERVER[0], NTP_SERVER[1], NTP_SERVER[2]);   // Zeitzone einstellen https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  do {
    delay(25);
    if (millis() - timeout >= 1e3) {
      Serial.printf("Warten auf NTP-Antwort %02ld sec\n", (millis() - timeout) / 1000);
      delay(975);
    }
    sntp_getreachability(0) ? reply = true : sntp_getreachability(1) ? reply = true : sntp_getreachability(2) ? reply = true : false;
  } while (millis() - timeout <= 16e3 && !reply);
  return reply;
}


// For a final application, check the API call limits per hour/minute to avoid getting blocked/banned
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 10 seconds (10000)
unsigned long timerDelay = 10000;

// jsonBuffer declaration
String jsonBuffer;

/*
      S
      E
      T
      U
      P
*/


// Setup for WiFi Connectiion, Display
void setup() {
  Serial.begin(115200);
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.display();
  // Draw a single pixel in white
  display.drawPixel(10, 10, SSD1306_WHITE);

  dht.begin();
  Serial.println("Device Started");
  Serial.println("------------------");
  Serial.println("Running DHT!");
  Serial.println("------------------");
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // Begin WiFi Connection
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  // Time config UTC
  configTime(2 * 3600, 0, "0.de.pool.ntp.org", "ptbtime1.ptb.de");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  bool timeSync = getNtpServer();
  Serial.printf("NTP Synchronisation %s!\n", timeSync ? "erfolgreich" : "fehlgeschlagen");
  Serial.println("Timer set to 10 seconds (timerDelay variable), it will take 10 seconds before publishing the first reading.");
}

// Check temp Diff
bool checkBound(float newValue, float prevValue, float maxDiff) {
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}
// Set temp, hum from DHT22 to 0
long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 0.2;

/*
//  httpGETReqest Function
String httpGETRequest(const char* serverName) {
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(espclient, url);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String weather = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    weather = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  weather.end();

  return weather;
}
*/

/*
      L
      O
      O
      P
*/
void Tempread() {
  int h = dht.readHumidity();
  float t = dht.readTemperature();

  float temp_1 = dht.readTemperature();
  float hum_1 = dht.readHumidity();

  // publish DHT informations to mqtt Broker & checkbound
  if (checkBound(temp_1, temp, diff)) {
    temp = temp_1;
    Serial.print("New temperature:");
    Serial.println(String(temp).c_str());
    mqttClient.publish(temperature, String(temp).c_str(), true);
  }
  if (checkBound(hum_1, hum, diff)) {
    hum = hum_1;
    Serial.print("New humidity:");
    Serial.println(String(hum).c_str());
    mqttClient.publish(humidity, String(hum).c_str(), true);
  }

}

// Program loop
void loop() {
  if (!mqttClient.connected()) {
    reconnect();
    mqttClient.connect(ID, mqtt_user, mqtt_password);
    mqttClient.publish(state_topic, "alive");
    mqttClient.subscribe(topic);
  }
  else {
    Serial.print(" NOOOOOTT CONNNNNNECTEDDD");
  }
  mqttClient.loop();

  /*
         W
         E
         A
         T
         H
         E
         R
  */

  String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + openWeatherMapApiKey;
/*
  //jsonBuffer
  jsonBuffer = httpGETRequest(serverPath.c_str());
  Serial.println(jsonBuffer);
  JSONVar myObject = JSON.parse(jsonBuffer);

  // JSON.typeof(jsonVar) can be used to get the type of the var
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }
  */
  /*
      D
      H
      T
  */
  Tempread();
  mqttClient.loop();
  // Read DHT TEMP and HUM
  /*
        T
        I
        M
        E
  */

  static time_t lastsec {0};
  time_t now = time(&now);
  localtime_r(&now, &tm);
  if (tm.tm_sec != lastsec) {
    lastsec = tm.tm_sec;
    strftime (buff, sizeof(buff), "%d.%m.%Y %H %M", &tm);             // http://www.cplusplus.com/reference/ctime/strftime/
    if (!(time(&now) % (SYNC_INTERVAL * 3600))) {
      getNtpServer(true);
    }
  }
  Serial.println("Stunde: "); Serial.println(tm.tm_hour);          // aktuelle Stunde
  Serial.println("Minute: "); Serial.println(tm.tm_min);

  /*
        S
        E
        R
        I
        A
        L
  */
  mqttClient.loop();
  //Serial print DHT
  int h = dht.readHumidity();
  float t = dht.readTemperature();

  Serial.print("Feuchtigkeit DHT: ");
  Serial.println(h);
  Serial.print("Temperature DHT: ");
  Serial.println(t);

  // Print Temp from Open Weather to Serial
  Serial.print("JSON object = ");
  //  Serial.println(myObject);
  Serial.print("Temperature: ");
  // Serial.println(myObject["main"]["temp"]);
  Serial.print("Feuchtigkeit: ");
  // Serial.println(myObject["main"]["humidity"]);

  /*
         P
         R
         I
         N
         T

         O
         L
         E
         D
  */
  // Print to OLED Display
  display.clearDisplay();
  display.dim(true);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.drawRect(1, 1, 85, 28, WHITE);
  display.setCursor(3, 4);
  display.print("Aussen");
  display.setCursor(3, 12);
  display.print("Temp: ");
  display.setCursor(53, 12);
  //display.print(myObject["main"]["temp"]);
  display.setCursor(3, 20);
  display.print("Feucht.:");
  display.setCursor(53, 20);
  //display.print(myObject["main"]["humidity"]);
  display.setCursor(3, 37);
  display.drawRect(1, 35, 85, 28, WHITE);
  display.print("Innen");
  display.setCursor(3, 45);
  display.print("Temp: ");
  display.setCursor(53, 45);
  display.print(t);
  display.setCursor(3, 53);
  display.print("Feucht.:");
  display.setCursor(53, 53);
  display.print(h);
  display.drawCircle(108, 32, 18, WHITE);
  display.setCursor(100, 23);
  display.print("Uhr");
  display.setCursor(94, 34);
  display.print(tm.tm_hour);
  display.setCursor(106, 34);
  display.print(":");
  display.setCursor(112, 34);
  display.print(tm.tm_min);

  display.display();
  delay(2000);
  mqttClient.loop();
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(ID, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      Serial.println(topic);
      mqttClient.publish(temperature, String(temp).c_str(), true);
      mqttClient.publish(humidity, String(hum).c_str(), true);
      mqttClient.subscribe(topic);
      Serial.print(mqttClient.state());
      Serial.print("Received messages;");
      Serial.println(topic);
      Serial.println("Callback update");
      Serial.print(topic);
      mqttClient.publish(command, "Alive");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
