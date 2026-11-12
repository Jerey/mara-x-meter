#include <TemperatureHistory.hpp>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_keeps_latest_value_per_interval() {
  TemperatureHistory history;
  TEST_ASSERT_TRUE(history.add(0, 20, 30));
  TEST_ASSERT_FALSE(history.add(1, 21, 31));
  TEST_ASSERT_FALSE(history.add(4, 22, 32));
  TEST_ASSERT_EQUAL_UINT16(1, history.size());
  TEST_ASSERT_EQUAL_UINT8(22, history.at(0).hx);
  TEST_ASSERT_EQUAL_UINT8(32, history.at(0).steam);
  TEST_ASSERT_TRUE(history.add(5, 23, 33));
  TEST_ASSERT_EQUAL_UINT16(2, history.size());
  TEST_ASSERT_EQUAL_UINT32(0, history.timeAt(0));
  TEST_ASSERT_EQUAL_UINT32(5, history.timeAt(1));
  TEST_ASSERT_EQUAL_UINT32(5, history.latestSeconds());
}

void test_fills_skipped_intervals() {
  TemperatureHistory history;
  history.add(0, 50, 60);
  history.add(20, 54, 64);  // intervals 5, 10, 15 skipped
  TEST_ASSERT_EQUAL_UINT16(5, history.size());
  TEST_ASSERT_EQUAL_UINT8(54, history.at(1).hx);
  TEST_ASSERT_EQUAL_UINT8(54, history.at(4).hx);
}

void test_first_sample_may_start_late() {
  TemperatureHistory history;
  history.add(123, 80, 110);
  TEST_ASSERT_EQUAL_UINT16(1, history.size());
  TEST_ASSERT_EQUAL_UINT32(120, history.timeAt(0));
}

void test_scrolls_when_full() {
  TemperatureHistory history;
  const uint32_t interval = TemperatureHistory::kSampleIntervalSeconds;
  for (uint32_t i = 0; i < TemperatureHistory::kCapacity + 10; ++i) {
    history.add(i * interval, static_cast<uint8_t>(i % 200), 100);
  }
  TEST_ASSERT_EQUAL_UINT16(TemperatureHistory::kCapacity, history.size());
  TEST_ASSERT_EQUAL_UINT32(10 * interval, history.timeAt(0));
  TEST_ASSERT_EQUAL_UINT8(10 % 200, history.at(0).hx);
  TEST_ASSERT_EQUAL_UINT8((TemperatureHistory::kCapacity + 9) % 200, history.at(history.size() - 1).hx);
}

void test_newest_time_moves_on_when_full() {
  // The graph is redrawn when latestSeconds() changes, which must keep working once the size no longer changes.
  TemperatureHistory history;
  const uint32_t interval = TemperatureHistory::kSampleIntervalSeconds;
  for (uint32_t i = 0; i < TemperatureHistory::kCapacity; ++i) {
    history.add(i * interval, 90, 120);
  }
  const uint32_t before = history.latestSeconds();
  TEST_ASSERT_TRUE(history.add(TemperatureHistory::kCapacity * interval, 90, 120));
  TEST_ASSERT_EQUAL_UINT16(TemperatureHistory::kCapacity, history.size());
  TEST_ASSERT_EQUAL_UINT32(before + interval, history.latestSeconds());
}

void test_clear_forgets_everything() {
  TemperatureHistory history;
  history.add(0, 50, 60);
  history.add(10, 51, 61);
  history.addShot(10, 25);
  history.clear();
  TEST_ASSERT_TRUE(history.empty());
  TEST_ASSERT_EQUAL_UINT8(0, history.shotCount());
  TEST_ASSERT_EQUAL_UINT32(0, history.latestSeconds());
  history.add(300, 70, 80);
  TEST_ASSERT_EQUAL_UINT32(300, history.timeAt(0));
}

void test_hx_steady_needs_band_and_calm() {
  TemperatureHistory history;
  for (uint32_t elapsedSeconds = 0; elapsedSeconds < 120; elapsedSeconds += 5) {
    history.add(elapsedSeconds, static_cast<uint8_t>(80 + elapsedSeconds / 10), 120);  // climbing from 80 to 91
  }
  TEST_ASSERT_FALSE(history.hxSteady(60, 90, 100, 2));  // still climbing, 86..91 in the last minute
  for (uint32_t elapsedSeconds = 120; elapsedSeconds < 200; elapsedSeconds += 5) {
    history.add(elapsedSeconds, static_cast<uint8_t>(94 + (elapsedSeconds / 5) % 2), 120);  // 94, 95, 94, 95 ...
  }
  TEST_ASSERT_TRUE(history.hxSteady(60, 90, 100, 2));
  TEST_ASSERT_FALSE(history.hxSteady(60, 96, 100, 2));  // out of the band
  TEST_ASSERT_FALSE(history.hxSteady(60, 90, 100, 0));  // wobbles by one degree
  history.add(200, 101, 120);
  TEST_ASSERT_FALSE(history.hxSteady(60, 90, 100, 2));  // one sample out of the band is enough
}

