# MQTT-TinyGSM-WiFi

This code is used to connect an MQTT Broker through GSM or WiFi

It is developed using the ESP32 DOIT Development Board

## Getting Started
### Prepration
Make sure you know the connection between the ESP32 to your GSM Modem (if used). Then configure the PIN definition macros accordingly.
You can choose a predefined `BOARD`, or create a custom board PIN definition

```
#define BOARD_TCALL
// #define BOARD_SIM800L
// #define BOARD_CUSTOM

#ifdef BOARD_TCALL
  // TTGO T-Call pins
  #define MODEM_RST            5
  #define MODEM_PWKEY          4
  #define MODEM_POWER_ON       23
  #define MODEM_TX             27
  #define MODEM_RX             26
#elif defined(BOARD_SIM800L)
  // SIM800L directly connected
  #define MODEM_RST            5
  #define MODEM_PWKEY          4
  #define MODEM_POWER_ON       23
  #define MODEM_TX             27
  #define MODEM_RX             26
#elif defined(BOARD_CUSTOM)
  // Custom Pins Here...
  #define MODEM_RST            99
  #define MODEM_PWKEY          99
  #define MODEM_POWER_ON       99
  #define MODEM_TX             99
  #define MODEM_RX             99
#endif
```

## Choosing Internet Connector
To pick between cellular or wifi to connect to the internet, uncomment one of the following

```
#define CELLULAR_ONLY // uncomment if want to use SIM800L as connector to internet
// #define WIFI_ONLY // uncomment if want to use WiFi as connector to internet
```