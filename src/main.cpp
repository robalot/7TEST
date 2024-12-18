#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <ElegantOTA.h>
#include <elop.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

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

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // Update every 60 seconds

void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);

// MCP23X17 I2C address
Adafruit_MCP23X17 mcp;

// Relay states
bool relayStates[24] = {0};

// Relay pins on the microcontroller
const int relayPins[8] = {32, 33, 25, 26, 27, 14, 12, 13};

// Relay pins on MCP23X17
const int mcpRelayPins[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

bool mcpConnected = false;
bool cycleCompleted = false;

// Separate variables for each digit in the time
int hourTens = 0;
int hourUnits = 0;
int minuteTens = 0;
int minuteUnits = 0;

String generateHTML();

void cyclePins();

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  timeClient.begin();
  timeClient.update();

 // Extract digits for hours and minutes
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();

  hourTens = hours / 10;
  hourUnits = hours % 10;
  minuteTens = minutes / 10;
  minuteUnits = minutes % 10;

  Serial.printf("Current time: %d%d:%d%d\n", hourTens, hourUnits, minuteTens, minuteUnits);

  

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

  // Initialize MCP23X17
  if (!mcp.begin_I2C()) { // Default address 0x20
    Serial.println("Error: MCP23X17 not found!");
  } else {
    mcpConnected = true;
    Serial.println("MCP23X17 connected successfully.");
  }

  // Initialize relay pins on MCP23X17 as outputs if connected
  if (mcpConnected) {
    for (int i = 0; i < sizeof(mcpRelayPins) / sizeof(mcpRelayPins[0]); i++) {
      mcp.pinMode(mcpRelayPins[i], OUTPUT);
      mcp.digitalWrite(mcpRelayPins[i], HIGH); // Set all relays to on initially
    }
  }

  // Initialize NTP client
  timeClient.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    timeClient.update();
    String formattedTime = timeClient.getFormattedTime();
    String timeWithoutSeconds = formattedTime.substring(0, 5); // Get HH:MM part of the time
    String html = "<html><body><h1>Relay Control</h1>";
    html += "<p>MCP23X17 Connection: " + String(mcpConnected ? "Successful" : "Failed") + "</p>";
    html += "<p>Current Time: " + timeWithoutSeconds + "</p>";
    html += "<h2>Current Time: ";
    html += String(hourTens) + String(hourUnits) + ":" + String(minuteTens) + String(minuteUnits);
    html += "</h2>";
    html += "<h3>Hour Tens: " + String(hourTens) + "</h3>";
    html += "<h3>Hour Units: " + String(hourUnits) + "</h3>";
    html += "<h3>Minute Tens: " + String(minuteTens) + "</h3>";
    html += "<h3>Minute Units: " + String(minuteUnits) + "</h3>";
    html += "</body></html>";
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
        mcp.digitalWrite(mcpRelayPins[relay - 8], relayStates[relay] ? LOW : HIGH); // Swapped HIGH and LOW
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
  timeClient.update();
  // Add code to control relays here if needed

  // Cycle pins once on startup
  if (!cycleCompleted) {
    cyclePins();
    cycleCompleted = true;
  }
}

void cyclePins() {
  // Save current states
  bool currentRelayStates[24];
  for (int i = 0; i < 8; i++) {
    currentRelayStates[i] = digitalRead(relayPins[i]);
  }
  if (mcpConnected) {
    for (int i = 0; i < 16; i++) {
      currentRelayStates[i + 8] = mcp.digitalRead(mcpRelayPins[i]);
    }
  }

  // Cycle each pin on the microcontroller
  for (const int& pin : relayPins) {
    digitalWrite(pin, HIGH);
    delay(1000);
    digitalWrite(pin, LOW);
  }

  // Cycle each pin on the MCP23X17
  if (mcpConnected) {
    for (int i = 0; i < sizeof(mcpRelayPins) / sizeof(mcpRelayPins[0]); i++) {
      mcp.digitalWrite(mcpRelayPins[i], LOW);
      delay(1000);
      mcp.digitalWrite(mcpRelayPins[i], HIGH);
    }
  }

  // Restore previous states
  for (int i = 0; i < 8; i++) {
    digitalWrite(relayPins[i], currentRelayStates[i]);
  }
  if (mcpConnected) {
    for (int i = 0; i < 16; i++) {
      mcp.digitalWrite(mcpRelayPins[i], currentRelayStates[i + 8]);
    }
  }
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
