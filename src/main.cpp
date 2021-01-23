#include "./bitmap.h"
#include <GxEPD.h>
#include <GxGDEW042T2/GxGDEW042T2.h> // 4.2" b/w
#include GxEPD_BitmapExamples
#include <ArduinoOTA.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSerif18pt7b.h>
#include <GxIO/GxIO.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <WiFiManager.h>
#include <vector>

#ifdef D1MINI
GxIO_Class io(SPI, /*CS=D8*/ SS, /*DC=D3*/ 0,
              /*RST=D1*/ 5); // arbitrary selection of D3(=0), D4(=2), selected
                             // for default of GxEPD_Class
GxEPD_Class display(io, /*RST=D1*/ 5,
                    /*BUSY=D2*/ 4); // default selection of D4(=2), D2(=4)
#endif
#ifdef NODEMCU
GxIO_Class io(SPI, /*CS=D8*/ /*SS*/ 5, /*DC=D3*/ 0,
              /*RST=D4*/ 12); // arbitrary selection of D3(=0), D4(=2), selected
                              // for default of GxEPD_Class
GxEPD_Class display(io, /*RST=D4*/ 12,
                    /*BUSY=D2*/ 4); // default selection of D4(=2), D2(=4)
#endif
// 192.168.178.90

// 30 min auf 360 Pixel breite xFirst = 20
// -->  0,2 pixel pro Sec --> 12 Pixel pro Min

// 120°C auf 240 Pixel höhe yfirst = 60
// --> 1.3 Pixel pro °C
// temp >= 20 && <= 140 --> 120°C
constexpr int16_t x0GraphArea = 20;
constexpr int16_t y0GraphArea = 60;
constexpr int16_t widthGraphArea = 360;
constexpr int16_t heightGraphArea = 240;
constexpr int16_t xLastGraphArea = widthGraphArea + x0GraphArea;
constexpr int16_t yLastGraphArea = heightGraphArea + y0GraphArea;
constexpr unsigned int maxTimeInMin = 30;
constexpr unsigned int maxTempInCel = 140;
constexpr unsigned int minTempInCel = 20;
// Draw a line every 20° C --> 40, 60, 80, 100, 120
constexpr unsigned int nrOfHorizontalLines = 5;
constexpr unsigned int distanceBetweenHorizontalLines =
    ((maxTempInCel - minTempInCel) / (nrOfHorizontalLines + 1));

//----------- Hostname -----------
constexpr const char *hostName = "MaraXMonitor";

//----------- AP -----------
constexpr const char *ssidAP = "AutoConnectAP";
constexpr const char *passwordAP = "password";

unsigned long nonBlockingDelayStart;
unsigned long nonBlockingDelayDuration;
bool mrBeansTurn = true;

//----------- InfoBar -----------
constexpr const int16_t xHeatingOnInfo = 0;
constexpr const int16_t widthHeatingOnInfo = 50;
constexpr const int16_t xHXInfo = xHeatingOnInfo + widthHeatingOnInfo;
constexpr const int16_t widthHXInfo = 125;
constexpr const int16_t xSteamInfo = xHXInfo + widthHXInfo;
constexpr const int16_t widthSteamInfo = 125;
constexpr const int16_t xShotTimer = xSteamInfo + widthSteamInfo;
constexpr const int16_t widthShotTimer = 100;
constexpr const int16_t heightInfoBar = y0GraphArea;
constexpr const int16_t yTextInfoBar = 5;

/**
 * Function to connect to a wifi. It waits until a connection is established.
 */
void connectToWifi() {
  WiFiManager wifiManager;
  wifiManager.setBreakAfterConfig(true);
  wifiManager.autoConnect(ssidAP, passwordAP);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");

  connectToWifi();
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  display.init(115200); // enable diagnostic output on Serial

  Serial.println("setup done");
}

void clearEntireDisplay() {
  display.eraseDisplay(false);
  display.eraseDisplay(true);
}

unsigned int getYForTemp(unsigned int temperature) {
  int16_t yPosPixel = yLastGraphArea;
  if (temperature <= minTempInCel) {
  } else if (temperature >= maxTempInCel) {
    yPosPixel = y0GraphArea;
  } else {
    // Lower end - Pxl/°C + y0 offset - temp offset  (NOTE: Inverse calculation)
    yPosPixel = yLastGraphArea -
                (double)heightGraphArea /
                    (double)(maxTempInCel - minTempInCel) * temperature +
                y0GraphArea - minTempInCel;
  }
  return yPosPixel;
}

void drawLine(unsigned int timeInSeconds, unsigned int temperature) {
  const int16_t yPosPixel = getYForTemp(temperature);

  int16_t xPosPixel = x0GraphArea;
  if (timeInSeconds >= maxTimeInMin * 60) {
    xPosPixel = xLastGraphArea;
  } else {
    xPosPixel = (double)widthGraphArea / (double)maxTimeInMin *
                    ((double)timeInSeconds / (double)60) +
                x0GraphArea;
  }

  Serial.print("T: ");
  Serial.print(timeInSeconds);
  Serial.print(" -> X: ");
  Serial.print(xPosPixel);
  Serial.print(" | Temp: ");
  Serial.print(temperature);
  Serial.print(" -> Y: ");
  Serial.println(yPosPixel);

  display.writePixel(xPosPixel, yPosPixel, GxEPD_BLACK);
  if (timeInSeconds % 100 == 0) {
    // X axis complete due to the labels on Y.
    display.updateWindow(0, y0GraphArea, GxGDEW042T2_WIDTH, heightGraphArea);
  }
}

