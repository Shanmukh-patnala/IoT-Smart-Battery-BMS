#include "wokwi-api.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// ============================================================
// INA219 CUSTOM WOKWI CHIP
// ============================================================

#define REG_CONFIG       0x00
#define REG_SHUNT        0x01
#define REG_BUS          0x02
#define REG_POWER        0x03
#define REG_CURRENT      0x04
#define REG_CALIBRATION  0x05

typedef struct {
  uint32_t attr_voltage;
  uint32_t attr_current;

  uint8_t current_reg;
  uint8_t byte_index;
} chip_state_t;

// ------------------------------------------------------------
// I2C connection
// ------------------------------------------------------------

bool on_i2c_connect(
    void *user_data,
    uint32_t address,
    bool read) {

  chip_state_t *chip =
      (chip_state_t *)user_data;

  chip->byte_index = 0;

  return address == 0x40;
}

// ------------------------------------------------------------
// I2C register selection
// ------------------------------------------------------------

bool on_i2c_write(
    void *user_data,
    uint8_t data) {

  chip_state_t *chip =
      (chip_state_t *)user_data;

  chip->current_reg = data;
  chip->byte_index = 0;

  return true;
}

// ------------------------------------------------------------
// INA219 register model
// ------------------------------------------------------------

static uint16_t get_register_value(
    chip_state_t *chip) {

  float voltage =
      attr_read_float(chip->attr_voltage);

  float current_mA =
      attr_read_float(chip->attr_current);

  if (chip->current_reg == REG_CONFIG) {
    return 0x399F;
  }

  if (chip->current_reg == REG_SHUNT) {
    // 10 uV/bit, 0.1 ohm simulated shunt.
    float shunt_mV =
        (current_mA / 1000.0f) * 0.1f * 1000.0f;

    int16_t raw_shunt =
        (int16_t)(shunt_mV / 0.01f);

    return (uint16_t)raw_shunt;
  }

  if (chip->current_reg == REG_BUS) {
    // 4 mV/bit, stored in bits [15:3].
    uint16_t bus_raw =
        (uint16_t)(voltage / 0.004f);

    return (uint16_t)(bus_raw << 3);
  }

  if (chip->current_reg == REG_POWER) {
    // 20 uW/bit.
    float power_mW =
        voltage * current_mA;

    return (uint16_t)(power_mW / 0.02f);
  }

  if (chip->current_reg == REG_CURRENT) {
    // Scaled to match the Arduino INA219 driver
    // under the calibration used by this simulation.
    float current_raw =
        current_mA * 10.0f;

    return (uint16_t)current_raw;
  }

  if (chip->current_reg == REG_CALIBRATION) {
    return 4096;
  }

  return 0;
}

// ------------------------------------------------------------
// I2C read
// ------------------------------------------------------------

uint8_t on_i2c_read(
    void *user_data) {

  chip_state_t *chip =
      (chip_state_t *)user_data;

  uint16_t raw =
      get_register_value(chip);

  uint8_t result;

  if (chip->byte_index == 0) {
    result = (uint8_t)((raw >> 8) & 0xFF);
    chip->byte_index = 1;
  } else {
    result = (uint8_t)(raw & 0xFF);
    chip->byte_index = 0;
  }

  return result;
}

// ------------------------------------------------------------
// I2C disconnect
// ------------------------------------------------------------

void on_i2c_disconnect(
    void *user_data) {

  chip_state_t *chip =
      (chip_state_t *)user_data;

  chip->byte_index = 0;
}

// ------------------------------------------------------------
// Initialization
// ------------------------------------------------------------

void chip_init(void) {

  chip_state_t *chip =
      malloc(sizeof(chip_state_t));

  if (!chip)
    return;

  chip->current_reg = 0;
  chip->byte_index = 0;

  chip->attr_voltage =
      attr_init_float(
          "batteryVoltage",
          4.20f);

  chip->attr_current =
      attr_init_float(
          "loadCurrent",
          150.0f);

  const i2c_config_t i2c_config = {
    .address = 0x40,

    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),

    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect,

    .user_data = chip
  };

  i2c_init(&i2c_config);
}
