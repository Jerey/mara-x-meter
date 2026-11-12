#include <MaraXProtocol.hpp>
#include <string.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_parses_coffee_priority_line() {
  MaraXReading reading;
  TEST_ASSERT_TRUE(parseMaraXLine("C1.06,116,124,093,0840,1,0", reading));
  TEST_ASSERT_EQUAL(static_cast<int>(MaraXReading::Mode::CoffeePriority), static_cast<int>(reading.mode));
  TEST_ASSERT_EQUAL_STRING("1.06", reading.firmware);
  TEST_ASSERT_EQUAL_UINT8(116, reading.steamTemperature);
  TEST_ASSERT_EQUAL_UINT8(124, reading.steamTarget);
  TEST_ASSERT_EQUAL_UINT8(93, reading.hxTemperature);
  TEST_ASSERT_EQUAL_UINT16(840, reading.boostCountdown);
  TEST_ASSERT_TRUE(reading.heatingOn);
  TEST_ASSERT_TRUE(reading.hasPumpField);
  TEST_ASSERT_FALSE(reading.pumpOn);
}

void test_parses_steam_priority_without_pump_field() {
  MaraXReading reading;
  TEST_ASSERT_TRUE(parseMaraXLine("V1.06,130,131,101,0000,0", reading));
  TEST_ASSERT_EQUAL(static_cast<int>(MaraXReading::Mode::SteamPriority), static_cast<int>(reading.mode));
  TEST_ASSERT_FALSE(reading.heatingOn);
  TEST_ASSERT_FALSE(reading.hasPumpField);
  TEST_ASSERT_EQUAL_UINT16(0, reading.boostCountdown);
}

void test_tolerates_carriage_return_and_spaces() {
  MaraXReading reading;
  TEST_ASSERT_TRUE(parseMaraXLine(" C2.01,116,124,093,0840,1,1\r", reading));
  TEST_ASSERT_TRUE(reading.pumpOn);
  TEST_ASSERT_EQUAL_STRING("2.01", reading.firmware);
}

void test_rejects_garbage_and_keeps_output_untouched() {
  MaraXReading reading;
  reading.hxTemperature = 42;
  TEST_ASSERT_FALSE(parseMaraXLine("", reading));
  TEST_ASSERT_FALSE(parseMaraXLine("hello world", reading));
  TEST_ASSERT_FALSE(parseMaraXLine("C1.06,116,124", reading));
  TEST_ASSERT_FALSE(parseMaraXLine("X1.06,116,124,093,0840,1,0", reading));
  TEST_ASSERT_FALSE(parseMaraXLine("C1.06,1a6,124,093,0840,1,0", reading));
  TEST_ASSERT_FALSE(parseMaraXLine("C1.06,,124,093,0840,1,0", reading));
  TEST_ASSERT_EQUAL_UINT8(42, reading.hxTemperature);
}

void test_line_reader_assembles_split_input() {
  LineReader reader;
  const char *part1 = "C1.06,116,";
  const char *part2 = "124,093,0840,1,0\r\n";
  for (const char *c = part1; *c; ++c) {
    TEST_ASSERT_FALSE(reader.push(*c));
  }
  bool complete = false;
  for (const char *c = part2; *c; ++c) {
    complete = reader.push(*c);
  }
  TEST_ASSERT_TRUE(complete);
  TEST_ASSERT_EQUAL_STRING("C1.06,116,124,093,0840,1,0", reader.line());
}

void test_line_reader_drops_overlong_lines_and_recovers() {
  LineReader reader;
  for (int i = 0; i < 200; ++i) {
    TEST_ASSERT_FALSE(reader.push('x'));
  }
  TEST_ASSERT_FALSE(reader.push('\n'));  // the overflowed line is dropped
  const char *good = "C1.06,116,124,093,0840,1,0\n";
  bool complete = false;
  for (const char *c = good; *c; ++c) {
    complete = reader.push(*c);
  }
  TEST_ASSERT_TRUE(complete);
  TEST_ASSERT_EQUAL_STRING("C1.06,116,124,093,0840,1,0", reader.line());
}

void test_line_reader_ignores_empty_lines() {
  LineReader reader;
  TEST_ASSERT_FALSE(reader.push('\n'));
  TEST_ASSERT_FALSE(reader.push('\r'));
  TEST_ASSERT_FALSE(reader.push('\n'));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_coffee_priority_line);
  RUN_TEST(test_parses_steam_priority_without_pump_field);
  RUN_TEST(test_tolerates_carriage_return_and_spaces);
  RUN_TEST(test_rejects_garbage_and_keeps_output_untouched);
  RUN_TEST(test_line_reader_assembles_split_input);
  RUN_TEST(test_line_reader_drops_overlong_lines_and_recovers);
  RUN_TEST(test_line_reader_ignores_empty_lines);
  return UNITY_END();
}
