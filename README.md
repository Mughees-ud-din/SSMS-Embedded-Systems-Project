# 🏫 Smart School Management System (SSMS)

> An embedded systems project built on the **8051 Microcontroller** to automate key daily operations in schools — developed by 2nd-year Electrical Engineering students at UET Peshawar, Nowshera Campus.

---

## 📌 Project Overview

We surveyed **7 schools** across the Lar and Rangpur region of Pakistan to identify real operational problems. Based on the survey data, we designed and built a working embedded system that solves four of the most common problems.

This is not a theoretical project. Every module was simulated in **Proteus**, coded in **Keil uVision (C51)**, and demonstrated on real hardware.

---

## ⚡ Modules

| # | Module | Description |
|---|--------|-------------|
| 1 | 🔔 **Automatic Bell System** | RTC-based bell that rings automatically at the start and end of every lecture period. No manual effort needed. |
| 2 | 📋 **Teacher-Class Mapping Board** | LCD display in the school hall showing which teacher is in which classroom, plus the current lecture number. when a teacher enters in class he/she presses a button and principal get notifies of their presence. |
| 3 | 🚨 **Emergency Siren** | A single button in the main hall triggers a school-wide emergency siren immediately. Uses external interrupt (INT0) for instant response. |
| 4 | ✅ **Keypad Student Attendance** | Students enter their unique ID on a 4x3 keypad. Attendance is marked automatically and displayed on an LCD. Eliminates manual attendance completely. |

---

## 🛠️ Tools & Hardware

| Category | Details |
|----------|---------|
| Microcontroller | AT89C51 (8051) |
| IDE | Keil uVision (C51 Compiler) |
| Simulation | Proteus 8 Professional |
| RTC Module | DS1307 |
| Display | 16x2 LCD (multiple) |
| Input | 4x3 Matrix Keypad, Push Buttons |
| Output | Relay Module, Buzzer / Siren |
| Power | 5V DC Regulated Supply |

---

## 📁 Repository Structure

```
SSMS/
│
├── 📂 docs/                        # Project documents
│   ├── SSMS_Project_Proposal.pdf
│   └── SSMS_Final_Report.pdf
│
├── 📂 simulation/                  # Proteus simulation files
│   ├── SSMS_Simulation.pdsprj
│   └── README.md
│
├── 📂 firmware/                    # Keil C source code
│   ├── bell_system.c
│   ├── teacher_board.c
│   ├── emergency_siren.c
│   ├── attendance_keypad.c
│   ├── main.c
│   └── README.md
│
├── 📂 media/                       # Hardware photos and demo video
│   ├── demo_video.mp4
│   ├── hardware_full.jpg
│   ├── proteus_simulation.png
│   ├── keil_code.png
│   └── README.md
│
└── README.md                       # You are here
```

---

## 🔬 Survey Background

Before building anything, we conducted a field survey across **7 schools**:

- Government Higher Secondary School Lar
- Government Primary School Lar
- Sadaqat Public School Lar
- Pak Pearl School Rangpur
- Al-Abbas Science School & College
- Government High School Kacha Mali Kheil
- Garrison High School kirri khesor

**Key findings that shaped this project:**
- Manual attendance was the biggest daily time waster
- Teachers frequently missed classrooms with no way to alert the principal
- No emergency alert system existed in any of the schools
- Manual bell ringing caused delays between periods

---

## 🎯 Results

All **4 modules** were successfully demonstrated and worked perfectly in the final demo.

- ✅ Bell rings automatically at correct times using DS1307 RTC
- ✅ Teacher board updates in real time across 3 classrooms
- ✅ Emergency siren activates instantly on button press
- ✅ Keypad attendance records all 5 test student IDs correctly

---

## 🔮 Future Upgrades

This system is designed to be expandable:

1. **Mobile App** — Change teacher names, bell schedule, and class assignments remotely
2. **Digital School Records** — Full student and teacher record management through the app
3. **Automatic Complaint System** — Complaints auto-routed to the relevant staff member
4. **Bus Tracking & Parent Alerts** — Real-time student location notifications for parents

---

## 👥 Team members

| Member |
|--------|
| Mughees ud Din|
| Muhammad Abuzar Khan|
| Khyber Khan|
| Muhammad Danish|
| Faizan Ullah|

**Supervisor:** Dr. Irfan Ahmad
**Institution:** University of Engineering & Technology Peshawar, Nowshera Campus
**Course:** EE 326 — Microcontroller & Embedded Systems
**Semester:** 4th Semester, Spring-2026

---

## 📄 License

This project is open for educational use. If you use any part of this work, please give credit to the team.

---

*Built with real survey data. Solving real school problems.*
