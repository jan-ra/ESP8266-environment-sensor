#include <Arduino.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include "./credentials.h"

String serverName = "https://timeapi.io/api/Time/current/zone?timeZone=";
unsigned long timerDelay = 5000;
unsigned long lastTime = 0;

unsigned long setTime = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

String getTime();
void log(String text);
void writeToSD(String filePath, String text);

void setup()
{
  Serial.begin(9600);
  delay(10);
  StaticJsonDocument<500> doc;
  Serial.println('\n');
  Serial.println("Initializing SD cards");

  const int chipSelect = 15; // Select pin D8
  if (!SD.begin(chipSelect))
  {
    Serial.println("Error while initializing SD Card");
    return;
  }

  log("SD card initalized");
  log("Connecting to Wifi");

  WiFi.begin(ssid, password);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
  }

  log("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  log("Pulling current time");
  if (WiFi.status() == WL_CONNECTED)
  {

    HTTPClient http;
    String serverPath = serverName + "Europe/Amsterdam";
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    http.begin(*client, serverPath.c_str());
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);

      DeserializationError error = deserializeJson(doc, payload);

      // Test if parsing succeeds.
      if (error)
      {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }

      hours = doc["hour"];
      minutes = doc["minute"];
      seconds = doc["seconds"];
      setTime = millis();
      log("Time set: " + getTime());
    }
    else
    {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void loop()
{
  log(getTime());
  delay(2000);
}

String getTime()
{
  unsigned long timePassed = millis() - setTime;

  unsigned long secondsadded = timePassed / 1000 + seconds;
  unsigned long minutesadded = secondsadded / 60 + minutes;
  unsigned long hoursadded = minutesadded / 60 + hours;
  // unsigned long daysadded = hoursadded / 24;
  timePassed %= 1000;
  secondsadded %= 60;
  minutesadded %= 60;
  hoursadded %= 24;

  String out = String(hoursadded) + ":" + String(minutesadded) + ":" + String(secondsadded);
  return out;
}

void log(String text)
{
  const String logText = getTime() + " LOG: " + text;
  Serial.println(logText);
  writeToSD("LOG.txt", logText);
}

void writeToSD(String filePath, String text)
{
  File dataFile = SD.open(filePath, FILE_WRITE);
  if (dataFile)
  {
    dataFile.println(text);
    dataFile.close();
  }
  else
  {
    Serial.println("Error while opening SD Card file");
  }
}
