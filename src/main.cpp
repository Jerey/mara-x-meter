#include <Arduino.h>
#include <ArduinoOTA.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>

#include <MaraXDisplay.hpp>
#include <MaraXProtocol.hpp>
#include <PowerMonitor.hpp>
#include <ShotTimer.hpp>
#include <TemperatureHistory.hpp>

// ----------------------------------------------------------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------------------------------------------------------

namespace {

constexpr const char *kHostName = "MaraXMonitor";  // mDNS name for OTA, see upload_port in platformio.ini.
constexpr const char *kApName = "MaraXMonitor";    // Access point opened, when no known wifi is reachable.
constexpr const char *kApPassword = "maraxmeter";

// Display pins on the D1 mini (SPI: SCK = D5, MOSI = D7).
constexpr uint8_t kDisplayCsPin = 15;   // D8
constexpr uint8_t kDisplayDcPin = 0;    // D3
constexpr uint8_t kDisplayRstPin = 5;   // D1
constexpr uint8_t kDisplayBusyPin = 4;  // D2

// Serial data from the Mara X on D4 (GPIO2). Only the TX line of the machine is needed, the machine never listens.
// Mind, that GPIO2 is a boot pin: if the line is low while the ESP resets (machine off, loose contact), the ESP does
// not boot at all and prints an endless "Fatal exception (0)" at 74880 baud. See the pull-up in the readme.
constexpr uint8_t kMaraXRxPin = 2;
constexpr uint32_t kMaraXBaud = 9600;
constexpr int kMaraXRxBuffer = 256;  // Enough for several lines while a panel refresh blocks the loop.

// Reed sensor on the vibration pump, D0 (GPIO16). The input reads LOW while the pump runs (with gaps, as the reed
// follows the vibration), so every LOW level counts as pump activity. GPIO16 cannot raise interrupts and is polled.
// A different pin (-DREED_PIN=n) is interrupt driven and catches the exact start of a shot even while the display
// refresh blocks the loop.
#ifndef REED_PIN
#define REED_PIN 16
#endif
constexpr uint8_t kReedPin = REED_PIN;
constexpr bool kReedUsesInterrupt = kReedPin != 16;
constexpr uint32_t kPumpStopThresholdMs = 1500;  // Silence after which the pump counts as stopped.
constexpr uint32_t kMinShotMs = 15000;  // Shorter pump runs are the machine refreshing the HX water or a backflush.

// The machine is ready for a shot, when the heat exchanger sat in this band and moved at most kReadyHxSpread degrees
// for kReadyForS seconds. The band depends on the machine and the taste, tune it.
constexpr uint8_t kReadyHxMin = 92;
constexpr uint8_t kReadyHxMax = 100;
constexpr uint8_t kReadyHxSpread = 2;
constexpr uint16_t kReadyForS = 60;

constexpr uint32_t kRenderIntervalMs = 1000;
constexpr uint32_t kFullRefreshIntervalMs = 10UL * 60UL * 1000UL;  // Clears e-paper ghosting now and then.

// Power loss detection, see PowerMonitor. ESP.getVcc() blocks the interrupts for about 150 us and corrupts any Mara
// X byte arriving at that moment. The supply is therefore sampled right after a complete line (the machine pauses
// for almost half a second then) and only falls back to a fixed interval while no data arrives.
constexpr uint16_t kBrownoutMillivolt = 500;
constexpr uint8_t kBrownoutConfirmations = 5;
constexpr uint32_t kPowerRecoveryMs = 2000;
// Right after switching on, the supercapacitor is still charging through its resistor and the regulator works at
// its limit, so the supply sags with every wifi burst. No power loss detection in that time, the capacitor could
// not bridge one anyway.
constexpr uint32_t kPowerGraceMs = 60UL * 1000UL;
// The capacitor cannot keep the D1 mini alive this long after a real power cut. If the meter is still running,
// the drop was something else (a weak supply, a noisy reading) and the meter starts over.
constexpr uint32_t kShutdownGiveUpMs = 120UL * 1000UL;
constexpr uint32_t kVccSampleIntervalMs = 100;
constexpr uint32_t kMaraXSilenceMs = 1000;

// Written into the RTC memory (survives a restart, not a power cycle) before the meter restarts itself after a
// power dip, so the next start can tell that apart from a crash.
constexpr uint32_t kRtcPowerDipMagic = 0x4D415241;  // "MARA"

constexpr uint32_t kWifiConnectTimeoutS = 15;
constexpr uint32_t kWifiPortalTimeoutMs = 180UL * 1000UL;
// Same rate as the boot ROM of the ESP8266, so one monitor shows the boot messages and this log.
constexpr uint32_t kSerialBaud = 74880;

// ----------------------------------------------------------------------------------------------------------------
// State
// ----------------------------------------------------------------------------------------------------------------

MaraXDisplay display(kDisplayCsPin, kDisplayDcPin, kDisplayRstPin, kDisplayBusyPin);
SoftwareSerial maraXSerial(kMaraXRxPin, -1);
LineReader lineReader;
ShotTimer shotTimer(kPumpStopThresholdMs, kMinShotMs);
TemperatureHistory history;
PowerMonitor powerMonitor(kBrownoutMillivolt, kBrownoutConfirmations, kPowerRecoveryMs);
WiFiManager wifiManager;

uint32_t sessionStartMs = 0;
uint32_t lastRenderMs = 0;
uint32_t lastFullRefreshMs = 0;
bool shuttingDown = false;

bool hasReading = false;
MaraXReading lastReading;
uint32_t maraXBytesReceived = 0;
uint32_t maraXLinesAccepted = 0;
uint32_t maraXLinesRejected = 0;
uint32_t lastMaraXReportMs = 0;
uint32_t lastMaraXLineMs = 0;

uint32_t lastVccSampleMs = 0;
bool vccSampleDue = false;
uint32_t shutdownStartedMs = 0;

const char *restartNote = nullptr;  // shown on the dashboard, when the meter did not start from a power on

DisplayModel::Wifi wifiState = DisplayModel::Wifi::Connecting;
char wifiIp[16] = { 0 };
bool otaStarted = false;
bool portalGaveUp = false;
uint32_t portalStartedMs = 0;

// Written by the reed interrupt: the first and the last pulse since the loop looked the last time.
volatile uint32_t reedFirstEdgeMs = 0;
volatile uint32_t reedLastEdgeMs = 0;
volatile bool reedEdgePending = false;

uint32_t elapsedSeconds(uint32_t nowMs) { return (nowMs - sessionStartMs) / 1000; }

// ----------------------------------------------------------------------------------------------------------------
// Reed sensor and shot timer
// ----------------------------------------------------------------------------------------------------------------

IRAM_ATTR void onReedEdge() {
  const uint32_t nowMs = millis();
  if (!reedEdgePending) {
    reedFirstEdgeMs = nowMs;
    reedEdgePending = true;
  }
  reedLastEdgeMs = nowMs;
}

void setupReedSensor() {
  if (kReedPin == 16) {
    pinMode(kReedPin, INPUT_PULLDOWN_16);
  } else {
    pinMode(kReedPin, INPUT_PULLUP);
  }
  if (kReedUsesInterrupt) {
    attachInterrupt(digitalPinToInterrupt(kReedPin), onReedEdge, FALLING);
  }
}

/**
 * @brief Passes the pump activity seen on the reed sensor to the shot timer.
 *
 * With the interrupt both the first and the last pulse since the last loop are reported, so the shot starts at the
 * right moment even when a display refresh kept the loop busy for a second.
 */
void pollReedSensor(uint32_t nowMs) {
  if (kReedUsesInterrupt) {
    if (reedEdgePending) {
      noInterrupts();
      const uint32_t firstMs = reedFirstEdgeMs;
      const uint32_t lastMs = reedLastEdgeMs;
      reedEdgePending = false;
      interrupts();
      shotTimer.noteActivity(firstMs);
      shotTimer.noteActivity(lastMs);
    }
    return;
  }
  if (digitalRead(kReedPin) == LOW) {
    shotTimer.noteActivity(nowMs);
  }
}

void handleShotEvents(uint32_t nowMs) {
  shotTimer.update(nowMs);
  if (shotTimer.takeStartedEvent()) {
    Serial.println(F("Pump started, shot timer running"));
  }
  if (shotTimer.takeStoppedEvent()) {
    Serial.printf("Pump stopped after %lu ms\n", static_cast<unsigned long>(shotTimer.lastRunMs()));
  }
  if (shotTimer.takeShotFinishedEvent()) {
    history.addShot(elapsedSeconds(shotTimer.lastShotStartedAtMs()),
                    static_cast<uint16_t>(shotTimer.lastShotMs() / 1000));
  }
}

// ----------------------------------------------------------------------------------------------------------------
// Mara X serial data
// ----------------------------------------------------------------------------------------------------------------

void readMaraX(uint32_t nowMs) {
  while (maraXSerial.available()) {
    ++maraXBytesReceived;
    if (!lineReader.push(static_cast<char>(maraXSerial.read()))) {
      continue;
    }
    Serial.println(lineReader.line());
    MaraXReading reading;
    if (!parseMaraXLine(lineReader.line(), reading)) {
      ++maraXLinesRejected;
      Serial.println(F("  (not a Mara X message)"));
      continue;
    }
    ++maraXLinesAccepted;
    lastMaraXLineMs = nowMs;
    vccSampleDue = true;
    hasReading = true;
    lastReading = reading;
    history.add(elapsedSeconds(nowMs), reading.hxTemperature, reading.steamTemperature);
    if (reading.hasPumpField && reading.pumpOn) {
      shotTimer.noteActivity(nowMs);
    }
  }
  // Some statistics now and then: a wiring or baud rate problem shows as bytes without lines, data loss as rejected
  // lines.
  if (nowMs - lastMaraXReportMs >= 10000) {
    lastMaraXReportMs = nowMs;
    Serial.printf("Mara X stats: %lu bytes, %lu lines ok, %lu rejected, %u bytes heap free\n",
                  static_cast<unsigned long>(maraXBytesReceived), static_cast<unsigned long>(maraXLinesAccepted),
                  static_cast<unsigned long>(maraXLinesRejected), ESP.getFreeHeap());
  }
}

// ----------------------------------------------------------------------------------------------------------------
// Wifi and OTA
// ----------------------------------------------------------------------------------------------------------------

/**
 * @brief Setup OTA
 *
 * Setup OTA with the given hostname. The hostname is used to identify the ESP in the network.
 */
void setupOTA() {
  ArduinoOTA.setHostname(kHostName);
  ArduinoOTA.onStart([]() { Serial.println(F("OTA start")); });
  ArduinoOTA.onEnd([]() { Serial.println(F("\nOTA end")); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA %u%%\r", total ? (progress * 100U / total) : 0U);
  });
  ArduinoOTA.onError([](ota_error_t error) { Serial.printf("OTA error %u\n", static_cast<unsigned>(error)); });
  ArduinoOTA.begin();
  otaStarted = true;
  Serial.print(F("OTA ready at "));
  Serial.println(WiFi.localIP());
}

/**
 * @brief Connect to the wifi using WiFiManager
 *
 * Typically, it will automatically reconnect to the previously configured wifi. In case it can't connect, it will
 * start an access point with the given kApName. The user can then connect to this access point and configure the
 * wifi. The access point does not block, so the dashboard works in the meantime and the meter also works without
 * any wifi at all.
 */
void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(kHostName);
  wifiManager.setHostname(kHostName);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConnectTimeout(kWifiConnectTimeoutS);
  wifiManager.setWiFiAutoReconnect(true);
  if (!wifiManager.autoConnect(kApName, kApPassword)) {
    portalStartedMs = millis();
    Serial.printf("No wifi, access point %s is open for %lu s\n", kApName,
                  static_cast<unsigned long>(kWifiPortalTimeoutMs / 1000));
  }
}

