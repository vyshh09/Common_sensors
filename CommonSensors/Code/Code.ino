#include <string.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ThingSpeak.h>
#include <WiFiClient.h>
#include <DHT.h>

#define DHTPIN 18
#define DHTTYPE DHT22
#define moisturepin 35
#define flowPin 23
#define relay1 5
// #define pirpin 27
// #define buzzerpin 13

// boolean pirnew = LOW;
// boolean pirold = LOW;

// boolean buzzerstate = false;
// unsigned long buzzertimer = 0;

DHT dht(DHTPIN, DHTTYPE);

unsigned long channel_num = 2534268;
const char* ssid = "Jinx Lol";
const char* password = "siggundali";

volatile long pulse;
unsigned long lastTime;

bool automaticMode = true;
float flow;

WiFiClient client;
WebServer server(80);


void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(relay1, OUTPUT);
  pinMode(flowPin, INPUT);
  pinMode(moisturepin, INPUT);
  // pinMode(buzzerpin, OUTPUT);
  // pinMode(pirpin, INPUT);

  attachInterrupt(digitalPinToInterrupt(flowPin), increase, RISING);
  pulse = 0;
  lastTime = millis();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
  server.on("/", HTTP_GET, handleRoot);
  server.begin();
  ThingSpeak.begin(client);
  delay(5000);
}
void handleRoot() {
  server.sendHeader("Content-Type", "text/html");
  server.send(200, "text/html", SendHTML());
}

void increase() {
  pulse++;
}

void loop() {
  Serial.println(WiFi.localIP());
  server.handleClient();
  flow = 2.663 * pulse / 1000 * 30;
  if (millis() - lastTime > 1000) {
    pulse = 0;
    lastTime = millis();
  }
  // Serial.println(flow);

  float moisture = analogRead(moisturepin);
  float t = dht.readTemperature();

  float h = dht.readHumidity();

  float airvalue = 2800.00;
  float watervalue = 920.00;
  float moisture_percent = (airvalue - moisture) / (airvalue - watervalue) * 100;

  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    if (mode == "automatic") {
      automaticMode = true;
    } else if (mode == "manual") {
      automaticMode = false;
    }
  }

  // Handle commands in manual mode
  if (!automaticMode && server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    if (cmd == "turn_on") {
      Serial.println("Turned On");
      digitalWrite(relay1, LOW);
    } else if (cmd == "turn_off") {
      Serial.println("Turned Off");
      digitalWrite(relay1, HIGH);
    }
    server.send(200, "text/plain", "Command received: " + cmd);
  }

  // Handle automatic mode based on moisture level
  if (automaticMode) {
    if (moisture_percent < 30) {
      Serial.println("dry condition");
      digitalWrite(relay1, LOW);
    } else if (moisture_percent > 60) {
      Serial.println("Wet condition");

      digitalWrite(relay1, HIGH);
      // delay(1800000);
    }
    // delay(10000);
    delay(1000);
  }
  // detectMotion();
  // controlBuzzer();
  int w = 1;
  int relayout = digitalRead(relay1);
  if (relayout == LOW && flow == 0.00) {
    Serial.println("NO WATER SUPPLY!");
    w = 0;
  }
  if (automaticMode == true) {
    Serial.println("Automatic mode");
  } else {
    Serial.println("Manual mode");
  }
  if (!isnan(t)) {
    Serial.println("Temperature: " + String(t) + " °C");
    if (!isnan(h)) {
      Serial.println("Humidity: " + String(h) + " %");
      if (!isnan(moisture_percent)) {
        Serial.println("Moisture Percent: " + String(moisture_percent));
        if (!isnan(flow)) {
          Serial.println("flow: " + String(flow));
          updateThingSpeak(t, h, moisture_percent, flow, relayout, automaticMode, w);
        }
      }
    } else {
      Serial.println("Failed to read humidity from DHT sensor!");
    }
  } else {
    Serial.println("Failed to read temperature from DHT sensor!");
  }
  delay(1000);
}

// void detectMotion() {
//   pirold = pirnew;
//   pirnew = digitalRead(pirpin);
//   if (pirold == LOW && pirnew == HIGH) {
//     Serial.println("Motion detected!");
//     digitalWrite(buzzerpin, HIGH);
//     buzzerstate = true;
//     buzzertimer = millis();
//   }
// }

// void controlBuzzer() {
//   if (buzzerstate == true) {
//     if (millis() - buzzertimer > 1000) {
//       digitalWrite(buzzerpin, LOW);
//       buzzerstate = false;
//       buzzertimer = 0;
//     }
//   }
// }

void updateThingSpeak(float temperature, float humidity, float soil_moisture, float v, float relayOut, bool automaticMode, int waterFlowing) {
  ThingSpeak.setField(1, temperature);
  ThingSpeak.setField(2, humidity);
  ThingSpeak.setField(3, soil_moisture);
  ThingSpeak.setField(4, v);
  ThingSpeak.setField(5, relayOut);
  ThingSpeak.setField(6, automaticMode);
  ThingSpeak.setField(7, waterFlowing);


  int status = ThingSpeak.writeFields(channel_num, "U3TGXBX93TNE30V1");
  if (status == 200) {
    Serial.println("ThingSpeak update successful!");
  }
  // else {
  //   Serial.println("Error updating ThingSpeak. HTTP error code " + String(status));
  // }
}
String SendHTML() {
  String ptr = "<!DOCTYPE html><html lang=\"en\">\n";
  ptr += "<head><meta charset=\"UTF-8\">\n";
  ptr += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
  ptr += "<title>Control Light</title>\n";
  ptr += "<style>body { font-family: Arial, sans-serif; }\n";
  ptr += ".container { text-align: center; margin-top: 50px; }\n";
  ptr += "button { padding: 10px 20px; margin: 10px; font-size: 16px; }\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div class=\"container\">\n";
  ptr += "<h2>Control Water Flow</h2>\n";

  // Add buttons for mode selection (Automatic/Manual)
  ptr += "<button id=\"automaticBtn\" onclick=\"setMode('automatic')\">Automatic</button>\n";
  ptr += "<button id=\"manualBtn\" onclick=\"setMode('manual')\">Manual</button>\n";

  // Add buttons for manual mode (Light On/Light Off)
  ptr += "<div id=\"manualControls\" style=\"display:none;\">\n";
  ptr += "<button id=\"turnOnBtn\" onclick=\"sendCommand('turn_on')\">Turn On</button>\n";
  ptr += "<button id=\"turnOffBtn\" onclick=\"sendCommand('turn_off')\">Turn Off</button>\n";
  ptr += "</div>\n";

  // JavaScript for mode selection and command sending
  ptr += "<script>\n";
  ptr += "function setMode(mode) {\n";
  ptr += "  var xhr = new XMLHttpRequest();\n";
  ptr += "  xhr.open(\"GET\", \"/?mode=\" + mode, true);\n";
  ptr += "  xhr.send();\n";
  ptr += "  if (mode === 'manual') {\n";
  ptr += "    document.getElementById('manualControls').style.display = 'block';\n";
  ptr += "  } else {\n";
  ptr += "    document.getElementById('manualControls').style.display = 'none';\n";
  ptr += "  }\n";
  ptr += "}\n";
  ptr += "function sendCommand(command) {\n";
  ptr += "  var xhr = new XMLHttpRequest();\n";
  ptr += "  xhr.open(\"GET\", \"/control?cmd=\" + command, true);\n";
  ptr += "  xhr.send();\n";
  ptr += "}\n";
  ptr += "</script>\n";

  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
