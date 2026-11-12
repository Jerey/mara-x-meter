#include <TemperatureHistory.hpp>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_keeps_latest_value_per_interval() {
  TemperatureHistory h;
  TEST_ASSERT_TRUE(h.add(0, 20, 30));
  TEST_ASSERT_FALSE(h.add(1, 21, 31));
  TEST_ASSERT_FALSE(h.add(4, 22, 32));
  TEST_ASSERT_EQUAL_UINT16(1, h.size());
  TEST_ASSERT_EQUAL_UINT8(22, h.at(0).hx);
  TEST_ASSERT_EQUAL_UINT8(32, h.at(0).steam);
  TEST_ASSERT_TRUE(h.add(5, 23, 33));
  TEST_ASSERT_EQUAL_UINT16(2, h.size());
  TEST_ASSERT_EQUAL_UINT32(0, h.timeAt(0));
  TEST_ASSERT_EQUAL_UINT32(5, h.timeAt(1));
  TEST_ASSERT_EQUAL_UINT32(5, h.latestS());
}

void test_fills_skipped_intervals() {
  TemperatureHistory h;
  h.add(0, 50, 60);
  h.add(20, 54, 64);  // intervals 5, 10, 15 skipped
  TEST_ASSERT_EQUAL_UINT16(5, h.size());
  TEST_ASSERT_EQUAL_UINT8(54, h.at(1).hx);
  TEST_ASSERT_EQUAL_UINT8(54, h.at(4).hx);
}

void test_first_sample_may_start_late() {
  TemperatureHistory h;
  h.add(123, 80, 110);
  TEST_ASSERT_EQUAL_UINT16(1, h.size());
  TEST_ASSERT_EQUAL_UINT32(120, h.timeAt(0));
}

void test_scrolls_when_full() {
  TemperatureHistory h;
  const uint32_t interval = TemperatureHistory::kSampleIntervalS;
  for (uint32_t i = 0; i < TemperatureHistory::kCapacity + 10; ++i) {
    h.add(i * interval, static_cast<uint8_t>(i % 200), 100);
  }
  TEST_ASSERT_EQUAL_UINT16(TemperatureHistory::kCapacity, h.size());
  TEST_ASSERT_EQUAL_UINT32(10 * interval, h.timeAt(0));
  TEST_ASSERT_EQUAL_UINT8(10 % 200, h.at(0).hx);
  TEST_ASSERT_EQUAL_UINT8((TemperatureHistory::kCapacity + 9) % 200, h.at(h.size() - 1).hx);
}

void test_newest_time_moves_on_when_full() {
  // The graph is redrawn when latestS() changes, which must keep working once the size no longer changes.
  TemperatureHistory h;
  const uint32_t interval = TemperatureHistory::kSampleIntervalS;
  for (uint32_t i = 0; i < TemperatureHistory::kCapacity; ++i) {
    h.add(i * interval, 90, 120);
  }
  const uint32_t before = h.latestS();
  TEST_ASSERT_TRUE(h.add(TemperatureHistory::kCapacity * interval, 90, 120));
  TEST_ASSERT_EQUAL_UINT16(TemperatureHistory::kCapacity, h.size());
  TEST_ASSERT_EQUAL_UINT32(before + interval, h.latestS());
}

void test_clear_forgets_everything() {
  TemperatureHistory h;
  h.add(0, 50, 60);
  h.add(10, 51, 61);
  h.addShot(10, 25);
  h.clear();
  TEST_ASSERT_TRUE(h.empty());
  TEST_ASSERT_EQUAL_UINT8(0, h.shotCount());
  TEST_ASSERT_EQUAL_UINT32(0, h.latestS());
  h.add(300, 70, 80);
  TEST_ASSERT_EQUAL_UINT32(300, h.timeAt(0));
}

void test_shots_are_capped() {
  TemperatureHistory h;
  for (uint32_t i = 0; i < TemperatureHistory::kMaxShots + 3; ++i) {
    h.addShot(i * 100, 25);
  }
  TEST_ASSERT_EQUAL_UINT8(TemperatureHistory::kMaxShots, h.shotCount());
  TEST_ASSERT_EQUAL_UINT32(300, h.shot(0).startS);
}

void test_window_steps_widen_with_session() {
  GraphWindow w = GraphWindow::forElapsed(0);
  TEST_ASSERT_EQUAL_UINT32(0, w.startS);
  TEST_ASSERT_EQUAL_UINT32(30 * 60, w.spanS);
  TEST_ASSERT_EQUAL_UINT16(5 * 60, w.tickS);

  w = GraphWindow::forElapsed(30 * 60);
  TEST_ASSERT_EQUAL_UINT32(30 * 60, w.spanS);
  w = GraphWindow::forElapsed(30 * 60 + 1);
  TEST_ASSERT_EQUAL_UINT32(45 * 60, w.spanS);
  w = GraphWindow::forElapsed(70 * 60);
  TEST_ASSERT_EQUAL_UINT32(90 * 60, w.spanS);
  TEST_ASSERT_EQUAL_UINT16(15 * 60, w.tickS);
  w = GraphWindow::forElapsed(179 * 60);
  TEST_ASSERT_EQUAL_UINT32(180 * 60, w.spanS);
  TEST_ASSERT_EQUAL_UINT32(0, w.startS);
}

void test_window_scrolls_after_three_hours() {
  GraphWindow w = GraphWindow::forElapsed(181 * 60);
  TEST_ASSERT_EQUAL_UINT32(180 * 60, w.spanS);
  TEST_ASSERT_EQUAL_UINT32(30 * 60, w.startS);  // rounded up to the next tick
  TEST_ASSERT_TRUE(w.startS + w.spanS >= 181 * 60);
  w = GraphWindow::forElapsed(250 * 60);
  TEST_ASSERT_EQUAL_UINT32(90 * 60, w.startS);
}

void test_x_mapping_clamps_and_scales() {
  GraphWindow w{ 0, 1800, 300 };
  TEST_ASSERT_EQUAL_INT16(40, w.xFor(0, 40, 360));
  TEST_ASSERT_EQUAL_INT16(40 + 359, w.xFor(1800, 40, 360));
  TEST_ASSERT_EQUAL_INT16(40 + 359, w.xFor(5000, 40, 360));
  TEST_ASSERT_EQUAL_INT16(40 + 179, w.xFor(900, 40, 360));
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
  RUN_TEST(test_shots_are_capped);
  RUN_TEST(test_window_steps_widen_with_session);
  RUN_TEST(test_window_scrolls_after_three_hours);
  RUN_TEST(test_x_mapping_clamps_and_scales);
  RUN_TEST(test_y_mapping);
  return UNITY_END();
}
