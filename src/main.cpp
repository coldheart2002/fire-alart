// Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h> // Include the mDNS library
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <ArduinoJson.h> // Include the ArduinoJson library for JSON handling

// Replace with your network credentials
const char *ssid = "CTU Registrar";
const char *password = "<<<r3g!strar2023>>>";

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

// Updates DHT readings every n seconds
const long interval = 1900;

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
        background-color: #e5ebe7;
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
          <p id="tempValue">
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
              >%BUZZER_STATUS%</span
            >
          </p>
        </div>
      </div>
    </div>

    <script>
      const highTemp = 40.0;
      const tempMon = () => {
        const temptValue = document.getElementById("tempValue");
        if (parseFloat(temptValue.textContent) >= highTemp) {
          temptValue.style.color = "red";
          temptValue.style.fontWeight = "bold";
        } else {
          temptValue.style.color = "";
          temptValue.style.fontWeight = "normal";
        }
      };

      setInterval(function () {
        fetch("/data") // Updated route to fetch data from /data endpoint
          .then((response) => response.json()) // Parse response as JSON
          .then((data) => {
            document.getElementById("temperature").textContent = parseFloat(data.temperature).toFixed(2);
            document.getElementById("humidity").textContent = parseFloat(data.humidity).toFixed(2);
            document.getElementById("buzzerStatus").textContent = data.buzzer_status;
            tempMon(); // Call temperature monitoring function
          })
          .catch((error) =>
            console.error("Error fetching data:", error)
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
    return buzzerOn ? "ON" : "OFF";
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

  // Route for /data to return JSON data
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    // Create a JSON object
    StaticJsonDocument<200> doc;

    // Fill the JSON object with temperature, humidity, and buzzer status data
    doc["temperature"] = t;
    doc["humidity"] = h;
    doc["buzzer_status"] = buzzerOn ? "ON" : "OFF";

    // Serialize the JSON object to a string
    String jsonString;
    serializeJson(doc, jsonString);

    // Send the JSON response
    request->send(200, "application/json", jsonString); });

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
      t = 0.0;
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
      h = 0.0;
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
