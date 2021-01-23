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
#include <Fonts/FreeSerif9pt7b.h>
#include <GxIO/GxIO.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <SoftwareSerial.h>
#include <Timer.h>
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

SoftwareSerial mySerial(D4, D6); // D6 - RX on Machine , D4 - TX on Machine
Timer updateGraphTimer;
unsigned long serialUpdateMillis = 0;
unsigned long timeSinceSetupFinished = 0;
unsigned long pumpStartedTime = 0;
unsigned long pumpStoppedTime =
    0; //!< Handle the 1/0 values from the pump. If longer than 500 ms, the pump
       //!< probably is off.
bool pumpRunning = false;
unsigned long shotTimerUpdateDelay = 0;

/**
 * Function to connect to a wifi. It waits until a connection is established.
 */
void connectToWifi() {
  WiFiManager wifiManager;
  wifiManager.setBreakAfterConfig(true);
  wifiManager.autoConnect(ssidAP, passwordAP);
}

const byte numChars = 32;
char receivedChars[numChars];

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
  // X axis complete due to the labels on Y.
  display.updateWindow(0, y0GraphArea, GxGDEW042T2_WIDTH, heightGraphArea);
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
    display.fillRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox,
                     heightStatusBox, GxEPD_WHITE);
    display.drawRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox,
                     heightStatusBox, GxEPD_BLACK);
  }
  display.updateWindow(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox,
                       heightStatusBox);
}

void setHXTemperature(unsigned int currentHXTemp) {
  constexpr int16_t x0HxTemp = xHXInfo + 2;
  constexpr int16_t y0HxTemp = yTextInfoBar + heightInfoBar / 2;
  display.fillRect(xHXInfo + 1, yTextInfoBar + 9, widthHXInfo - 2,
                   heightInfoBar - 2 - yTextInfoBar - 9, GxEPD_WHITE);
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x0HxTemp, y0HxTemp);
  char *output = (char *)malloc(100 * sizeof(char));
  sprintf(output, "%3d", currentHXTemp);
  display.print(output);
  display.updateWindow(xHXInfo, 0, widthHXInfo, heightInfoBar);
  free(output);
  display.setFont(nullptr);
}

void setSteamTemperature(unsigned int currentSteamTemp,
                         unsigned int targetSteamTemp) {
  constexpr int16_t x0SteamTemp = xSteamInfo + 2;
  constexpr int16_t y0SteamTemp = yTextInfoBar + heightInfoBar / 2;
  display.fillRect(xSteamInfo + 1, yTextInfoBar + 9, widthSteamInfo - 2,
                   heightInfoBar - 2 - yTextInfoBar - 9, GxEPD_WHITE);
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x0SteamTemp, y0SteamTemp);
  char *output = (char *)malloc(100 * sizeof(char));
  sprintf(output, "%3d/%3d", currentSteamTemp, targetSteamTemp);
  display.print(output);
  display.updateWindow(xSteamInfo, 0, widthSteamInfo, heightInfoBar);
  free(output);
  display.setFont(nullptr);
}

void setShotTimer(unsigned int timerValueInS) {
  constexpr int16_t x0Timer = xShotTimer + 2;
  constexpr int16_t y0Timer = yTextInfoBar + heightInfoBar / 2;
  display.fillRect(xShotTimer + 1, yTextInfoBar + 9, widthShotTimer - 2,
                   heightInfoBar - 2 - yTextInfoBar - 9, GxEPD_WHITE);
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x0Timer, y0Timer);
  char *output = (char *)malloc(100 * sizeof(char));
  sprintf(output, "%d", timerValueInS);
  display.print(output);
  display.updateWindow(xShotTimer, 0, widthShotTimer, heightInfoBar);
  free(output);
  display.setFont(nullptr);
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

  // setHeatingStatus(rand() % 2);
  // setHXTemperature(rand() % 140, rand() % 140);
  // setSteamTemperature(rand() % 140, rand() % 140);
  // setShotTimer(rand() % 1800);
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
  // prepareTemperatureDrawingArea();

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

char rc;
char endMarker = '\n';
static byte ndx = 0;
void getMachineInput() {
  while (mySerial.available()) {
    serialUpdateMillis = millis();
    rc = mySerial.read();

    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    } else {
      receivedChars[ndx] = '\0';
      ndx = 0;
      Serial.println(receivedChars);
    }
  }

  if (millis() - serialUpdateMillis > 5000) {
    serialUpdateMillis = millis();
    memset(receivedChars, 0, numChars);
    Serial.print("Request serial update: ");
    Serial.println(mySerial.availableForWrite());
    mySerial.write(0x11);
  }
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
  display.fillScreen(GxEPD_WHITE);
  clearEntireDisplay();
  // drawRandomBootScreen();
  // display.fillScreen(GxEPD_WHITE);
  // clearEntireDisplay();
  prepareInfoBar();
  prepareTemperatureDrawingArea();

  // TODO: Pump Pin -> Add docu or so.
  pinMode(D0, INPUT_PULLDOWN_16);

  mySerial.begin(9600);
  memset(receivedChars, 0, numChars);
  Serial.print("Request initial serial update: ");
  Serial.println(mySerial.availableForWrite());
  mySerial.write(0x11);
  timeSinceSetupFinished = millis();
}
void updateValuesInDisplay(unsigned int currentTimeInSeconds) {
  unsigned int currentValue = 0;
  auto result = strtok(receivedChars, ",");

  unsigned int currentSteamTemp = 0;
  while (result != NULL) {
    switch (currentValue) {
    case 0:
      break;
    case 1:
      // Steam temp in C
      currentSteamTemp = atoi(result);
      break;
    case 2:
      // Target Steam temp in C
      setSteamTemperature(currentSteamTemp, atoi(result));
      break;
    case 3:
      // HX temp in C
      setHXTemperature(atoi(result));
      drawLine(currentTimeInSeconds, atoi(result));
      break;
    case 5:
      // Heating On Off
      setHeatingStatus(atoi(result));
      break;
    default:
      break;
    }
    result = strtok(NULL, ",");
    currentValue++;
  }
}

void handlePump() {
  // Pump running, timer not yet
  if (!pumpRunning && !digitalRead(D0)) {
    pumpStartedTime = millis();
    pumpRunning = true;
    Serial.println("Pump started -> Starting shot timer");
  }
  // Pump not running, timer running --> Wait for 500ms to be on the safe side.
  if (pumpRunning && digitalRead(D0)) {
    if (pumpStoppedTime == 0) {
      pumpStoppedTime = millis();
    }
    if (millis() - pumpStoppedTime > 1500) {
      pumpRunning = false;
      pumpStoppedTime = 0;
      display.invertDisplay(false);
      Serial.println("Pump stoped -> Stoping shot timer");
    }
  } else {
    pumpStoppedTime = 0;
  }
}

void loop() {
  getMachineInput();
  if ((millis() - nonBlockingDelayStart) > nonBlockingDelayDuration) {
    Serial.println((double)(millis() - timeSinceSetupFinished) / (double)1000);
    updateValuesInDisplay((double)(millis() - timeSinceSetupFinished) /
                          (double)1000);
    nonBlockingDelayStart = millis();
    nonBlockingDelayDuration = 1000;
  }

  handlePump();
  if (pumpRunning && (millis() - shotTimerUpdateDelay) > 1000) {
    shotTimerUpdateDelay = millis();
    setShotTimer((millis() - pumpStartedTime) / 1000);
  }
  ArduinoOTA.handle();
}
