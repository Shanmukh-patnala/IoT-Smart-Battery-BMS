#!/usr/bin/env python3
"""
Simple serial logger for the ESP32 BMS.

The ESP32 emits:
DATA,Voltage=4.196,Current=150.00,...

Usage:
    python python/bms_logger.py COM5 data/runtime/bms.csv
"""

from __future__ import annotations
import csv
import re
import sys
import time

try:
    import serial
except ImportError:
    print("Install pyserial first: pip install pyserial")
    raise SystemExit(1)

FIELDS = [
    "Voltage", "Current", "Power", "Temperature",
    "SoC", "SoH", "Cycles", "Capacity", "RUL", "Status"
]


def parse(line: str):
    if not line.startswith("DATA,"):
        return None

    values = {}
    for field in FIELDS:
        match = re.search(rf"{field}=([^,\\r\\n]+)", line)
        if match:
            values[field] = match.group(1)

    if len(values) != len(FIELDS):
        return None

    return values


def main():
    if len(sys.argv) != 3:
        print("Usage: python python/bms_logger.py <serial_port> <output_csv>")
        raise SystemExit(1)

    port = sys.argv[1]
    output = sys.argv[2]

    ser = serial.Serial(port, 115200, timeout=1)

    with open(output, "a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["timestamp"] + FIELDS)
        if f.tell() == 0:
            writer.writeheader()

        print(f"Logging {port} -> {output}")

        try:
            while True:
                line = ser.readline().decode(errors="ignore").strip()
                row = parse(line)
                if row:
                    row["timestamp"] = time.time()
                    writer.writerow(row)
                    f.flush()
                    print(row)
        except KeyboardInterrupt:
            print("\nLogging stopped.")
        finally:
            ser.close()


if __name__ == "__main__":
    main()
