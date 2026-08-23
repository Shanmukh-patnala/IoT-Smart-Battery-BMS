#include <Wire.h>
#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================================
// SMART BATTERY BMS - ESP32
// ============================================================

static const float BATTERY_CAPACITY_MAH = 2600.0f;
static const float NOMINAL_LIFE_CYCLES = 400.0f;
static const float EOL_SOH_PERCENT = 80.0f;

static const float MIN_VOLTAGE = 3.00f;
static const float MAX_VOLTAGE = 4.20f;
static const float MAX_CURRENT_MA = 1000.0f;
static const float MAX_TEMPERATURE_C = 45.0f;

static const uint8_t INA219_ADDRESS = 0x40;
static const int DS18B20_PIN = 4;

Adafruit_INA219 ina219(INA219_ADDRESS);
OneWire oneWire(DS18B20_PIN);
DallasTemperature temperatureSensor(&oneWire);

struct BatteryStatus {
  float voltage;
  float current_mA;
  float power_mW;
  float temperature_C;

  float soc;
  float soh;
  float accumulated_mAh;
  float cycle_count;
  float available_capacity_mAh;
  float remaining_life_cycles;

  const char* status;
};

BatteryStatus batteryStatus = {};

unsigned long lastSampleMs = 0;
unsigned long lastPrintMs = 0;

// ------------------------------------------------------------
// Utility
// ------------------------------------------------------------

