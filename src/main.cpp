#include "Arduino.h"
#include "Ticker.h"
#include "ESP8266WiFi.h"
#include "PubSubClient.h"
#include "DHT.h"

Ticker       ticker;
WiFiClient   wifi_client;
PubSubClient mqtt_client(wifi_client);
DHT          dht(DHTPIN, DHTTYPE);

int loop_led_state = HIGH;

void feedback_tick() {
  auto state = digitalRead(LED);
  digitalWrite(LED, !state);
}

void setup_wifi() {
  ticker.attach(1, feedback_tick);
  Serial.print("(Wifi) Connecting to ");
  Serial.print(WIFI_SSID);
  Serial.print("...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println(" connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  ticker.detach();
  digitalWrite(LED, loop_led_state);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("(Mqtt) Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  loop_led_state = ((char)payload[0] == '0') ? HIGH : LOW;
}

void reconnect() {
  ticker.attach(0.5, feedback_tick);

  Serial.print("(Mqtt) Connecting to ");
  Serial.print(MQTT_SERVER);
  Serial.print("...");
  
  while (!mqtt_client.connect(MQTT_CLIENT_ID)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println(" connected");
  mqtt_client.subscribe(LED_LOOP_TOPIC); //resubscribe

  ticker.detach();
  digitalWrite(LED, loop_led_state);
}

void measure(float &temperature, float &humidity) 
{
  ticker.attach(0.2, feedback_tick);
  
  Serial.print("(DHT) Measuring...");
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  while (isnan(temperature) || isnan(humidity)) {
    Serial.print(".");
    delay(500);
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
  }
  Serial.println(" ok");
  
  Serial.print("temperature: " ); 
  Serial.println(temperature);
  Serial.print("humidity: " ); 
  Serial.println(humidity);

  ticker.detach();
  digitalWrite(LED, loop_led_state);
}

char send_buffer[16];
void send(const char* channel, float value) { 
  Serial.print("(Mqtt) Sending to ");
  Serial.print(channel); 
  Serial.print("...");

  dtostrf(value, 4, 2, send_buffer);
  if (mqtt_client.publish(channel, send_buffer)) {
    Serial.println(" ok");
  } else {
    Serial.println(" failed");
  }
}

void setup_mqtt() {
  mqtt_client.setServer(MQTT_SERVER, 1883);
  mqtt_client.setCallback(callback);
}

void setup()
{
  pinMode(DHTPIN, INPUT);
  pinMode(LED, OUTPUT);
  Serial.begin(9600);
  dht.begin();

  setup_wifi();
  setup_mqtt();
}

long last = -MEASURE_SLEEP; // measure first time
void loop()
{
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();

  digitalWrite(LED, loop_led_state);

  long now = millis();
  if (now - last > MEASURE_SLEEP) {
    last = now;

    float temperature;
    float humidity;    
    measure(temperature, humidity);
    send(TEMPERATURE_TOPIC, temperature);
    send(HUMIDITY_TOPIC, humidity);
  }
}
