#include <reg51.h>

// =========================================
//  SMART SCHOOL MANAGEMENT SYSTEM (SSMS)
//  Single AT89S51 @ 11.0592 MHz
// =========================================

// -- RTC I2C pins --
sbit SDA = P1^1;
sbit SCL = P1^0;

// -- Classroom teacher-arrived buttons --
sbit CLASS1_BTN = P1^2;
sbit CLASS2_BTN = P1^3;
sbit CLASS3_BTN = P1^4;

// -- Yellow LEDs (teacher missing warning) --
sbit LED_MISS_C1 = P1^0;
sbit LED_MISS_C2 = P1^6;
sbit LED_MISS_C3 = P1^7;

// -- Keypad Row pins --
sbit ROW1 = P3^0;   // Keypad pin A
sbit ROW2 = P3^1;   // Keypad pin B
sbit ROW3 = P3^2;   // Keypad pin C
sbit ROW4 = P3^3;   // Keypad pin D

// -- Keypad Column pins --
sbit COL1 = P3^4;   // Keypad column 1
sbit COL2 = P3^5;   // Keypad column 2
sbit COL3 = P3^6;   // Keypad column 3

// -- Emergency button --
sbit EMG_BTN = P3^7;

// -- Main Hall LCD1 data pins (4-bit mode) --
sbit LCD1_D4 = P2^0;
sbit LCD1_D5 = P2^1;
sbit LCD1_D6 = P2^2;
sbit LCD1_D7 = P2^3;

// -- LCD3 control (Principal Office 16x2) --
sbit LCD3_RS = P2^4;
sbit LCD3_EN = P2^5;

// -- Green LED (attendance confirmed) --
sbit LED_ATTEND = P2^6;

// -- Single shared buzzer --
sbit BUZZER = P2^7;

// -- LCD1 control (Main Hall 20x4) --
sbit LCD1_RS = P0^0;
sbit LCD1_EN = P0^1;

// -- LCD2 control (Attendance 16x2) --
sbit LCD2_RS = P0^2;
sbit LCD2_EN = P0^3;

/*
   LCD2 and LCD3 share P0.4-P0.7 as D4-D7.
   They remain separate because each LCD has its own RS and EN pins.
*/

// =========================================
//  DS1307 RAM LAYOUT
// =========================================
#define SCHED_FLAG_ADDR   0x08  // DS1307 RAM flag address
#define SCHED_FLAG_VAL    0xAA
#define SCHED_BASE_ADDR   0x09  // Schedule starts at 0x09
#define MAX_PERIODS       8
#define SCHOOL_END_IDX    7
#define ATTEND_COUNT_ADDR 0x20  // DS1307 RAM address for attendance count

// =========================================
//  TEACHER & STUDENT DATA (code memory)
// =========================================
unsigned char code CLASS1_TEACHER[] = "Mughees ";
unsigned char code CLASS2_TEACHER[] = "Abuzar  ";
unsigned char code CLASS3_TEACHER[] = "Faizan  ";

unsigned char code STU_ID[5][4] = {
    "001","002","003","004","005"
};
unsigned char code STU_NAME[5][9] = {
    "Ali Khan",
    "Umar Shah",
    "Zaid Ali ",
    "Hassan  ",
    "Bilal   "
};
#define STU_COUNT 5

// =========================================
//  GLOBAL STATE
// =========================================
unsigned char cur_hr  = 8;
unsigned char cur_mn  = 0;
unsigned char cur_sec = 0;

unsigned char schedule[MAX_PERIODS][2];

// FIX: active_period stays set from period start
// until NEXT period starts (not just for one minute)
unsigned char active_period   = 0xFF;
unsigned char last_period_idx = 0xFF; // tracks last period that rang

bit bell_already_rung    = 0;
bit teacher_present_c1   = 0;
bit teacher_present_c2   = 0;
bit teacher_present_c3   = 0;
bit emergency_active     = 0;
bit all_periods_done     = 0;
bit classes_over         = 0;  // set when school end time reached

unsigned char attend_count = 0;  // persists in DS1307 RAM

#define MODE_NORMAL   0
#define MODE_SCHEDULE 1
#define MODE_ATTEND   2
#define MODE_TIME     3
unsigned char kpad_mode = MODE_NORMAL;

unsigned char input_buf[6];
unsigned char input_idx = 0;

unsigned char lcd3_cycle_step  = 0;
unsigned int  lcd3_cycle_timer = 0;
#define LCD3_LECTURE_TICKS  2
#define LCD3_CLASS_TICKS    4

// =========================================
//  DELAY FUNCTIONS
// =========================================
void delay_us(unsigned int us) {
    unsigned int i;
    for(i = 0; i < us; i++);
}

void delay_ms(unsigned int ms) {
    unsigned int i, j;
    for(i = 0; i < ms; i++)
        for(j = 0; j < 120; j++);
}

// =========================================
//  I2C BIT-BANG
// =========================================
void i2c_start(void) {
    SDA=1; SCL=1; delay_us(5);
    SDA=0; delay_us(5); SCL=0;
}

void i2c_stop(void) {
    SDA=0; SCL=1; delay_us(5);
    SDA=1; delay_us(5);
}

void i2c_write_bit(bit b) {
    SDA=b; delay_us(2);
    SCL=1; delay_us(5);
    SCL=0; delay_us(2);
}

bit i2c_read_bit(void) {
    bit b;
    SDA=1; delay_us(2);
    SCL=1; delay_us(5);
    b=SDA; SCL=0;
    return b;
}

void i2c_write_byte(unsigned char val) {
    unsigned char i;
    for(i=0; i<8; i++) {
        i2c_write_bit((val & 0x80) ? 1 : 0);
        val <<= 1;
    }
    i2c_read_bit();
}

unsigned char i2c_read_byte(bit send_ack) {
    unsigned char val=0, i;
    for(i=0; i<8; i++)
        val = (val << 1) | i2c_read_bit();
    i2c_write_bit(!send_ack);
    return val;
}

