/*
 * main.c
 * Keil C51 source for an 8051-based bus stop announcer with EEPROM persistence.
 * - HD44780 2x16 character LCD (8-bit mode on P2)
 * - 4x4 matrix keypad on P3 (rows: P3.0..P3.3, cols: P3.4..P3.7)
 * - Buzzer on P2.0 (changed per user request)
 * - AT24C02 EEPROM via I2C on P1.6(SDA)/P1.7(SCL)
 * - Two lines, each with 8 station names (16 chars max), persisted to EEPROM
 * - No nested functions; simple standalone functions for Proteus/Keil C51
 */

#include <reg51.h>
#include <string.h>

typedef unsigned char uchar;
typedef unsigned int uint;

/* Pins */
sbit LCD_RS = P1 ^ 0;
sbit LCD_EN = P1 ^ 1;
/* Buzzer moved to P2.0 as requested */
sbit BEEP   = P2 ^ 0;

sbit SDA = P1 ^ 6; /* I2C data */
sbit SCL = P1 ^ 7; /* I2C clock */

/* Key matrix uses P3: rows control low nibble, cols read high nibble */
#define P3_ROWS_MASK  0x0F
#define P3_COLS_SHIFT 4

/* storage: two lines x 8 stops x 16 chars */
char Line1[8][17] = {"StationA","StationB","StationC","StationD","StationE","StationF","StationG","StationH"};
char Line2[8][17] = {"Line2A","Line2B","Line2C","Line2D","Line2E","Line2F","Line2G","Line2H"};

char InputBuf[17];
uchar InputIdx = 0;
uchar CurLine = 0; /* 0 = Line1, 1 = Line2 */
uchar CurSta = 0;  /* 0..7 */
bit EditFlag = 0;

/* I2C/AT24C02 parameters */
#define EEPROM_ADDR_W 0xA0 /* write */
#define EEPROM_ADDR_R 0xA1 /* read */
#define STATION_LEN 16
#define TOTAL_STATIONS 16 /* 2 lines x 8 */

/* delays */
void delay_ms(uint ms) {
    uint i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 120; j++) {
            _nop_();
        }
    }
}

void delay_us(uint us) {
    uint i;
    for (i = 0; i < us; i++) _nop_();
}

/* LCD functions (HD44780 8-bit) */
void lcd_strobe(void) {
    LCD_EN = 1;
    delay_us(2);
    LCD_EN = 0;
    delay_us(2);
}

void lcd_write_cmd(uchar cmd) {
    LCD_RS = 0;
    P2 = cmd;
    lcd_strobe();
    delay_ms(2);
}

void lcd_write_data(uchar dat) {
    LCD_RS = 1;
    P2 = dat;
    lcd_strobe();
    delay_ms(2);
}

void lcd_init(void) {
    delay_ms(50);
    LCD_RS = 0; LCD_EN = 0; P2 = 0x00;
    lcd_write_cmd(0x38); /* 8-bit, 2 lines, 5x8 */
    lcd_write_cmd(0x0C); /* display on */
    lcd_write_cmd(0x01); /* clear */
    delay_ms(2);
    lcd_write_cmd(0x06); /* entry mode */
}

void lcd_clear(void) {
    lcd_write_cmd(0x01);
    delay_ms(2);
}

void lcd_set_pos(uchar row, uchar col) {
    uchar addr = (row == 0) ? (0x00 + col) : (0x40 + col);
    lcd_write_cmd(0x80 | addr);
}

void lcd_puts(const char *s) {
    uchar i = 0;
    while (s[i]) {
        lcd_write_data((uchar)s[i]);
        i++;
    }
}

/* Beep: ms milliseconds of buzzing (simple on/off) */
void beep(uchar ms) {
    uchar t;
    for (t = 0; t < ms; t++) {
        BEEP = 1;
        delay_ms(1);
    }
    BEEP = 0;
}

/* --- Keypad (4x4) --- */

/* set P3 lower nibble to given value while preserving upper nibble */
void p3_write_low_nibble(uchar low) {
    uchar v = P3 & 0xF0; /* keep cols */
    v |= (low & 0x0F);
    P3 = v;
}

void rows_release(void) {
    p3_write_low_nibble(0x0F); /* all rows high */
}

void drive_row(uchar r) {
    uchar low = 0x0F;
    switch (r) {
        case 0: low = 0x0E; break; /* 1110 */
        case 1: low = 0x0D; break; /* 1101 */
        case 2: low = 0x0B; break; /* 1011 */
        case 3: low = 0x07; break; /* 0111 */
    }
    p3_write_low_nibble(low);
}

uchar read_cols(void) {
    uchar v = P3 & 0xF0;
    return (v >> 4) & 0x0F; /* bit0 -> col0 (P3.4) */
}

