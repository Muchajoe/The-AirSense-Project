//TODO
// ++ Warnungen und schwellenwerte einstellbar
// ++ AP Webinterface
// -- STA Webinterface
// ++ Watchdog
//    Speicher
// ++ factoryreset
//    Config via Webinterface
#include <esp_system.h>
#include <rom/ets_sys.h>

//#include <nvs_flash.h>
//#include <nvs.h>
#include <ArduinoJson.h>
#include <strings.h>
#include <FS.h>
#include <LittleFS.h>

#include <Wire.h>
#include <math.h>

#include <PMS.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BME680.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

hw_timer_t *timer = NULL;     // Watchdog

const char* apSSID = "AirSense";
const char* apPassword = "12345678";
//const int PMS_RX =
//const int PMS_TX =
const int SCD_SDA = 4;        // SDA-Pin für SCD30 WEIß
const int SCD_SCL = 5;        // SCL-Pin für SCD30
const int OLED_SDA = 47;      // SDA-Pin für display
const int OLED_SCL = 21;      // SCL-Pin für display
const int SCREEN_WIDTH = 128; // OLED display width, in pixels
const int SCREEN_HEIGHT = 128;// OLED display height, in pixels
const int OLED_RESET = -1;    // can set an oled reset pin if desired
const int TOUCH_PIN = 1;      // Set Touchpin
const int wdtTimeout = 6000;  // Watchdog time in ms to trigger the watchdog

const int numSensors = 8;  // number of sending mesaurements  ----------------------------------------------------------------
const int arraySize = 49;  // number of mesaurements          ----------------------------------------------------------------
float sensorValues[numSensors][arraySize] = {0};
const char* mesaurementNames[numSensors] = {"Temperatur", "Luftfeuchtigkeit", "Druck", "Licht", "CO2", "Lautstärke", "Vibration", "Spannung"};

const double a = 17.62;   // Temperaturkoeffizient
const double b = 243.12;  // Skalenkonstante
const double Rv = 461.5;  // spezifische Gaskonstante für Wasserdampf in J/(kg*K)
double alpha = 0;         // steampressure-Logarythm-coeffizient SCD
double alphaBme = 0;      // steampressure-Logarythm-coeffizient BME
double dewPoint = 0;      // dewpoint calculated from SCD Mesaruements
double dewPointBme = 0;   // dewpoint calculated from BME Mesaruements

float temp = 0;         // sensor value
float tempf = 0;        // sensor value
float hum = 0;          // sensor value
float co2 = 0;          // sensor value
float bmeTemp = 0;      // sensor value
float bmeTempf = 0;     // sensor value
float bmeHum = 0;       // sensor value
float bmePress = 0;     // sensor value
float bmeVoc = 0;       // sensor value
float version = 0.38;   // version number

int pm10 = 0;                       // sensor value
int pm25 = 0;                       // sensor value
int pm100 = 0;                      // sensor value
int resetDelay = 10000;             // über Webinterface anpassbar dispaly timeout USER
int calibrationInterval = 3600000;  // Touchcalibrationintervall in ms (3600000 = 3 Stunden)
int printInterval = 5000;
int mInterval = 10;                 // Sensor interval PMS 7003 USER
int BmeInterval = 10000;            // Sensor Interval BME 680
int contrast = 1;                   // Display brightness USER
int touchSense = 100;               // set the touch sensitivity. Less is more sensitiv
int touchTreshhold = 500000;        // Treshhold Touchpin (autocalibrate)
int co2Limit = 1500;                // Co2 Limit zum anzeigen eines "!" beim überschreiten USER
int wsLiveInterval = 1000;          // every seconds send JSON via Websocket
int PMSInterval = 30000;            // the interval of PMS Sleep
int chartInterval = 1000;        // the interval for sending new data to the webchart (1800000 ms = 30 min)
int uptime = 0;                     // uptime in min

unsigned char tempUnit = 'C';       // C = Celcius // F = Fahrenheit USER

uint counter = 0; // loop counter
volatile bool touchDetected = true;
unsigned long lastTouchTime = 0;
unsigned long lastCalibrationTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastBmeInterval = 0;
unsigned long lastWsLiveSend = 0;
unsigned long lastPMSread = 0;
unsigned long lastChartInerval = 0;
unsigned long currentMillis = 0;

Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET, 1000000, 100000);
Adafruit_SCD30 scd30;
Adafruit_BME680 bme;

PMS pms(Serial2);
PMS::DATA data;

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

