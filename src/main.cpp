#include "bitmap.h"
#include <ArduinoOTA.h>
#include <GxEPD.h>
#include <GxGDEW042T2/GxGDEW042T2.h>  // 4.2" b/w

#include <Fonts/FreeSerif12pt7b.h>
#include <GxIO/GxIO.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>

#ifdef D1MINI
GxIO_Class io(SPI, /*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D1*/ 5);  // arbitrary selection of D3(=0), D4(=2), selected
                                                              // for default of GxEPD_Class
GxEPD_Class display(io, /*RST=D1*/ 5, /*BUSY=D2*/ 4);         // default selection of D4(=2), D2(=4)
#endif
#ifdef NODEMCU
GxIO_Class io(SPI, /*CS=D8*/ /*SS*/ 5, /*DC=D3*/ 0, /*RST=D4*/ 12);  // arbitrary selection of D3(=0), D4(=2), selected
                                                                     // for default of GxEPD_Class
GxEPD_Class display(io, /*RST=D4*/ 12, /*BUSY=D2*/ 4);               // default selection of D4(=2), D2(=4)
#endif

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
constexpr unsigned int distanceBetweenHorizontalLines = ((maxTempInCel - minTempInCel) / (nrOfHorizontalLines + 1));

//----------- Hostname -----------
constexpr const char *hostName = "MaraXMonitor";

//----------- AP -----------
constexpr const char *ssidAP = "AutoConnectAP";
constexpr const char *passwordAP = "password";

unsigned long lastDisplayUpdate;
unsigned long displayUpdateFrequency = 1000;  //(ms)

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

//----------- MaraXSerial -----------
SoftwareSerial maraXSerial(D4, D6);  // D6 - RX on Machine , D4 - TX on Machine
const byte nrMaraXChars = 32;
char currentMaraXString[nrMaraXChars];
char currentMaraXChar;
unsigned long serialUpdateMillis = 0;
unsigned long timePointSetupFinished = 0;

/**
 * Handle the 1/0 values from the pump. If longer than X ms, the pump probably is off.
 */
unsigned long pumpStoppedTime = 0;

/**
 * Stores, how long the pump has been running. This is used, to only switch off the display after a long enough period.
 */
unsigned long pumpRunningTime = 0;
bool pumpRunning = false;
unsigned long shotTimerUpdateDelay = 0;
unsigned long pumpStartedTime = 0;
constexpr const uint8_t reedSensorPin = D0;

/**
 * The reed sensor receives 0's and 1's when the pump is running.
 * This time defines, how long 1's (pump not running) have to be received, until the shot timer is stopped.
 */
constexpr const unsigned long thresholdPumpNoLongerRunning = 1500;

/**
 * Time until the display shall be switched off, if the shot timer ran long enough.
 */
constexpr unsigned long displayOffDelay = 10000;

/**
 * As soon as the pump has been switched off, the timepoint is stored to  automatically switch off the display after
 * @see displayOffDelay.
 */
unsigned long displayOffStartTime = 0;

/**
 * Switch the display only off, if the shot timer ran at least 15 s.
 * The pump sometimes runs up to 10s, so it can happen, that the display is automatically switch off.
 */
constexpr unsigned long minShotTimerForDisplayOff = 15000;

/**
 * Indicates, whether the display has already been switched off.
 */
bool displayWentToSleep = false;

/**
 * Function to connect to a wifi. It waits until a connection is established.
 */
void connectToWifi() {
  WiFiManager wifiManager;
  wifiManager.setBreakAfterConfig(true);
  wifiManager.autoConnect(ssidAP, passwordAP);
}

/**
 * @brief Erases the entire display.
 */
void clearEntireDisplay() {
  display.eraseDisplay(false);
  display.eraseDisplay(true);
}

/**
 * @brief Helper function to get the Y position of a temperature within the graph.
 *
 * @param temperature The temperature, for which the y position shall be
 * evaluated.
 * @return The y pixel position on the display.
 */