// =========================================
//  DS1307 RTC FUNCTIONS
// =========================================
unsigned char bcd_to_dec(unsigned char b){ return ((b>>4)*10)+(b&0x0F); }
unsigned char dec_to_bcd(unsigned char d){ return ((d/10)<<4)|(d%10); }

void rtc_write_byte(unsigned char addr, unsigned char val) {
    i2c_start();
    i2c_write_byte(0xD0);
    i2c_write_byte(addr);
    i2c_write_byte(val);
    i2c_stop();
}

unsigned char rtc_read_byte(unsigned char addr) {
    unsigned char val;
    i2c_start();
    i2c_write_byte(0xD0);
    i2c_write_byte(addr);
    i2c_stop();
    i2c_start();
    i2c_write_byte(0xD1);
    val = i2c_read_byte(0);
    i2c_stop();
    return val;
}

void rtc_set_time(unsigned char hr, unsigned char mn, unsigned char sec) {
    i2c_start();
    i2c_write_byte(0xD0);
    i2c_write_byte(0x00);
    i2c_write_byte(dec_to_bcd(sec));
    i2c_write_byte(dec_to_bcd(mn));
    i2c_write_byte(dec_to_bcd(hr));
    i2c_write_byte(0x01);
    i2c_write_byte(0x01);
    i2c_write_byte(dec_to_bcd(6));
    i2c_write_byte(dec_to_bcd(26));
    i2c_write_byte(0x00);
    i2c_stop();
}

void rtc_read_time(void) {
    unsigned char raw_sec;
    unsigned char raw_mn;
    unsigned char raw_hr;
    unsigned char new_sec;
    unsigned char new_mn;
    unsigned char new_hr;

    i2c_start();
    i2c_write_byte(0xD0);
    i2c_write_byte(0x00);
    i2c_stop();

    i2c_start();
    i2c_write_byte(0xD1);

    raw_sec = i2c_read_byte(1);
    raw_mn  = i2c_read_byte(1);
    raw_hr  = i2c_read_byte(0);

    i2c_stop();

    /* Remove the CH bit from seconds and unused bit 7 from minutes */
    new_sec = bcd_to_dec(raw_sec & 0x7F);
    new_mn  = bcd_to_dec(raw_mn  & 0x7F);

    /*
       DS1307 hour register supports both 24-hour and 12-hour mode.

       In 12-hour mode:
       bit 6 = 1
       bit 5 = PM flag
       bits 4..0 = BCD hour 1..12

       Without this conversion a value such as 0x73 is displayed as 73,
       which caused the incorrect time visible on the Main Hall LCD.
    */
    if(raw_hr & 0x40) {
        unsigned char hr12;

        hr12 = bcd_to_dec(raw_hr & 0x1F);

        if(hr12 == 12)
            new_hr = (raw_hr & 0x20) ? 12 : 0;
        else
            new_hr = (raw_hr & 0x20) ? (hr12 + 12) : hr12;
    } else {
        new_hr = bcd_to_dec(raw_hr & 0x3F);
    }

    /*
       Accept only valid RTC values. If the I2C bus momentarily returns
       invalid data, retain the previously valid time instead of showing
       impossible values such as 73:05:05.
    */
    if(new_sec <= 59 && new_mn <= 59 && new_hr <= 23) {
        cur_sec = new_sec;
        cur_mn  = new_mn;
        cur_hr  = new_hr;
    }
}

void rtc_save_period(unsigned char idx, unsigned char hr, unsigned char mn) {
    unsigned char addr = SCHED_BASE_ADDR + (idx * 2);
    rtc_write_byte(addr,     hr);
    rtc_write_byte(addr + 1, mn);
    rtc_write_byte(SCHED_FLAG_ADDR, SCHED_FLAG_VAL);
}

void rtc_load_schedule(void) {
    unsigned char i;
    unsigned char addr;
    unsigned char flag;

    flag = rtc_read_byte(SCHED_FLAG_ADDR);

    if(flag == SCHED_FLAG_VAL) {
        for(i=0; i<MAX_PERIODS; i++) {
            addr = SCHED_BASE_ADDR + (i * 2);

            schedule[i][0] = rtc_read_byte(addr);
            schedule[i][1] = rtc_read_byte(addr + 1);

            /*
               Reject corrupted or uninitialized RTC RAM values.
               A valid schedule must have hour 0..23 and minute 0..59.
            */
            if(schedule[i][0] > 23 || schedule[i][1] > 59) {
                schedule[i][0] = 0;
                schedule[i][1] = 0;
            }
        }
    } else {
        /*
           No saved schedule exists. Clear all entries so the Main Hall
           LCD shows:
               Lec:--  Nxt:None
           instead of reading random DS1307 RAM as a schedule.
        */
        for(i=0; i<MAX_PERIODS; i++) {
            schedule[i][0] = 0;
            schedule[i][1] = 0;
        }
    }
}

// =========================================
//  LCD CORE (4-bit mode)
// =========================================
void lcd_send_nibble(unsigned char n, unsigned char nib, unsigned char is_data) {
    /*
       LCD1 uses P2.0-P2.3 for D4-D7.
       LCD2 and LCD3 share P0.4-P0.7 for D4-D7.
       No display function or command sequence is changed.
    */
    if(n==1) {
        LCD1_D4 = (nib & 0x10) ? 1 : 0;
        LCD1_D5 = (nib & 0x20) ? 1 : 0;
        LCD1_D6 = (nib & 0x40) ? 1 : 0;
        LCD1_D7 = (nib & 0x80) ? 1 : 0;
    } else {
        P0 = (P0 & 0x0F) | (nib & 0xF0);
    }

    if     (n==1) LCD1_RS=is_data;
    else if(n==2) LCD2_RS=is_data;
    else          LCD3_RS=is_data;
    delay_us(1);
    if(n==1)      {LCD1_EN=1;delay_us(2);LCD1_EN=0;}
    else if(n==2) {LCD2_EN=1;delay_us(2);LCD2_EN=0;}
    else          {LCD3_EN=1;delay_us(2);LCD3_EN=0;}
    delay_us(2);
}

