#include <Arduino.h>
#include <ElegantOTA.h>
#include <elop.h>

#define ESP32

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>

const char* ssid = "28125D";
const char* password = "RKKT1999!1988$";

AsyncWebServer server(80);

unsigned long ota_progress_millis = 0;

void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);

// Define relay pins
const int relayPins[] = {32, 33, 25, 26, 27, 14, 12, 13};
bool relayStates[sizeof(relayPins) / sizeof(relayPins[0])] = {0};

void setup(void) {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize relay pins as outputs
  for (int i = 0; i < sizeof(relayPins) / sizeof(relayPins[0]); i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW); // Set all relays to off initially
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><body><h1>Relay Control</h1>";
    html += "<div style='display: grid; grid-template-rows: repeat(5, 50px); grid-template-columns: repeat(3, 50px); gap: 10px;'>";
    html += "<button onclick=\"toggleRelay(3)\" style=\"grid-row: 2; grid-column: 1; background-color: " + String(relayStates[3] ? "green" : "red") + ";\">A1</button>";
    html += "<button onclick=\"toggleRelay(4)\" style=\"grid-row: 1; grid-column: 2; background-color: " + String(relayStates[4] ? "green" : "red") + ";\">A2</button>";
    html += "<button onclick=\"toggleRelay(5)\" style=\"grid-row: 2; grid-column: 3; background-color: " + String(relayStates[5] ? "green" : "red") + ";\">A3</button>";
    html += "<button onclick=\"toggleRelay(0)\" style=\"grid-row: 4; grid-column: 3; background-color: " + String(relayStates[0] ? "green" : "red") + ";\">A4</button>";
    html += "<button onclick=\"toggleRelay(1)\" style=\"grid-row: 5; grid-column: 2; background-color: " + String(relayStates[1] ? "green" : "red") + ";\">A5</button>";
    html += "<button onclick=\"toggleRelay(2)\" style=\"grid-row: 4; grid-column: 1; background-color: " + String(relayStates[2] ? "green" : "red") + ";\">A6</button>";
    html += "<button onclick=\"toggleRelay(6)\" style=\"grid-row: 3; grid-column: 2; background-color: " + String(relayStates[6] ? "green" : "red") + ";\">A7</button>";
    html += "</div>";
    html += "<script>function toggleRelay(relay) { fetch('/toggle?relay=' + relay).then(response => response.text()).then(data => { location.reload(); }); }</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("relay")) {
      int relay = request->getParam("relay")->value().toInt();
      if (relay >= 0 && relay < sizeof(relayPins) / sizeof(relayPins[0])) {
        relayStates[relay] = !relayStates[relay];
        digitalWrite(relayPins[relay], relayStates[relay] ? HIGH : LOW);
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
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}