uchar key_scan_once(void) {
    uchar r, cols;
    rows_release();
    delay_us(20);
    cols = read_cols();
    if ((cols & 0x0F) == 0x0F) return 0xFF; /* none */
    for (r = 0; r < 4; r++) {
        drive_row(r);
        delay_us(20);
        cols = read_cols();
        if ((cols & 0x0F) != 0x0F) {
            uchar c;
            for (c = 0; c < 4; c++) {
                if (((cols >> c) & 0x01) == 0) {
                    rows_release();
                    return (uchar)(r * 4 + c);
                }
            }
        }
    }
    rows_release();
    return 0xFF;
}

uchar get_key(void) {
    uchar k;
    do {
        k = key_scan_once();
        if (k == 0xFF) return 0xFF;
        delay_ms(20);
    } while (key_scan_once() != k);
    /* wait for release */
    do { delay_ms(10); } while (key_scan_once() != 0xFF);
    delay_ms(20);
    return k;
}

/* map keypad codes to characters for editing */
char keymap[16] = {
    '0','1','2','3',
    '4','5','6','7',
    '8','9','A','B',
    'C','D','*','#'
};

/* --- I2C bit-bang for AT24C02 --- */

void I2C_Delay(void) { delay_us(5); }

void I2C_Start(void) {
    SDA = 1; SCL = 1; I2C_Delay();
    SDA = 0; I2C_Delay();
    SCL = 0; I2C_Delay();
}

void I2C_Stop(void) {
    SDA = 0; SCL = 1; I2C_Delay();
    SDA = 1; I2C_Delay();
}

bit I2C_WriteByte(uchar dat) {
    uchar i;
    bit ack;
    for (i = 0; i < 8; i++) {
        SDA = (dat & 0x80) ? 1 : 0;
        dat <<= 1;
        SCL = 1; I2C_Delay();
        SCL = 0; I2C_Delay();
    }
    SDA = 1; /* release SDA for ACK */
    SCL = 1; I2C_Delay();
    ack = SDA; /* 0 = ACK */
    SCL = 0; I2C_Delay();
    return (ack == 0);
}

uchar I2C_ReadByte(bit ack) {
    uchar i, dat = 0;
    SDA = 1; /* release SDA */
    for (i = 0; i < 8; i++) {
        dat <<= 1;
        SCL = 1; I2C_Delay();
        if (SDA) dat |= 0x01;
        SCL = 0; I2C_Delay();
    }
    /* send ACK/NACK */
    SDA = (ack) ? 0 : 1; /* ack=1 -> drive 0 */
    SCL = 1; I2C_Delay();
    SCL = 0; I2C_Delay();
    SDA = 1; /* release */
    return dat;
}

/* write up to 16 bytes starting at mem_addr (must not cross page boundary) */
bit eeprom_write_page(uchar mem_addr, const uchar *buf, uchar len) {
    uchar i;
    if (len == 0) return 1;
    I2C_Start();
    if (!I2C_WriteByte(EEPROM_ADDR_W)) { I2C_Stop(); return 0; }
    if (!I2C_WriteByte(mem_addr)) { I2C_Stop(); return 0; }
    for (i = 0; i < len; i++) {
        if (!I2C_WriteByte(buf[i])) { I2C_Stop(); return 0; }
    }
    I2C_Stop();
    /* wait tWR (~5ms) */
    delay_ms(5);
    return 1;
}

/* read len bytes starting at mem_addr into buf */
bit eeprom_read(uchar mem_addr, uchar *buf, uchar len) {
    uchar i;
    I2C_Start();
    if (!I2C_WriteByte(EEPROM_ADDR_W)) { I2C_Stop(); return 0; }
    if (!I2C_WriteByte(mem_addr)) { I2C_Stop(); return 0; }
    /* restart for read */
    I2C_Start();
    if (!I2C_WriteByte(EEPROM_ADDR_R)) { I2C_Stop(); return 0; }
    for (i = 0; i < len; i++) {
        buf[i] = I2C_ReadByte(i != (len - 1)); /* ack all but last */
    }
    I2C_Stop();
    return 1;
}