void handleWifi(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaStarted) {
      setupOTA();
      strncpy(wifiIp, WiFi.localIP().toString().c_str(), sizeof(wifiIp) - 1);
    }
    ArduinoOTA.handle();
    wifiState = DisplayModel::Wifi::Connected;
    return;
  }
  wifiIp[0] = '\0';
  if (wifiManager.getConfigPortalActive()) {
    wifiManager.process();
    if (!portalGaveUp && nowMs - portalStartedMs > kWifiPortalTimeoutMs) {
      portalGaveUp = true;
      wifiManager.stopConfigPortal();
      Serial.println(F("Closing the access point, running without wifi"));
    }
    wifiState = portalGaveUp ? DisplayModel::Wifi::Off : DisplayModel::Wifi::Portal;
    return;
  }
  wifiState = portalGaveUp ? DisplayModel::Wifi::Off : DisplayModel::Wifi::Connecting;
}

// ----------------------------------------------------------------------------------------------------------------
// Power loss
// ----------------------------------------------------------------------------------------------------------------

/// @brief Samples the supply, when it is safe to do so, and tells, whether the power was cut.
bool powerIsFailing(uint32_t nowMs) {
  if (nowMs - sessionStartMs < kPowerGraceMs) {
    return false;
  }
  const bool silent = nowMs - lastMaraXLineMs > kMaraXSilenceMs;
  if (!vccSampleDue && !(silent && nowMs - lastVccSampleMs >= kVccSampleIntervalMs)) {
    return false;
  }
  vccSampleDue = false;
  lastVccSampleMs = nowMs;
  return powerMonitor.noteSupply(ESP.getVcc());
}