void setHeatingStatus(bool heatingOn) {
  constexpr int16_t y0HeatingStatusBox = heightInfoBar / 4;
  constexpr int16_t heightStatusBox = heightInfoBar / 2;
  constexpr int16_t widthStatusBox = widthHeatingOnInfo / 2;
  constexpr int16_t x0HeatingStatusBox =
      xHeatingOnInfo + widthHeatingOnInfo / 4;
  if (heatingOn) {
    display.fillRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox,
                     heightStatusBox, GxEPD_BLACK);
  } else {
    display.drawRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox,
                     heightStatusBox, GxEPD_BLACK);
  }
  display.updateWindow(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox,
                       heightStatusBox);
}

void setHXTemperature(unsigned int currentHXTemp, unsigned int targetHXTemp) {
  constexpr int16_t x0HxTemp = xHXInfo + 2;
  constexpr int16_t y0HxTemp = yTextInfoBar + heightInfoBar / 2;
  display.setCursor(x0HxTemp, y0HxTemp);
  char *output = (char *)malloc(100 * sizeof(char));
  sprintf(output, "%3d/%3d", currentHXTemp, targetHXTemp);
  display.print(output);
  display.updateWindow(xHXInfo, 0, widthHXInfo, heightInfoBar);
  free(output);
}

void prepareInfoBar() {
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(xHeatingOnInfo + 2, yTextInfoBar);
  display.println("Heating");
  display.setCursor(xHXInfo + 2, yTextInfoBar);
  display.println("HX");
  display.setCursor(xSteamInfo + 2, yTextInfoBar);
  display.println("Steam");
  display.setCursor(xShotTimer + 2, yTextInfoBar);
  display.println("Timer");
  display.drawRect(xHeatingOnInfo, 0, widthHeatingOnInfo, heightInfoBar,
                   GxEPD_BLACK);
  display.drawRect(xHXInfo, 0, widthHXInfo, heightInfoBar, GxEPD_BLACK);
  display.drawRect(xSteamInfo, 0, widthSteamInfo, heightInfoBar, GxEPD_BLACK);
  display.drawRect(xShotTimer, 0, widthShotTimer, heightInfoBar, GxEPD_BLACK);
  display.updateWindow(0, 0, GxGDEW042T2_WIDTH, heightInfoBar);

  setHeatingStatus(rand() % 2);
  setHXTemperature(rand() % 140, rand() % 140);
}

void prepareTemperatureDrawingArea() {
  // Text:
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(1, y0GraphArea);
  display.println("T/C");
  display.println(maxTempInCel);
  display.setCursor(1, yLastGraphArea - 10);
  display.println(minTempInCel);
  display.update();

  for (unsigned int i = 1; i <= nrOfHorizontalLines; ++i) {
    const unsigned int yHorizontal =
        getYForTemp(i * distanceBetweenHorizontalLines + minTempInCel);
    display.drawLine(x0GraphArea, yHorizontal, xLastGraphArea, yHorizontal,
                     GxEPD_BLACK);
    display.setCursor(1, yHorizontal);
    display.println(i * distanceBetweenHorizontalLines + minTempInCel);
  }

  display.drawRoundRect(x0GraphArea, y0GraphArea, widthGraphArea,
                        heightGraphArea, 10, GxEPD_BLACK);
  display.updateWindow(0, y0GraphArea, GxGDEW042T2_WIDTH, heightGraphArea);
}

void drawGraph() {
  prepareTemperatureDrawingArea();

  // 30 min == 1800 Sekunden
  for (int i = 0; i < 1800; i++) {
    drawLine(i, i / 15 + 20);
    delay(10);
  }

  // Delete fully.
  clearEntireDisplay();
}

void drawRandomBootScreen() {
  switch (rand() % 3) {
  case 0:
    display.drawPicture(MrBeanFromBottomRight, sizeof(MrBeanFromBottomRight));
    break;
  case 1:
    display.drawPicture(MrBeanAnticipated, sizeof(MrBeanAnticipated));
    break;
  case 2:
    display.drawPicture(MrBeanSurprised, sizeof(MrBeanSurprised));
    break;
  }
}

void goToSleep() {
  clearEntireDisplay();
  display.powerDown();
  nonBlockingDelayStart = millis();
  nonBlockingDelayDuration = 10000;
}

void loop() {
  if ((millis() - nonBlockingDelayStart) > nonBlockingDelayDuration) {
    if (mrBeansTurn) {
      display.fillScreen(GxEPD_WHITE);
      clearEntireDisplay();
      drawRandomBootScreen();
      nonBlockingDelayStart = millis();
      nonBlockingDelayDuration = 1000;
      mrBeansTurn = false;
    } else {
      display.fillScreen(GxEPD_WHITE);
      clearEntireDisplay();
      prepareInfoBar();
      drawGraph();
      nonBlockingDelayStart = millis();
      nonBlockingDelayDuration = 10000;
      mrBeansTurn = true;
    }

    // goToSleep();
  }
  ArduinoOTA.handle();
}
