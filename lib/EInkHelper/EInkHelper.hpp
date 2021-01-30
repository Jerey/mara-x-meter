#pragma once
#include <GxEPD.h>
#include <GxGDEW042T2/GxGDEW042T2.h>  // 4.2" b/w

#include "Arduino.h"

#include <GxIO/GxIO.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <bootscreen.h>
#include <stdint.h>

class EInkHelper {
 public:
  EInkHelper();

  /**
   * @brief Shows a boot screen and draws all boxes and labels, which are present at any time.
   */
  void setupDisplay();

  /**
   * @brief Helper function to draw a pixel within the graph.
   *
   * @param timeInSeconds The time point, when that temperature was active.
   * @param temperature The temperature.
   */
  void drawPixelInGraph(unsigned int timeInSeconds, unsigned int temperature);

  /**
   * @brief Updates the symbol indicating, whether the heating is on or not.
   */
  void setHeatingStatus(bool heatingOn);

  /**
   * @brief Updates the text in the hx (heat exchanger) info bar box.
   *
   * @param currentHXTemp The current hx temp received by the mara x.
   */
  void setHXTemperature(unsigned int currentHXTemp);

  /**
   * @brief Updates the text in the steam info bar box.
   *
   * @param currentSteamTemp The current steam temp received by the mara x.
   * @param targetSteamTemp The target steam temp received by the mara x.
   */
  void setSteamTemperature(unsigned int currentSteamTemp, unsigned int targetSteamTemp);

  /**
   * The display has to be switched off properly to avoid pixel burn.
   */
  void goToSleep();

  /**
   * To avoid pixel burn, the display will go to sleep when no longer needed.
   */
  bool isDisplayAwake();

  /**
   * @brief Updates the shot timer.
   *
   * @param pumpRunning Only update the shot timer, if the pump is still running.
   * @param currentMillis What is the current millis (time point).
   * @param pumpStartedTime When was the pump last started.
   */
  void handleShotTimer(bool pumpRunning, const unsigned long &currentMillis, const unsigned long &pumpStartedTime);

  /**
   * @brief Refresh all values in the window.
   *
   * This has to be called whenever a value, that has been set, shall be visible.
   * The display is not automatically updated, as it is rather slow.
   */
  void updateWindow();

 private:
  /**
   * @brief Erases the entire display.
   */
  void clearEntireDisplay();

  /**
   * @brief Helper function to get the Y position of a temperature within the graph.
   *
   * @param temperature The temperature, for which the y position shall be
   * evaluated.
   * @return The y pixel position on the display.
   */
  unsigned int getYForTemp(unsigned int temperature);

  /**
   * @brief Updates the text in the shot timer info bar box.
   *
   * @param timerValueInS The current shot timer value in seconds.
   */
  void setShotTimer(unsigned int timerValueInS);

  /**
   * @brief Creates the static parts of the info bar.
   */
  void prepareInfoBar();

  /**
   * @brief Prepares the graph area and adds the labels.
   */
  void prepareTemperatureDrawingArea();

  /**
   * @brief Draws a picture of mr bean while setting up.
   */
  void drawRandomBootScreen();

  GxIO_Class io;
  GxEPD_Class display;

  const int16_t x0GraphArea;
  const int16_t y0GraphArea;
  const int16_t widthGraphArea;
  const int16_t heightGraphArea;
  const int16_t xLastGraphArea;
  const int16_t yLastGraphArea;
  const unsigned int maxTimeInMin;
  const unsigned int maxTempInCel;
  const unsigned int minTempInCel;
  // Draw a line every 20° C --> 40, 60, 80, 100, 120
  const unsigned int nrOfHorizontalLines;
  const unsigned int distanceBetweenHorizontalLines;

  //----------- InfoBar -----------
  const int16_t xHeatingOnInfo;
  const int16_t widthHeatingOnInfo;
  const int16_t xHXInfo;
  const int16_t widthHXInfo;
  const int16_t xSteamInfo;
  const int16_t widthSteamInfo;
  const int16_t xShotTimer;
  const int16_t widthShotTimer;
  const int16_t heightInfoBar;
  const int16_t yTextInfoBar;
  unsigned long shotTimerUpdateDelay;

  /**
   * Indicates, whether the display has already been switched off.
   */
  bool displayWentToSleep;
};
