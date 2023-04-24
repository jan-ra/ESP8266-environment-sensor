#include <Arduino.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>
#include "./credentials.h"
#include "./counter_functions.h"
#include <set>
#include <string>

// CONSTANTS
#define disable 0
#define enable 1
#define PURGETIME 300000
#define SAMPLERATE 60000
#define LED D0
#define AUDIO_SENSOR_PIN A0

// VARIABLES
String timeServer = "https://timeapi.io/api/Time/current/zone?timeZone=Europe/Amsterdam";
unsigned long lastTime = 0;
unsigned long setTime = 0;
unsigned int hours = 0;
unsigned int minutes = 0;
unsigned int seconds = 0;
unsigned int channel = 1;
unsigned int ledstatus = true;
unsigned int lastLEDon = true;
const int sampleWindow = 50;
unsigned int sample;
int db;
int clients_known_count_old;
BH1750 lightMeter;

// FUNCTIONS
String getTime();
String outputDevices();
void log(String text);
void writeToSD(String filePath, String text);
void showDevices();
void writeData(String text);
void purgeDevice();

void setup()
{
  Serial.begin(115200);
  Wire.begin(D2, D1);

  delay(10);
  StaticJsonDocument<500> doc;
  pinMode(LED, OUTPUT);
  pinMode(AUDIO_SENSOR_PIN, INPUT);
  lightMeter.begin();
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
  {
    delay(1000);
    if (i % 2 == 0)
    {
      digitalWrite(LED, LOW);
    }
    else
    {
      digitalWrite(LED, HIGH);
    }
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
    String serverPath = timeServer;
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    http.begin(*client, serverPath.c_str());
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0)
    {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      DeserializationError error = deserializeJson(doc, payload);

      if (error)
      {
        digitalWrite(LED, LOW);
        log(F("deserializeJson() failed: "));
        log(error.f_str());
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
      digitalWrite(LED, LOW);
    }
    http.end();
  }

  log("Setting up Wifi Monitor");
  delay(10);
  wifi_set_opmode(STATION_MODE); // Promiscuous works only with station mode
  wifi_set_channel(channel);
  delay(10);
  wifi_promiscuous_enable(disable);
  wifi_set_promiscuous_rx_cb(promisc_cb); // Set up promiscuous callback
  delay(10);
  wifi_promiscuous_enable(enable);
  digitalWrite(LED, HIGH);
}

void loop()
{
  channel = 1;
  wifi_set_channel(channel);
  while (true)
  {
    nothing_new++; // Array is not finite, check bounds and adjust if required
    if (nothing_new > 200)
    { // monitor channel for 200 ms
      nothing_new = 0;
      channel++;
      if (channel == 15)
        break; // Only scan channels 1 to 14
      wifi_set_channel(channel);
    }
    delay(1); // critical processing timeslice for NONOS SDK! No delay(0) yield()

    if (clients_known_count > clients_known_count_old)
    {
      clients_known_count_old = clients_known_count;
    }
    if (millis() % SAMPLERATE == 0)
    {
      unsigned long startMillis = millis(); // Start of sample window
      float peakToPeak = 0;                 // peak-to-peak level
      unsigned int signalMax = 0;           // minimum value
      unsigned int signalMin = 1024;        // maximum value
      while (millis() - startMillis < sampleWindow)
      {
        sample = analogRead(AUDIO_SENSOR_PIN);
        if (sample < 1024) // toss out spurious readings
        {
          if (sample > signalMax)
          {
            signalMax = sample; // save just the max levels
          }
          else if (sample < signalMin)
          {
            signalMin = sample; // save just the min levels
          }
        }
      }
      peakToPeak = signalMax - signalMin; // max - min = peak-peak amplitude
      db = map(peakToPeak, 20, 900, 49.5, 90);
      float lux = lightMeter.readLightLevel();
      Serial.print("Light: ");
      Serial.print(lux);
      Serial.println(" lx");
      writeData(outputDevices() + "," + String(lux) + "," + String(db));
      log("wrote data -> " + outputDevices() + "," + String(lux) + "," + String(db));
    }
    if (millis() % 5000 == 0)
    {
      digitalWrite(LED, LOW);
      lastLEDon = millis();
    }
    if (millis() == lastLEDon + 100)
    {
      digitalWrite(LED, HIGH);
    }
  }
  purgeDevice();
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

void writeData(String text)
{
  const String dataText = getTime() + "," + text;
  writeToSD("data.txt", dataText);
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

void purgeDevice()
{
  for (int u = 0; u < clients_known_count; u++)
  {
    if ((millis() - clients_known[u].lastDiscoveredTime) > PURGETIME)
    {
      Serial.print("purge Client");
      Serial.println(u);
      for (int i = u; i < clients_known_count; i++)
        memcpy(&clients_known[i], &clients_known[i + 1], sizeof(clients_known[i]));
      clients_known_count--;
      break;
    }
  }
}

String outputDevices()
{
  int strong = 0;
  int medium = 0;
  int weak = 0;
  // show Clients
  for (int u = 0; u < clients_known_count; u++)
  {
    if (clients_known[u].rssi >= -50)
    {
      strong++;
    }
    else if (clients_known[u].rssi >= -70)
    {
      medium++;
    }
    else
    {
      weak++;
    }
  }
  String out = String(strong) + "," + String(medium) + "," + String(weak);
  return out;
}
´´