extern TwoWire Wire;
extern TwoWire Wire1;

void webRoutes();
void factory_reset();
void webSocketSendLive();
void webSocketSendChart();
void touchCalibrate();
void SerialPrint();
void displayOn();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

void ARDUINO_ISR_ATTR resetModule() { // Watchdog
  ets_printf("wd reboot\n");
  esp_restart();
  }

void IRAM_ATTR onTouch() { // Touchinterrupt
  touchDetected = true;  // set the true flag if a touch is detected
  //int touchValue = touchRead(TOUCH_PIN);
  Serial.print(touchRead(TOUCH_PIN));
  Serial.println(" - Touch-Sensor triggered");
  lastTouchTime = currentMillis;
  }

void setup() {    // ------------------------------------------ SETUP STARTS HERE ------------------------------------------
  Serial.begin(115200);                            // Start serial monitor for USB communication to the pc
  Serial2.begin(9600);                             // Start serial connection to the PMS7003 sensor

  WiFi.softAP(apSSID, apPassword);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("IP-adress: ");
  Serial.println(apIP);



   // start LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Fehler beim Starten von LittleFS");
    return;
  }

  webRoutes();

  // Webserver starten
  server.begin();
  Serial.println("Webserver gestartet!");

  touchCalibrate();

  timer = timerBegin(1000000);                     // Watchdog timer 1Mhz resolution
  timerAttachInterrupt(timer, &resetModule);       // Watchdog attach callback
  timerAlarm(timer, wdtTimeout * 1000, false, 0);  // Watchdog set time in us

  Wire.begin(SCD_SDA, SCD_SCL);
  scd30.begin(0x61, &Wire);
  bme.begin(0x77, &Wire);

  Wire1.begin(OLED_SDA, OLED_SCL);
  display.begin(0x3c, true);

  // I2C Kommunikation initialisieren
  //if (!scd30.begin()) {
  //  Serial.println("SCD30-Sensor konnte nicht gefunden werden. Bitte überprüfen Sie die Verbindung.");
  //  while (1) delay(10);
  //}

  Serial.print("SCD30 Sensor gestartet! - ");
  scd30.setMeasurementInterval(mInterval);
  Serial.print(scd30.getMeasurementInterval());

  Serial.println(" BME Sensor gestartet!");
  bme.setTemperatureOversampling(BME680_OS_4X);
  bme.setHumidityOversampling(BME680_OS_4X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  //bme.setGasHeater(320, 150); // gas heater 320°C for 150 ms

  pms.passiveMode();

  display.setRotation(0); // set the rotation
  display.clearDisplay();
  display.display();// clear display
  display.cp437(true);
  display.setContrast (contrast);
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println(F("AirSense"));
  display.setTextSize(1);
  display.println(F("Booting..."));
  display.print(F("V. "));
  display.println(version);
  display.print(F("IP: "));
  display.println(apIP);

  display.display();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  server.begin();

  delay(3000);
  }