unsigned int getYForTemp(unsigned int temperature) {
  int16_t yPosPixel = yLastGraphArea;
  if (temperature <= minTempInCel) {
  } else if (temperature >= maxTempInCel) {
    yPosPixel = y0GraphArea;
  } else {
    // Lower end - Pxl/°C + y0 offset - temp offset  (NOTE: Inverse calculation)
    yPosPixel = yLastGraphArea -
                static_cast<float>(heightGraphArea) / static_cast<float>(maxTempInCel - minTempInCel) * temperature +
                y0GraphArea - minTempInCel;
  }
  return yPosPixel;
}

/**
 * @brief Helper function to draw a pixel within the graph.
 *
 * @param timeInSeconds The time point, when that temperature was active.
 * @param temperature The temperature.
 */
void drawPixelInGraph(unsigned int timeInSeconds, unsigned int temperature) {
  const int16_t yPosPixel = getYForTemp(temperature);

  int16_t xPosPixel = x0GraphArea;
  if (timeInSeconds >= maxTimeInMin * 60) {
    xPosPixel = xLastGraphArea;
  } else {
    xPosPixel = static_cast<float>(widthGraphArea) / static_cast<float>(maxTimeInMin) *
                    (static_cast<float>(timeInSeconds) / 60.0) +
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
}

/**
 * @brief Updates the symbol indicating, whether the heating is on or not.
 */
void setHeatingStatus(bool heatingOn) {
  constexpr int16_t y0HeatingStatusBox = heightInfoBar / 4;
  constexpr int16_t heightStatusBox = heightInfoBar / 2;
  constexpr int16_t widthStatusBox = widthHeatingOnInfo / 2;
  constexpr int16_t x0HeatingStatusBox = xHeatingOnInfo + widthHeatingOnInfo / 4;
  if (heatingOn) {
    display.fillRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox, heightStatusBox, GxEPD_BLACK);
  } else {
    display.fillRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox, heightStatusBox, GxEPD_WHITE);
    display.drawRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox, heightStatusBox, GxEPD_BLACK);
  }
}

/**
 * @brief Updates the text in the hx (heat exchanger) info bar box.
 *
 * @param currentHXTemp The current hx temp received by the mara x.
 */
void setHXTemperature(unsigned int currentHXTemp) {
  constexpr int16_t x0HxTemp = xHXInfo + 2;
  constexpr int16_t y0HxTemp = yTextInfoBar + heightInfoBar / 2;
  display.fillRect(xHXInfo + 1, yTextInfoBar + 9, widthHXInfo - 2, heightInfoBar - 2 - yTextInfoBar - 9, GxEPD_WHITE);
  display.setFont(&FreeSerif12pt7b);
  display.setCursor(x0HxTemp, y0HxTemp);
  char *output = (char *)malloc(100 * sizeof(char));
  sprintf(output, "%3d", currentHXTemp);
  display.print(output);
  free(output);
  display.setFont(nullptr);
}

/**
 * @brief Updates the text in the steam info bar box.
 *
 * @param currentSteamTemp The current steam temp received by the mara x.
 * @param targetSteamTemp The target steam temp received by the mara x.
 */
void setSteamTemperature(unsigned int currentSteamTemp, unsigned int targetSteamTemp) {
  constexpr int16_t x0SteamTemp = xSteamInfo + 2;
  constexpr int16_t y0SteamTemp = yTextInfoBar + heightInfoBar / 2;
  display.fillRect(xSteamInfo + 1, yTextInfoBar + 9, widthSteamInfo - 2, heightInfoBar - 2 - yTextInfoBar - 9,
                   GxEPD_WHITE);
  display.setFont(&FreeSerif12pt7b);
  display.setCursor(x0SteamTemp, y0SteamTemp);
  char *output = (char *)malloc(100 * sizeof(char));
  sprintf(output, "%3d/%3d", currentSteamTemp, targetSteamTemp);
  display.print(output);
  free(output);
  display.setFont(nullptr);
}

/**
 * @brief Updates the text in the shot timer info bar box.
 *
 * @param timerValueInS The current shot timer value in seconds.
 */
