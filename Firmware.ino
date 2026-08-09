
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

// =====================================================
// Wi-Fi
// =====================================================

const char* ssid = "Predictive maintenance system";
const char* password = "123456798";

// =====================================================
// DHT11
// =====================================================

#define DHTPIN D4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// =====================================================
// Sensors
// =====================================================

const int voltagePin = A0;

// Fire sensor
#define FIRE_PIN D3

// =====================================================
// Relay Outputs
// =====================================================

#define FAN_RELAY     D5
#define BULB_RELAY    D6
#define ALARM_RELAY   D7
#define BREAKER_RELAY D8

// Most relay modules are Active LOW
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// Most flame sensor modules:
// LOW = Fire detected
#define FIRE_DETECTED LOW

// =====================================================
// Web Server
// =====================================================

ESP8266WebServer server(80);

// =====================================================
// Global Variables
// =====================================================

float temperature = 0.0;
float humidity = 0.0;
float sensorVoltage = 0.0;

String faultStatus = "HEALTHY";
String actionStatus = "NO PM REQUIRED";

int remainingLife = 100;

bool fireDetected = false;

bool fanState = false;
bool bulbState = false;
bool alarmState = false;
bool breakerState = false;

String anomalousSignature = "None";
String directive = "Continue Monitoring";
String pmClassification = "Normal Operation";

// =====================================================
// Read Sensors
// =====================================================

void readSensors()
{
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  int adcValue = analogRead(voltagePin);

  // ESP8266 ADC:
  // Raw ADC = 0 to 1023
  // Chip ADC input = approximately 0 to 1V

  sensorVoltage = (adcValue * 1.0) / 1023.0;

  // Fire sensor
  fireDetected = (digitalRead(FIRE_PIN) == FIRE_DETECTED);
}

// =====================================================
// Predictive Maintenance Logic
// =====================================================

void calculateMaintenanceStatus()
{
  // -----------------------------------------------
  // Sensor Error
  // -----------------------------------------------

  if (isnan(temperature) || isnan(humidity))
  {
    faultStatus = "SENSOR ERROR";
    actionStatus = "CHECK SENSOR";

    remainingLife = 0;

    anomalousSignature = "Sensor Failure";
    directive = "Inspect Sensor";
    pmClassification = "Maintenance Required";

    return;
  }

  // =================================================
  // FIRE CONDITION - HIGHEST PRIORITY
  // =================================================

  if (fireDetected)
  {
    faultStatus = "CRITICAL";
    actionStatus = "IMMEDIATE ACTION REQUIRED";

    remainingLife = 0;

    anomalousSignature = "Fire Hazard";
    directive = "Immediate Inspection";
    pmClassification = "Immediate Action Required";

    // Fan OFF
    digitalWrite(FAN_RELAY, RELAY_OFF);
    fanState = false;

    // Bulb OFF
    digitalWrite(BULB_RELAY, RELAY_OFF);
    bulbState = false;

    // Alarm ON
    digitalWrite(ALARM_RELAY, RELAY_ON);
    alarmState = true;

    // Breaker TRIP
    digitalWrite(BREAKER_RELAY, RELAY_ON);
    breakerState = true;

    return;
  }

  // =================================================
  // CRITICAL
  // =================================================

  if (temperature > 60 ||
      humidity > 90 ||
      sensorVoltage < 0.50)
  {
    faultStatus = "CRITICAL";
    actionStatus = "IMMEDIATE PM REQUIRED";

    remainingLife = 20;

    anomalousSignature = "Critical Condition";
    directive = "Immediate Inspection";
    pmClassification = "Immediate Action Required";

    // Fan ON
    digitalWrite(FAN_RELAY, RELAY_ON);
    fanState = true;

    // Bulb ON during normal non-fire operation
    digitalWrite(BULB_RELAY, RELAY_ON);
    bulbState = true;

    // Alarm ON
    digitalWrite(ALARM_RELAY, RELAY_ON);
    alarmState = true;

    // Breaker remains normal here
    digitalWrite(BREAKER_RELAY, RELAY_OFF);
    breakerState = false;
  }

  // =================================================
  // HIGH RISK
  // =================================================

  else if ((temperature > 50 && temperature <= 60) ||
           (humidity > 80 && humidity <= 90) ||
           (sensorVoltage >= 0.50 && sensorVoltage < 0.70))
  {
    faultStatus = "HIGH RISK";
    actionStatus = "PM REQUIRED WITHIN 7 DAYS";

    remainingLife = 50;

    anomalousSignature = "Abnormal Signature";
    directive = "Schedule Inspection";
    pmClassification = "Maintenance Within 7 Days";

    digitalWrite(FAN_RELAY, RELAY_ON);
    fanState = true;

    digitalWrite(BULB_RELAY, RELAY_ON);
    bulbState = true;

    digitalWrite(ALARM_RELAY, RELAY_OFF);
    alarmState = false;

    digitalWrite(BREAKER_RELAY, RELAY_OFF);
    breakerState = false;
  }

  // =================================================
  // MODERATE RISK
  // =================================================

  else if ((temperature > 40 && temperature <= 50) ||
           (humidity > 70 && humidity <= 80) ||
           (sensorVoltage >= 0.70 && sensorVoltage < 0.85))
  {
    faultStatus = "MODERATE RISK";
    actionStatus = "PM REQUIRED WITHIN 1 MONTH";

    remainingLife = 75;

    anomalousSignature = "Early Deviation";
    directive = "Monitor Condition";
    pmClassification = "Maintenance Within 1 Month";

    digitalWrite(FAN_RELAY, RELAY_ON);
    fanState = true;

    digitalWrite(BULB_RELAY, RELAY_ON);
    bulbState = true;

    digitalWrite(ALARM_RELAY, RELAY_OFF);
    alarmState = false;

    digitalWrite(BREAKER_RELAY, RELAY_OFF);
    breakerState = false;
  }

  // =================================================
  // HEALTHY
  // =================================================

  else
  {
    faultStatus = "HEALTHY";
    actionStatus = "NO PM REQUIRED";

    remainingLife = 100;

    anomalousSignature = "None";
    directive = "Continue Monitoring";
    pmClassification = "Normal Operation";

    digitalWrite(FAN_RELAY, RELAY_ON);
    fanState = true;

    digitalWrite(BULB_RELAY, RELAY_ON);
    bulbState = true;

    digitalWrite(ALARM_RELAY, RELAY_OFF);
    alarmState = false;

    digitalWrite(BREAKER_RELAY, RELAY_OFF);
    breakerState = false;
  }
}