void loop() {     // ------------------------------------------ MAIN LOOP STARTS HERE ------------------------------------------
  timerWrite(timer, 0);  //reset timer (feed watchdog)
  currentMillis = millis();

  if (scd30.dataReady()) {  // Read the scd 30 mesaurements
    if (!scd30.read()) {
      Serial.println("Failed to read SCD30-Data!");
      return;
      }

    temp = scd30.temperature;
    tempf = scd30.temperature * 1.8 + 32;
    hum = scd30.relative_humidity;
    co2 = scd30.CO2;

    alpha = (a * temp) / (b + temp) + log(hum / 100.0);
    dewPoint = (b * alpha) / (a - alpha);
    }

  //if (bme.performReading() && millis() - lastBmeInterval > BmeInterval) { // bme.performReading
    if (bme.beginReading() && currentMillis - lastBmeInterval > BmeInterval) { // Read the BME 680 mesaurements
        if (!bme.endReading()) {
          Serial.println("Failed to read BME680-Data!");
          return;
    }

    bmeTemp = bme.readTemperature();
    bmeTempf = bme.readTemperature() * 1.8 + 32;
    bmeHum = bme.readHumidity();
    bmePress = bme.readPressure() / 100.0;
    bmeVoc = bme.gas_resistance / 1000.0; // VOC Resistance in KOHM
    alphaBme = (a * bmeTemp) / (b + bmeTemp) + log(bmeHum / 100.0);
    dewPointBme = (b * alphaBme) / (a - alphaBme);
    lastBmeInterval = currentMillis;
  }

  if (currentMillis - lastPMSread > PMSInterval) { // Touch pin recalibration
      pms.wakeUp();
      if (currentMillis - lastPMSread > PMSInterval + 30000){
      pms.requestRead();
        if (pms.readUntil(data)){                 // Read the PMS7003 mesaurements
        pm10 = data.PM_AE_UG_1_0;
        pm25 = data.PM_AE_UG_2_5;
        pm100 = data.PM_AE_UG_10_0;
        pms.sleep();
        lastPMSread = currentMillis;
          }
      }
  }

    if (currentMillis - lastPrintTime > printInterval) { // serialprint value in interval
    SerialPrint();
    lastPrintTime = currentMillis;
    }

  if (touchDetected == true) {                    // Display ON
    displayOn();
    }

  if (currentMillis - lastTouchTime > resetDelay && touchDetected == true && resetDelay != 0) { // Display OFF (Display timeout)
      Serial.println("Reset after 5sec");
      touchDetected = false;
      display.clearDisplay();
      display.display();
      //lastTouchTime = millis(); is in the interrupt function
    }

  if (currentMillis - lastCalibrationTime > calibrationInterval) { // Touch pin recalibration
      touchCalibrate();
      lastCalibrationTime = currentMillis;
    }

  if (currentMillis - lastWsLiveSend > wsLiveInterval) { // Send live data every interval
      webSocketSendLive();
      lastWsLiveSend = currentMillis;
    }

  if (currentMillis - lastChartInerval > chartInterval) { // Touch pin recalibration
      webSocketSendChart();
      lastChartInerval = currentMillis;
    }


  webSocket.loop();
  server.handleClient();
}

