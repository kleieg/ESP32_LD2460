// set hostname used for MQTT tag and WiFi
#define HOSTNAME "ESP-LD2460"
#define MQTT_BROKER "sym_mqtt"
#define VERSION "v 1.0.0"

#define MQTT_INTERVAL 1000
#define RECONNECT_INTERVAL 5000
#define LED_BLINK_INTERVAL 500
#define RELAY_RESET_INTERVAL 5000


#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
#define SERIALINIT Serial.begin(115200);
#define GPIO_LED  5
#else
#define SERIALINIT
#define GPIO_LED  1
#endif

 // Define the RX and TX pins for Serial 2
#define RXD2 16
#define TXD2 17

#define LD2460_BAUD 115200