// =====================================================
// Dashboard HTML
// =====================================================

String dashboard()
{
  String html = "";

  html += "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";

  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";

  // Refresh dashboard every 3 seconds
  html += "<meta http-equiv='refresh' content='3'>";

  html += "<title>Predictive Maintenance</title>";

  html += "<style>";

  html += "body{";
  html += "margin:0;";
  html += "padding:20px;";
  html += "font-family:Arial,sans-serif;";
  html += "background:#080d19;";
  html += "color:white;";
  html += "}";

  html += ".container{";
  html += "max-width:1100px;";
  html += "margin:auto;";
  html += "}";

  html += "h1{";
  html += "text-align:center;";
  html += "color:#00bcd4;";
  html += "font-size:26px;";
  html += "}";

  html += ".cards{";
  html += "display:grid;";
  html += "grid-template-columns:repeat(auto-fit,minmax(200px,1fr));";
  html += "gap:15px;";
  html += "}";

  html += ".card{";
  html += "background:#111b2e;";
  html += "padding:20px;";
  html += "border-radius:12px;";
  html += "text-align:center;";
  html += "box-shadow:0 4px 15px rgba(0,0,0,.3);";
  html += "}";

  html += ".card h3{";
  html += "font-size:13px;";
  html += "color:#8491a7;";
  html += "}";

  html += ".value{";
  html += "font-size:28px;";
  html += "font-weight:bold;";
  html += "}";

  html += ".panel{";
  html += "background:#111b2e;";
  html += "margin-top:25px;";
  html += "padding:25px;";
  html += "border-radius:12px;";
  html += "}";

  html += ".row{";
  html += "display:flex;";
  html += "justify-content:space-between;";
  html += "padding:15px 5px;";
  html += "border-bottom:1px solid #243149;";
  html += "}";

  html += ".green{color:#00d084;font-weight:bold;}";
  html += ".red{color:#ff4d4d;font-weight:bold;}";
  html += ".yellow{color:#ffc107;font-weight:bold;}";
  html += ".cyan{color:#00bcd4;font-weight:bold;}";

  html += "</style>";

  html += "</head>";

  html += "<body>";

  html += "<div class='container'>";

  // ===================================================
  // TITLE
  // ===================================================

  html += "<h1>⚙ Predictive Maintenance based Industrial Automation</h1>";

  // ===================================================
  // TOP CARDS
  // ===================================================

  html += "<div class='cards'>";

  // Temperature
  html += "<div class='card'>";
  html += "<h3>TEMPERATURE</h3>";
  html += "<div class='value'>";

  if (isnan(temperature))
    html += "ERROR";
  else
  {
    html += String(temperature, 1);
    html += " °C";
  }

  html += "</div>";
  html += "</div>";

  // Humidity
  html += "<div class='card'>";
  html += "<h3>HUMIDITY</h3>";
  html += "<div class='value'>";

  if (isnan(humidity))
    html += "ERROR";
  else
  {
    html += String(humidity, 1);
    html += " %";
  }

  html += "</div>";
  html += "</div>";

  // Voltage
  html += "<div class='card'>";
  html += "<h3>INPUT VOLTAGE</h3>";
  html += "<div class='value'>";
  html += String(sensorVoltage, 2);
  html += " V";
  html += "</div>";
  html += "</div>";

  // System Health
  html += "<div class='card'>";
  html += "<h3>SYSTEM HEALTH</h3>";
  html += "<div class='value'>";

  if (faultStatus == "HEALTHY")
    html += "<span class='green'>HEALTHY</span>";

  else if (faultStatus == "MODERATE RISK")
    html += "<span class='yellow'>MODERATE</span>";

  else
    html += "<span class='red'>CRITICAL</span>";

  html += "</div>";
  html += "</div>";

  // Remaining Life
  html += "<div class='card'>";
  html += "<h3>ESTIMATED REMAINING LIFE</h3>";
  html += "<div class='value'>";

  html += String(remainingLife);
  html += " %";

  html += "</div>";
  html += "</div>";

  html += "</div>";

  // ===================================================
  // STATUS PANEL
  // ===================================================

  html += "<div class='panel'>";

  html += "<h2>Actuator & Safety Status</h2>";

  // Fan
  html += "<div class='row'>";
  html += "<span>Actuator Fan Status</span>";

  if (fanState)
    html += "<span class='green'>ON</span>";
  else
    html += "<span class='red'>OFF</span>";

  html += "</div>";

  // Bulb
  html += "<div class='row'>";
  html += "<span>Bulb / Load Status</span>";

  if (bulbState)
    html += "<span class='green'>ON</span>";
  else
    html += "<span class='red'>OFF</span>";

  html += "</div>";

  // Alarm
  html += "<div class='row'>";
  html += "<span>Master Alarm Relay</span>";

  if (alarmState)
    html += "<span class='red'>TRIGGERED</span>";
  else
    html += "<span class='green'>NORMAL</span>";

  html += "</div>";

  // Breaker
  html += "<div class='row'>";
  html += "<span>Breaker Interlocking Trip</span>";

  if (breakerState)
    html += "<span class='red'>TRIPPED</span>";
  else
    html += "<span class='green'>NORMAL</span>";

  html += "</div>";

  // Anomaly
  html += "<div class='row'>";
  html += "<span>Anomalous Signature</span>";

  if (fireDetected)
    html += "<span class='red'>FIRE HAZARD</span>";
  else
    html += "<span class='cyan'>" + anomalousSignature + "</span>";

  html += "</div>";

  // Directive
  html += "<div class='row'>";
  html += "<span>Prescriptive Directive</span>";

  if (fireDetected)
    html += "<span class='red'>IMMEDIATE INSPECTION</span>";
  else
    html += "<span>" + directive + "</span>";

  html += "</div>";

  // PM Classification
  html += "<div class='row'>";
  html += "<span>PM Schedule Classification</span>";

  if (fireDetected)
    html += "<span class='red'>IMMEDIATE ACTION REQUIRED</span>";
  else
    html += "<span>" + pmClassification + "</span>";

  html += "</div>";

  // Fault Status
  html += "<div class='row'>";
  html += "<span>Fault Status</span>";

  if (faultStatus == "HEALTHY")
    html += "<span class='green'>HEALTHY</span>";
  else if (faultStatus == "MODERATE RISK")
    html += "<span class='yellow'>MODERATE RISK</span>";
  else
    html += "<span class='red'>" + faultStatus + "</span>";

  html += "</div>";

  // Action
  html += "<div class='row'>";
  html += "<span>Maintenance Action</span>";
  html += "<span>" + actionStatus + "</span>";
  html += "</div>";

  html += "</div>";

  html += "</div>";

  html += "</body>";

  html += "</html>";

  return html;
}