void lcd_write(unsigned char n, unsigned char byte, unsigned char is_data) {
    lcd_send_nibble(n, byte,      is_data);
    lcd_send_nibble(n, byte << 4, is_data);
    delay_ms(2);
}

void lcd_cmd (unsigned char n, unsigned char c) { lcd_write(n,c,0); }
void lcd_char(unsigned char n, unsigned char d) { lcd_write(n,d,1); }

void lcd_str(unsigned char n, unsigned char *s) {
    while(*s) lcd_char(n,*s++);
}

void lcd_str_code(unsigned char n, unsigned char code *s) {
    while(*s) lcd_char(n,*s++);
}

void lcd_goto_16x2(unsigned char n, unsigned char r, unsigned char c) {
    lcd_cmd(n, (r==0?0x80:0xC0)+c);
}

void lcd_goto_20x4(unsigned char n, unsigned char r, unsigned char c) {
    unsigned char addr;
    if     (r==0) addr=0x80+c;
    else if(r==1) addr=0xC0+c;
    else if(r==2) addr=0x94+c;
    else          addr=0xD4+c;
    lcd_cmd(n,addr);
}

void lcd_2digit(unsigned char n, unsigned char v) {
    lcd_char(n,'0'+v/10);
    lcd_char(n,'0'+v%10);
}

void lcd_clear(unsigned char n) { lcd_cmd(n,0x01); delay_ms(2); }

void lcd_init_single(unsigned char n) {
    delay_ms(20);
    lcd_send_nibble(n,0x30,0); delay_ms(5);
    lcd_send_nibble(n,0x30,0); delay_ms(1);
    lcd_send_nibble(n,0x30,0); delay_ms(1);
    lcd_send_nibble(n,0x20,0); delay_ms(1);
    lcd_cmd(n,0x28); lcd_cmd(n,0x0C);
    lcd_cmd(n,0x06); lcd_cmd(n,0x01);
    delay_ms(2);
}

// =========================================
//  KEYPAD SCAN (4x3)
// =========================================
unsigned char keypad_scan(void) {
    ROW1=ROW2=ROW3=ROW4=1; COL1=COL2=COL3=1;

    ROW1=0; delay_us(5);
    if(!COL1){delay_ms(10);if(!COL1){ROW1=1;return '1';}}
    if(!COL2){delay_ms(10);if(!COL2){ROW1=1;return '2';}}
    if(!COL3){delay_ms(10);if(!COL3){ROW1=1;return '3';}}
    ROW1=1;

    ROW2=0; delay_us(5);
    if(!COL1){delay_ms(10);if(!COL1){ROW2=1;return '4';}}
    if(!COL2){delay_ms(10);if(!COL2){ROW2=1;return '5';}}
    if(!COL3){delay_ms(10);if(!COL3){ROW2=1;return '6';}}
    ROW2=1;

    ROW3=0; delay_us(5);
    if(!COL1){delay_ms(10);if(!COL1){ROW3=1;return '7';}}
    if(!COL2){delay_ms(10);if(!COL2){ROW3=1;return '8';}}
    if(!COL3){delay_ms(10);if(!COL3){ROW3=1;return '9';}}
    ROW3=1;

    ROW4=0; delay_us(5);
    if(!COL1){delay_ms(10);if(!COL1){ROW4=1;return '*';}}
    if(!COL2){delay_ms(10);if(!COL2){ROW4=1;return '0';}}
    if(!COL3){delay_ms(10);if(!COL3){ROW4=1;return '#';}}
    ROW4=1;

    return 0;
}

void keypad_wait_release(void) {
    while(keypad_scan()); delay_ms(20);
}

// =========================================
//  STUDENT LOOKUP
// =========================================
unsigned char find_student(unsigned char *id) {
    unsigned char i, j, ok;
    for(i=0; i<STU_COUNT; i++) {
        ok=1;
        for(j=0; j<3; j++)
            if(id[j]!=STU_ID[i][j]){ok=0;break;}
        if(ok) return i;
    }
    return 0xFF;
}

// =========================================
//  SCHEDULE HELPERS
// =========================================
unsigned char to_12hr(unsigned char hr24, unsigned char *am_pm) {
    unsigned char hr12 = hr24;
    *am_pm = 0;
    if(hr24 == 0) {
        // Midnight 00:00 = 12:00 AM
        hr12   = 12;
        *am_pm = 0;
    } else if(hr24 < 12) {
        // 01:00 - 11:59 AM
        hr12   = hr24;
        *am_pm = 0;
    } else if(hr24 == 12) {
        // Noon 12:00 PM
        hr12   = 12;
        *am_pm = 1;
    } else {
        // 13:00 - 23:59 PM
        hr12   = hr24 - 12;
        *am_pm = 1;
    }
    return hr12;
}

void get_next_bell(unsigned char *nhr, unsigned char *nmn) {
    unsigned char i;
    // Only check lecture periods 0-6, NOT school end time (index 7)
    for(i=0; i<SCHOOL_END_IDX; i++) {
        if(schedule[i][0]==0 && schedule[i][1]==0) continue;
        if(schedule[i][0]>cur_hr ||
          (schedule[i][0]==cur_hr && schedule[i][1]>cur_mn)) {
            *nhr=schedule[i][0]; *nmn=schedule[i][1]; return;
        }
    }
    *nhr=0; *nmn=0;
}

