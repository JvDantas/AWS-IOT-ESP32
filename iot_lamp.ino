#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

#include "DHT.h"
#include "Wire.h";
#include <NTPClient.h>

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "$aws/things/test_esp32/shadow/update"
#define AWS_IOT_SUBSCRIBE_TOPIC "$aws/things/test_esp32/shadow/update/delta"


//Sensors
#define DHTPIN 23
#define DHTTYPE DHT11
#define SN 15

int msgReceived = 0;
String rcvdPayload;
char sndPayloadOff[512];
char sndPayloadOn[512];
// to sensors
String Hora; 
float Temperatura = 0;
float Umidade = 0;
int Level = 0;

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

DHT dht(DHTPIN, DHTTYPE);
WiFiUDP udp;
NTPClient ntp(udp, "0.br.pool.ntp.org", -4*3600, 60000);


//------------------- FUNCTION ---------------------

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("");
  Serial.println("###################### Starting Execution ########################");
  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  Serial.println("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }

  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");
}

void messageHandler(String &topic, String &payload) {
  msgReceived = 1;
  rcvdPayload = payload;
}

void readsensors(){
  // Coletando dados
  Temperatura = dht.readTemperature();
  Umidade = dht.readHumidity();
  Level = digitalRead(SN);
  Hora = ntp.getFormattedTime();
  // Printando dados
  Serial.println(F("Umidade: "));
  Serial.print(Umidade);
  Serial.println(F("%  Temperatura: "));
  Serial.print(Temperatura);
  if ((Level == 1)) {
    Serial.println("Nivel: Vazio");
  }
  else if ((Level == 0) ) {
    Serial.println("Nivel: Alto");
  }
 Serial.println(Hora);
}

void publishSensors()
{
  Serial.println("publishing sensors to MQTT");
  StaticJsonDocument<200> doc;
  doc["humidity"] = Umidade;
  doc["temperature"] = Temperatura;
  doc["level"] = Level;
  doc["hour"] = Hora;
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

//------------------- MAIN CODE ---------------------

void setup() {
  delay(2000);
  
  Serial.begin(9600);
  pinMode(13, OUTPUT);
  pinMode(SN, INPUT);
  
  sprintf(sndPayloadOn,"{\"state\": { \"reported\": { \"status\": \"on\" } }}");
  sprintf(sndPayloadOff,"{\"state\": { \"reported\": { \"status\": \"off\" } }}");
  
  connectAWS();
  
  dht.begin();
  ntp.begin();              
  ntp.forceUpdate();  
  
  Serial.println("Setting Lamp Status to Off");
  client.publish(AWS_IOT_PUBLISH_TOPIC, sndPayloadOff);
  
  Serial.println("##############################################");
}

void loop() {
    readsensors();
    publishSensors();
   if(msgReceived == 1)
    {
//      This code will run whenever a message is received on the SUBSCRIBE_TOPIC_NAME Topic
        delay(100);
        msgReceived = 0;
        Serial.print("Received Message:");
        Serial.println(rcvdPayload);
        StaticJsonDocument<200> sensor_doc;
        DeserializationError error_sensor = deserializeJson(sensor_doc, rcvdPayload);
        const char *sensor = sensor_doc["state"]["status"];
 
        Serial.print("AWS Says:");
        Serial.println(sensor); 
        
        if(strcmp(sensor, "on") == 0)
        {
         Serial.println("IF CONDITION");
         Serial.println("Turning Lamp On");
         digitalWrite(13, HIGH);
         client.publish(AWS_IOT_PUBLISH_TOPIC, sndPayloadOn);
        }
        else 
        {
         Serial.println("ELSE CONDITION");
         Serial.println("Turning Lamp Off");
         digitalWrite(13, LOW);
         client.publish(AWS_IOT_PUBLISH_TOPIC, sndPayloadOff);
        }
      Serial.println("##############################################");
    }
  client.loop();
  delay(2000);
}
