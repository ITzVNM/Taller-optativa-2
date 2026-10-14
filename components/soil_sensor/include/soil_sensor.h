#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    float ph;
    float moisture_percent;
    float temperature_c;
    uint16_t conductivity_us_cm;
    uint16_t salt;
    uint16_t nitrogen_mg_kg;
    uint16_t phosphorus_mg_kg;
    uint16_t potassium_mg_kg;
} soil_sensor_readings_t;

/** Calculate the Modbus RTU CRC-16 (low byte is transmitted first). */
uint16_t soil_modbus_crc16(const uint8_t *data, size_t length);

/** Build a Modbus function 0x03 request. out must point to at least 8 bytes. */
size_t soil_modbus_build_read_holding(uint8_t address, uint16_t start_register,
                                      uint16_t register_count, uint8_t out[8]);

/** Validate and unpack a normal Modbus function 0x03 response. */
bool soil_modbus_parse_read_response(const uint8_t *frame, size_t frame_length,
                                     uint8_t expected_address,
                                     uint16_t expected_register_count,
                                     uint16_t *registers_out);

/** Number of consecutive registers (0x0000..0x0007) of the 7-in-1 sensor. */
#define SOIL_SENSOR_REGISTER_COUNT 8
#define SOIL_SENSOR_START_REGISTER 0x0000

/**
 * Decode the 8 registers read from 0x0000 (Soil Multi-parameter Sensor Manual V2.2):
 * 0 temperature (signed, /10 C), 1 moisture (/10 %), 2 EC (uS/cm), 3 salt,
 * 4 nitrogen, 5 phosphorus, 6 potassium (mg/kg), 7 pH (/10).
 */
soil_sensor_readings_t soil_sensor_decode_registers(
    const uint16_t regs[SOIL_SENSOR_REGISTER_COUNT]);
