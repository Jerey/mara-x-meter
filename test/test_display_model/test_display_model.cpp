#include <DisplayModel.hpp>
#include <MaraXProtocol.hpp>  // also tells PlatformIO, that the protocol library is needed
#include <string.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

static DisplayModel sampleModel() {
  DisplayModel m;
  m.hasReading = true;
  m.reading.mode = MaraXReading::Mode::CoffeePriority;
  m.reading.hxTemperature = 94;
  m.reading.steamTemperature = 123;
  m.reading.steamTarget = 124;
  m.reading.boostCountdown = 600;
  m.reading.heatingOn = true;
  m.wifi = DisplayModel::Wifi::Connected;
  strncpy(m.ip, "192.168.1.42", sizeof(m.ip) - 1);
  return m;
}

void test_same_values_look_the_same() {
  const DisplayModel a = sampleModel();
  DisplayModel b = sampleModel();
  TEST_ASSERT_TRUE(a.sameAs(b));
  strncpy(b.reading.firmware, "9.99", sizeof(b.reading.firmware));  // not shown
  b.reading.pumpOn = true;                                          // not shown either
  TEST_ASSERT_TRUE(a.sameAs(b));
}

void test_shown_values_matter() {
  const DisplayModel a = sampleModel();
  DisplayModel b = sampleModel();
  b.reading.hxTemperature = 95;
  TEST_ASSERT_FALSE(a.sameAs(b));
  b = sampleModel();
  b.shotSeconds = 1;
  TEST_ASSERT_FALSE(a.sameAs(b));
  b = sampleModel();
  strncpy(b.ip, "192.168.1.43", sizeof(b.ip) - 1);
  TEST_ASSERT_FALSE(a.sameAs(b));
}

void test_boost_countdown_only_matters_in_minutes() {
  const DisplayModel a = sampleModel();
  DisplayModel b = sampleModel();
  b.reading.boostCountdown = 599;  // still 10 minutes
  TEST_ASSERT_TRUE(a.sameAs(b));
  b.reading.boostCountdown = 541;  // still 10 minutes
  TEST_ASSERT_TRUE(a.sameAs(b));
  b.reading.boostCountdown = 540;  // 9 minutes
  TEST_ASSERT_FALSE(a.sameAs(b));
  TEST_ASSERT_EQUAL_UINT16(0, DisplayModel::boostMinutes(MaraXReading{}));
}

void test_without_reading_the_reading_is_ignored() {
  DisplayModel a;
  DisplayModel b;
  b.reading.hxTemperature = 80;
  TEST_ASSERT_TRUE(a.sameAs(b));
  b.hasReading = true;
  TEST_ASSERT_FALSE(a.sameAs(b));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_same_values_look_the_same);
  RUN_TEST(test_shown_values_matter);
  RUN_TEST(test_boost_countdown_only_matters_in_minutes);
  RUN_TEST(test_without_reading_the_reading_is_ignored);
  return UNITY_END();
}
