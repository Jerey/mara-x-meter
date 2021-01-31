#include "Arduino.h"
#include <EInkHelper.hpp>
#include <Fonts/FreeSerif12pt7b.h>
#include <bootscreen.h>
#include <stdint.h>

#ifdef D1MINI
constexpr uint8_t csPin = SS;
constexpr uint8_t rstPin = 5;
#endif
#ifdef NODEMCU
constexpr uint8_t csPin = 5;
constexpr uint8_t rstPin = 12;
#endif
EInkHelper::EInkHelper()
    : io(SPI, /*CS=D8*/ csPin, /*DC=D3*/ 0, /*RST=D1*/ rstPin),
      display(io, /*RST=D1*/ 5, /*BUSY=D2*/ 4),
      x0GraphArea{ 20 },
      y0GraphArea{ 60 },
      widthGraphArea{ 360 },
      heightGraphArea{ 240 },
      xLastGraphArea(widthGraphArea + x0GraphArea),
      yLastGraphArea(heightGraphArea + y0GraphArea),
      maxTimeInMin{ 30 },
      maxTempInCel{ 140 },
      minTempInCel{ 20 },
      nrOfHorizontalLines{ 5 },
      distanceBetweenHorizontalLines{ (maxTempInCel - minTempInCel) / (nrOfHorizontalLines + 1) },
      xHeatingOnInfo{ 0 },
      widthHeatingOnInfo{ 50 },
      xHXInfo(xHeatingOnInfo + widthHeatingOnInfo),
      widthHXInfo{ 125 },
      xSteamInfo(xHXInfo + widthHXInfo),
      widthSteamInfo{ 125 },
      xShotTimer(xSteamInfo + widthSteamInfo),
      widthShotTimer{ 100 },
      heightInfoBar{ y0GraphArea },
      yTextInfoBar{ 5 },
      shotTimerUpdateDelay{ 0 },
      displayWentToSleep{ false } {}

void EInkHelper::clearEntireDisplay() {
  display.eraseDisplay(false);
  display.eraseDisplay(true);
}
unsigned int EInkHelper::getYForTemp(unsigned int temperature) {
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
void EInkHelper::drawPixelInGraph(unsigned int timeInSeconds, unsigned int temperature) {
  const int16_t yPosPixel = getYForTemp(temperature);

  int16_t xPosPixel = x0GraphArea;
  if (timeInSeconds >= maxTimeInMin * 60) {
    xPosPixel = xLastGraphArea;
  } else {
    xPosPixel = static_cast<float>(widthGraphArea) / static_cast<float>(maxTimeInMin) *
                    (static_cast<float>(timeInSeconds) / 60.0) +
                x0GraphArea;
  }

  display.writePixel(xPosPixel, yPosPixel, GxEPD_BLACK);
}
void EInkHelper::setHeatingStatus(bool heatingOn) {
  int16_t y0HeatingStatusBox = heightInfoBar / 4;
  int16_t heightStatusBox = heightInfoBar / 2;
  int16_t widthStatusBox = widthHeatingOnInfo / 2;
  int16_t x0HeatingStatusBox = xHeatingOnInfo + widthHeatingOnInfo / 4;
  if (heatingOn) {
    display.fillRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox, heightStatusBox, GxEPD_BLACK);
  } else {
    display.fillRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox, heightStatusBox, GxEPD_WHITE);
    display.drawRect(x0HeatingStatusBox, y0HeatingStatusBox, widthStatusBox, heightStatusBox, GxEPD_BLACK);
  }
}
void EInkHelper::setHXTemperature(unsigned int currentHXTemp) {
  int16_t x0HxTemp = xHXInfo + 2;
  int16_t y0HxTemp = yTextInfoBar + heightInfoBar / 2;
  display.fillRect(xHXInfo + 1, yTextInfoBar + 9, widthHXInfo - 2, heightInfoBar - 2 - yTextInfoBar - 9, GxEPD_WHITE);
  display.setFont(&FreeSerif12pt7b);
  display.setCursor(x0HxTemp, y0HxTemp);
  char *output = (char *)malloc(100 * sizeof(char));
  sprintf(output, "%3d", currentHXTemp);
  display.print(output);
  free(output);
  display.setFont(nullptr);
}
void EInkHelper::setSteamTemperature(unsigned int currentSteamTemp, unsigned int targetSteamTemp) {
  int16_t x0SteamTemp = xSteamInfo + 2;
  int16_t y0SteamTemp = yTextInfoBar + heightInfoBar / 2;
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
void EInkHelper::setShotTimer(unsigned int timerValueInS) {
  int16_t x0Timer = xShotTimer + 2;
  int16_t y0Timer = yTextInfoBar + heightInfoBar / 2;
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
void EInkHelper::prepareInfoBar() {
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
void EInkHelper::prepareTemperatureDrawingArea() {
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
void EInkHelper::drawRandomBootScreen() {
  switch (rand() % 3) {
    case 0: display.drawPicture(MrBeanFromBottomRight, sizeof(MrBeanFromBottomRight)); break;
    case 1: display.drawPicture(MrBeanAnticipated, sizeof(MrBeanAnticipated)); break;
    case 2: display.drawPicture(MrBeanSurprised, sizeof(MrBeanSurprised)); break;
  }
}
void EInkHelper::goToSleep() {
  clearEntireDisplay();
  display.powerDown();
  displayWentToSleep = true;
}
bool EInkHelper::isDisplayAwake() { return !displayWentToSleep; }
void EInkHelper::setupDisplay() {
  display.init(115200);  // enable diagnostic output on Serial
  display.fillScreen(GxEPD_WHITE);
  clearEntireDisplay();
  drawRandomBootScreen();
  display.fillScreen(GxEPD_WHITE);
  clearEntireDisplay();
  prepareInfoBar();
  prepareTemperatureDrawingArea();
}
void EInkHelper::handleShotTimer(bool pumpRunning, const unsigned long &currentMillis,
                                 const unsigned long &pumpStartedTime) {
  // Set the shottimer only every second.
  if (pumpRunning && (currentMillis - shotTimerUpdateDelay) > 1000) {
    shotTimerUpdateDelay = currentMillis;
    setShotTimer((currentMillis - pumpStartedTime) / 1000);
  }
}
void EInkHelper::updateWindow() { display.updateWindow(0, 0, GxGDEW042T2_WIDTH, GxGDEW042T2_HEIGHT); }