// =========================================
//  LCD1 — MAIN HALL 20x4
//
//  FIX: Row 0 does NOT say "SMART BELL SYSTEM"
//  Row 0: Time HH:MM:SS AM/PM
//  Row 1: Lec:X  Nxt:HH:MM AM/PM
//  Row 2: C1:Mughees  C2:Abuzar
//  Row 3: C3:Faizan
// =========================================
void lcd1_show_normal(void) {
    unsigned char am_pm, hr12;
    unsigned char next_hr=0, next_mn=0;
    unsigned char next_hr12, next_ap;
    unsigned char schedule_set = 0;
    unsigned char i;

    hr12 = to_12hr(cur_hr, &am_pm);

    // Row 0: Time always shows
    lcd_goto_20x4(1,0,0);
    lcd_str(1,"Time:");
    lcd_2digit(1,hr12); lcd_char(1,':');
    lcd_2digit(1,cur_mn); lcd_char(1,':');
    lcd_2digit(1,cur_sec); lcd_char(1,' ');
    lcd_str(1,am_pm?"PM":"AM");
    lcd_str(1,"   ");

    // Check if any schedule is set
    for(i=0; i<MAX_PERIODS; i++) {
        if(schedule[i][0]!=0 || schedule[i][1]!=0) {
            schedule_set = 1; break;
        }
    }

    // Show dashes if: no schedule set OR classes over
    if(!schedule_set || classes_over) {
        lcd_goto_20x4(1,1,0); lcd_str(1,"Lec:--  Nxt:--:--   ");
        lcd_goto_20x4(1,2,0); lcd_str(1,"C1:------  C2:------");
        lcd_goto_20x4(1,3,0); lcd_str(1,"C3:------           ");
    } else {
        // Schedule is set and classes running
        get_next_bell(&next_hr, &next_mn);
        next_hr12 = to_12hr(next_hr, &next_ap);

        lcd_goto_20x4(1,1,0);
        if(active_period!=0xFF) {
            lcd_str(1,"Lec:");
            lcd_char(1,'0'+active_period+1);
        } else {
            lcd_str(1,"Lec:--");
        }
        lcd_str(1,"  Nxt:");
        if(next_hr==0 && next_mn==0) {
            lcd_str(1,"None      ");
        } else {
            lcd_2digit(1,next_hr12); lcd_char(1,':');
            lcd_2digit(1,next_mn);   lcd_char(1,' ');
            lcd_str(1,next_ap?"PM":"AM");
            lcd_str(1,"   ");
        }
        // Show teacher names ONLY when lecture is active
        if(active_period!=0xFF) {
            lcd_goto_20x4(1,2,0); lcd_str(1,"C1:Mughees  C2:Abzr ");
            lcd_goto_20x4(1,3,0); lcd_str(1,"C3:Faizan           ");
        } else {
            lcd_goto_20x4(1,2,0); lcd_str(1,"C1:------  C2:------");
            lcd_goto_20x4(1,3,0); lcd_str(1,"C3:------           ");
        }
    }
}

void lcd1_show_lecture_started(unsigned char idx) {
    lcd_clear(1);
    lcd_goto_20x4(1,0,0); lcd_str(1,"                    ");
    lcd_goto_20x4(1,1,0);
    lcd_str(1,"  Lecture ");
    lcd_char(1,'0'+idx+1);
    lcd_str(1," Started! ");
    lcd_goto_20x4(1,2,0); lcd_str(1,"  Teachers Please   ");
    lcd_goto_20x4(1,3,0); lcd_str(1,"  Go to Classes!    ");
}

void lcd1_show_emergency(void) {
    lcd_clear(1);
    lcd_goto_20x4(1,0,0); lcd_str(1,"!!!! EMERGENCY !!!!!");
    lcd_goto_20x4(1,1,0); lcd_str(1,"                    ");
    lcd_goto_20x4(1,2,0); lcd_str(1,"   Evacuate Now!    ");
    lcd_goto_20x4(1,3,0); lcd_str(1,"!!!! EMERGENCY !!!!!");
}

// =========================================
//  LCD2 — ATTENDANCE 16x2
// =========================================
// Forward declaration
void lcd3_show_cycle_frame(void);

// Calculate which period is active based on current time
// Called after manual time update so LCD shows correct lecture
void calculate_active_period(void) {
    unsigned char i;
    unsigned char found     = 0xFF;
    unsigned char last_valid = 0xFF;

    // Reset flags
    all_periods_done = 0;
    classes_over     = 0;

    // If no schedule set at all ? just show dashes
    {
        unsigned char any_set = 0;
        for(i=0; i<MAX_PERIODS; i++)
            if(schedule[i][0]!=0 || schedule[i][1]!=0){ any_set=1; break; }
        if(!any_set) {
            active_period   = 0xFF;
            last_period_idx = 0xFF;
            return;
        }
    }

    // FIRST — check if school end time (period 8) has passed
    if(schedule[SCHOOL_END_IDX][0]!=0 || schedule[SCHOOL_END_IDX][1]!=0) {
        if(cur_hr > schedule[SCHOOL_END_IDX][0] ||
          (cur_hr == schedule[SCHOOL_END_IDX][0] &&
           cur_mn >= schedule[SCHOOL_END_IDX][1])) {
            active_period    = 0xFF;
            last_period_idx  = 0xFF;
            all_periods_done = 1;
            classes_over     = 1;
            teacher_present_c1=0;
            teacher_present_c2=0;
            teacher_present_c3=0;
            LED_MISS_C1=0; LED_MISS_C2=0; LED_MISS_C3=0;
            lcd3_cycle_step=0; lcd3_cycle_timer=0;
            lcd3_show_cycle_frame();
            return;
        }
    }

    // SECOND — find which lecture period is currently active
    // Only check periods 0-6 (not school end time index 7)
    for(i=0; i<SCHOOL_END_IDX; i++) {
        if(schedule[i][0]==0 && schedule[i][1]==0) continue;
        last_valid = i;
        if(cur_hr > schedule[i][0] ||
          (cur_hr == schedule[i][0] && cur_mn >= schedule[i][1])) {
            found = i;
        }
    }

    active_period   = found;
    last_period_idx = found;

    // If all lecture periods have passed and end time not set
    // treat as classes over
    if(found != 0xFF) {
        unsigned char next_exists = 0;
        for(i=0; i<SCHOOL_END_IDX; i++) {
            if(schedule[i][0]==0 && schedule[i][1]==0) continue;
            if(schedule[i][0] > cur_hr ||
              (schedule[i][0] == cur_hr && schedule[i][1] > cur_mn)) {
                next_exists = 1; break;
            }
        }
        // No future periods exist ? last period already started
        // Only show teachers if a future period still exists
        // i.e. we are between periods, not after all periods
        if(!next_exists && found == last_valid) {
            // All periods started — keep active_period for display
            // but teacher names should show during active lecture
        }
    }

    teacher_present_c1=0;
    teacher_present_c2=0;
    teacher_present_c3=0;
    LED_MISS_C1 = (found!=0xFF) ? 1 : 0;
    LED_MISS_C2 = (found!=0xFF) ? 1 : 0;
    LED_MISS_C3 = (found!=0xFF) ? 1 : 0;

    lcd3_cycle_step=0;
    lcd3_cycle_timer=0;
    lcd3_show_cycle_frame();
}

