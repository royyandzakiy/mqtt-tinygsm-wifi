// *** main.cpp ***
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <EspSoftwareSerial.h>
#include "credentials.h"

//======================================================================//
// CONFIGURATIONS

// #define CELLULAR_ONLY // uncomment if want to use SIM800L as connector to internet
#define WIFI_ONLY // uncomment if want to use WiFi as connector to internet

// ***** TINY GSM CONFIGURATIONS *****
// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

// Your GPRS credentials, if any
#ifndef CREDENTIALS_H
  const char apn[] = "APN";
  const char gprsUser[] = "APN_GPRS_USER";
  const char gprsPass[] = "APN_GPRS_PASS";
#endif

// set GSM PIN, if any
#define GSM_PIN ""

// Select your modem:
#define TINY_GSM_MODEM_SIM800

// Software Serial to SIM800L Modem
SoftwareSerial SerialSim800L(10, 9); // RX, TX

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG Serial

// Range to attempt to autobaud
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200

// Define how you're planning to connect to the internet
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// Just in case someone defined the wrong thing..
#if TINY_GSM_USE_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true
#endif
#if TINY_GSM_USE_WIFI && not defined TINY_GSM_MODEM_HAS_WIFI
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#endif

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialSim800L, Serial);
  TinyGsm modem(debugger);
#else
TinyGsm modem(SerialSim800L);
#endif

// ***** WIFI & MQTT CONFIGURATIONS *****
// WIFI & MQTT Credentials
#ifndef CREDENTIALS_H
  // Your WiFi connection credentials, if applicable
  const char wifiSSID[] = "WIFI_SSID";
  const char wifiPass[] = "WIFI_PASS";

  // MQTT details
  const char* broker = "MQTT_BROKER_SERVER";
  const char* mqtt_user = "MQTT_BROKER_USER";
  const char* mqtt_pass = "MQTT_BROKER_PASS";
#endif

const char* topicLed = "GsmClientTest/led";
const char* topicInit = "GsmClientTest/init";
const char* topicLedStatus = "GsmClientTest/ledStatus";

// ****** GLOBAL VARIABLES *****
#ifdef CELLULAR_ONLY
  TinyGsmClient client(modem);
#else defined(WIFI_ONLY)
  #include <WiFi.h>
  WiFiClient client;
#endif

PubSubClient mqtt(client);

// ****** OTHER VARIABLES *****
#define LED_PIN 13
int ledStatus = LOW;

uint32_t lastReconnectAttempt = 0;

//======================================================================//
// Prototypes

void mqttCallback(char*, byte*, unsigned int);
boolean mqttConnect();
void setup_tinygsm();
void setup_mqtt();

//======================================================================//
// Main

void setup() {
  // Set console baud rate
  Serial.begin(9600);
  delay(10);

  pinMode(LED_PIN, OUTPUT);

  Serial.println("Wait...");

#ifdef CELLULAR_ONLY
  setup_tinygsm();
#else defined(WIFI_ONLY)
  setup_wifi();
#endif
  setup_mqtt();
}

void loop() {
  if (!mqtt.connected()) {
    Serial.println("=== MQTT NOT CONNECTED ===");
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
    }
    delay(100);
    return;
  }

  mqtt.loop();
}

//======================================================================//
// Functions

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, len);
  Serial.println();

  // Only proceed if incoming message's topic matches
  if (String(topic) == topicLed) {
    ledStatus = !ledStatus;
    digitalWrite(LED_PIN, ledStatus);
    mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
  }
}

boolean mqttConnect() {
  Serial.print("Connecting to ");
  Serial.print(broker);

  // Connect to MQTT Broker without authentication
  // boolean status = mqtt.connect("Aquifera-ESP32-Client");

  // Or, if you want to authenticate MQTT:
  boolean status = mqtt.connect("Aquifera-ESP32-Client", mqtt_user, mqtt_pass);

  if (status == false) {
    Serial.println(" fail");
    return false;
  }
  Serial.println(" success");
  mqtt.publish(topicInit, "GsmClientTest started");
  mqtt.subscribe(topicLed);
  return mqtt.connected();
}

void setup_tinygsm() {
  // Set GSM module baud rate
  TinyGsmAutoBaud(SerialSim800L, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  // SerialSim800L.begin(9600);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  modem.restart();
  // modem.init();

  String modemInfo = modem.getModemInfo();
  Serial.print("Modem Info: ");
  Serial.println(modemInfo);

#if TINY_GSM_USE_GPRS
  // Unlock your SIM card with a PIN if needed
  if ( GSM_PIN && modem.getSimStatus() != 3 ) {
    modem.simUnlock(GSM_PIN);
  }
#endif

#if TINY_GSM_USE_WIFI
    // Wifi connection parameters must be set before waiting for the network
  Serial.print(F("Setting SSID/password..."));
  if (!modem.networkConnect(wifiSSID, wifiPass)) {
    Serial.println(" fail");
    delay(10000);
    return;
  }
  Serial.println(" success");
#endif

#if TINY_GSM_USE_GPRS && defined TINY_GSM_MODEM_XBEE
  // The XBee must run the gprsConnect function BEFORE waiting for network!
  modem.gprsConnect(apn, gprsUser, gprsPass);
#endif

  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    Serial.println(" fail");
    delay(10000);
    return;
  }
  Serial.println(" success");

  if (modem.isNetworkConnected()) {
    Serial.println("Network connected");
  }

#if TINY_GSM_USE_GPRS
  // GPRS connection parameters are usually set after network registration
    Serial.print(F("Connecting to "));
    Serial.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println(" fail");
      delay(10000);
      return;
    }
    Serial.println(" success");

  if (modem.isGprsConnected()) {
    Serial.println("GPRS connected");
  }
#endif
}

void setup_mqtt() {
  // MQTT Broker setup
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
}

void setup_wifi() {
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
}