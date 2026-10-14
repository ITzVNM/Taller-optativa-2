#include "unity.h"

#include "soil_sensor.h"

TEST_CASE("Modbus request matches the manual example (8 registers from 0x0000)", "[soil_sensor]")
{
    uint8_t request[8];
    const size_t length = soil_modbus_build_read_holding(0x00, 0x0000, 0x0008, request);

    const uint8_t expected[] = {0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x45, 0xDD};
    TEST_ASSERT_EQUAL_UINT32(sizeof(expected), length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, request, sizeof(expected));
}

TEST_CASE("Modbus response is rejected if the CRC changes", "[soil_sensor]")
{
    const uint8_t bad_response[] = {0x01, 0x03, 0x02, 0x00, 0x20, 0x5A, 0x3C};
    uint16_t value = 0;

    TEST_ASSERT_FALSE(soil_modbus_parse_read_response(bad_response, sizeof(bad_response),
                                                       0x01, 1, &value));
}

TEST_CASE("Modbus response decodes a big endian holding register", "[soil_sensor]")
{
    const uint8_t response[] = {0x01, 0x03, 0x02, 0x00, 0x20, 0xB9, 0x9C};
    uint16_t value = 0;

    TEST_ASSERT_TRUE(soil_modbus_parse_read_response(response, sizeof(response),
                                                      0x01, 1, &value));
    TEST_ASSERT_EQUAL_UINT16(32, value);
}

TEST_CASE("Manual example response decodes all 8 registers", "[soil_sensor]")
{
    // Data of the manual example (its printed CRC A7 B5 is wrong; the real one is 70 90).
    const uint8_t response[] = {0x01, 0x03, 0x10, 0xFF, 0xDD, 0x01, 0x64, 0x03, 0x20, 0x01, 0x90,
                                0x01, 0x64, 0x01, 0x64, 0x01, 0x64, 0x00, 0x46, 0x70, 0x90};
    uint16_t regs[SOIL_SENSOR_REGISTER_COUNT] = {0};

    TEST_ASSERT_TRUE(soil_modbus_parse_read_response(response, sizeof(response), 0x01,
                                                      SOIL_SENSOR_REGISTER_COUNT, regs));
    const soil_sensor_readings_t values = soil_sensor_decode_registers(regs);

    TEST_ASSERT_FLOAT_WITHIN(0.001F, -3.5F, values.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 35.6F, values.moisture_percent);
    TEST_ASSERT_EQUAL_UINT16(800, values.conductivity_us_cm);
    TEST_ASSERT_EQUAL_UINT16(400, values.salt);
    TEST_ASSERT_EQUAL_UINT16(356, values.nitrogen_mg_kg);
    TEST_ASSERT_EQUAL_UINT16(356, values.phosphorus_mg_kg);
    TEST_ASSERT_EQUAL_UINT16(356, values.potassium_mg_kg);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 7.0F, values.ph);
}
