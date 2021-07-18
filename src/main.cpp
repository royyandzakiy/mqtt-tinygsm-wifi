// *** main.cpp ***
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include "credentials.h"

//======================================================================//
// CONFIGURATIONS

#define CELLULAR_ONLY // uncomment if want to use SIM800L as connector to internet
// #define WIFI_ONLY // uncomment if want to use WiFi as connector to internet

// ***** TINY GSM CONFIGURATIONS *****
// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

// set GSM PIN, if any
#define GSM_PIN ""

// Select your modem:
#define TINY_GSM_MODEM_SIM800

// Software Serial to SIM800L Modem
SoftwareSerial SerialSim800L(10, 9); // RX, TX

// Your GPRS credentials, if any
#ifndef CREDENTIALS_H
  const char apn[] = "APN";
  const char gprsUser[] = "APN_GPRS_USER";
  const char gprsPass[] = "APN_GPRS_PASS";
#endif

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

#include <TinyGsmClient.h>

#define SerialSim Serial1

// #ifdef DUMP_AT_COMMANDS
//   #include <StreamDebugger.h>
//   StreamDebugger debugger(SerialSim800L, Serial);
//   TinyGsm modem(debugger);
// #else
// TinyGsm modem(SerialSim800L);
// #endif

// ***** WIFI & MQTT CONFIGURATIONS *****
const char* topic_commands = "waterbox/W0001/commands"; // used to retrieve actions that is needed to be done by Waterbox
const char* topic_temp_sensor = "waterbox/W0001/temp_sensor";
const char* topic_flow_sensor = "waterbox/W0001/flow_sensor";
const char* topic_test = "waterbox/W0001/test";
const char* topic_test_2 = "waterbox/W0001/test-2";

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

// ****** GLOBAL VARIABLES *****
#ifdef CELLULAR_ONLY
  TinyGsm modem(SerialSim);
  TinyGsmClient client(modem);
#elif defined(WIFI_ONLY)
  #include <WiFi.h>
  WiFiClient client;
#endif

PubSubClient mqtt(client);
uint32_t lastReconnectAttempt = 999999L; // fill in  withlarge number to connect direclty at first try

// ****** OTHER VARIABLES *****
#define LED_PIN 13
int ledStatus = LOW;

//======================================================================//
// Prototypes

void modem_power_on();
void setup_tinygsm();
void setup_wifi();
void setup_mqtt();
void mqttCallback(char*, byte*, unsigned int);
boolean mqttConnect();
void publish_message(const char*, const char*);

//======================================================================//
// Main

void setup() {
  // Set console baud rate
  Serial.begin(115200);
  delay(10);

  pinMode(LED_PIN, OUTPUT);

  Serial.println("Wait...");

#ifdef CELLULAR_ONLY
  setup_tinygsm();
#elif defined(WIFI_ONLY)
  setup_wifi();
#endif
  setup_mqtt();
}

long lastSend = 0;
int count = 0;

void loop() {
  if (!mqtt.connected()) {
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      Serial.println("=== MQTT NOT CONNECTED ===");
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
    }
    delay(100);
    return;
  }

  if (millis() - lastSend > 3000) {
    Serial.println("oke");
    count++;
    publish_message(topic_test, "Aquifera-ESP32-Client beat");
    lastSend = millis();
  }

  mqtt.loop();
}

//======================================================================//
// Functions
// TinyGSM
// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22
// BME280 pins
#define I2C_SDA_2            18
#define I2C_SCL_2            19

void modem_power_on(){
  pinMode(MODEM_PWKEY,OUTPUT);
  pinMode(MODEM_RST,OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  digitalWrite(MODEM_PWKEY,HIGH);
  digitalWrite(MODEM_RST,HIGH);
  digitalWrite(MODEM_POWER_ON,HIGH);

  // power on the simcom
  delay(1000);
  digitalWrite(MODEM_PWKEY,LOW);
  delay(1000);
  digitalWrite(MODEM_PWKEY,HIGH);
  delay(1000);
}

void setup_tinygsm() {
  delay(1000);

  // Set GSM module baud rate
  // TinyGsmAutoBaud(SerialSim800L, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  // SerialSim800L.begin(28800);
  SerialSim.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(6000);
  modem_power_on();

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  if (!modem.restart()) {
      Serial.println(" fail");
    }
    else{
      Serial.println(" success");      
    }
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

  // Serial.print("Waiting for network...");
  // if (!modem.waitForNetwork()) {
  //   Serial.println(" fail");
  //   delay(10000);
  //   return;
  // }
  // Serial.println(" success");

  // if (modem.isNetworkConnected()) {
  //   Serial.println("Network connected");
  // }

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

// WiFi
void setup_wifi() {
  #ifdef WIFI_ONLY
  WiFi.begin(wifiSSID, wifiPass);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("Connecting to WiFi: ");
    Serial.println(wifiSSID);
  }
  Serial.println("WiFi Connected!");
  #endif
}

// MQTT
void setup_mqtt() {
  // MQTT Broker setup
  Serial.print("Setup mqtt");
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
  Serial.println(mqttConnect() ? " ...done" : " ...failed");
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, len);
  Serial.println();
}

boolean mqttConnect() {
  Serial.print("Mqtt connecting to ");
  Serial.print(broker);
  const char* client_name = "Aquifera-ESP32-Client";

  // Connect to MQTT Broker without authentication
  // boolean status = mqtt.connect("Aquifera-ESP32-Client");

  // Or, if you want to authenticate MQTT:
  boolean status = mqtt.connect(client_name, mqtt_user, mqtt_pass);

  if (status == false) {
    Serial.println(" fail");
    return false;
  }
  Serial.println(" success");
  
  // subscribe to topic
  mqtt.subscribe(topic_test);
  mqtt.subscribe(topic_test_2);
  mqtt.subscribe(topic_commands);
  mqtt.subscribe(topic_flow_sensor);
  mqtt.subscribe(topic_temp_sensor);
  
  Serial.print("Subscribed topic [");
  Serial.print(topic_test);
  Serial.println("]");
  Serial.print("Subscribed topic [");
  Serial.print(topic_test_2);
  Serial.println("]");
  Serial.print("Subscribed topic [");
  Serial.print(topic_commands);
  Serial.println("]");
  Serial.print("Subscribed topic [");
  Serial.print(topic_flow_sensor);
  Serial.println("]");
  Serial.print("Subscribed topic [");
  Serial.print(topic_temp_sensor);
  Serial.println("]");
  
  // publish to topic
  publish_message(topic_test_2, "Aquifera-ESP32-Client started");
  
  return mqtt.connected();
}

void publish_message(const char* topic, const char* payload) {
  mqtt.publish(topic, payload);
  Serial.print("Published ["); 
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(payload);
}