void lcd2_show_idle(void) {
    lcd_goto_16x2(2,0,0); lcd_str(2,"#=Att  *=Sched  ");
    lcd_goto_16x2(2,1,0); lcd_str(2,"0=Time *=Bkspc  ");
}

void lcd2_show_confirmed(unsigned char idx) {
    unsigned char am_pm, hr12=to_12hr(cur_hr,&am_pm);
    lcd_clear(2);
    lcd_goto_16x2(2,0,0);
    lcd_str(2,"ID:"); lcd_str_code(2,STU_ID[idx]);
    lcd_char(2,' '); lcd_str_code(2,STU_NAME[idx]);
    lcd_goto_16x2(2,1,0);
    lcd_2digit(2,hr12); lcd_char(2,':');
    lcd_2digit(2,cur_mn); lcd_char(2,' ');
    lcd_str(2,am_pm?"PM":"AM");
    lcd_str(2," Pres ");
}

// LCD2 — classes over: show attendance count
void lcd2_show_classes_over(void) {
    lcd_goto_16x2(2,0,0); lcd_str(2,"Present Today:  ");
    lcd_goto_16x2(2,1,0);
    lcd_str(2,"Students:");
    lcd_char(2,'0'+attend_count);
    lcd_char(2,'/');
    lcd_char(2,'0'+STU_COUNT);
    lcd_str(2,"      ");
}

// =========================================
//  LCD3 — PRINCIPAL OFFICE 16x2
// =========================================
void lcd3_show_no_lecture(void) {
    lcd_goto_16x2(3,0,0); lcd_str(3,"Principal Office");
    lcd_goto_16x2(3,1,0); lcd_str(3," No Lecture Now ");
}

void lcd3_show_classes_over(void) {
    lcd_clear(3);
    lcd_goto_16x2(3,0,0); lcd_str(3," CLASSES OVER   ");
    lcd_goto_16x2(3,1,0); lcd_str(3," Have Good Day! ");
}

void lcd3_show_emergency(void) {
    lcd_clear(3);
    lcd_goto_16x2(3,0,0); lcd_str(3,"                ");
    lcd_goto_16x2(3,1,0); lcd_str(3,"!! EMERGENCY !! ");
}

void lcd3_show_cycle_frame(void) {
    unsigned char cls;
    bit is_present;

    // Classes over ? show classes over message
    if(classes_over) {
        lcd3_show_classes_over();
        return;
    }

    // If no period has started yet
    if(active_period==0xFF && last_period_idx==0xFF) {
        lcd_goto_16x2(3,0,0); lcd_str(3,"Principal Office");
        lcd_goto_16x2(3,1,0); lcd_str(3,"Awaiting Lecture");
        return;
    }

    switch(lcd3_cycle_step) {
        case 0:
            lcd_clear(3);
            lcd_goto_16x2(3,0,0); lcd_str(3,"Lecture No: ");
            // Use active_period if set, else last period
            if(active_period!=0xFF)
                lcd_char(3,'0'+active_period+1);
            else
                lcd_char(3,'0'+last_period_idx+1);
            lcd_goto_16x2(3,1,0); lcd_str(3,"                ");
            break;

        case 1:
        case 2:
        case 3:
            cls = lcd3_cycle_step;
            if     (cls==1) is_present=teacher_present_c1;
            else if(cls==2) is_present=teacher_present_c2;
            else            is_present=teacher_present_c3;

            lcd_clear(3);
            lcd_goto_16x2(3,0,0);
            lcd_char(3,'C'); lcd_char(3,'0'+cls); lcd_char(3,':');
            if     (cls==1) lcd_str_code(3,CLASS1_TEACHER);
            else if(cls==2) lcd_str_code(3,CLASS2_TEACHER);
            else            lcd_str_code(3,CLASS3_TEACHER);

            lcd_goto_16x2(3,1,0);
            if(is_present) {
                lcd_str(3,"PRESENT         ");
                if(cls==1) LED_MISS_C1=0;
                if(cls==2) LED_MISS_C2=0;
                if(cls==3) LED_MISS_C3=0;
            } else {
                lcd_str(3,"MISSING !!!     ");
                if(cls==1) LED_MISS_C1=1;
                if(cls==2) LED_MISS_C2=1;
                if(cls==3) LED_MISS_C3=1;
            }
            break;
    }
}

void lcd3_update_cycle(void) {
    unsigned int ticks;
    if(emergency_active) return;
    lcd3_cycle_timer++;
    ticks = (lcd3_cycle_step==0) ? LCD3_LECTURE_TICKS : LCD3_CLASS_TICKS;
    if(lcd3_cycle_timer>=ticks) {
        lcd3_cycle_timer=0;
        lcd3_cycle_step++;
        if(lcd3_cycle_step>3) lcd3_cycle_step=0;
        lcd3_show_cycle_frame();
    }
}

