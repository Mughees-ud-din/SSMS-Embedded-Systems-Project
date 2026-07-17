# 📂 simulation — Proteus Simulation Files

This folder contains the complete Proteus circuit simulation for all 4 modules of the Smart School Management System.

---

## 📄 Files

| File | Description |
|------|-------------|
| `SSMS_Simulation.pdsprj` | Main Proteus project file containing all 4 module circuits |

---

## ▶️ How to Open

1. Install **Proteus 8 Professional** (or later)
2. Open Proteus
3. Go to `File` → `Open Project`
4. Select `SSMS_Simulation.pdsprj`
5. The full circuit will load with all components

---

## ⚙️ Simulation Settings

| Setting | Value |
|---------|-------|
| Microcontroller | AT89C51 |
| Crystal Frequency | 11.0592 MHz |
| LCD Component | LM016L (16x2)&(20x4) |
| RTC | DS1307 |
| Power Supply | 5V DC |

---

## 📌 How to Run the Simulation

1. Open the project in Proteus
2. Load the compiled `.hex` file into the AT89C51:
   - Right-click the 8051 component
   - Select **Edit Properties**
   - Under **Program File**, browse to the `.hex` file from the `firmware/` folder
3. Press the **Play** button (green triangle) to start simulation
4. Observe the LCD outputs, relay triggers, and button responses

---

## ⚠️ Notes

- The attendance module is simulated using keypad input.
- Set simulation speed to **Real Time** for accurate RTC and timer behavior.
- always add external **10kΩ pull-up resistors** on P0.0–P0.7.
- If the LCD shows garbage characters, check the contrast (Vo) pin connection and potentiometer setting.

---

> Simulation was completed and verified before hardware assembly. All bugs were fixed in simulation first.