void webRoutes() {

  // serve Index.html
  server.on("/", HTTP_GET, []() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
      server.send(404, "text/plain", "index not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  });

    server.on("/reset", HTTP_GET, []() {
    factory_reset();
  });

        server.on("/static/chart.js", HTTP_GET, []() {
    File file = LittleFS.open("/static/chart.js", "r");
    if (!file) {
      server.send(404, "text/plain", "chartjs not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  });

      server.on("/static/bootstrap.bundle.min.js", HTTP_GET, []() {
    File file = LittleFS.open("/static/bootstrap.bundle.min.js", "r");
    if (!file) {
      server.send(404, "text/plain", "bootstrapjs not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  });

    server.on("/static/bootstrap.min.css", HTTP_GET, []() {
    File file = LittleFS.open("/static/bootstrap.min.css", "r");
    if (!file) {
      server.send(404, "text/plain", "css not found");
      return;
    }
    server.streamFile(file, "text/css");
    file.close();
  });

    server.on("/static/favicon.png", HTTP_GET, []() {
    File file = LittleFS.open("/static/favicon.png", "r");
    if (!file) {
      server.send(404, "text/plain", "favicon not found");
      return;
    }
    server.streamFile(file, "image/x-icon");
    file.close();
  });

    server.on("/", HTTP_POST, []() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
      server.send(404, "text/plain", "index.html not found");
      return;
    }
    if (server.hasArg("tempUnit")) {
      String userInput = server.arg("tempUnit");
      tempUnit = char(userInput[0]);
    }
    if (server.hasArg("displayTimeout")) {
    String userInput2 = server.arg("displayTimeout");
    //Serial.print(userInput2);
      if (userInput2 == "0"){
        resetDelay = 0;
        touchDetected = true;
        //Serial.print(resetDelay);
      }
        if (userInput2 == "1"){
        resetDelay = 10000;
        //Serial.print(resetDelay);
        touchDetected = true;
      }
    }
    server.streamFile(file, "text/html");
    file.close();
  });
}

void factory_reset() { // factory reset
  //nvs_flash_erase(); // erase all user data
  esp_restart();
  }

void webSocketSendChart() {
  Serial.print("WebsocketChart Send");
}

void webSocketSendLive() {
        StaticJsonDocument<800> jsonDoc;
        jsonDoc["temperatureScd"] = String(temp, 2).toFloat();
        jsonDoc["temperatureScdF"] = String(tempf, 2).toFloat();
        jsonDoc["temperatureBme"] = String(bmeTemp, 2).toFloat();
        jsonDoc["temperatureBmeF"] = String(bmeTempf, 2).toFloat();
        jsonDoc["humidityScd"] = String(hum, 2).toFloat();
        jsonDoc["humidityBme"] = String(bmeHum, 2).toFloat();
        jsonDoc["dewpointScd"] = String(dewPoint, 2).toFloat();
        jsonDoc["dewpointBme"] = String(dewPointBme, 2).toFloat();
        jsonDoc["bmePress"] = String(bmePress, 2).toFloat();
        jsonDoc["bmeVoc"] = String(bmeVoc, 2).toFloat();
        jsonDoc["co2"] = String(co2, 2).toFloat();
        jsonDoc["pm10"] = pm10;
        jsonDoc["pm25"] = pm25;
        jsonDoc["pm100"] = pm100;
        jsonDoc["uptime"] = uptime;
        jsonDoc["tempUnit"] = tempUnit;
        jsonDoc["mesaurementNames"] = mesaurementNames[1];

        // convert JSON to String
        String jsonData;
        serializeJson(jsonDoc, jsonData);

        // Sent JSON-Data to all Websocket clients
        webSocket.broadcastTXT(jsonData);
}

void touchCalibrate() { // calibrate the Touchpin
  int touchValue = touchRead(TOUCH_PIN); // Baseline
  touchTreshhold = touchValue + touchSense;
  touchAttachInterrupt(TOUCH_PIN, onTouch, touchTreshhold); // Touchinterrupt
  Serial.print("Touch Calibrated - Baseline: ");
  Serial.print(touchValue);
  Serial.print(" New treshold: ");
  Serial.println(touchTreshhold);
  }

void SerialPrint() {          // prints the values serial
    Serial.print(co2);         // CO2 in ppm
    Serial.print(", ");

    Serial.print(temp);       // Temperatur in °C
    Serial.print(" - ");
    Serial.print(tempf);      // Temperatur in °F
    Serial.print(", ");

    Serial.print(hum);        // humidity in % rel
    Serial.print(", ");

    Serial.print(dewPoint);   // dewpoint in °C
    Serial.print(" // ");

    Serial.print(bmeTemp);
    Serial.print(", ");

    Serial.print(bmeHum);
    Serial.print(", ");

    Serial.print(bmeVoc);
    Serial.print(", ");

    Serial.print(bmePress);
    Serial.print(", //");

    Serial.print("Tre: ");
    //touchValue = touchRead(TOUCH_PIN); // read the touchpin value for adjusting reasons
    Serial.print(touchRead(TOUCH_PIN));
    Serial.print(", - ");

    uptime = millis() / 60000.0;
    Serial.print(uptime);
    Serial.print(", ");

    Serial.print(", --");
    Serial.print(pm10);
    Serial.print(", ");
    Serial.print(pm25);
    Serial.print(", ");
    Serial.print(pm100);

    Serial.println("");
    counter ++;

    delay(500);
}

void displayOn() {    //displays the value on the oled
      display.clearDisplay();
      display.setContrast (contrast);
      display.setTextColor(SH110X_WHITE);

      display.setCursor(0,0);
      display.setTextSize(3);
      if (tempUnit == 'C'){display.print(temp);}
      if (tempUnit == 'F'){display.print(tempf);}
      display.setTextSize(1);
      display.write(0xF8);
      display.println((char)tempUnit);

      display.setCursor(0,30);
      display.setTextSize(3);
      display.print(hum);
      display.setTextSize(1);
      display.write(0x25);
      display.println("");

      display.setCursor(0,58);
      display.setTextSize(2);
      display.print(co2);
      display.setTextSize(1);
      display.print("ppm");
      display.setTextSize(2);
      if (co2 > co2Limit){display.print("!");}

      display.setCursor(0,78);
      display.setTextSize(2);
      display.print(bmePress);
      display.setTextSize(1);
      display.print("hPa");

      display.setCursor(0,95);
      display.setTextSize(2);
      display.print((int)bmeVoc);
      display.setTextSize(1);
      display.print("voc");

      display.setCursor(0,114);
      display.setTextSize(2);
      display.print(pm10);
      display.setTextSize(1);
      display.print("pm1");
      display.setTextSize(2);
      display.print(pm25);
      display.setTextSize(1);
      display.print("pm25");
      display.display();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.print("New client connected: ");
        Serial.println(ip);
        webSocket.sendTXT(num, "connected!"); // send welcome message
      }
      break;
    case WStype_DISCONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.print("Client disconnected: ");
        Serial.println(ip);
      }
      break;
    case WStype_TEXT:
      {
        Serial.printf("message from client: %s\n", payload);
        // Hier kannst du auf Nachrichten vom Client reagieren
      }
      break;
  }
}