void setShotTimer(unsigned int timerValueInS) {
  constexpr int16_t x0Timer = xShotTimer + 2;
  constexpr int16_t y0Timer = yTextInfoBar + heightInfoBar / 2;
  display.fillRect(xShotTimer + 1, yTextInfoBar + 9, widthShotTimer - 2, heightInfoBar - 2 - yTextInfoBar - 9,
                   GxEPD_WHITE);
  display.setFont(&FreeSerif12pt7b);
  display.setCursor(x0Timer, y0Timer);
  char *output = (char *)malloc(100 * sizeof(char));
  sprintf(output, "%d", timerValueInS);
  display.print(output);
  free(output);
  display.setFont(nullptr);
}

/**
 * @brief Creates the static parts of the info bar.
 */
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
  display.drawRect(xHeatingOnInfo, 0, widthHeatingOnInfo, heightInfoBar, GxEPD_BLACK);
  display.drawRect(xHXInfo, 0, widthHXInfo, heightInfoBar, GxEPD_BLACK);
  display.drawRect(xSteamInfo, 0, widthSteamInfo, heightInfoBar, GxEPD_BLACK);
  display.drawRect(xShotTimer, 0, widthShotTimer, heightInfoBar, GxEPD_BLACK);
}

/**
 * @brief Prepares the graph area and adds the labels.
 */
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
    const unsigned int yHorizontal = getYForTemp(i * distanceBetweenHorizontalLines + minTempInCel);
    display.drawLine(x0GraphArea, yHorizontal, xLastGraphArea, yHorizontal, GxEPD_BLACK);
    display.setCursor(1, yHorizontal);
    display.println(i * distanceBetweenHorizontalLines + minTempInCel);
  }

  display.drawRoundRect(x0GraphArea, y0GraphArea, widthGraphArea, heightGraphArea, 10, GxEPD_BLACK);
}

/**
 * @brief Draws a picture of mr bean while setting up.
 */
void drawRandomBootScreen() {
  switch (rand() % 3) {
    case 0: display.drawPicture(MrBeanFromBottomRight, sizeof(MrBeanFromBottomRight)); break;
    case 1: display.drawPicture(MrBeanAnticipated, sizeof(MrBeanAnticipated)); break;
    case 2: display.drawPicture(MrBeanSurprised, sizeof(MrBeanSurprised)); break;
  }
}

/**
 * The display has to be switched off properly to avoid pixel burn.
 */
void goToSleep() {
  clearEntireDisplay();
  display.powerDown();
  displayWentToSleep = true;
}

/**
 * @brief Evaluates and stores the current mara x input.
 */
void getMachineInput() {
  static byte currentIndex = 0;
  while (maraXSerial.available()) {
    serialUpdateMillis = millis();
    currentMaraXChar = maraXSerial.read();

    if (currentMaraXChar != '\n') {
      currentMaraXString[currentIndex] = currentMaraXChar;
      currentIndex++;
      if (currentIndex >= nrMaraXChars) {
        currentIndex = nrMaraXChars - 1;
      }
    } else {
      currentMaraXString[currentIndex] = '\0';
      currentIndex = 0;
      Serial.println(currentMaraXString);
    }
  }

  if (millis() - serialUpdateMillis > 5000) {
    serialUpdateMillis = millis();
    memset(currentMaraXString, 0, nrMaraXChars);
    Serial.print("Request serial update: ");
    Serial.println(maraXSerial.availableForWrite());
    maraXSerial.write(0x11);
  }
}

/**
 * @brief Prepares the OTA updates.
 */
