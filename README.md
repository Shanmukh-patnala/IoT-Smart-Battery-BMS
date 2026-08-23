# IoT-Based Smart Battery Health and End-of-Life Prediction System

An ESP32-based battery monitoring and predictive Battery Management System (BMS) prototype that combines INA219 electrical measurements, DS18B20 temperature sensing, Coulomb-counting based SoC estimation, degradation-based SoH estimation, cycle tracking, Remaining Useful Life (RUL) estimation, CSV logging, and a Python analytics pipeline.

## Architecture

```text
18650 / Battery Model
        |
        +---- INA219 (I2C) ----> Voltage / Current / Power
        |
        +---- DS18B20 ---------> Temperature
        |
       ESP32
        |
        +---- SoC estimation
        +---- SoH estimation
        +---- Cycle counting
        +---- Capacity degradation
        +---- RUL estimation
        |
     Serial CSV
        |
   Python analytics
        |
   Dataset / prediction
```

## Features

- ESP32 target
- INA219-compatible custom Wokwi sensor model
- DS18B20 temperature monitoring
- Voltage, current and power monitoring
- Coulomb counting for accumulated charge
- SoC estimation from battery voltage
- Capacity-based SoH estimation
- Equivalent full-cycle tracking
- Remaining Useful Life estimate in cycles
- Safety status classification: NORMAL / UNDER_VOLTAGE / OVER_VOLTAGE / OVER_CURRENT / OVER_TEMPERATURE
- CSV serial output for ML/data analysis
- Python feature extraction and RUL estimation
- Wokwi simulation files included

## Repository structure

```text
smart-battery-bms/
├── sketch.ino
├── diagram.json
├── libraries.txt
├── ina219.chip.c
├── ina219.chip.json
├── data/
│   └── sample_battery_data.csv
├── python/
│   ├── bms_logger.py
│   └── rul_predictor.py
├── docs/
│   └── architecture.md
├── .gitignore
├── LICENSE
└── README.md
```

## Simulation

Open the project in Wokwi and start the simulation. The serial monitor reports:

- Voltage
- Current
- Power
- Temperature
- SoC
- SoH
- Accumulated charge
- Cycle count
- Available capacity
- Remaining life
- Safety status

The custom INA219 model uses I2C address `0x40`.

## Default battery model

| Parameter | Value |
|---|---:|
| Nominal capacity | 2600 mAh |
| Initial SoH | 100 % |
| Initial SoC | ~100 % |
| Nominal full voltage | 4.20 V |
| Minimum voltage | 3.00 V |
| Maximum voltage | 4.20 V |
| Current limit | 1000 mA |
| Temperature limit | 45 °C |
| End-of-life SoH | 80 % |
| Nominal life | 400 cycles |

These are simulation parameters, not a substitute for a production battery protection system.

## Serial CSV format

```text
DATA,Voltage=4.196,Current=150.00,Power=629.40,Temperature=22.00,SoC=99.60,SoH=100.00,Cycles=0.000123,Capacity=2600.00,RUL=400.00,Status=NORMAL
```

## Python analytics

`python/rul_predictor.py` can consume exported CSV records and generate degradation/RUL estimates.

Install:

```bash
pip install pandas numpy matplotlib scikit-learn
```

Run:

```bash
python python/rul_predictor.py data/sample_battery_data.csv
```

## Important engineering note

This is an educational/research prototype. RUL is an engineering estimate based on the simulated degradation model. A production BMS requires validated battery characterization, protection hardware, calibrated sensors, thermal validation, fault handling, and a validated electrochemical/ML model.

## Author

**Shanmukh Patnala**

GitHub: https://github.com/Shanmukh-patnala

## Visual documentation

The repository includes four engineering diagrams under `docs/diagrams/`:

- `system-architecture.svg` — complete embedded-to-analytics architecture
- `wokwi-connections.svg` — ESP32, custom INA219, and DS18B20 simulation connections
- `data-flow.svg` — telemetry path from sensors to Python analytics
- `prediction-flow.svg` — SoH and RUL estimation flow

These SVG files render directly in GitHub and can also be used in project reports and presentations.