/// @brief Restarts the meter and leaves a mark, so the next start knows it came from a power dip.
void restartAfterPowerDip(const __FlashStringHelper *why) {
  Serial.println(why);
  Serial.flush();
  uint32_t magic = kRtcPowerDipMagic;
  ESP.rtcUserMemoryWrite(0, &magic, sizeof(magic));
  ESP.restart();
}

/**
 * @brief Restarts the meter, if the power comes back after the shutdown (outlet switched on again quickly).
 *
 * Also restarts, when the meter is still alive long after the shutdown: then the power never went away and the
 * display must not stay dark.
 */
void handlePowerRecovery(uint32_t nowMs) {
  if (nowMs - shutdownStartedMs >= kShutdownGiveUpMs) {
    restartAfterPowerDip(F("Still running long after the shutdown, the power is there, restarting"));
  }
  if (nowMs - lastVccSampleMs < kVccSampleIntervalMs) {
    return;
  }
  lastVccSampleMs = nowMs;
  if (powerMonitor.noteSupplyAfterShutdown(ESP.getVcc(), nowMs)) {
    restartAfterPowerDip(F("Power is back, restarting"));
  }
}

// ----------------------------------------------------------------------------------------------------------------
// Rendering
// ----------------------------------------------------------------------------------------------------------------

