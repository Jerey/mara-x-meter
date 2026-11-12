#include <DisplayModel.hpp>
#include <MaraXProtocol.hpp>  // also tells PlatformIO, that the protocol library is needed
#include <string.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

static DisplayModel sampleModel() {
  DisplayModel model;
  model.hasReading = true;
  model.reading.mode = MaraXReading::Mode::CoffeePriority;
  model.reading.hxTemperature = 94;
  model.reading.steamTemperature = 123;
  model.reading.steamTarget = 124;
  model.reading.boostCountdown = 600;
  model.reading.heatingOn = true;
  model.wifi = DisplayModel::Wifi::Connected;
  strncpy(model.ip, "192.168.1.42", sizeof(model.ip) - 1);
  return model;
}

void test_same_values_look_the_same() {
  const DisplayModel expected = sampleModel();
  DisplayModel changed = sampleModel();
  TEST_ASSERT_TRUE(expected.sameAs(changed));
  strncpy(changed.reading.firmware, "9.99", sizeof(changed.reading.firmware));  // not shown
  changed.reading.pumpOn = true;                                                // not shown either
  TEST_ASSERT_TRUE(expected.sameAs(changed));
}

void test_shown_values_matter() {
  const DisplayModel expected = sampleModel();
  DisplayModel changed = sampleModel();
  changed.reading.hxTemperature = 95;
  TEST_ASSERT_FALSE(expected.sameAs(changed));
  changed = sampleModel();
  changed.shotSeconds = 1;
  TEST_ASSERT_FALSE(expected.sameAs(changed));
  changed = sampleModel();
  strncpy(changed.ip, "192.168.1.43", sizeof(changed.ip) - 1);
  TEST_ASSERT_FALSE(expected.sameAs(changed));
}

void test_boost_countdown_is_not_shown() {
  const DisplayModel expected = sampleModel();
  DisplayModel changed = sampleModel();
  changed.reading.boostCountdown = 0;
  TEST_ASSERT_TRUE(expected.sameAs(changed));
}

void test_without_reading_the_reading_is_ignored() {
  DisplayModel expected;
  DisplayModel changed;
  changed.reading.hxTemperature = 80;
  TEST_ASSERT_TRUE(expected.sameAs(changed));
  changed.hasReading = true;
  TEST_ASSERT_FALSE(expected.sameAs(changed));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_same_values_look_the_same);
  RUN_TEST(test_shown_values_matter);
  RUN_TEST(test_boost_countdown_is_not_shown);
  RUN_TEST(test_without_reading_the_reading_is_ignored);
  return UNITY_END();
}