// =========================================
//  BELL CHECK
//
//  FIX: active_period stays set from when
//       period starts until NEXT period starts
//       NOT reset after one minute
//
//  FIX: all_periods_done only set after
//       LAST period has started
// =========================================
void check_bell(void) {
    unsigned char i;
    unsigned char total_valid = 0;
    unsigned char last_valid_idx = 0;

    // If classes already over ? skip bell check entirely
    if(classes_over) return;

    // Count valid periods (exclude school end time index)
    for(i=0; i<SCHOOL_END_IDX; i++) {
        if(!(schedule[i][0]==0 && schedule[i][1]==0)) {
            total_valid++;
            last_valid_idx = i;
        }
    }
    if(total_valid==0) return;

    // Check if school end time reached
    if(schedule[SCHOOL_END_IDX][0]!=0 || schedule[SCHOOL_END_IDX][1]!=0) {
        if(cur_hr==schedule[SCHOOL_END_IDX][0] &&
           cur_mn==schedule[SCHOOL_END_IDX][1]) {
            if(!bell_already_rung) {
                bell_already_rung = 1;
                active_period     = 0xFF;
                all_periods_done  = 1;
                classes_over      = 1;  // set classes over flag

                // Show CLASSES OVER on LCD1
                if(!emergency_active) {
                    lcd_clear(1);
                    lcd_goto_20x4(1,0,0); lcd_str(1,"                    ");
                    lcd_goto_20x4(1,1,0); lcd_str(1,"  ** CLASSES OVER **");
                    lcd_goto_20x4(1,2,0); lcd_str(1,"                    ");
                    lcd_goto_20x4(1,3,0); lcd_str(1,"   Have a Good Day! ");
                }

                // Ring bell 7 seconds for school end
                BUZZER=1; delay_ms(7000);
                BUZZER=0;
                delay_ms(1000);

                // Show CLASSES OVER on LCD1 for 10 seconds
                lcd_clear(1);
                lcd_goto_20x4(1,0,0); lcd_str(1,"                    ");
                lcd_goto_20x4(1,1,0); lcd_str(1,"  ** CLASSES OVER **");
                lcd_goto_20x4(1,2,0); lcd_str(1,"                    ");
                lcd_goto_20x4(1,3,0); lcd_str(1,"   Have a Good Day! ");
                delay_ms(10000);

                // After 10 secs ? normal display with dashes
                if(!emergency_active) lcd1_show_normal();

                // LCD2 shows attendance count
                lcd2_show_classes_over();

                // LCD3 shows classes over
                lcd3_cycle_step=0;
                lcd3_cycle_timer=0;
                if(!emergency_active) lcd3_show_cycle_frame();

                // Turn off all LEDs
                LED_MISS_C1=0;
                LED_MISS_C2=0;
                LED_MISS_C3=0;
            }
            return;
        }
    }

    // Check if any lecture period matches current time
    for(i=0; i<SCHOOL_END_IDX; i++) {
        if(schedule[i][0]==0 && schedule[i][1]==0) continue;
        if(cur_hr==schedule[i][0] && cur_mn==schedule[i][1]) {
            if(!bell_already_rung) {
                bell_already_rung = 1;
                active_period     = i;
                last_period_idx   = i;
                all_periods_done  = 0;

                // Reset teacher presence
                teacher_present_c1=0;
                teacher_present_c2=0;
                teacher_present_c3=0;
                LED_MISS_C1=1;
                LED_MISS_C2=1;
                LED_MISS_C3=1;

                // Show lecture started
                if(!emergency_active)
                    lcd1_show_lecture_started(active_period);

                // When period 2 starts (index 1) ? close attendance
                // LCD2 switches from idle to attendance count display
                if(active_period == 1) {
                    lcd2_show_classes_over();
                }

                // Reset LCD3 cycle
                lcd3_cycle_step=0;
                lcd3_cycle_timer=0;
                if(!emergency_active) lcd3_show_cycle_frame();

                // Ring bell for 7 seconds (lecture start)
                BUZZER=1; delay_ms(7000);
                BUZZER=0;
                delay_ms(1000);

                if(!emergency_active) lcd1_show_normal();
            }
            return;
        }
    }

    // Reset bell flag when minute changes
    bell_already_rung = 0;
}

// =========================================
//  EMERGENCY TOGGLE HANDLER
// =========================================
void handle_emergency_toggle(void) {
    delay_ms(50);
    if(EMG_BTN!=0) return;
    while(EMG_BTN==0); delay_ms(20);

    if(!emergency_active) {
        emergency_active=1;
        BUZZER=1;
        lcd1_show_emergency();
        lcd3_show_emergency();
    } else {
        emergency_active=0;
        BUZZER=0;
        if(kpad_mode==MODE_NORMAL) lcd1_show_normal();
        lcd3_cycle_step=0;
        lcd3_cycle_timer=0;
        lcd3_show_cycle_frame();
    }
}

// =========================================
//  CLASSROOM BUTTONS CHECK
// =========================================
void check_classroom_buttons(void) {
    if(CLASS1_BTN==0 && active_period!=0xFF && !teacher_present_c1) {
        delay_ms(50);
        if(CLASS1_BTN==0) {
            teacher_present_c1=1; LED_MISS_C1=0;
            while(CLASS1_BTN==0);
        }
    }
    if(CLASS2_BTN==0 && active_period!=0xFF && !teacher_present_c2) {
        delay_ms(50);
        if(CLASS2_BTN==0) {
            teacher_present_c2=1; LED_MISS_C2=0;
            while(CLASS2_BTN==0);
        }
    }
    if(CLASS3_BTN==0 && active_period!=0xFF && !teacher_present_c3) {
        delay_ms(50);
        if(CLASS3_BTN==0) {
            teacher_present_c3=1; LED_MISS_C3=0;
            while(CLASS3_BTN==0);
        }
    }
    if(EMG_BTN==0) handle_emergency_toggle();
}