DisplayModel buildModel(uint32_t nowMs) {
  DisplayModel model;
  model.hasReading = hasReading;
  model.reading = lastReading;
  model.shotRunning = shotTimer.running();
  model.hasLastShot = shotTimer.hasLastShot();
  model.shotSeconds = static_cast<uint16_t>(shotTimer.elapsedMs(nowMs) / 1000);
  model.readyForShot = hasReading && history.hxSteady(kReadyForS, kReadyHxMin, kReadyHxMax, kReadyHxSpread);
  model.wifi = wifiState;
  strncpy(model.ip, wifiIp, sizeof(model.ip) - 1);
  model.apName = kApName;
  model.restartNote = restartNote;
  return model;
}

void render(uint32_t nowMs) {
  // The full refresh flickers, so not while a shot runs and someone is looking at the timer.
  const bool fullDue = !shotTimer.running() && nowMs - lastFullRefreshMs >= kFullRefreshIntervalMs;
  const MaraXDisplay::Refresh refresh = display.render(buildModel(nowMs), history, elapsedSeconds(nowMs), fullDue);
  if (refresh == MaraXDisplay::Refresh::Full) {
    lastFullRefreshMs = nowMs;
  }
}

}  // namespace

// cppcheck-suppress unknownMacro
ADC_MODE(ADC_VCC)  // the ADC measures the supply instead of A0, see PowerMonitor

