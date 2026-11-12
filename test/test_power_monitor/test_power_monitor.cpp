#include <PowerMonitor.hpp>
#include <unity.h>

void setUp() {}
void tearDown() {}

static void settle(PowerMonitor &monitor, uint16_t millivolt) {
  for (int i = 0; i < 3; ++i) {
    monitor.noteSupply(millivolt);
  }
}

void test_needs_several_low_readings_in_a_row() {
  PowerMonitor monitor(500, 3, 2000);
  settle(monitor, 3300);
  TEST_ASSERT_FALSE(monitor.noteSupply(3290));
  TEST_ASSERT_EQUAL_UINT16(3300, monitor.maxMillivolt());
  TEST_ASSERT_FALSE(monitor.noteSupply(2700));
  TEST_ASSERT_FALSE(monitor.noteSupply(2700));
  TEST_ASSERT_TRUE(monitor.noteSupply(2650));
}

void test_single_dip_is_ignored() {
  PowerMonitor monitor(500, 3, 2000);
  settle(monitor, 3300);
  TEST_ASSERT_FALSE(monitor.noteSupply(2700));
  TEST_ASSERT_FALSE(monitor.noteSupply(2700));
  TEST_ASSERT_FALSE(monitor.noteSupply(3250));  // back to normal, the count starts over
  TEST_ASSERT_FALSE(monitor.noteSupply(2700));
  TEST_ASSERT_FALSE(monitor.noteSupply(2700));
  TEST_ASSERT_TRUE(monitor.noteSupply(2700));
}

void test_drop_is_relative_to_the_highest_voltage_seen() {
  // A weak USB supply of 3.0 V must not count as a brownout of a 3.3 V one.
  PowerMonitor monitor(500, 1, 2000);
  TEST_ASSERT_FALSE(monitor.noteSupply(3000));
  TEST_ASSERT_FALSE(monitor.noteSupply(2900));
  TEST_ASSERT_TRUE(monitor.noteSupply(2400));
}

void test_single_high_reading_does_not_lift_the_reference() {
  PowerMonitor monitor(500, 3, 2000);
  settle(monitor, 3300);
  TEST_ASSERT_FALSE(monitor.noteSupply(3900));  // noise
  TEST_ASSERT_EQUAL_UINT16(3300, monitor.maxMillivolt());
  TEST_ASSERT_FALSE(monitor.noteSupply(3300));
  TEST_ASSERT_FALSE(monitor.noteSupply(3300));
  TEST_ASSERT_FALSE(monitor.noteSupply(3300));
  TEST_ASSERT_EQUAL_UINT16(3300, monitor.maxMillivolt());
  // A real rise is taken over, with the lowest of the readings that confirmed it.
  monitor.noteSupply(3400);
  monitor.noteSupply(3420);
  monitor.noteSupply(3410);
  TEST_ASSERT_EQUAL_UINT16(3400, monitor.maxMillivolt());
}

void test_recovery_needs_the_supply_back_for_a_while() {
  PowerMonitor monitor(500, 1, 2000);
  monitor.noteSupply(3300);
  TEST_ASSERT_TRUE(monitor.noteSupply(2700));
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(2600, 10000));
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(3300, 11000));  // back, the clock starts
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(3300, 12500));
  TEST_ASSERT_TRUE(monitor.noteSupplyAfterShutdown(3300, 13000));
}

void test_recovery_starts_over_after_another_dip() {
  PowerMonitor monitor(500, 1, 2000);
  monitor.noteSupply(3300);
  monitor.noteSupply(2700);
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(3300, 1000));
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(3000, 2000));  // dips again
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(3300, 3500));
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(3300, 5000));
  TEST_ASSERT_TRUE(monitor.noteSupplyAfterShutdown(3300, 5500));
}

void test_recovery_when_the_voltage_rises_again() {
  // The reference was lifted by a good supply before, the supply after the restart is weaker: the voltage never
  // gets close to the reference again, but it clearly rises from the lowest point, so the power must be back.
  PowerMonitor monitor(500, 1, 2000);
  monitor.noteSupply(3500);
  TEST_ASSERT_TRUE(monitor.noteSupply(2900));
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(2800, 1000));
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(2700, 2000));  // still going down
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(2800, 3000));  // a bit up, not enough
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(2960, 4000));  // 260 mV above the lowest point
  TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(2960, 5000));
  TEST_ASSERT_TRUE(monitor.noteSupplyAfterShutdown(2960, 6000));
}

void test_dying_capacitor_never_recovers() {
  PowerMonitor monitor(500, 1, 2000);
  monitor.noteSupply(3300);
  TEST_ASSERT_TRUE(monitor.noteSupply(2700));
  for (uint32_t t = 0; t < 20; ++t) {
    TEST_ASSERT_FALSE(monitor.noteSupplyAfterShutdown(static_cast<uint16_t>(2700 - t * 10), 1000 + t * 1000));
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_needs_several_low_readings_in_a_row);
  RUN_TEST(test_single_dip_is_ignored);
  RUN_TEST(test_drop_is_relative_to_the_highest_voltage_seen);
  RUN_TEST(test_single_high_reading_does_not_lift_the_reference);
  RUN_TEST(test_recovery_needs_the_supply_back_for_a_while);
  RUN_TEST(test_recovery_starts_over_after_another_dip);
  RUN_TEST(test_recovery_when_the_voltage_rises_again);
  RUN_TEST(test_dying_capacitor_never_recovers);
  return UNITY_END();
}