static float clampFloat(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

// Voltage-based SoC model for a single-cell Li-ion demonstration.
// A real BMS should use a calibrated OCV/SOC curve.
static float calculateSoC(float voltage) {
  float soc = ((voltage - MIN_VOLTAGE) /
               (MAX_VOLTAGE - MIN_VOLTAGE)) * 100.0f;

  return clampFloat(soc, 0.0f, 100.0f);
}

// Capacity fade model:
// 100% SoH at cycle 0 -> 80% SoH at 400 equivalent cycles.
static float calculateSoH(float cycles) {
  float soh = 100.0f -
              ((100.0f - EOL_SOH_PERCENT) /
               NOMINAL_LIFE_CYCLES) * cycles;

  return clampFloat(soh, EOL_SOH_PERCENT, 100.0f);
}

static float calculateRUL(float cycles) {
  float remaining = NOMINAL_LIFE_CYCLES - cycles;
  return remaining < 0.0f ? 0.0f : remaining;
}

static const char* calculateSafetyStatus(
    float voltage,
    float current_mA,
    float temperature_C) {

  if (voltage < MIN_VOLTAGE)
    return "UNDER_VOLTAGE";

  if (voltage > MAX_VOLTAGE)
    return "OVER_VOLTAGE";

  if (current_mA > MAX_CURRENT_MA)
    return "OVER_CURRENT";

  if (temperature_C > MAX_TEMPERATURE_C)
    return "OVER_TEMPERATURE";

  return "NORMAL";
}

// ------------------------------------------------------------
// Battery state update
// ------------------------------------------------------------

static void updateBatteryStatus(
    float voltage,
    float current_mA,
    float power_mW,
    float temperature_C,
    float elapsedSeconds) {

  // Coulomb counting / charge accumulation.
  // Current is assumed to be load/discharge current.
  if (current_mA > 0.0f && elapsedSeconds > 0.0f) {
    batteryStatus.accumulated_mAh +=
        current_mA * elapsedSeconds / 3600.0f;
  }

  // One equivalent full discharge cycle = capacity consumed.
  batteryStatus.cycle_count =
      batteryStatus.accumulated_mAh / BATTERY_CAPACITY_MAH;

  batteryStatus.voltage = voltage;
  batteryStatus.current_mA = current_mA;
  batteryStatus.power_mW = power_mW;
  batteryStatus.temperature_C = temperature_C;

  batteryStatus.soc = calculateSoC(voltage);
  batteryStatus.soh = calculateSoH(batteryStatus.cycle_count);

  batteryStatus.available_capacity_mAh =
      BATTERY_CAPACITY_MAH * batteryStatus.soh / 100.0f;

  batteryStatus.remaining_life_cycles =
      calculateRUL(batteryStatus.cycle_count);

  batteryStatus.status =
      calculateSafetyStatus(
          voltage,
          current_mA,
          temperature_C);
}

// ------------------------------------------------------------
// Human-readable status
// ------------------------------------------------------------

static void printBMSStatus() {
  Serial.println();
  Serial.println("==============================================");
  Serial.println("              BATTERY BMS STATUS");
  Serial.println("==============================================");

  Serial.print("Voltage            : ");
  Serial.print(batteryStatus.voltage, 3);
  Serial.println(" V");

  Serial.print("Current            : ");
  Serial.print(batteryStatus.current_mA, 2);
  Serial.println(" mA");

  Serial.print("Power              : ");
  Serial.print(batteryStatus.power_mW, 2);
  Serial.println(" mW");

  Serial.print("Temperature        : ");
  Serial.print(batteryStatus.temperature_C, 2);
  Serial.println(" C");

  Serial.print("SoC                : ");
  Serial.print(batteryStatus.soc, 2);
  Serial.println(" %");

  Serial.print("SoH                : ");
  Serial.print(batteryStatus.soh, 2);
  Serial.println(" %");

  Serial.print("Accumulated        : ");
  Serial.print(batteryStatus.accumulated_mAh, 4);
  Serial.println(" mAh");

  Serial.print("Cycle Count        : ");
  Serial.print(batteryStatus.cycle_count, 6);
  Serial.println();

  Serial.print("Available Capacity : ");
  Serial.print(batteryStatus.available_capacity_mAh, 2);
  Serial.println(" mAh");

  Serial.print("Remaining Life     : ");
  Serial.print(batteryStatus.remaining_life_cycles, 2);
  Serial.println(" cycles");

  Serial.print("STATUS             : ");
  Serial.println(batteryStatus.status);

  Serial.println("==============================================");
}

// ------------------------------------------------------------
// Machine-readable CSV-like record
// ------------------------------------------------------------

static void printCSV() {
  Serial.print("DATA,Voltage=");
  Serial.print(batteryStatus.voltage, 3);

  Serial.print(",Current=");
  Serial.print(batteryStatus.current_mA, 2);

  Serial.print(",Power=");
  Serial.print(batteryStatus.power_mW, 2);

  Serial.print(",Temperature=");
  Serial.print(batteryStatus.temperature_C, 2);

  Serial.print(",SoC=");
  Serial.print(batteryStatus.soc, 2);

  Serial.print(",SoH=");
  Serial.print(batteryStatus.soh, 2);

  Serial.print(",Cycles=");
  Serial.print(batteryStatus.cycle_count, 6);

  Serial.print(",Capacity=");
  Serial.print(batteryStatus.available_capacity_mAh, 2);

  Serial.print(",RUL=");
  Serial.print(batteryStatus.remaining_life_cycles, 2);

  Serial.print(",Status=");
  Serial.println(batteryStatus.status);
}

// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("==============================================");
  Serial.println("          WOKWI SMART 18650 BMS");
  Serial.println("==============================================");
  Serial.println("BMS System Ready.");
  Serial.println();

  Wire.begin();

  if (!ina219.begin()) {
    Serial.println("ERROR: INA219 not detected at 0x40.");
    while (true) {
      delay(1000);
    }
  }

  // Suitable range for the simulated 0-2 A load.
  ina219.setCalibration_32V_2A();

  temperatureSensor.begin();

  batteryStatus.accumulated_mAh = 0.0f;
  batteryStatus.cycle_count = 0.0f;
  batteryStatus.soh = 100.0f;
  batteryStatus.remaining_life_cycles = NOMINAL_LIFE_CYCLES;

  lastSampleMs = millis();
  lastPrintMs = 0;
}

// ------------------------------------------------------------
// Main loop
// ------------------------------------------------------------

void loop() {
  unsigned long now = millis();

  // Sample every 500 ms.
  if (now - lastSampleMs < 500)
    return;

  float elapsedSeconds =
      (now - lastSampleMs) / 1000.0f;

  lastSampleMs = now;

  // INA219 measurements.
  float busVoltage = ina219.getBusVoltage_V();
  float shuntVoltage_mV = ina219.getShuntVoltage_mV();
  float current_mA = ina219.getCurrent_mA();

  // Battery-side voltage includes the shunt drop.
  float loadVoltage =
      busVoltage + (shuntVoltage_mV / 1000.0f);

  float power_mW =
      loadVoltage * current_mA;

  // DS18B20 measurement.
  temperatureSensor.requestTemperatures();
  float temperature_C =
      temperatureSensor.getTempCByIndex(0);

  if (temperature_C == DEVICE_DISCONNECTED_C) {
    temperature_C = 25.0f;
  }

  updateBatteryStatus(
      loadVoltage,
      current_mA,
      power_mW,
      temperature_C,
      elapsedSeconds);

  // Print detailed status every 2 seconds.
  if (now - lastPrintMs >= 2000) {
    lastPrintMs = now;

    printBMSStatus();
    printCSV();
  }
}
