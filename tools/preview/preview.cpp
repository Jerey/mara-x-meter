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

/// Roughly what a Mara X does from cold: the steam boiler climbs to its target, the HX follows and the machine
/// cycles the heating around the target afterwards.
void fillHistory(TemperatureHistory &history, uint32_t untilS, uint32_t shotAtS, uint32_t secondShotAtS) {
  for (uint32_t t = 0; t <= untilS; t += TemperatureHistory::kSampleIntervalS) {
    const double seconds = static_cast<double>(t);
    double steam = 22 + 102 * (1 - exp(-seconds / 420.0));
    double hx = 22 + 74 * (1 - exp(-seconds / 520.0));
    if (t > 900) {
      steam += 4 * sin(seconds / 90.0);
      hx += 2.5 * sin(seconds / 95.0 + 1);
    }
    auto dip = [&](uint32_t shotS) {
      if (shotS > 0 && t >= shotS && t < shotS + 120) {
        hx -= 6 * exp(-static_cast<double>(t - shotS) / 60.0);
      }
    };
    dip(shotAtS);
    dip(secondShotAtS);
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
    fillHistory(history, 24 * 60, 0, 0);
    DisplayModel model;
    model.hasReading = true;
    model.reading.mode = MaraXReading::Mode::CoffeePriority;
    model.reading.hxTemperature = 94;
    model.reading.steamTemperature = 123;
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
  // Scene 3: 70 minutes, two shots done, boost counting down, no wifi.
  {
    TemperatureHistory history;
    fillHistory(history, 70 * 60, 26 * 60, 61 * 60);
    DisplayModel model;
    model.hasReading = true;
    model.reading.mode = MaraXReading::Mode::SteamPriority;
    model.reading.hxTemperature = 101;
    model.reading.steamTemperature = 128;
    model.reading.steamTarget = 130;
    model.reading.heatingOn = false;
    model.reading.boostCountdown = 840;
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
