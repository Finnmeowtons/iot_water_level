#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define FLOAT_SWITCH 14   // Float water level sensor
#define FAUCET_RELAY 0    // Relay for the faucet
#define PUMP_RELAY 13      // Relay for the pump

// WiFi & MQTT
const char* ssid = "tp-link";
const char* password = "09270734452";
const char* mqtt_server = "157.245.204.46";

WiFiClient espClient;
PubSubClient client(espClient);

bool autoPumpMode = true;    // Default: Automatic pump mode
bool autoFaucetMode = true;  // Default: Automatic faucet mode

void setup() {
  Serial.begin(115200);
  pinMode(FLOAT_SWITCH, INPUT_PULLUP);
  pinMode(FAUCET_RELAY, OUTPUT);
  pinMode(PUMP_RELAY, OUTPUT);
  digitalWrite(PUMP_RELAY, LOW);   // Ensure pump relay starts OFF
  digitalWrite(FAUCET_RELAY, HIGH); // Ensure faucet starts OPEN (HIGH = Open)

  WiFi.begin(ssid, password);
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

  int floatState = digitalRead(FLOAT_SWITCH);

  Serial.print("Float: ");
  Serial.print(floatState);
  Serial.print(" | Pump: ");
  Serial.print(digitalRead(PUMP_RELAY));
  Serial.print(" | Faucet: ");
  Serial.println(digitalRead(FAUCET_RELAY));

  // Publish float sensor state
  client.publish("water-level/float", floatState == LOW ? "LOW" : "OK");

  // **Automatic Pump Logic**
  if (autoPumpMode) {
    if (floatState == LOW) {  // Water low, turn pump ON
      digitalWrite(PUMP_RELAY, HIGH);
      Serial.println("🚰 Water LOW! Pump ON.");
    } else {  // Water OK, turn pump OFF
      digitalWrite(PUMP_RELAY, LOW);
      Serial.println("✅ Water OK. Pump OFF.");
    }
  }

  // **Automatic Faucet Logic**
  if (autoFaucetMode) {
    if (floatState == LOW) {  // At 20% → Faucet CLOSES
      digitalWrite(FAUCET_RELAY, LOW);
      Serial.println("❌ Water 20%! Faucet CLOSED.");
    } else if (floatState == HIGH) {  // At 50%+ → Faucet OPENS
      digitalWrite(FAUCET_RELAY, HIGH);
      Serial.println("🚰 Water 50%+ Faucet OPEN.");
    }
  }

  delay(1000);
}

// **Handle MQTT Messages**
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("MQTT Message Received: ");
  Serial.println(message);

  if (strcmp(topic, "water-level/pump-control") == 0) {
    if (message == "true") {
      digitalWrite(PUMP_RELAY, HIGH);
      Serial.println("🔧 Manual Pump ON");
    } else if (message == "false") {
      digitalWrite(PUMP_RELAY, LOW);
      Serial.println("🔧 Manual Pump OFF");
    }
  } 
  else if (strcmp(topic, "water-level/auto-pump") == 0) {
    if (message == "true") {
      autoPumpMode = true;
      Serial.println("Auto Pump Mode: ENABLED");
    } else if (message == "false") {
      autoPumpMode = false;
      Serial.println("Auto Pump Mode: DISABLED");
      digitalWrite(PUMP_RELAY, LOW);
    }
  } 
  else if (strcmp(topic, "water-level/faucet-control") == 0) {
    if (message == "true") {
      digitalWrite(FAUCET_RELAY, HIGH);
      Serial.println("🔧 Manual Faucet OPEN");
    } else if (message == "false") {
      digitalWrite(FAUCET_RELAY, LOW);
      Serial.println("🔧 Manual Faucet CLOSED");
    }
  } 
  else if (strcmp(topic, "water-level/auto-faucet") == 0) {
    if (message == "true") {
      autoFaucetMode = true;
      Serial.println("Auto Faucet Mode: ENABLED");
    } else if (message == "false") {
      autoFaucetMode = false;
      Serial.println("Auto Faucet Mode: DISABLED");
      digitalWrite(FAUCET_RELAY, HIGH);  // Default to Open
    }
  }
}

// **Reconnect MQTT**
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP8266_WaterMonitor")) {
      Serial.println("connected");
      client.subscribe("water-level/pump-control");
      client.subscribe("water-level/auto-pump");
      client.subscribe("water-level/faucet-control");
      client.subscribe("water-level/auto-faucet");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying...");
      delay(2000);
    }
  }
}