// =========================================
//  KEYPAD HANDLERS
// =========================================

// ATTENDANCE MODE — # exit, * backspace
void keypad_handle_attendance(unsigned char key) {
    unsigned char idx;
    if(key=='#') {
        kpad_mode=MODE_NORMAL; input_idx=0;
        lcd_clear(2); lcd2_show_idle(); return;
    }
    if(key=='*') {
        if(input_idx>0) {
            input_idx--;
            lcd_goto_16x2(2,1,3+input_idx);
            lcd_char(2,'_');
        }
        return;
    }
    if(key<'0'||key>'9') return;
    input_buf[input_idx++]=key;
    lcd_goto_16x2(2,1,3+input_idx-1); lcd_char(2,key);
    if(input_idx==3) {
        input_buf[3]='\0';
        idx=find_student(input_buf);
        if(idx!=0xFF) {
            LED_ATTEND=1;
            attend_count++;
            if(attend_count>STU_COUNT) attend_count=STU_COUNT;
            // Save count to DS1307 RAM
            rtc_write_byte(ATTEND_COUNT_ADDR, attend_count);
            lcd2_show_confirmed(idx);
            delay_ms(3000);
            LED_ATTEND=0;
        } else {
            lcd_clear(2);
            lcd_goto_16x2(2,0,0); lcd_str(2,"ID Not Found!   ");
            lcd_goto_16x2(2,1,0); lcd_str(2,"Try Again       ");
            delay_ms(2000);
        }
        input_idx=0;
        lcd_clear(2);
        lcd_goto_16x2(2,0,0); lcd_str(2,"ATTEND MODE     ");
        lcd_goto_16x2(2,1,0); lcd_str(2,"ID:             ");
    }
}

// SCHEDULE MODE — # exit/save, * backspace
void keypad_handle_schedule(unsigned char key) {
    unsigned char p, h, m;
    if(key=='#') {
        // Exit and save
        kpad_mode=MODE_NORMAL; input_idx=0;
        lcd_clear(1);
        lcd_goto_20x4(1,0,0); lcd_str(1,"                    ");
        lcd_goto_20x4(1,1,0); lcd_str(1,"  Schedule Saved!   ");
        lcd_goto_20x4(1,2,0); lcd_str(1,"  Stored in RTC RAM ");
        lcd_goto_20x4(1,3,0); lcd_str(1,"                    ");
        delay_ms(2000);
        lcd1_show_normal(); return;
    }
    if(key=='*') {
        // Backspace
        if(input_idx>0) {
            input_idx--;
            if(input_idx==0) {
                lcd_clear(1);
                lcd_goto_20x4(1,0,0); lcd_str(1,"   SCHEDULE MODE    ");
                lcd_goto_20x4(1,1,0); lcd_str(1,"Period 1-7, 8=EndTm ");
                lcd_goto_20x4(1,2,0); lcd_str(1," *=Bksp  #=Save/Exit");
                lcd_goto_20x4(1,3,0); lcd_str(1,"                    ");
            } else {
                lcd_goto_20x4(1,2,11+input_idx-1);
                lcd_char(1,'_');
            }
        }
        return;
    }
    input_buf[input_idx++]=key;
    if(input_idx==1) {
        lcd_clear(1);
        lcd_goto_20x4(1,0,0); lcd_str(1,"   SCHEDULE MODE    ");
        if(key=='8') {
            lcd_goto_20x4(1,1,0); lcd_str(1,"School End Time:    ");
        } else {
            lcd_goto_20x4(1,1,0); lcd_str(1,"Period: "); lcd_char(1,key);
        }
        lcd_goto_20x4(1,2,0);
        if(input_buf[0]=='8') {
            lcd_str(1,"End Time HHMM:      ");
        } else {
            lcd_str(1,"Start Time HHMM:    ");
        }
        lcd_goto_20x4(1,3,0); lcd_str(1," *=Bksp  #=Save/Exit");
    } else if(input_idx<=5) {
        lcd_goto_20x4(1,2,11+input_idx-2); lcd_char(1,key);
    }
    if(input_idx==5) {
        // Period 8 = school end time stored at SCHOOL_END_IDX
        if(input_buf[0]=='8') {
            p = SCHOOL_END_IDX;
        } else {
            p = input_buf[0]-'1';
        }
        h=(input_buf[1]-'0')*10+(input_buf[2]-'0');
        m=(input_buf[3]-'0')*10+(input_buf[4]-'0');
        if(p<=SCHOOL_END_IDX) {
            schedule[p][0]=h; schedule[p][1]=m;
            rtc_save_period(p,h,m);
        }
        input_idx=0;
        lcd_goto_20x4(1,1,0); lcd_str(1,"Period Saved!       ");
        lcd_goto_20x4(1,2,0); lcd_str(1,"Next period or #Exit");
    }
}

// TIME SET MODE — * backspace, 0 exit/cancel
void keypad_handle_timeset(unsigned char key) {
    unsigned char h, m;
    if(key=='#' && input_idx==0) {
        // 0 pressed with nothing entered = exit
        kpad_mode=MODE_NORMAL; input_idx=0;
        lcd_clear(2); lcd2_show_idle(); return;
    }
    if(key=='*') {
        // Backspace
        if(input_idx>0) {
            input_idx--;
            lcd_goto_16x2(2,1,input_idx);
            lcd_char(2,'_');
        }
        return;
    }
    if(key<'0'||key>'9') return;
    input_buf[input_idx++]=key;
    lcd_goto_16x2(2,1,input_idx-1); lcd_char(2,key);
    if(input_idx==4) {
        h=(input_buf[0]-'0')*10+(input_buf[1]-'0');
        m=(input_buf[2]-'0')*10+(input_buf[3]-'0');
        rtc_set_time(h,m,0);
        cur_hr=h; cur_mn=m; cur_sec=0;

        // Recalculate which lecture is active at new time
        calculate_active_period();

        input_idx=0; kpad_mode=MODE_NORMAL;
        lcd_clear(2);
        lcd_goto_16x2(2,0,0); lcd_str(2,"Time Updated!   ");
        lcd_goto_16x2(2,1,0); lcd_str(2,"                ");
        delay_ms(2000); lcd2_show_idle();
    }
}

