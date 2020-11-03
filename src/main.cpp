
#include "Arduino.h"

#include <WiFi.h>
#include "zabbixSender.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "settings.h"

//-----------SENSOR----------
#define DHTPIN 14
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

//Define DHTSensorData_t Structure
typedef struct DHTSensorData_t
{
  float hum;
  float temp;
  float tempi;
}DHTSensorData_t;

// ****** Functions *******
DHTSensorData_t getData(){
  DHTSensorData_t sd;
  sd.hum = dht.readHumidity();
  // Read temperature as Celsius (the default)
  sd.temp = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  //float f = dht.readTemperature(true);
  sd.tempi = dht.computeHeatIndex(sd.temp, sd.hum, false);

  return sd;
}
// ****** Main *******

void setup() {   
    Serial.begin(115200);
    delay(100);    
    Serial.println("=== SETUP Start ===");
    
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);

    Serial.println("Waiting for WiFi... ");   
    // indicate to user
    digitalWrite(GREEN_LED, HIGH);   
    WiFi.begin(ssid, wifiKey);
    // current wifi ap connect cycles
    int wccycles = 0;
    while(WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        Serial.print((int)WiFi.status());
        wccycles++;
        // try maxWifiConnectcycles than deepsleep
        if (wccycles > maxWifiConnectcycles) {
          Serial.printf("\n[ERROR] WLAN TIMEOUT - Abort\n");
          // indicate to user
          digitalWrite(GREEN_LED, LOW);
          digitalWrite(RED_LED, HIGH);
          delay(1000);
          digitalWrite(RED_LED, LOW);
        }
        delay(300);
    }
    
    // we are connected, turn off
    digitalWrite(GREEN_LED, LOW);    
    Serial.printf("\nWiFi connected. IP address:");
    Serial.println(WiFi.localIP());
    Serial.println("=== SETUP End ===");
} // setup

void loop() {
    Serial.println("=== START ==="); 
    // init sensor
    // a changer par DHT 
    dht.begin();
 // Can be optimized
 // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  //float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println(F("[ERROR] Failed to read from DHT sensor!"));
        //indicate to user
        digitalWrite(RED_LED, HIGH);
        delay(1000);
        digitalWrite(RED_LED, LOW);
  }
    //crée la fonction getData()

    DHTSensorData_t sd = getData();       
    Serial.printf("Collected values. hum:%2.2f, temp:%2.2f, tempi:%2.2f\n", sd.hum, sd.temp, sd.tempi);

    ZabbixSender zs;
    String jsonPayload;
    jsonPayload = zs.createPayload(hostname, sd.hum, sd.temp, sd.tempi);
      // create message in zabbix sender format
    String msg = zs.createMessage(jsonPayload);
    
    // connect and transmit
    Serial.printf("==> Connecting to server :%s:%d\n", server, port);
    // connect to server
    WiFiClient client;
    // indicate to user
    digitalWrite(GREEN_LED, HIGH);      
    if (!client.connect(server, port)) {
        Serial.printf("[ERROR] Connection FAILED - Abort\n"); 
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(RED_LED, HIGH);
        delay(1000);
        digitalWrite(RED_LED, LOW); 
    }
    
    Serial.println("Connected. Sending data.");
    client.print(msg);  
    unsigned long startTime = millis();
    while (client.available() == 0) {
        if (millis() - startTime > maxTransmissionTimeout) {
            Serial.printf("[ERROR] Transmission TIMEOUT - Abort\n");
            client.stop();
            // indicate to user
            digitalWrite(GREEN_LED, LOW);
            digitalWrite(RED_LED, HIGH);
            delay(1000);
            digitalWrite(RED_LED, LOW);
        }
    }
    
    digitalWrite(GREEN_LED, LOW);
    // read reply from zabbix server
    while(client.available()) {
        String line = client.readStringUntil('\r');
        Serial.print(line);
    }    
    Serial.println();

    Serial.printf("Closing connection - Sleeping for %d sec...\n", sendDataIntervallSec);
    client.stop(); 

    delay(sendDataIntervallSec);
}

