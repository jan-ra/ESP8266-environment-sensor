#include <Arduino.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>
#include <AES.h>
#include <AES.cpp>
#include <credentials.h>
#include <wifi_functions.h>
#include <base64.h>

#define disable 0
#define enable 1
#define DEVICEPURGETIME 300000
#define SAMPLERATE 60000
#define SOUNDMEASUREINTERVAl 1000

// PINS
#define LED_PIN D0
#define AUDIO_SENSOR_PIN A0
#define LIGHT_SENSOR_SCL D1
#define LIGHT_SENSOR_SDA D2
#define SD_DATA_PIN D8

// VARIABLES
const int ADC_RESOLUTION = 1024;
const float Vref = 3.3;
const float soundCalibration = 65.0;

unsigned long setTime = 0;
unsigned long soundSampleStart;
unsigned long lastSampleTime = 0;
unsigned int hours = 0;
unsigned int minutes = 0;
unsigned int seconds = 0;
unsigned int channel = 1;
unsigned int ledstatus = true;
unsigned int lastLEDon = true;

int deviceCount[3] = {0, 0, 0};
int maxVoltage = 0;
int minVoltage = ADC_RESOLUTION;

String timeServer = "https://timeapi.io/api/Time/current/zone?timeZone=Europe/Amsterdam";

BH1750 lightMeter;
StaticJsonDocument<500> doc;
HTTPClient http;
std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

// FUNCTIONS (inital listing needed for platformIO conversion for ESP8266)
String getTime();
String outputDevices();
String encrypt(unsigned char plain[], int size);
float measureSound();
void log(String text);
void writeToSD(String filePath, String text);
void showDevices();
void writeData(String text);
void purgeDevices();
void beginWifiCallback();
void pullCurrentTime();

void setup()
{
  Serial.begin(115200);

  Wire.begin(LIGHT_SENSOR_SDA, LIGHT_SENSOR_SCL);
  pinMode(LED_PIN, OUTPUT);
  analogReference(EXTERNAL);

  lightMeter.begin();
  Serial.println('\n');
  Serial.println("Initializing SD cards");

  if (!SD.begin(SD_DATA_PIN))
  {
    Serial.println("Error while initializing SD Card");
    return;
  }

  log("SD card initalized");
  log("Connecting to Wifi");

  // Flashing LED iwhile waiting for successful WIFI connection
  WiFi.begin(ssid, password);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    int state = (i % 2 == 0) ? LOW : HIGH;
    digitalWrite(LED_PIN, state);
  }

  log("Connection established!");
  log("Pulling current time");

  // set System time for Sensor logging
  if (WiFi.status() == WL_CONNECTED)
  {
    pullCurrentTime();
  }

  log("Begin Wifi Callbacks");
  beginWifiCallback();
  digitalWrite(LED_PIN, HIGH);
}

void loop()
{
  channel = 1;
  wifi_set_channel(channel);
  while (true)
  {
    no_new_device++;
    // monitor channel for 200 increments
    if (no_new_device > 200)
    {
      no_new_device = 0;
      channel++;
      // Only need to scan channels 1 to 14
      if (channel == 15)
        break;
      wifi_set_channel(channel);
    }

    // Write sensor data once samplerate is reached
    if (millis() - lastSampleTime >= SAMPLERATE)
    {
      float db = measureSound();
      float lux = lightMeter.readLightLevel();
      writeData(
          String(deviceCount[0]) + "," +
          String(deviceCount[1]) + "," +
          String(deviceCount[2]) + "," +
          String(lux) + "," +
          String(db));
      log("wrote data");
      lastSampleTime = millis() - (millis() - lastSampleTime);
    }
  }

  // remove devices that have not been acquired for longer time
  purgeDevices();

  // critical processing timeslice for NONOS SDK! No delay(0) yield()
  delay(1);
}

String getTime()
{
  // assumes no date passage (should be expanded)
  unsigned long timePassed = millis() - setTime;
  unsigned long secondsadded = timePassed / 1000 + seconds;
  unsigned long minutesadded = secondsadded / 60 + minutes;
  unsigned long hoursadded = minutesadded / 60 + hours;

  timePassed %= 1000;
  secondsadded %= 60;
  minutesadded %= 60;
  hoursadded %= 24;

  String out = String(hoursadded) + ":" +
               String(minutesadded) + ":" +
               String(secondsadded);
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
  // prefix with time and encryt
  String dataText = getTime() + "," + text;
  int size = dataText.length();
  unsigned char *plain = (unsigned char *)dataText.c_str();
  String encText = encrypt(plain, size);
  writeToSD("data.txt", encText);
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

void purgeDevices()
{
  for (int u = 0; u < clients_known_count; u++)
  {
    if ((millis() - clients_known[u].lastDiscoveredTime) > DEVICEPURGETIME)
    {
      for (int i = u; i < clients_known_count; i++)
        memcpy(&clients_known[i], &clients_known[i + 1], sizeof(clients_known[i]));
      clients_known_count--;
      break;
    }
  }
}

void countDevices()
{
  deviceCount[0] = 0;
  deviceCount[1] = 0;
  deviceCount[2] = 0;
  for (int u = 0; u < clients_known_count; u++)
  {
    if (clients_known[u].rssi >= -50)
    {
      deviceCount[0]++;
    }
    else if (clients_known[u].rssi >= -70)
    {
      deviceCount[1]++;
    }
    else
    {
      deviceCount[2]++;
    }
  }
}

float measureSound()
{
  maxVoltage = 0;
  minVoltage = ADC_RESOLUTION;
  soundSampleStart = millis();

  while (millis() - soundSampleStart <= SOUNDMEASUREINTERVAl)
  {
    // Read the analog input value
    int analogValue = analogRead(AUDIO_SENSOR_PIN);

    // Update peak voltages
    if (analogValue > maxVoltage)
    {
      maxVoltage = analogValue;
    }

    if (analogValue < minVoltage)
    {
      minVoltage = analogValue;
    }
  }

  // Calculate the peak-to-peak voltage
  float peakToPeakVoltage = (maxVoltage - minVoltage) * Vref / ADC_RESOLUTION;

  // Calculate the decibel value using the peak-to-peak voltage and calibration offset
  float dB = 20 * log10(peakToPeakVoltage / Vref) + soundCalibration;
  return dB;
}

void pullCurrentTime()
{
  client->setInsecure();
  http.begin(*client, timeServer.c_str());
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      digitalWrite(LED_PIN, LOW);
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
    digitalWrite(LED_PIN, LOW);
  }
  http.end();
}

void beginWifiCallback()
{
  delay(10);
  wifi_set_opmode(STATION_MODE); // Promiscuous works only with station mode
  wifi_set_channel(channel);
  delay(10);
  wifi_promiscuous_enable(disable);
  wifi_set_promiscuous_rx_cb(promisc_cb); // Set up promiscuous callback
  delay(10);
  wifi_promiscuous_enable(enable);
}

String encrypt(unsigned char plain[], int size)
{
  int remainder = size % 16;
  int padding = (remainder == 0) ? 0 : (16 - remainder);
  for (int i = 0; i < padding; i++)
  {
    plain[size + i] = 'X';
  }
  size += padding;

  AES aes(AESKeyLength::AES_128);
  unsigned int plainLen = size * sizeof(unsigned char); // bytes in plaintext
  unsigned int outLen = size;                           // out param - bytes in сiphertext
  unsigned char *e = aes.EncryptCBC(plain, plainLen, key, iv);
  String base64String = base64::encode(e, outLen);
  return base64String;
}