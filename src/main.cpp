#include <Arduino.h>
#include <Wire.h>
#include <PCF8575.h>
#include <ElegantOTA.h>
#include <elop.h>

#define ESP32

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>

#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1

const char* ssid = "28125D";
const char* password = "RKKT1999!1988$";

AsyncWebServer server(80);

unsigned long ota_progress_millis = 0;

void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);

// PCF8575 I2C address
PCF8575 pcf8575(0x20);

// Relay states
bool relayStates[24] = {0};

// Relay pins on the microcontroller
const int relayPins[8] = {32, 33, 25, 26, 27, 14, 12, 13};

// Relay pins on PCF8575
const int pcfRelayPins[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

bool pcf8575Connected = false;

String generateHTML();

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize relay pins on the microcontroller as outputs
  for (const int& pin : relayPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW); // Set all relays to off initially
  }

  // Initialize PCF8575
  pcf8575Connected = pcf8575.begin();

  // Initialize relay pins on PCF8575 as outputs if connected
  if (pcf8575Connected) {
    for (int pin : pcfRelayPins) {
      pcf8575.pinMode(pin, OUTPUT);
      pcf8575.digitalWrite(pin, LOW); // Set all relays to off initially
    }
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><body><h1>Relay Control</h1>";
    html += "<p>PCF8575 Connection: " + String(pcf8575Connected ? "Successful" : "Failed") + "</p>";
    html += "<div style='display: grid; grid-template-rows: repeat(8, 50px); grid-template-columns: repeat(3, 50px); gap: 10px;'>";
    for (int i = 0; i < 8; i++) {
      html += "<button onclick=\"toggleRelay(" + String(i) + ")\" style=\"grid-row: " + String(i + 1) + "; grid-column: 1; background-color: " + String(relayStates[i] ? "green" : "red") + ";\">A" + String(i + 1) + "</button>";
    }
    for (int i = 0; i < 8; i++) {
      html += "<button onclick=\"toggleRelay(" + String(i + 8) + ")\" style=\"grid-row: " + String(i + 1) + "; grid-column: 2; background-color: " + String(relayStates[i + 8] ? "green" : "red") + ";\">B" + String(i + 1) + "</button>";
    }
    for (int i = 0; i < 8; i++) {
      html += "<button onclick=\"toggleRelay(" + String(i + 16) + ")\" style=\"grid-row: " + String(i + 1) + "; grid-column: 3; background-color: " + String(relayStates[i + 16] ? "green" : "red") + ";\">C" + String(i + 1) + "</button>";
    }
    html += "</div>";
    html += "<script>function toggleRelay(relay) { fetch('/toggle?relay=' + relay).then(response => response.text()).then(data => { location.reload(); }); }</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("relay")) {
      int relay = request->getParam("relay")->value().toInt();
      if (relay >= 0 && relay < 8) {
        relayStates[relay] = !relayStates[relay];
        digitalWrite(relayPins[relay], relayStates[relay] ? HIGH : LOW);
      } else if (relay >= 8 && relay < 24) {
        relayStates[relay] = !relayStates[relay];
        pcf8575.digitalWrite(pcfRelayPins[relay - 8], relayStates[relay] ? HIGH : LOW);
      }
    }
    request->send(200, "text/plain", "OK");
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  ElegantOTA.loop();
  // Add code to control relays here if needed
}

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log OTA progress
  Serial.printf("OTA update progress: %d%%\n", (current * 100) / final);
}

void onOTAEnd(bool success) {
  // Log when OTA has ended
  if (success) {
    Serial.println("OTA update successful!");
  } else {
    Serial.println("OTA update failed!");
  }
}