/**
 * @brief Finds out, why the meter starts, and keeps a short note for the dashboard, if it was not a power on.
 *
 * A restart after a power dip is marked in the RTC memory by handlePowerRecovery(). Everything else comes from the
 * reset reason of the ESP: a crash, a watchdog or a software restart (OTA update).
 */
void noteRestartReason() {
  uint32_t magic = 0;
  ESP.rtcUserMemoryRead(0, &magic, sizeof(magic));
  if (magic == kRtcPowerDipMagic) {
    magic = 0;
    ESP.rtcUserMemoryWrite(0, &magic, sizeof(magic));
    restartNote = "POWER DIP";
  } else {
    switch (ESP.getResetInfoPtr()->reason) {
      case REASON_DEFAULT_RST:
      case REASON_EXT_SYS_RST: restartNote = nullptr; break;
      case REASON_WDT_RST: restartNote = "HW WATCHDOG"; break;
      case REASON_EXCEPTION_RST: restartNote = "EXCEPTION"; break;
      case REASON_SOFT_WDT_RST: restartNote = "SW WATCHDOG"; break;
      case REASON_SOFT_RESTART: restartNote = "RESTART"; break;
      default: restartNote = "UNKNOWN"; break;
    }
  }
  Serial.print(F("Reset reason: "));
  Serial.println(restartNote ? restartNote : "power on");
  Serial.println(ESP.getResetInfo());
}

// ----------------------------------------------------------------------------------------------------------------
// Arduino entry points
// ----------------------------------------------------------------------------------------------------------------

void setup() {
  Serial.begin(kSerialBaud);
  Serial.println();
  Serial.println(F("Mara X meter starting"));
  noteRestartReason();
  Serial.flush();  // the display driver re-opens Serial for its diagnostics, which would cut pending output

  display.begin(kSerialBaud);
  display.showBootScreen(ESP.random());
  Serial.println(F("Display ready"));

  setupReedSensor();
  maraXSerial.begin(kMaraXBaud, SWSERIAL_8N1, kMaraXRxPin, -1, false, kMaraXRxBuffer);
  Serial.println(F("Listening to the Mara X"));

  setupWifi();

  sessionStartMs = millis();
  lastFullRefreshMs = sessionStartMs;
  render(sessionStartMs);
  Serial.println(F("Setup finished"));
}

void loop() {
  const uint32_t nowMs = millis();
  if (shuttingDown) {
    handlePowerRecovery(nowMs);
    delay(50);
    return;
  }

  pollReedSensor(nowMs);
  readMaraX(nowMs);
  handleShotEvents(nowMs);

  if (powerIsFailing(nowMs)) {
    Serial.println(F("Power loss detected, showing the off screen"));
    WiFi.mode(WIFI_OFF);  // every milliampere saved from the supercap buys time for the two full refreshes
    display.shutdown(ESP.random());
    shuttingDown = true;
    shutdownStartedMs = millis();
    return;
  }

  handleWifi(nowMs);

  if (nowMs - lastRenderMs >= kRenderIntervalMs) {
    lastRenderMs = nowMs;
    render(nowMs);
  }
}
