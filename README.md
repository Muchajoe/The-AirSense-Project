# The AirSense Project
Aaalright! This is my Final project for Harvards CS50. It is a "low budget" airquality sensor with (i think) high quality components. I use the Arduino IDE for programming. That means the main programm runs in C++. In my opinion that is very smiliar to the courses C programming language. With around 70$ (but i think if you do a bit research you can do it cheaper) you will get the following measurements.
- Temperatur (scd30)
- Temperatur (BME680)
- Humidity (BME680)
- Humidity (SCD30)
- CO2 (SCD30)
- PM1 (PMS7003)
- PM2.5 (PMS7003)
- PM10 (PMS7003)
- Pressure (BME680),
- VOC (BME680)

If you touch the sensor Pin the values will shown to you on the small 1.5" Oled screen for 30 sec (default). And you can see the values via a Webinterface by connecting direct to the AirSense Wlan (see Webinterface). It should be ok to use a 5V 1A USB power suply

#### Video Demo:
https://youtu.be/_k8XbBeKSHA

# Components
What we need? I think you can get these compononts a bit cheaper than my estimated prices.


|Compononent 	 |Usage							   |Price (estimated)
|----------------|---------------------------------|-----------------------------|
|ESP32S3 n16r8	 |That thing runs the code         |10$          |
|PMS7003		 |particel measurement sensor      |10$            |
|BME680			 |VOC and Pressure sensor          |10$|
|SCD30			 |For CO2, Humidity, Temperatur    |30$         |
|OLED 1.5"		 |To show the measurements 128x128 |7$            |
|Cables/round plate			 |For VOC and Pressure measurement |3$|
|3D Printed case |Not available now				   |-|
|TOTAL |			   |70$|

## Wirering
how to wire the components to the ESP32. The BME680 Sensor, the OLED Screen and the SCD30 are wired with a I2C bus. The ESP32 has more than one I2C bus so i use one for the Screen and the other for the sensors. The PMS7003 sensor runs serial with the TX/RX pins of the ESP32

|ESP Pin	 	 	| Device Pin
|-------------------|---------------------------------|
|19	 	            |TX PMS7003                       |
|20		 	        |RX PMS7003                       |
|1  			 	|TOUCH_PIN(for the touch detection) |
|47			 	    |OLED_SDA                           |
|21         	 	|OLED_SCL |
|4	                |Sensor SDA (BME680 and SCD30)|
|5               	|Sensor SCL (BME680 and SCD30)|
| 				|			   |

IMPORTANT dont forget that the PMS7003 sensor needs 5v for the fan! And the ESP cant serve that 5V to the 5v Pin via powering from the USB. So you need to power the ESP32 with a 5V source, that you can take the 5v from that source too for the PMS7003 sensor. The other components can be powered with the onboard 3.3V pins. The Sensors SDA and SDL should be wired to both sensors BME680 and SCD30 (paralel). The power consumption is estimated about 750mA so i would recommend a at least 1A USB Plug.


## Librarys
For the Arduino IDE you need to install the following librarys:
- #include  <esp_system.h>
- #include  <rom/ets_sys.h>
- #include  <ArduinoJson.h>
- #include  <FS.h>
- #include  <LittleFS.h>
- #include  <Wire.h>
- #include  <math.h>
- #include  <PMS.h>
- #include  <Adafruit_SCD30.h>
- #include  <Adafruit_BME680.h>
- #include  <Adafruit_GFX.h>
- #include  <Adafruit_SH110X.h>
- #include  <WiFi.h>
- #include  <WebServer.h>
- #include  <WebSocketsServer.h>

## LittleFS
This project uses the LittleFS filesystem. I have made it with the "arduino-littlefs-upload-1.5.2.vsix" (https://github.com/earlephilhower/arduino-littlefs-upload) you have to install it by using the guide from that project to your Arduino IDE and follow the steps to upload the files in the project folder.

## Install
Download the whole project folder (you need the whole folder). Run the Arduino IDE open the Main file (acutally: AirSense.cpp) close the Serial Monitor and Press Strg+Shift+P and type in "Upload to littleFS to Pico/Esp8266/Esp32". And run this. This will upload the files from the data folder (for example the index.html). If you have uploaded the files you can compile and upload the Main sketch file by clicking the compile and upload botton (arrow to the right). That will take a few minutes.

## Run it!
If you have compiled and uploaded the code and you have uploaded the additional files it should work. Maybe you have to press the reset botton or restart the esp32 manually. First the ESP32 will boot up and show you: "AirSense" on the splash screen. After a few seconds the screen changes. It should show the measurements. But this need a few seconds. CO2 measurement needs about 30 seconds. You will notice that your Display turns of after a few seconds (default 10sec). So you have to touch the "Touchpin" to activate the display (my idea is to make a little steelplate as touch sensor to the pin).

## Webinterface (Login)
You can connect your mobile device. Take you Smartphone or any other device and search for a new  Wifi named "AirSense". Connect directly to them and put in the Password "12345678". Now open a Browser and type in the IP adress that should be actually be "192.168.4.1". If you start the ESP32 you see the IP in the first row. This function is called the AcessPoint mode (AP Mode). Now you should see the Live data from your AirSense. Go to the left side to do some settings like change °C to °F. Or other things (not implemented now). A regular WIFI mode (connecting to your WIFI Router) will be implemented soon. That is called Stationary Mode (STA Mode).

## 3D Printed Case
The 3D Printed case is still Work in progress.

## Thanks
Thank you so much to David J. Malan, Doug Loyd, Brenda Anderson, Glenn Holloway, Rongxin Liu and of course to the whole CS50 Team. Thank you for making this education free available to the World.

## What it does?
- Every sensor has thier own readout interval. For Example the Particle sensor readout interval is 1 min. After starting the device the Sensor needs 30 sec to warmup and send thier data and after that it is going in a sleep mode (fan off). After 30 sec the sensor will be waked up and need again 30 sec warmup for the next readings. That means in total every 1 min a measurement
- The CO2 sensor needs about 30 sec for the first mesaurement. After that there comes new data every 10 seconds
- The touchpin needs a recalibration if the temp and humidity changes fast. So the Touchpin auto recalibrate every 3 hours
- The Sensor Data will send via a WebSocket to the Webinterface. As long as your Status is "connected" you get the newest data on your Smartphone