/* load all names from EEPROM into Line1/Line2. If EEPROM seems empty (all 0xFF), write defaults back */
void load_names_from_eeprom(void) {
    uchar data[STATION_LEN];
    uchar i, s;
    uchar all_ff = 1;
    /* read first byte to check if initialized */
    if (!eeprom_read(0, data, 1)) return; /* read failed -> keep defaults */
    if (data[0] != 0xFF) all_ff = 0;
    if (all_ff) {
        /* EEPROM empty (0xFF): write current RAM defaults into EEPROM */
        for (s = 0; s < 8; s++) {
            memset(data, 0x20, STATION_LEN); /* fill spaces */
            strncpy((char*)data, Line1[s], STATION_LEN);
            eeprom_write_page((uchar)(s * STATION_LEN), data, STATION_LEN);
        }
        for (s = 0; s < 8; s++) {
            memset(data, 0x20, STATION_LEN);
            strncpy((char*)data, Line2[s], STATION_LEN);
            eeprom_write_page((uchar)((8 + s) * STATION_LEN), data, STATION_LEN);
        }
        return;
    }
    /* otherwise read all stations */
    for (s = 0; s < 8; s++) {
        if (eeprom_read((uchar)(s * STATION_LEN), data, STATION_LEN)) {
            for (i = 0; i < STATION_LEN; i++) Line1[s][i] = (data[i] == 0x20) ? '\0' : data[i];
            Line1[s][STATION_LEN] = '\0';
        }
    }
    for (s = 0; s < 8; s++) {
        if (eeprom_read((uchar)((8 + s) * STATION_LEN), data, STATION_LEN)) {
            for (i = 0; i < STATION_LEN; i++) Line2[s][i] = (data[i] == 0x20) ? '\0' : data[i];
            Line2[s][STATION_LEN] = '\0';
        }
    }
}

void save_station_to_eeprom(uchar line, uchar station) {
    uchar data[STATION_LEN];
    uchar i;
    uchar addr = (uchar)((line * 8 + station) * STATION_LEN);
    memset(data, 0x20, STATION_LEN);
    if (line == 0) strncpy((char*)data, Line1[station], STATION_LEN);
    else strncpy((char*)data, Line2[station], STATION_LEN);
    eeprom_write_page(addr, data, STATION_LEN);
}

/* display refresh */
void refresh_lcd(void) {
    char *cur = (CurLine == 0) ? Line1[CurSta] : Line2[CurSta];
    char *next = (CurLine == 0) ? Line1[(CurSta < 7) ? (CurSta + 1) : 0] : Line2[(CurSta < 7) ? (CurSta + 1) : 0];
    lcd_clear();
    lcd_set_pos(0, 0);
    lcd_puts("Cur:");
    lcd_puts(cur);
    lcd_set_pos(1, 0);
    lcd_puts("Next:");
    lcd_puts(next);
    if (EditFlag) {
        lcd_set_pos(0, 10);
        lcd_puts(InputBuf);
    }
}

/* save InputBuf to current station */
void save_input_to_station(void) {
    if (CurLine == 0) {
        strncpy(Line1[CurSta], InputBuf, STATION_LEN);
        Line1[CurSta][STATION_LEN] = '\0';
    } else {
        strncpy(Line2[CurSta], InputBuf, STATION_LEN);
        Line2[CurSta][STATION_LEN] = '\0';
    }
    save_station_to_eeprom(CurLine, CurSta);
}

/* handle key (non-nested) */
void handle_key(uchar k) {
    if (k == 0xFF) return;
    if (!EditFlag) {
        switch (k) {
            case 0: /* switch line */
                CurLine = (CurLine == 0) ? 1 : 0;
                CurSta = 0;
                beep(50);
                break;
            case 1: /* prev */
                if (CurSta == 0) CurSta = 7; else CurSta--;
                beep(30);
                break;
            case 2: /* next */
                CurSta++; if (CurSta >= 8) CurSta = 0;
                beep(120); /* arrival */
                break;
            case 3: /* enter edit mode */
                EditFlag = 1;
                InputIdx = 0;
                memset(InputBuf, 0, sizeof(InputBuf));
                beep(30);
                break;
            default:
                break;
        }
    } else {
        if (k <= 11) {
            if (InputIdx < STATION_LEN) {
                InputBuf[InputIdx++] = keymap[k];
                InputBuf[InputIdx] = '\0';
                beep(20);
            }
        } else if (k == 14) { /* '*' backspace */
            if (InputIdx > 0) {
                InputIdx--;
                InputBuf[InputIdx] = '\0';
                beep(20);
            }
        } else if (k == 15) { /* '#' save */
            save_input_to_station();
            EditFlag = 0;
            InputIdx = 0;
            memset(InputBuf, 0, sizeof(InputBuf));
            beep(100);
        } else if (k == 12 || k == 13) {
            if (InputIdx < STATION_LEN) {
                InputBuf[InputIdx++] = keymap[k];
                InputBuf[InputIdx] = '\0';
                beep(20);
            }
        }
    }
}

void main(void) {
    uchar k;
    /* initialize ports: set P3 rows high and leave cols as inputs
       P3 default after reset has quasi-bidirectional behavior; write 1s to rows */
    P3 = 0xF0 | 0x0F;
    BEEP = 0;
    lcd_init();
    /* load EEPROM data (if present) or write defaults */
    load_names_from_eeprom();
    refresh_lcd();
    while (1) {
        k = get_key();
        if (k != 0xFF) {
            handle_key(k);
            refresh_lcd();
        }
        delay_ms(10);
    }
}