// =====================================================
// Web Server Handler
// =====================================================

void handleRoot()
{
  server.send(200, "text/html", dashboard());
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  // DHT
  dht.begin();

  // Fire sensor
  pinMode(FIRE_PIN, INPUT);

  // Relays
  pinMode(FAN_RELAY, OUTPUT);
  pinMode(BULB_RELAY, OUTPUT);
  pinMode(ALARM_RELAY, OUTPUT);
  pinMode(BREAKER_RELAY, OUTPUT);

  // Initial safe state
  digitalWrite(FAN_RELAY, RELAY_OFF);
  digitalWrite(BULB_RELAY, RELAY_OFF);
  digitalWrite(ALARM_RELAY, RELAY_OFF);
  digitalWrite(BREAKER_RELAY, RELAY_OFF);

  // Wi-Fi
  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi Connected!");

  Serial.print("Dashboard IP: ");
  Serial.println(WiFi.localIP());

  // Web server
  server.on("/", handleRoot);

  server.begin();

  Serial.println("Web Server Started");
  Serial.println("Open the IP address in your browser.");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  // Handle dashboard requests
  server.handleClient();

  // Read sensors
  readSensors();

  // Calculate condition
  calculateMaintenanceStatus();

  // Serial Monitor
  Serial.println("================================");

  Serial.print("Temperature : ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity    : ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Voltage     : ");
  Serial.print(sensorVoltage);
  Serial.println(" V");

  Serial.print("Fire        : ");

  if (fireDetected)
    Serial.println("DETECTED");
  else
    Serial.println("NORMAL");

  Serial.print("Fault Status: ");
  Serial.println(faultStatus);

  Serial.print("Action      : ");
  Serial.println(actionStatus);

  Serial.print("Remaining Life: ");
  Serial.print(remainingLife);
  Serial.println(" %");

  Serial.println("================================");

  delay(5000);
}