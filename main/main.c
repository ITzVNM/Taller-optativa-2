#include <inttypes.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soil_sensor.h"

#define RS485_UART UART_NUM_1
#define RX_TIMEOUT_MS 500
#define VEXT_GPIO GPIO_NUM_36

static const char *TAG = "soil_rs485";

static esp_err_t read_holding_registers(uint16_t start_register, uint16_t register_count,
                                        uint16_t *registers_out)
{
    uint8_t request[8];
    const size_t request_length = soil_modbus_build_read_holding(
        CONFIG_SOIL_SENSOR_SLAVE_ADDRESS, start_register, register_count, request);
    const size_t expected_response_length = 5U + ((size_t)register_count * 2U);
    uint8_t response[5 + SOIL_SENSOR_REGISTER_COUNT * 2] = {0}; // 8 registers = 21 bytes.

    if (expected_response_length > sizeof(response)) {
        return ESP_ERR_INVALID_SIZE;
    }

    // HW-726 detects UART activity and switches its RS485 direction itself.
    // Therefore this firmware must not drive a DE or /RE GPIO.
    ESP_ERROR_CHECK(uart_flush_input(RS485_UART));
    const int written = uart_write_bytes(RS485_UART, (const char *)request, request_length);
    if (written != (int)request_length ||
        uart_wait_tx_done(RS485_UART, pdMS_TO_TICKS(100)) != ESP_OK) {
        return ESP_FAIL;
    }

    const int read = uart_read_bytes(RS485_UART, response, expected_response_length,
                                     pdMS_TO_TICKS(RX_TIMEOUT_MS));
    if (read != (int)expected_response_length) {
        ESP_LOGW(TAG, "timeout at 0x%04X: received %d/%u bytes", start_register, read,
                 (unsigned)expected_response_length);
        return ESP_ERR_TIMEOUT;
    }

    if (!soil_modbus_parse_read_response(response, (size_t)read,
                                         CONFIG_SOIL_SENSOR_SLAVE_ADDRESS,
                                         register_count, registers_out)) {
        ESP_LOGW(TAG, "invalid Modbus response at 0x%04X", start_register);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t read_sensor(soil_sensor_readings_t *readings)
{
    uint16_t regs[SOIL_SENSOR_REGISTER_COUNT];

    // Soil Multi-parameter Sensor Manual V2.2: registers 0x0000..0x0007 =
    // temperature, moisture, EC, salt, N, P, K, pH (one single request).
    ESP_RETURN_ON_ERROR(read_holding_registers(SOIL_SENSOR_START_REGISTER,
                                               SOIL_SENSOR_REGISTER_COUNT, regs),
                        TAG, "register read failed");

    *readings = soil_sensor_decode_registers(regs);
    return ESP_OK;
}

static void configure_hardware(void)
{
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_SOIL_SENSOR_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(RS485_UART, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(RS485_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART, CONFIG_SOIL_SENSOR_UART_TX_GPIO,
                                 CONFIG_SOIL_SENSOR_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

#if CONFIG_SOIL_SENSOR_USE_VEXT
    const gpio_config_t vext_pin = {.pin_bit_mask = 1ULL << VEXT_GPIO, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&vext_pin));
    ESP_ERROR_CHECK(gpio_set_level(VEXT_GPIO, 0)); // Vext on the Heltec V3 is active low.
    vTaskDelay(pdMS_TO_TICKS(250));
#endif
}

void app_main(void)
{
    configure_hardware();
    ESP_LOGI(TAG, "RS485 ready: 8N1, %d baud, slave %d", CONFIG_SOIL_SENSOR_BAUD_RATE,
             CONFIG_SOIL_SENSOR_SLAVE_ADDRESS);

    while (true) {
        soil_sensor_readings_t values;
        if (read_sensor(&values) == ESP_OK) {
            printf("{\"temperature_c\":%.1f,\"moisture_percent\":%.1f,\"ec_us_cm\":%u,\"salt\":%u,"
                   "\"ph\":%.1f,\"nitrogen_mg_kg\":%u,\"phosphorus_mg_kg\":%u,"
                   "\"potassium_mg_kg\":%u}\n",
                   values.temperature_c, values.moisture_percent, values.conductivity_us_cm,
                   values.salt, values.ph, values.nitrogen_mg_kg, values.phosphorus_mg_kg,
                   values.potassium_mg_kg);
        } else {
            ESP_LOGE(TAG, "sensor read failed; see README troubleshooting section");
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SOIL_SENSOR_PERIOD_MS));
    }
}
