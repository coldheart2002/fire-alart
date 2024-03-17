// Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h> // Include the mDNS library
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// Replace with your network credentials
const char *ssid = "PLDTHOMEFIBR947b0";
const char *password = "PLDTWIFIur2ez";

#define DHTPIN 5     // Digital pin D1 connected to the DHT sensor
#define BUZZER_PIN 4 // Digital pin D2 connected to the buzzer

// Uncomment the type of sensor in use:
#define DHTTYPE DHT11 // DHT 11
// #define DHTTYPE    DHT22     // DHT 22 (AM2302)
// #define DHTTYPE    DHT21     // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

// current temperature & humidity, updated in loop()
float t = 0.0;
float h = 0.0;

// Buzzer status
bool buzzerOn = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0; // will store last time DHT was updated
unsigned long buzzerOffTime = 0;  // will store last time buzzer was turned off

// Updates DHT readings every 10 seconds
const long interval = 5000;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <script
      src="https://kit.fontawesome.com/ac3db00adf.js"
      crossorigin="anonymous"
    ></script>
    <style>
      * {
        margin: 0;
        padding: 0;
        box-sizing: border-box;
        font-family: tahoma;
      }
      i {
        font-size: 2.5rem;
        margin-right: 20px;
      }
      .main {
        height: 100vh;
        text-align: center;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        background-color: lightgray;
      }
      h2 {
        font-size: 3rem;
        margin-bottom: 30px;
      }
      .container {
        box-shadow: 2px 4px 6px;
        margin-bottom: 20px;
        border-radius: 10px;
        display: flex;
        flex-direction: column;
        align-items: center;
        padding: 30px;
        background-color: hsla(140, 12%, 91%, 0.778);
      }
      .readings {
        display: flex;
        flex-direction: column;
      }

      .label {
        font-size: 1.5rem;
        display: flex;
        align-items: center;
        justify-content: center;
        text-transform: uppercase;
        padding: 10px;
      }
      .value {
        font-size: 2.5rem;
      }
      .units {
        font-size: 1.5rem;
      }
    </style>
  </head>
  <body>
    <div class="main">
      <h2>ESP8266 Fire Alarm System</h2>
      <div class="readings">
        <div class="container">
          <p class="label">
            <i class="fa-solid fa-temperature-low" style="color: #059e8a"></i>
            <span class="dht-labels">Temperature</span>
          </p>
          <p>
            <span id="temperature" class="value">%TEMPERATURE%</span
            ><sup class="units">&deg;C</sup>
          </p>
        </div>

        <div class="container">
          <p class="label">
            <i class="fas fa-tint" style="color: #00add6"></i>
            <span class="dht-labels">Humidity</span>
          </p>
          <p>
            <span id="humidity" class="value">%HUMIDITY%</span>
            <sup class="units">&percnt;</sup>
          </p>
        </div>
        <div class="container">
          <p class="label">
            <i class="fas fa-volume-high" style="color: #d60000"></i>
            <span class="dht-labels">Buzzer status</span>
          </p>
          <p>
            <span
              id="buzzerStatus"
              class="value"
              style="text-transform: uppercase"
              >%BUZZER_STATUS%</span
            >
          </p>
        </div>
      </div>
    </div>

    <script>
      const highTemp = 40.0;
      const tempMon = () => {
        const temptValue = document.getElementById("temperature");
        if (parseFloat(temptValue.textContent) >= highTemp) {
          temptValue.parentElement.style.color = "red";
          temptValue.parentElement.style.fontWeight = "bold";
          temptValue.parentElement.parentElement.style.backgroundColor =
            "#FFC107";
        } else {
          temptValue.parentElement.style.color = "";
          temptValue.style.fontWeight = "normal";
          temptValue.parentElement.parentElement.style.backgroundColor =
            "hsla(140, 12%, 91%, 0.778)";
        }
      };

      setInterval(function () {
        fetch("/temperature")
          .then((response) => response.text())
          .then((data) => {
            document.getElementById("temperature").textContent = data;
          })
          .catch((error) =>
            console.error("Error fetching temperature:", error)
          );
      }, 2000);

      setInterval(function () {
        fetch("/humidity")
          .then((response) => response.text())
          .then((data) => {
            document.getElementById("humidity").textContent = data;
          })
          .catch((error) => console.error("Error fetching humidity:", error));
      }, 2000);

      setInterval(function () {
        fetch("/buzzerStatus")
          .then((response) => response.text())
          .then((data) => {
            document.getElementById("buzzerStatus").textContent = data;
          })
          .catch((error) =>
            console.error("Error fetching buzzer status:", error)
          );
      }, 2000);

      tempMon();
    </script>
  </body>
</html>
)rawliteral";

// Replaces placeholder with DHT and buzzer status values
String processor(const String &var)
{
  // Serial.println(var);
  if (var == "TEMPERATURE")
  {
    return String(t);
  }
  else if (var == "HUMIDITY")
  {
    return String(h);
  }
  else if (var == "BUZZER_STATUS")
  {
    return buzzerOn ? "On" : "Off";
  }
  return String();
}

void setup()
{
  // Configure buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Serial port for debugging purposes
  Serial.begin(115200);
  dht.begin();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println(".");
  }

  // Start mDNS service
  if (!MDNS.begin("esp8266"))
  {
    Serial.println("Error starting mDNS");
  }
  Serial.println("mDNS responder started");

  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html, processor); });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", String(t).c_str()); });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", String(h).c_str()); });
  server.on("/buzzerStatus", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", buzzerOn ? "On" : "Off"); });

  // Start server
  server.begin();
}

void loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    // save the last time you updated the DHT values
    previousMillis = currentMillis;
    // Read temperature as Celsius (the default)
    float newT = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    // float newT = dht.readTemperature(true);
    // if temperature read failed, don't change t value
    if (isnan(newT))
    {
      Serial.println("Failed to read from DHT sensor!");
    }
    else
    {
      t = newT;
      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.println(" °C");
    }
    // Read Humidity
    float newH = dht.readHumidity();
    // if humidity read failed, don't change h value
    if (isnan(newH))
    {
      Serial.println("Failed to read from DHT sensor!");
    }
    else
    {
      h = newH;
      Serial.print("Humidity: ");
      Serial.print(h);
      Serial.println(" %");
    }

    // Check temperature for buzzer control
    if (t >= 40 && !buzzerOn)
    {
      buzzerOn = true;
      digitalWrite(BUZZER_PIN, HIGH);
    }
    else
    {
      buzzerOn = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}
