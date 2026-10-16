# Adaptive humidity and air-circulation control for an acoustic guitar cabinet

Supplementary files for the manuscript *Adaptive Humidity and Air Circulation Control for Acoustic Guitar Cabinet Using Microcontroller-Based Mamdani Fuzzy Logic* (Journal of Applied Science, Technology & Humanities, under review).

All results come from simulation in MATLAB R2025b and the Wokwi ESP32 simulator. No physical cabinet, sensor, or actuator was built.

## Contents

| File | Description | Used in the manuscript |
|---|---|---|
| `MATLAB/LemariGitar.fis` | Mamdani FIS: 2 inputs, 3 outputs, 9 rules, MIN–MAX inference, centroid | Tables 2–3, Figures 5–9 |
| `Tabel_uji.csv` | MATLAB centroids and PWM at the 17 test points | Tables 4–5 |
| `Tabel_perbandingan_MATLAB_Wokwi.csv` | PWM from MATLAB and from the Wokwi firmware, with differences | ESP32 Simulation in Wokwi |
| `Tabel_sensitivitas.csv` | MATLAB PWM at 24 °C for RH reading errors of −5 to +5% RH | Table 6 |
| `PROGRAM/sketch.ino` | ESP32 firmware | Algorithm 1, Figures 13–15 |
| `PROGRAM/diagram.json` | Wokwi circuit | Figure 3, Table 1 |
| `PROGRAM/libraries.txt` | Arduino libraries used by the firmware | |
| `Gambar_*.png`, screenshots | Source images of the figures | Figures |

Column names are in Indonesian: *Kasus* = case, *Suhu* = temperature (°C), *Kipas* = fan, *Inti* = core point, *Transisi* = transition point, *Galat* = reading error (% RH), *selisih* = difference.

## Reproducing the results

**MATLAB.** Load the FIS with `fis = readfis('MATLAB/LemariGitar.fis');` and evaluate a point with `y = evalfis(fis, [T RH]);`. The outputs are ordered humidifier, dehumidifier, fan. Map each centroid to PWM with `PWM = (y - 19.1842)/(80.8158 - 19.1842)*100`, then set values below 10% to 0 (dead-band). The constants are the discrete centroids of the full Low and High output sets (101 sample points).

**Wokwi.** Open https://wokwi.com/projects/476931824202426369 or load `PROGRAM/` into a new ESP32 project.
- `MODE_UJI 1` prints the 17 test points to the Serial Monitor in CSV format and holds each point for 3 s.
- `MODE_UJI 0` reads the simulated DHT22 slider every 2 s. If a reading is invalid (NaN), the firmware skips the update and the LEDs keep their last duty cycle.

PWM is generated at 5 kHz with 8-bit resolution on GPIO25 (humidifier), GPIO26 (dehumidifier), and GPIO27 (fan).
