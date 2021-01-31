
#include <ArduinoOTA.h>

#include <EInkHelper.hpp>
#include <SoftwareSerial.h>
#include <WiFiManager.h>

//----------- Hostname -----------
constexpr const char *hostName = "MaraXMonitor";

//----------- AP -----------
constexpr const char *ssidAP = "AutoConnectAP";
constexpr const char *passwordAP = "password";

//----------- EInk Diagramm Helper -----------
EInkHelper eInkHelper;
unsigned long lastDisplayUpdate;
constexpr unsigned long displayUpdateFrequency = 1000;  //(ms)

//----------- MaraXSerial -----------
SoftwareSerial maraXSerial(D4, D6);  // D6 - RX on Machine , D4 - TX on Machine
constexpr byte nrMaraXChars = 32;
char currentMaraXString[nrMaraXChars];
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

unsigned long pumpStartedTime = 0;
constexpr uint8_t reedSensorPin = D0;

/**
 * The reed sensor receives 0's and 1's when the pump is running.
 * This time defines, how long 1's (pump not running) have to be received, until the shot timer is stopped.
 */
constexpr unsigned long thresholdPumpNoLongerRunning = 1500;

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
 * Function to connect to a wifi. It waits until a connection is established.
 */
void connectToWifi() {
  WiFiManager wifiManager;
  wifiManager.setBreakAfterConfig(true);
  wifiManager.autoConnect(ssidAP, passwordAP);
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
 * @brief Opens up the serial communication to the mara x.
 */
void setupMaraXCommunication() {
  maraXSerial.begin(9600);
  memset(currentMaraXString, 0, nrMaraXChars);
}

void setup() {
  Serial.begin(115200);

  connectToWifi();
  setupOTA();
  eInkHelper.setupDisplay();

  pinMode(reedSensorPin, INPUT_PULLDOWN_16);
  setupMaraXCommunication();
  timePointSetupFinished = millis();
}

/**
 * @brief Evaluates and stores the current mara x input.
 */
void readMaraXSerial() {
  byte currentIndex = 0;
  while (maraXSerial.available()) {
    char currentMaraXChar = maraXSerial.read();

    if (currentMaraXChar != '\n') {  // Stream of chars finished
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
        eInkHelper.drawPixelInGraph(currentTimeInSeconds, currentSteamTemp);
        break;
      case 2:
        // Target Steam temp in C
        eInkHelper.setSteamTemperature(currentSteamTemp, atoi(result));
        break;
      case 3: {
        // HX temp in C
        auto hxTemperature = atoi(result);
        eInkHelper.setHXTemperature(hxTemperature);
        eInkHelper.drawPixelInGraph(currentTimeInSeconds, hxTemperature);
      } break;
      case 5:
        // Heating On Off
        eInkHelper.setHeatingStatus(atoi(result));
        break;
      default: break;
    }
    result = strtok(NULL, ",");
    currentValueIndex++;
  }
}

void handlePump() {
  // Pump running not yet recognized, input indicates that it is running
  const auto currentMillis = millis();
  if (!pumpRunning && !digitalRead(reedSensorPin)) {
    pumpStartedTime = currentMillis;
    pumpRunning = true;
    Serial.println("Pump started -> Starting shot timer");
  }

  // The reed sensor signals 0's and 1's while the pump is running. If we have stored, that the pump is running, but the
  // reed sensor indicates that it no longer is running, we have to wait a certain threshold to ensure, that the pump
  // really stopped.
  if (pumpRunning && digitalRead(reedSensorPin)) {
    if (pumpStoppedTime == 0) {
      pumpStoppedTime = currentMillis;
    }
    if (currentMillis - pumpStoppedTime > thresholdPumpNoLongerRunning) {
      pumpRunning = false;
      pumpStoppedTime = 0;
      Serial.println("Pump stoped -> Stoping shot timer");
      displayOffStartTime = currentMillis;
      pumpRunningTime = currentMillis - pumpStartedTime;
    }
  } else {
    pumpStoppedTime = 0;
  }
}

bool hasShotBeenPulled(const unsigned long &currentMillis) {
  return (eInkHelper.isDisplayAwake() && (pumpRunningTime > minShotTimerForDisplayOff) && (displayOffStartTime != 0) &&
          (currentMillis - displayOffStartTime) > displayOffDelay);
}
/**
 * @brief Writes and updates the values in the display
 */
void handleDisplayUpdate(const unsigned long &currentMillis) {
  if ((currentMillis - lastDisplayUpdate) > displayUpdateFrequency) {
    updateMaraXValuesInDisplay(static_cast<float>(currentMillis - timePointSetupFinished) / 1000.0);
    lastDisplayUpdate = currentMillis;
    eInkHelper.updateWindow();
  }
}

void loop() {
  readMaraXSerial();
  handlePump();
  const auto currentMillis = millis();
  if (hasShotBeenPulled(currentMillis)) {
    eInkHelper.goToSleep();
  } else {
    eInkHelper.handleShotTimer(pumpRunning, currentMillis, pumpStartedTime);
    handleDisplayUpdate(currentMillis);
  }
  ArduinoOTA.handle();
}
