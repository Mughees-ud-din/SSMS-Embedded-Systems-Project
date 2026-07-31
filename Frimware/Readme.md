# 📂 firmware — Keil C Source Code

This folder contains all C51 source code written in **Keil uVision** for the 8051 microcontroller.

---

## 🛠️ How to Compile

1. Install **Keil uVision 4** or later
2. Open Keil and create a new project
3. Select **AT89C51** as the target microcontroller
4. Add all `.c` files from this folder to the project
5. Set crystal frequency to **11.0592 MHz** in target settings
6. Build the project (`F7` or `Build` button)
7. The output `.hex` file will be generated in the project folder
8. Load this `.hex` into Proteus or flash it onto the physical 8051

---

## ⚙️ Hardware Configuration

| 8051 Pin | Connected To | Module |
|----------|-------------|--------|
| P1.6 (SCL) | DS1307 SCL | Bell System |
| P1.7 (SDA) | DS1307 SDA | Bell System |
| P3.4 | Bell Relay | Bell System |
| P0.0–P0.7 | LCD Data Bus (shared) | Teacher Board |
| P2.1–P2.4 | LCD Enable pins (individual) | Teacher Board |
| P2.0 | LCD RS (shared) | Teacher Board |
| P3.5 | Class 1 button | Teacher Board |
| P3.6 | Class 2 button | Teacher Board |
| P3.7 | Class 3 button | Teacher Board |
| P2.5 | Principal LCD RS | Teacher Board |
| P2.6 | Principal LCD EN | Teacher Board |
| P3.2 (INT0) | Emergency button | Emergency Siren |
| P3.3 | Siren relay | Emergency Siren |
| P2.0–P2.7 | Attendance LCD data | Attendance |
| P3.0 | Attendance LCD RS | Attendance |
| P3.1 | Attendance LCD EN | Attendance |

---

## 📌 Important Notes for Developers

- **I2C for DS1307** is implemented as software I2C (bit-banging) on P1.6 and P1.7. The 8051 has no hardware I2C.
- **Emergency button** uses `IT0 = 1` in the TCON register — falling edge trigger. This gives instant response without polling.
- **Bell schedule** is stored as an array of 24-hour time values (e.g., 800 = 8:00 AM, 845 = 8:45 AM).
- **Attendance IDs** are stored as a hardcoded lookup table of 5 student IDs and names for the prototype.
- **All LCD displays** share the P0 data bus. Individual LCDs are selected by toggling their separate EN pins.
- Do not use `delay()` for the 5-minute missing-teacher timer. Use **Timer 0 interrupt** to count elapsed time without blocking the main loop.

---

> Code is written in C51 and compiled with Keil. Do not use Arduino IDE or standard C compilers — they are not compatible with 8051 C51 syntax.
