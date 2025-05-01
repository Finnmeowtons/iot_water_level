#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#define FLOAT_LOW 13   // Float sensor for low water level (20%)
#define FLOAT_MID 14   // Float sensor for mid water level (50%)
#define FLOAT_HIGH 12  // Float sensor for high water level (100%)

#define FAUCET_RELAY 5  // Relay for the faucet
#define PUMP_RELAY 4    // Relay for the pump

// WiFi & MQTT
// const char* ssid = "HUAWEI-2.4G-G9ad";
// const char* password = "77t3PXz4";
const char* mqtt_server = "157.245.204.46";

WiFiClient espClient;
PubSubClient client(espClient);

bool autoPumpMode = true;    // Default: Automatic pump mode
bool autoFaucetMode = true;  // Default: Automatic faucet mode
bool isMAISMode = false;     // Default: Tank-based automation
int lastWaterLevel = -1;     // Stores last published water level
String lastFaucetOpenTank = "false";
String lastFaucetOpenMais = "false";
String lastPumpOpenMais = "false";
String lastPumpOpenTank = "false";

unsigned long lastPublishTime = 0;  // Tracks last publish time
const long publishInterval = 3000;  // 10 seconds

void setup() {
  Serial.begin(9600);
  Serial.println("Booting ESP8266...");
  pinMode(FLOAT_LOW, INPUT_PULLUP);
  pinMode(FLOAT_MID, INPUT_PULLUP);
  pinMode(FLOAT_HIGH, INPUT_PULLUP);
  pinMode(FAUCET_RELAY, OUTPUT);
  pinMode(PUMP_RELAY, OUTPUT);

  digitalWrite(PUMP_RELAY, LOW);     // Ensure pump relay starts OFF
  digitalWrite(FAUCET_RELAY, HIGH);  // Ensure faucet starts OPEN (HIGH = Open)

   // Initialize WiFi using WiFiManager
  WiFiManager wifiManager;
  
  // Uncomment to reset saved credentials (for testing)
  // wifiManager.resetSettings();

if (!wifiManager.autoConnect("ESP8266-WATER-CONTROL")) { //TODO Change Device Number
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
    delay(1000);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  int lowState = digitalRead(FLOAT_LOW);
  int midState = digitalRead(FLOAT_MID);
  int highState = digitalRead(FLOAT_HIGH);

  String currentFaucetOpenTank;
  String currentFaucetOpenMais;
  String currentPumpOpenTank;
  String currentPumpOpenMais;

  int currentWaterLevel = (highState == LOW) ? 100 : (midState == LOW) ? 50
                                                   : (lowState == LOW) ? 20
                                                                       : 0;

  bool pumpState = digitalRead(PUMP_RELAY) == HIGH;
  bool faucetState = digitalRead(FAUCET_RELAY) == HIGH;

  unsigned long currentMillis = millis();

  if (currentMillis - lastPublishTime >= publishInterval) {
    lastPublishTime = currentMillis;

    StaticJsonDocument<200> jsonDoc;
    jsonDoc["water_level"] = currentWaterLevel;
    jsonDoc["pump_state"] = pumpState;
    jsonDoc["faucet_state"] = faucetState;
    jsonDoc["auto_faucet"] = autoFaucetMode;
    jsonDoc["auto_pump"] = autoPumpMode;
    jsonDoc["mode"] = isMAISMode ? "mais" : "tank";

    char jsonBuffer[256];
    serializeJson(jsonDoc, jsonBuffer);

    client.publish("water-level/full-state", jsonBuffer);

    // Serial.print("Published JSON: ");
    // Serial.println(jsonBuffer);
  }

  // Serial.print("LOW: ");
  // Serial.print(lowState == LOW ? "ON" : "OFF");
  // Serial.print(" | MID: ");
  // Serial.print(midState == LOW ? "ON" : "OFF");
  // Serial.print(" | HIGH: ");
  // Serial.print(highState == LOW ? "ON" : "OFF");
  // Serial.print(" | LEVEL: ");
  // Serial.println(currentWaterLevel);

  if (!isMAISMode) {
    if (autoPumpMode) {
      digitalWrite(PUMP_RELAY, currentWaterLevel == 20 || currentWaterLevel == 0 ? HIGH : LOW);
      // Serial.println(currentWaterLevel == 20 || currentWaterLevel == 0 ? "🚰 Pump ON (Water LOW)" : "✅ Pump OFF");
    }

    if (autoFaucetMode) {
      digitalWrite(FAUCET_RELAY, (midState == LOW || highState == LOW) ? HIGH : LOW);
      // Serial.println((midState == LOW || highState == LOW) ? "🚰 Faucet OPEN" : "❌ Faucet CLOSED");
    }
  } else {
    if (autoPumpMode) {
      digitalWrite(PUMP_RELAY, currentWaterLevel == 20 || currentWaterLevel == 0 ? HIGH : LOW);
      // Serial.println(currentWaterLevel == 20 || currentWaterLevel == 0 ? "🚰 MAIS Mode: Pump ON (Water LOW)" : "✅ Pump OFF");
    }
    if (autoFaucetMode) {
      // Serial.println("🔧 Faucet controlled by MAIS.");
    }
  }

  delay(1000);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("MQTT Message Received: ");
  Serial.println(message);

  if (strcmp(topic, "water-level/pump-control") == 0) {
    digitalWrite(PUMP_RELAY, message == "true" ? HIGH : LOW);
  } else if (strcmp(topic, "water-level/auto-pump") == 0) {
    digitalWrite(PUMP_RELAY, LOW);
    autoPumpMode = message == "true";
    Serial.println(autoPumpMode ? "Auto Pump Mode: ENABLED" : "Auto Pump Mode: DISABLED");
  } else if (strcmp(topic, "water-level/faucet-control") == 0) {
    digitalWrite(FAUCET_RELAY, message == "true" ? HIGH : LOW);
  } else if (strcmp(topic, "water-level/auto-faucet") == 0) {
    digitalWrite(FAUCET_RELAY, LOW);
    autoFaucetMode = message == "true";
    Serial.println(autoFaucetMode ? "Auto Faucet Mode: ENABLED" : "Auto Faucet Mode: DISABLED");
  } else if (strcmp(topic, "water-level/mode") == 0) {
    digitalWrite(FAUCET_RELAY, LOW);
    digitalWrite(PUMP_RELAY, LOW);
    // autoFaucetMode= true;
    // autoPumpMode= true;
    isMAISMode = message == "mais";
    Serial.println(isMAISMode ? "Switched to MAIS Mode" : "Switched to Tank-Based Mode");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP8266_WaterMonitor")) {
      Serial.println("connected");
      client.subscribe("water-level/pump-control");
      client.subscribe("water-level/auto-pump");
      client.subscribe("water-level/faucet-control");
      client.subscribe("water-level/auto-faucet");
      client.subscribe("water-level/mode");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying...");
      delay(2000);
    }
  }
}
