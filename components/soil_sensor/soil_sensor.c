#include "soil_sensor.h"

uint16_t soil_modbus_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1U) ^ 0xA001U) : (uint16_t)(crc >> 1U);
        }
    }
    return crc;
}

size_t soil_modbus_build_read_holding(uint8_t address, uint16_t start_register,
                                      uint16_t register_count, uint8_t out[8])
{
    out[0] = address;
    out[1] = 0x03;
    out[2] = (uint8_t)(start_register >> 8U);
    out[3] = (uint8_t)start_register;
    out[4] = (uint8_t)(register_count >> 8U);
    out[5] = (uint8_t)register_count;

    const uint16_t crc = soil_modbus_crc16(out, 6);
    out[6] = (uint8_t)crc;
    out[7] = (uint8_t)(crc >> 8U);
    return 8;
}

bool soil_modbus_parse_read_response(const uint8_t *frame, size_t frame_length,
                                     uint8_t expected_address,
                                     uint16_t expected_register_count,
                                     uint16_t *registers_out)
{
    const size_t expected_data_bytes = (size_t)expected_register_count * 2U;
    const size_t expected_frame_length = expected_data_bytes + 5U;
    if (frame == NULL || registers_out == NULL || expected_register_count == 0 ||
        frame_length != expected_frame_length || frame[0] != expected_address ||
        frame[1] != 0x03 || frame[2] != expected_data_bytes) {
        return false;
    }

    const uint16_t received_crc = (uint16_t)frame[frame_length - 2U] |
                                  ((uint16_t)frame[frame_length - 1U] << 8U);
    if (soil_modbus_crc16(frame, frame_length - 2U) != received_crc) {
        return false;
    }

    for (uint16_t i = 0; i < expected_register_count; ++i) {
        const size_t offset = 3U + ((size_t)i * 2U);
        registers_out[i] = ((uint16_t)frame[offset] << 8U) | frame[offset + 1U];
    }
    return true;
}

soil_sensor_readings_t soil_sensor_decode_registers(
    const uint16_t regs[SOIL_SENSOR_REGISTER_COUNT])
{
    soil_sensor_readings_t readings = {
        .temperature_c = (float)(int16_t)regs[0] / 10.0F,
        .moisture_percent = (float)regs[1] / 10.0F,
        .conductivity_us_cm = regs[2],
        .salt = regs[3],
        .nitrogen_mg_kg = regs[4],
        .phosphorus_mg_kg = regs[5],
        .potassium_mg_kg = regs[6],
        .ph = (float)regs[7] / 10.0F,
    };
    return readings;
}