void test_hx_steady_needs_enough_samples() {
  TemperatureHistory history;
  for (uint32_t elapsedSeconds = 0; elapsedSeconds < 60; elapsedSeconds += 5) {
    history.add(elapsedSeconds, 95, 120);
  }
  TEST_ASSERT_FALSE(history.hxSteady(60, 90, 100, 2));  // 55 s of samples
  history.add(60, 95, 120);
  TEST_ASSERT_TRUE(history.hxSteady(60, 90, 100, 2));
}

void test_shots_are_capped() {
  TemperatureHistory history;
  for (uint32_t i = 0; i < TemperatureHistory::kMaxShots + 3; ++i) {
    history.addShot(i * 100, 25);
  }
  TEST_ASSERT_EQUAL_UINT8(TemperatureHistory::kMaxShots, history.shotCount());
  TEST_ASSERT_EQUAL_UINT32(300, history.shot(0).startSeconds);
}

void test_window_steps_widen_with_session() {
  GraphWindow window = GraphWindow::forElapsed(0);
  TEST_ASSERT_EQUAL_UINT32(0, window.startSeconds);
  TEST_ASSERT_EQUAL_UINT32(30 * 60, window.spanSeconds);
  TEST_ASSERT_EQUAL_UINT16(5 * 60, window.tickSeconds);

  window = GraphWindow::forElapsed(30 * 60);
  TEST_ASSERT_EQUAL_UINT32(30 * 60, window.spanSeconds);
  window = GraphWindow::forElapsed(30 * 60 + 1);
  TEST_ASSERT_EQUAL_UINT32(45 * 60, window.spanSeconds);
  window = GraphWindow::forElapsed(70 * 60);
  TEST_ASSERT_EQUAL_UINT32(90 * 60, window.spanSeconds);
  TEST_ASSERT_EQUAL_UINT16(15 * 60, window.tickSeconds);
  window = GraphWindow::forElapsed(179 * 60);
  TEST_ASSERT_EQUAL_UINT32(180 * 60, window.spanSeconds);
  TEST_ASSERT_EQUAL_UINT32(0, window.startSeconds);
}

void test_window_scrolls_after_three_hours() {
  GraphWindow window = GraphWindow::forElapsed(181 * 60);
  TEST_ASSERT_EQUAL_UINT32(180 * 60, window.spanSeconds);
  TEST_ASSERT_EQUAL_UINT32(30 * 60, window.startSeconds);  // rounded up to the next tick
  TEST_ASSERT_TRUE(window.startSeconds + window.spanSeconds >= 181 * 60);
  window = GraphWindow::forElapsed(250 * 60);
  TEST_ASSERT_EQUAL_UINT32(90 * 60, window.startSeconds);
}

void test_x_mapping_clamps_and_scales() {
  GraphWindow window{ 0, 1800, 300 };
  TEST_ASSERT_EQUAL_INT16(40, window.xFor(0, 40, 360));
  TEST_ASSERT_EQUAL_INT16(40 + 359, window.xFor(1800, 40, 360));
  TEST_ASSERT_EQUAL_INT16(40 + 359, window.xFor(5000, 40, 360));
  TEST_ASSERT_EQUAL_INT16(40 + 179, window.xFor(900, 40, 360));
  GraphWindow scrolled{ 600, 1800, 300 };
  TEST_ASSERT_EQUAL_INT16(40, scrolled.xFor(100, 40, 360));
  TEST_ASSERT_EQUAL_INT16(40, scrolled.xFor(600, 40, 360));
}

void test_y_mapping() {
  TEST_ASSERT_EQUAL_INT16(80 + 199, yForTemperature(20, 80, 200, 20, 140));
  TEST_ASSERT_EQUAL_INT16(80 + 199, yForTemperature(5, 80, 200, 20, 140));
  TEST_ASSERT_EQUAL_INT16(80, yForTemperature(140, 80, 200, 20, 140));
  TEST_ASSERT_EQUAL_INT16(80, yForTemperature(200, 80, 200, 20, 140));
  TEST_ASSERT_EQUAL_INT16(80 + 199 - 99, yForTemperature(80, 80, 200, 20, 140));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_keeps_latest_value_per_interval);
  RUN_TEST(test_fills_skipped_intervals);
  RUN_TEST(test_first_sample_may_start_late);
  RUN_TEST(test_scrolls_when_full);
  RUN_TEST(test_newest_time_moves_on_when_full);
  RUN_TEST(test_clear_forgets_everything);
  RUN_TEST(test_hx_steady_needs_band_and_calm);
  RUN_TEST(test_hx_steady_needs_enough_samples);
  RUN_TEST(test_shots_are_capped);
  RUN_TEST(test_window_steps_widen_with_session);
  RUN_TEST(test_window_scrolls_after_three_hours);
  RUN_TEST(test_x_mapping_clamps_and_scales);
  RUN_TEST(test_y_mapping);
  return UNITY_END();
}
