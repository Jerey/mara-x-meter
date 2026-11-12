// Renders the dashboard with synthetic data into PBM files, so the layout can be checked without the hardware.
// Build and run with tools/preview/build.sh.
#include <Adafruit_GFX.h>
#include <DashboardPainter.hpp>
#include <LogoScreen.hpp>
#include <TemperatureHistory.hpp>

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr int kWidth = kPanelWidth;
constexpr int kHeight = kPanelHeight;

/// @brief Smooth 0..1 ramp for x in [0, 1].
double smoothstep(double x) {
  x = x < 0 ? 0 : (x > 1 ? 1 : x);
  return x * x * (3 - 2 * x);
}

/// @brief The dip of the heat exchanger after a shot: a short spike, then down by 13 degrees, back within six minutes.
double shotDip(double sinceShotS) {
  if (sinceShotS < 0) {
    return 0;
  }
  if (sinceShotS < 20) {
    return 4 * (1 - sinceShotS / 20);
  }
  if (sinceShotS < 80) {
    return -13 * smoothstep((sinceShotS - 20) / 60);
  }
  return -13 * (1 - smoothstep((sinceShotS - 80) / 300));
}

/// What a Mara X does from cold, traced from a real morning: the steam boiler shoots past 140 degrees while it heats
/// the heat exchanger and then drifts down to its target, the heat exchanger sits still for a few minutes and then
/// climbs in an S curve to just below 100.
void fillHistory(TemperatureHistory &history, uint32_t untilS, uint32_t shotAtS, uint32_t secondShotAtS) {
  for (uint32_t t = 0; t <= untilS; t += TemperatureHistory::kSampleIntervalS) {
    const double seconds = static_cast<double>(t);
    double steam = 22 + 8 * smoothstep(seconds / 120) + 115 * smoothstep((seconds - 120) / 660);
    if (seconds > 780) {
      steam -= 27 * smoothstep((seconds - 780) / 1320);
    }
    if (seconds > 2100) {
      steam += 2 * sin((seconds - 2100) / 90.0);
    }
    double hx = 22 + 75 * smoothstep((seconds - 240) / 960);
    if (seconds > 1200) {
      hx += 1.5 * sin(seconds / 150.0);
    }
    if (shotAtS > 0) {
      hx += shotDip(seconds - static_cast<double>(shotAtS));
    }
    if (secondShotAtS > 0) {
      hx += shotDip(seconds - static_cast<double>(secondShotAtS));
    }
    history.add(t, static_cast<uint8_t>(hx + 0.5), static_cast<uint8_t>(steam + 0.5));
  }
  if (shotAtS > 0) {
    history.addShot(shotAtS, 27);
  }
  if (secondShotAtS > 0) {
    history.addShot(secondShotAtS, 31);
  }
}

void writePbm(const char *path, const GFXcanvas1 &canvas) {
  FILE *file = fopen(path, "wb");
  if (!file) {
    perror(path);
    return;
  }
  fprintf(file, "P4\n%d %d\n", kWidth, kHeight);
  fwrite(canvas.getBuffer(), 1, (kWidth / 8) * kHeight, file);  // bit set = black, same as PBM
  fclose(file);
  printf("wrote %s\n", path);
}

void renderScene(const char *path, const DisplayModel &model, const TemperatureHistory &history, uint32_t elapsedS) {
  fprintf(stderr, "scene %s: %u samples, first hx=%u last hx=%u latest=%lus\n", path, history.size(),
          history.empty() ? 0 : history.at(0).hx, history.empty() ? 0 : history.at(history.size() - 1).hx,
          static_cast<unsigned long>(history.latestS()));
  GFXcanvas1 canvas(kWidth, kHeight);
  canvas.fillScreen(0);
  DashboardPainter painter(canvas, 1, 0);
  painter.paint(model, history, GraphWindow::forElapsed(elapsedS));
  writePbm(path, canvas);
}

void renderLogo(const char *path, uint32_t seed, const char *tag) {
  GFXcanvas1 canvas(kWidth, kHeight);
  canvas.fillScreen(0);
  LogoScreen::paint(canvas, 1, 0, seed, tag);
  writePbm(path, canvas);
}

}  // namespace

int main(int argc, char **argv) {
  const char *outDir = argc > 1 ? argv[1] : ".";
  char path[512];

  // Scene 1: just switched on, no data from the machine yet, access point open.
  {
    TemperatureHistory history;
    DisplayModel model;
    model.wifi = DisplayModel::Wifi::Portal;
    model.apName = "MaraXMonitor";
    snprintf(path, sizeof(path), "%s/1-startup.pbm", outDir);
    renderScene(path, model, history, 0);
  }
  // Scene 2: 24 minutes in, heating cycling, a shot running right now.
  {
    TemperatureHistory history;
    fillHistory(history, 24 * 60, 24 * 60 - 18, 0);
    DisplayModel model;
    model.hasReading = true;
    model.reading.mode = MaraXReading::Mode::CoffeePriority;
    model.reading.hxTemperature = 97;
    model.reading.steamTemperature = 131;
    model.reading.steamTarget = 124;
    model.reading.heatingOn = true;
    model.reading.boostCountdown = 0;
    model.shotRunning = true;
    model.shotSeconds = 18;
    model.wifi = DisplayModel::Wifi::Connected;
    strncpy(model.ip, "192.168.1.42", sizeof(model.ip) - 1);
    model.restartNote = "SW WATCHDOG";
    snprintf(path, sizeof(path), "%s/2-shot-running.pbm", outDir);
    renderScene(path, model, history, 24 * 60);
  }
  // Scene 3: 70 minutes, two shots done, machine ready again and heating, no wifi.
  {
    TemperatureHistory history;
    fillHistory(history, 70 * 60, 26 * 60, 61 * 60);
    DisplayModel model;
    model.hasReading = true;
    model.reading.mode = MaraXReading::Mode::SteamPriority;
    model.reading.hxTemperature = 96;
    model.reading.steamTemperature = 121;
    model.reading.steamTarget = 124;
    model.reading.heatingOn = true;
    model.reading.boostCountdown = 840;
    model.readyForShot = true;
    model.shotRunning = false;
    model.hasLastShot = true;
    model.shotSeconds = 31;
    model.wifi = DisplayModel::Wifi::Off;
    snprintf(path, sizeof(path), "%s/3-two-shots.pbm", outDir);
    renderScene(path, model, history, 70 * 60);
  }
  // Scenes 4 to 6: the logo as boot and off screen, three of its random variants.
  snprintf(path, sizeof(path), "%s/4-boot-screen.pbm", outDir);
  renderLogo(path, 1, "SYSTEM ONLINE");
  snprintf(path, sizeof(path), "%s/5-off-screen.pbm", outDir);
  renderLogo(path, 2, "SYSTEM OFFLINE");
  snprintf(path, sizeof(path), "%s/6-off-screen-b.pbm", outDir);
  renderLogo(path, 3, "SYSTEM OFFLINE");
  return 0;
}