void setupOTA() {
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress(
      [](unsigned int progress, unsigned int total) { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
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
  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Shows a boot screen and draws all boxes and labels, which are present at any time.
 */
void setupDisplay() {
  display.init(115200);  // enable diagnostic output on Serial
  display.fillScreen(GxEPD_WHITE);
  clearEntireDisplay();
  drawRandomBootScreen();
  display.fillScreen(GxEPD_WHITE);
  clearEntireDisplay();
  prepareInfoBar();
  prepareTemperatureDrawingArea();
}

/**
 * @brief Opens up the serial communication to the mara x.
 */
void setupMaraXCommunication() {
  maraXSerial.begin(9600);
  memset(currentMaraXString, 0, nrMaraXChars);
  Serial.print("Request initial serial update: ");
  Serial.println(maraXSerial.availableForWrite());
  maraXSerial.write(0x11);
}

void setup() {
  Serial.begin(115200);

  connectToWifi();
  setupOTA();
  setupDisplay();

  pinMode(reedSensorPin, INPUT_PULLDOWN_16);
  setupMaraXCommunication();
  timePointSetupFinished = millis();
}

/**
 * @brief Extracts and updates all values received from the mara x.
 *
 * @param currentTimeInSeconds The elapsed time since the tracking was started.
 * It is used in the graph as X axis.
 */
void updateMaraXValuesInDisplay(unsigned int currentTimeInSeconds) {
  unsigned int currentValueIndex = 0;
  auto result = strtok(currentMaraXString, ",");

  unsigned int currentSteamTemp = 0;
  while (result != NULL) {
    switch (currentValueIndex) {
      case 0: break;
      case 1:
        // Steam temp in C
        currentSteamTemp = atoi(result);
        drawPixelInGraph(currentTimeInSeconds, currentSteamTemp);
        break;
      case 2:
        // Target Steam temp in C
        setSteamTemperature(currentSteamTemp, atoi(result));
        break;
      case 3:
        // HX temp in C
        setHXTemperature(atoi(result));
        drawPixelInGraph(currentTimeInSeconds, atoi(result));
        break;
      case 5:
        // Heating On Off
        setHeatingStatus(atoi(result));
        break;
      default: break;
    }
    result = strtok(NULL, ",");
    currentValueIndex++;
  }
}

void handlePump() {
  // Pump running not yet recognized, input indicates that it is running
  if (!pumpRunning && !digitalRead(reedSensorPin)) {
    pumpStartedTime = millis();
    pumpRunning = true;
    Serial.println("Pump started -> Starting shot timer");
  }
  // Pump no longer running, timer running --> Wait for
  // thresholdPumpNoLongerRunning to be on the safe side.
  if (pumpRunning && digitalRead(reedSensorPin)) {
    if (pumpStoppedTime == 0) {
      pumpStoppedTime = millis();
    }
    if (millis() - pumpStoppedTime > thresholdPumpNoLongerRunning) {
      pumpRunning = false;
      pumpStoppedTime = 0;
      display.invertDisplay(false);
      Serial.println("Pump stoped -> Stoping shot timer");
      displayOffStartTime = millis();
      pumpRunningTime = millis() - pumpStartedTime;
    }
  } else {
    pumpStoppedTime = 0;
  }
}

bool hasShotBeenPulled(const unsigned long &currentMillis) {
  return (!displayWentToSleep && (pumpRunningTime > minShotTimerForDisplayOff) && (displayOffStartTime != 0) &&
          (currentMillis - displayOffStartTime) > displayOffDelay);
}

void handlShotTimer(const unsigned long &currentMillis) {
  // Set the shottimer only every second.
  if (pumpRunning && (currentMillis - shotTimerUpdateDelay) > 1000) {
    shotTimerUpdateDelay = currentMillis;
    setShotTimer((currentMillis - pumpStartedTime) / 1000);
  }
}

/**
 * @brief Writes and updates the values in the display
 */
void handleDisplayUpdate(const unsigned long &currentMillis) {
  if ((currentMillis - lastDisplayUpdate) > displayUpdateFrequency) {
    updateMaraXValuesInDisplay(static_cast<float>(currentMillis - timePointSetupFinished) / 1000.0);
    lastDisplayUpdate = currentMillis;
    display.updateWindow(0, 0, GxGDEW042T2_WIDTH, GxGDEW042T2_HEIGHT);
  }
}

void loop() {
  getMachineInput();
  handlePump();
  const auto currentMillis = millis();
  if (hasShotBeenPulled(currentMillis)) {
    goToSleep();
  } else {
    handlShotTimer(currentMillis);
    handleDisplayUpdate(currentMillis);
  }
  ArduinoOTA.handle();
}