void process_keypad(void) {
    unsigned char key=keypad_scan();
    if(key==0) return;
    keypad_wait_release();

    if(kpad_mode==MODE_NORMAL) {
        if(key=='*') {
            kpad_mode=MODE_SCHEDULE; input_idx=0;
            lcd_clear(1);
            lcd_goto_20x4(1,0,0); lcd_str(1,"   SCHEDULE MODE    ");
            lcd_goto_20x4(1,1,0); lcd_str(1,"1-7:Lecture 8:EndTim");
            lcd_goto_20x4(1,2,0); lcd_str(1," *=Bksp  #=Save/Exit");
            lcd_goto_20x4(1,3,0); lcd_str(1,"                    ");
            return;
        }
        if(key=='#') {
            kpad_mode=MODE_ATTEND; input_idx=0;
            lcd_clear(2);
            lcd_goto_16x2(2,0,0); lcd_str(2,"ATTEND MODE     ");
            lcd_goto_16x2(2,1,0); lcd_str(2,"ID: *=Bksp #=Ex ");
            delay_ms(5000);
            lcd_goto_16x2(2,1,0); lcd_str(2,"ID:             ");
            return;
        }
        if(key=='0') {
            kpad_mode=MODE_TIME; input_idx=0;
            lcd_clear(2);
            lcd_goto_16x2(2,0,0); lcd_str(2,"SET TIME(HHMM)  ");
            lcd_goto_16x2(2,1,0); lcd_str(2,"*=Bksp  #=Exit  ");
            return;
        }
        if(key=='9') {
            // Reset attendance count
            attend_count=0;
            rtc_write_byte(ATTEND_COUNT_ADDR, 0);
            classes_over=0;
            lcd_clear(2);
            lcd_goto_16x2(2,0,0); lcd_str(2,"Count Reset!    ");
            lcd_goto_16x2(2,1,0); lcd_str(2,"Present: 0 /5   ");
            delay_ms(2000);
            lcd2_show_idle();
            return;
        }
    }
    if(kpad_mode==MODE_SCHEDULE) keypad_handle_schedule(key);
    if(kpad_mode==MODE_ATTEND)   keypad_handle_attendance(key);
    if(kpad_mode==MODE_TIME)     keypad_handle_timeset(key);
}

// =========================================
//  MAIN
// =========================================
void main(void) {
    BUZZER=0;
    LED_MISS_C1=0; LED_MISS_C2=0; LED_MISS_C3=0; LED_ATTEND=0;
    CLASS1_BTN=1; CLASS2_BTN=1; CLASS3_BTN=1; EMG_BTN=1;
    COL1=1; COL2=1; COL3=1;
    ROW1=1; ROW2=1; ROW3=1; ROW4=1;

    lcd_init_single(1);
    lcd_init_single(2);
    lcd_init_single(3);

    // Startup
    lcd_goto_20x4(1,0,0); lcd_str(1,"                    ");
    lcd_goto_20x4(1,1,0); lcd_str(1,"   SSMS SYSTEM      ");
    lcd_goto_20x4(1,2,0); lcd_str(1,"   Initializing..   ");
    lcd_goto_20x4(1,3,0); lcd_str(1,"                    ");
    lcd_goto_16x2(2,0,0); lcd_str(2," Attendance Sys ");
    lcd_goto_16x2(2,1,0); lcd_str(2,"   Ready...     ");
    lcd_goto_16x2(3,0,0); lcd_str(3," Principal Off. ");
    lcd_goto_16x2(3,1,0); lcd_str(3,"   Ready...     ");
    delay_ms(2000);

    rtc_load_schedule();

    // Load saved attendance count from DS1307 RAM
    attend_count = rtc_read_byte(ATTEND_COUNT_ADDR);
    if(attend_count > STU_COUNT) attend_count = 0;

    // Calculate active period based on current time
    rtc_read_time();
    calculate_active_period();

    // Show correct startup display based on time
    if(classes_over) {
        // Classes already over ? show dashes immediately
        lcd1_show_normal();
        lcd2_show_classes_over();
        lcd3_show_cycle_frame();
    } else if(active_period >= 1) {
        // Past period 1 ? show count on LCD2
        lcd1_show_normal();
        lcd2_show_classes_over();
        lcd3_show_cycle_frame();
    } else {
        // Normal startup
        lcd1_show_normal();
        lcd2_show_idle();
        lcd3_show_cycle_frame();
    }

    while(1) {
        rtc_read_time();
        check_bell();
        check_classroom_buttons();
        process_keypad();

        if(kpad_mode==MODE_NORMAL && !emergency_active) {
            // Re-check classes over every loop in case it was missed on startup
            if(!classes_over &&
               (schedule[SCHOOL_END_IDX][0]!=0 || schedule[SCHOOL_END_IDX][1]!=0)) {
                if(cur_hr > schedule[SCHOOL_END_IDX][0] ||
                  (cur_hr == schedule[SCHOOL_END_IDX][0] &&
                   cur_mn >= schedule[SCHOOL_END_IDX][1])) {
                    classes_over     = 1;
                    all_periods_done = 1;
                    active_period    = 0xFF;
                    LED_MISS_C1=0; LED_MISS_C2=0; LED_MISS_C3=0;
                }
            }
            lcd1_show_normal();
            // Show attendance count from period 2 onwards
            if(active_period >= 1 || classes_over)
                lcd2_show_classes_over();
        }

        lcd3_update_cycle();
        delay_ms(500);
    }
}