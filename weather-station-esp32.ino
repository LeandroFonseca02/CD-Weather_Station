#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "time.h"
#include "DHT.h"

#define DHTPIN 15     // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11   // DHT 11

DHT dht(DHTPIN, DHTTYPE);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;


// Replace with your network credentials
const char* ssid = "";
const char* password = "";


float temperature;
float humidity;
char lastUpdatedTime[100];


AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long lastTime = 0;  
unsigned long timerDelay = 2000;  // send readings timer

void getTemperatureReadings(){

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}

void getFormattedDateTime(){
  struct tm timeinfo;

  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  strftime(lastUpdatedTime,100, "%d-%m-%Y \t %H:%M:%S", &timeinfo);
}

String processor(const String& var){
  getTemperatureReadings();
  if(var == "TEMPERATURE"){
    return String(temperature);
  }
  else if(var == "HUMIDITY"){
    return String(humidity);
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Weather Station</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2><i class="fas fa-cloud-sun" style="color:#99c1f1;"></i> 
    <span>Weather Station - ESP32</span> 
  </h2>
  <p>
    <i class="far fa-clock" style="color:#000000;"></i> 
    <span class="dht-labels">Last Update</span><br> 
    <span id="datetime"></span>
  </p>
    
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperature</span><br>
    <span id="temp"></span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Humidity</span><br>
    <span id="hum"></span>
    <sup class="units">%</sup>
  </p>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('temperature', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("hum").innerHTML = e.data;
 }, false);

 source.addEventListener('datetime', function(e) {
  console.log("datetime", e.data);
  document.getElementById("datetime").innerHTML = e.data;
 }, false);
 
}
</script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
  
  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  dht.begin();


  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
  });
  server.addHandler(&events);
  server.begin();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    getFormattedDateTime();
    getTemperatureReadings();
    Serial.printf("Temperature = %.2f ºC \n", temperature);
    Serial.printf("Humidity = %.2f % \n", humidity);
    Serial.println();

    // Send Events to the Web Server with the Sensor Readings
    events.send("ping",NULL,millis());
    events.send(String(temperature).c_str(),"temperature",millis());
    events.send(String(humidity).c_str(),"humidity",millis());
    events.send(String(lastUpdatedTime).c_str(),"datetime",millis());
    lastTime = millis();
  }
}
