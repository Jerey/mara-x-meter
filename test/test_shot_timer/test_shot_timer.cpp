#include <ShotTimer.hpp>
#include <unity.h>

void setUp() {}
void tearDown() {}

/// @brief Simulates the reed sensor: a pulse every 100 ms between startMs and endMs.
static void pulse(ShotTimer &timer, uint32_t startMs, uint32_t endMs) {
  for (uint32_t nowMs = startMs; nowMs <= endMs; nowMs += 100) {
    timer.noteActivity(nowMs);
    timer.update(nowMs);
  }
}

void test_starts_on_first_pulse_and_counts() {
  ShotTimer timer(1500, 5000);
  TEST_ASSERT_FALSE(timer.running());
  TEST_ASSERT_EQUAL_UINT32(0, timer.elapsedMs(10000));

  timer.noteActivity(10000);
  TEST_ASSERT_TRUE(timer.running());
  TEST_ASSERT_TRUE(timer.takeStartedEvent());
  TEST_ASSERT_FALSE(timer.takeStartedEvent());
  TEST_ASSERT_EQUAL_UINT32(2500, timer.elapsedMs(12500));
}

void test_stops_after_silence_and_records_duration_without_threshold() {
  ShotTimer timer(1500, 5000);
  pulse(timer, 10000, 38000);  // 28 s shot
  timer.update(39000);
  TEST_ASSERT_TRUE(timer.running());  // 1 s silence is still within threshold
  TEST_ASSERT_FALSE(timer.takeShotFinishedEvent());
  timer.update(39600);
  TEST_ASSERT_FALSE(timer.running());
  TEST_ASSERT_TRUE(timer.takeStoppedEvent());
  TEST_ASSERT_TRUE(timer.takeShotFinishedEvent());
  TEST_ASSERT_FALSE(timer.takeShotFinishedEvent());
  TEST_ASSERT_TRUE(timer.hasLastShot());
  TEST_ASSERT_EQUAL_UINT32(28000, timer.lastShotMs());
  TEST_ASSERT_EQUAL_UINT32(10000, timer.lastShotStartedAtMs());
  TEST_ASSERT_EQUAL_UINT32(28000, timer.elapsedMs(50000));
}

void test_short_pump_runs_are_not_kept_as_last_shot() {
  ShotTimer timer(1500, 5000);
  pulse(timer, 1000, 3000);  // 2 s HX refresh by the machine
  timer.update(5000);
  TEST_ASSERT_FALSE(timer.running());
  TEST_ASSERT_TRUE(timer.takeStoppedEvent());
  TEST_ASSERT_FALSE(timer.takeShotFinishedEvent());
  TEST_ASSERT_FALSE(timer.hasLastShot());
  TEST_ASSERT_EQUAL_UINT32(0, timer.elapsedMs(5000));
  TEST_ASSERT_EQUAL_UINT32(2000, timer.lastRunMs());
}

void test_gaps_inside_a_shot_do_not_split_it() {
  ShotTimer timer(1500, 5000);
  pulse(timer, 0, 10000);
  timer.update(11200);  // 1.2 s gap
  TEST_ASSERT_TRUE(timer.running());
  pulse(timer, 11300, 25000);
  timer.update(27000);
  TEST_ASSERT_FALSE(timer.running());
  TEST_ASSERT_EQUAL_UINT32(25000, timer.lastShotMs());
}

void test_second_shot_replaces_first() {
  ShotTimer timer(1500, 5000);
  pulse(timer, 0, 20000);
  timer.update(22000);
  pulse(timer, 60000, 90000);
  timer.update(92000);
  TEST_ASSERT_EQUAL_UINT32(30000, timer.lastShotMs());
  TEST_ASSERT_EQUAL_UINT32(60000, timer.lastShotStartedAtMs());
}

void test_handles_millis_wraparound() {
  ShotTimer timer(1500, 5000);
  const uint32_t start = 0xFFFFFFFFu - 5000;
  pulse(timer, start, start + 4000);  // last pulse 1000 ms before the wrap
  timer.noteActivity(1000);           // 2001 ms later, after the wrap
  timer.update(1500);
  TEST_ASSERT_TRUE(timer.running());
  TEST_ASSERT_EQUAL_UINT32(5000 + 1 + 2500, timer.elapsedMs(2500));
  timer.update(3000);
  TEST_ASSERT_FALSE(timer.running());
  TEST_ASSERT_EQUAL_UINT32(5000 + 1 + 1000, timer.lastShotMs());
}

void test_pulse_newer_than_now_does_not_end_the_shot() {
  // An interrupt can stamp a pulse a few milliseconds after the loop took its time stamp.
  ShotTimer timer(1500, 5000);
  timer.noteActivity(1002);
  timer.update(1000);
  TEST_ASSERT_TRUE(timer.running());
  TEST_ASSERT_EQUAL_UINT32(0, timer.elapsedMs(1000));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_on_first_pulse_and_counts);
  RUN_TEST(test_stops_after_silence_and_records_duration_without_threshold);
  RUN_TEST(test_short_pump_runs_are_not_kept_as_last_shot);
  RUN_TEST(test_gaps_inside_a_shot_do_not_split_it);
  RUN_TEST(test_second_shot_replaces_first);
  RUN_TEST(test_handles_millis_wraparound);
  RUN_TEST(test_pulse_newer_than_now_does_not_end_the_shot);
  return UNITY_END();
}
