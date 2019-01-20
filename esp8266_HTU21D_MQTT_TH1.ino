#include "SparkFunHTU21D.h"
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#define wifi_ssid       "yourWifi"
#define wifi_password   "yourPassword"
#define AIO_SERVER      "yourMQTTip"
#define AIO_SERVERPORT  1883 //MQTT Broker port
#define AIO_USERNAME    "yourMQTTBrokerUserName"
#define AIO_KEY         "yourMQTTBrokerPassword"
#define humd_topic      "sensor/TH1/humidity" //  "TH#" used to identify sensor
#define temp_topic      "sensor/TH1/temperature"
#define stat_topic      "sensor/TH1/state"
// mosquitto_sub -h 127.0.0.1 -v -t "sensor/#

WiFiClient espClient;
Adafruit_MQTT_Client mqtt(&espClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish thtemp = Adafruit_MQTT_Publish(&mqtt, temp_topic, MQTT_QOS_1);
Adafruit_MQTT_Publish thhumd = Adafruit_MQTT_Publish(&mqtt, humd_topic, MQTT_QOS_1);
Adafruit_MQTT_Publish thstat = Adafruit_MQTT_Publish(&mqtt, stat_topic, MQTT_QOS_1);

HTU21D myHumidity;

const int tSleep = 240; //sec, note: default keep alive is 300sec
bool useDeepSleep = true;

void setup() {
  Serial.begin(115200);
  setup_wifi();
  // wait for serial port to open
  while (!Serial) {
    delay(10);
  }

  myHumidity.begin();
  
  mqtt.will(stat_topic, "OFF"); 
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void MQTT_connect() {
  int8_t ret;
  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }
  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}

bool checkBound(float newValue, float prevValue, float maxDiff) {
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}

long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 1.0;

void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition.
  MQTT_connect();
  thstat.publish("RUN");
  
  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;
 
    float newTemp = myHumidity.readTemperature();
    if (checkBound(newTemp, temp, diff)) {
      temp = newTemp;
      Serial.print("New temperature:");
      Serial.println(String(temp).c_str());
      if (! thtemp.publish(String(temp).c_str())) {
        Serial.println("Failed");
      } else {
        Serial.println("OK");
      }
    }

    float newHum = myHumidity.readHumidity();
    if (checkBound(newHum, hum, diff)) {
      hum = newHum;
      Serial.print("New humidity:");
      Serial.println(String(hum).c_str());
      if (! thhumd.publish(String(hum).c_str())) {
        Serial.println("Failed");
      } else {
        Serial.println("OK");
      }
    }

    // either for scheduled deep sleep or to force deep sleep if wifi 
    // is not connected so that wifi goes through setup again.
    if (useDeepSleep || (WiFi.status() != WL_CONNECTED)) {
      thstat.publish("ZZZ");
      //mqtt.disconnect();
      Serial.print("Going to sleep for ");
      Serial.print(String(tSleep).c_str());
      Serial.println(" second(s).");
      ESP.deepSleep(tSleep * 1e6);
      delay(5000);
    } else {
      //light sleep?
    }
  }
}
