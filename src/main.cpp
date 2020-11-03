
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

// ****** Functions *******
void goDeepsleep() {    
    WiFi.disconnect(true);
    delay(100);
    esp_sleep_enable_timer_wakeup(sendDataIntervallSec * 1000 * 1000ULL);
    Serial.printf("Deepsleep starts now for %d seconds.\n", sendDataIntervallSec);
    delay(100);
    esp_deep_sleep_start();
}

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
          goDeepsleep();
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
  // print starting process
    Serial.println("=== START ==="); 
  // init sensor
    dht.begin();
  // Can be optimized
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)

  //crée la fonction getData()
    DHTSensorData_t sd = getData();       
    
  // Check if any reads failed and exit early (to try again).
    if (isnan(sd.hum) || isnan(sd.temp)) {
        Serial.println(F("[ERROR] Failed to read from DHT sensor!"));
        //indicate to user
        digitalWrite(RED_LED, HIGH);
        delay(1000);
        digitalWrite(RED_LED, LOW);
        goDeepsleep();
  }
    // print data collected to serial console
    Serial.printf("Collected values. hum:%2.2f, temp:%2.2f, tempi:%2.2f\n", sd.hum, sd.temp, sd.tempi);
    Serial.println("-------------------------");
    //inisialize zabbix sender
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
        goDeepsleep();
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
            goDeepsleep();
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
    goDeepsleep();

    //delay(sendDataIntervallSec*1000);
}

