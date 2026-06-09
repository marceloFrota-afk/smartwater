#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <VL53L0X.h>

// ===== SENSOR =====
VL53L0X sensor;

#define SDA_PIN 6
#define SCL_PIN 7

// ===== WIFI =====
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ===== HIVEMQ =====
const char* mqtt_server = "YOUR_MQTT_SERVER";

const int mqtt_port = 8883;

const char* mqtt_user = "YOUR_MQTT_USERNAME";
const char* mqtt_password = "YOUR_MQTT_PASSWORD";

// ===== TOPICO =====
const char* topico = "YOUR_MQTT_TOPIC";

// ===== MQTT =====
WiFiClientSecure espClient;
PubSubClient client(espClient);

void conectarWiFi() {

  Serial.println("Conectando WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado!");
}

void conectarMQTT() {

  while (!client.connected()) {

    Serial.println("Conectando MQTT...");

    String clientId = "ESP32-C6";

    if (client.connect(
      clientId.c_str(),
      mqtt_user,
      mqtt_password
    )) {

      Serial.println("MQTT conectado!");

    } else {

      Serial.print("Erro MQTT: ");
      Serial.println(client.state());

      delay(2000);
    }
  }
}

void setup() {

  Serial.begin(115200);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // SENSOR
  sensor.setTimeout(500);

  if (!sensor.init()) {

    Serial.println("Falha VL53L0X");

    while (1);
  }

  sensor.startContinuous();

  // WIFI
  conectarWiFi();

  // TLS
  espClient.setInsecure();

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {

  if (!client.connected()) {
    conectarMQTT();
  }

  client.loop();

  int distancia =
  sensor.readRangeContinuousMillimeters();

  if (!sensor.timeoutOccurred()) {

        // ===== LIMITES =====
    int distanciaCheio = 100;
    int distanciaVazio = 1000;

    distancia = distancia - 35;
    if (distancia < 0) {
      distancia = 0;
    }

    // ===== CALCULO =====
    float porcentagem =
    ((float)(distanciaVazio - distancia) /
    (distanciaVazio - distanciaCheio)) * 100.0;

    // Limites
    if (porcentagem > 100) porcentagem = 100;
    if (porcentagem < 0) porcentagem = 0;

    // Converter para texto
    char mensagem[10];

    dtostrf(porcentagem, 4, 1, mensagem);

    char json[100];

    sprintf(
      json,
      "{\"distancia\":%d,\"porcentagem\":%.1f}",
      distancia,
      porcentagem
    );

    client.publish(topico, json);

    Serial.print("Enviado: ");
    Serial.println(mensagem);
  }

  delay(2000);
}