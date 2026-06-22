/*
 * main.c
 * Keil C51 source for an 8051-based bus stop announcer.
 * - Uses HD44780-compatible 2x16 character LCD (8-bit mode on P2)
 * - Uses a 4x4 matrix keypad on P3 (rows: P3.0..P3.3, cols: P3.4..P3.7)
 * - Buzzer on P1.3
 * - Stores two lines, each with 8 station names (editable via keypad)
 * - Can switch between lines, move prev/next station, edit station names
 * - Displays current station on LCD line 1 and next station on LCD line 2
 * NOTE: avoids nested functions; all functions are simple and standalone
 */

#include <reg51.h>
#include <string.h>

/* pin assignments */
sbit BEEP = P1 ^ 3; /* buzzer */

/* LCD (HD44780) - 8-bit data on P2, control on P1 bits */
sbit LCD_RS = P1 ^ 0;
sbit LCD_EN = P1 ^ 1;

/* Keypad on P3: rows P3.0..P3.3 (outputs), cols P3.4..P3.7 (inputs, with pull-ups) */
#define ROW1 P3_0
#define ROW2 P3_1
#define ROW3 P3_2
#define ROW4 P3_3

/* helpers to access individual bits of P3 (Keil doesn't provide P3_0 macro by default) */
#define P3_0 (P3 & 0x01)
#define P3_1 (P3 & 0x02)
#define P3_2 (P3 & 0x04)
#define P3_3 (P3 & 0x08)

/* columns are bits 4..7 */
#define COL_MASK (0xF0)

/* general types */
typedef unsigned char uchar;
typedef unsigned int uint;

/* buffers and state */
char Line1[8][17] = {"StationA","StationB","StationC","StationD","StationE","StationF","StationG","StationH"};
char Line2[8][17] = {"Line2A","Line2B","Line2C","Line2D","Line2E","Line2F","Line2G","Line2H"};
char InputBuf[17];
uchar InputIdx = 0;
uchar CurLine = 0; /* 0 => Line1, 1 => Line2 */
uchar CurSta = 0;  /* 0..7 */
bit EditFlag = 0;

/* simple delays (approx for 12MHz); adjust if different crystal */
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
    for (i = 0; i < us; i++) {
        _nop_();
    }
}

/* LCD low-level */
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
    /* power up delay */
    delay_ms(50);
    LCD_RS = 0;
    LCD_EN = 0;
    P2 = 0x00;
    /* function set: 8-bit, 2 lines, 5x8 */
    lcd_write_cmd(0x38);
    /* display on, cursor off */
    lcd_write_cmd(0x0C);
    /* clear */
    lcd_write_cmd(0x01);
    delay_ms(2);
    /* entry mode */
    lcd_write_cmd(0x06);
}

void lcd_clear(void) {
    lcd_write_cmd(0x01);
    delay_ms(2);
}

void lcd_set_pos(uchar row, uchar col) {
    uchar addr;
    if (row == 0) addr = 0x00 + col;
    else addr = 0x40 + col;
    lcd_write_cmd(0x80 | addr);
}

void lcd_puts(char *s) {
    uchar i = 0;
    while (s[i]) {
        lcd_write_data((uchar)s[i]);
        i++;
    }
}

/* simple beep */
void beep(uchar ms) {
    uchar t;
    for (t = 0; t < ms; t++) {
        BEEP = 1;
        delay_ms(1);
    }
    BEEP = 0;
}

/* Keypad functions - use P3
   Rows: P3.0..P3.3 as outputs (drive low one by one)
   Cols: P3.4..P3.7 as inputs (read)
   Key numbering: row*4 + colindex (0..15)
*/

/* set all rows high (releasing) */
void rows_release(void) {
    /* write 1 to row bits: since we read P3, we need to set port value
       but P3 contains inputs for cols too. To change row outputs safely,
       use bit operations: write P3 with ones in lower nibble while preserving upper nibble.
       On 8051 direct read-modify-write to P3 affects both.
       We'll write a byte combining current upper nibble and lower nibble set to 1s.
    */
    uchar v = P3 & 0xF0; /* keep cols */
    v |= 0x0F; /* rows high */
    P3 = v;
}

/* drive a single row low (0..3) */
void drive_row(uchar r) {
    uchar v = P3 & 0xF0; /* keep cols */
    switch (r) {
        case 0: v |= 0x0E; break; /* 1110 -> row0 low */
        case 1: v |= 0x0D; break; /* 1101 */
        case 2: v |= 0x0B; break; /* 1011 */
        case 3: v |= 0x07; break; /* 0111 */
        default: v |= 0x0F; break;
    }
    P3 = v;
}

/* read columns, return 4-bit value (bit0 -> col0 P3.4) where 0 means pressed */
uchar read_cols(void) {
    uchar p = P3 & 0xF0;
    /* shift down so bit0 corresponds to P3.4 */
    return (p >> 4) & 0x0F;
}

/* scan matrix keypad once, return key code 0..15, or 0xFF if none */
uchar key_scan_once(void) {
    uchar r, cols;
    rows_release();
    delay_us(20);
    cols = read_cols();
    if ((cols & 0x0F) == 0x0F) return 0xFF; /* no key (all high) */

    for (r = 0; r < 4; r++) {
        drive_row(r);
        delay_us(20);
        cols = read_cols();
        if ((cols & 0x0F) != 0x0F) {
            uchar c;
            for (c = 0; c < 4; c++) {
                if (((cols >> c) & 0x01) == 0) {
                    /* found key: row r, col c */
                    rows_release();
                    return (uchar)(r * 4 + c);
                }
            }
        }
    }
    rows_release();
    return 0xFF;
}

/* debounced key read: wait for stable press and release */
uchar get_key(void) {
    uchar k;
    do {
        k = key_scan_once();
        if (k == 0xFF) return 0xFF;
        delay_ms(20);
    } while (key_scan_once() != k);
    /* wait for release */
    do {
        delay_ms(10);
    } while (key_scan_once() != 0xFF);
    delay_ms(20);
    return k;
}

/* mapping of numeric keypad entries to characters for editing */
char keymap[16] = {
    /* 0..3 */ '0','1','2','3',
    /* 4..7 */ '4','5','6','7',
    /* 8..11*/ '8','9','A','B',
    /* 12..15*/ 'C','D','*','#'
};

/* show current and next station on LCD */
void refresh_lcd(void) {
    char *cur = (CurLine == 0) ? Line1[CurSta] : Line2[CurSta];
    char *next;
    if (CurLine == 0) next = Line1[(CurSta < 7) ? (CurSta + 1) : 0];
    else next = Line2[(CurSta < 7) ? (CurSta + 1) : 0];

    lcd_clear();
    lcd_set_pos(0, 0);
    lcd_puts("Cur:");
    lcd_puts(cur);
    /* ensure second line shows Next: ... */
    lcd_set_pos(1, 0);
    lcd_puts("Next:");
    lcd_puts(next);

    if (EditFlag) {
        /* show edit buffer on the right of first line (if space) */
        lcd_set_pos(0, 12);
        lcd_puts(InputBuf);
    }
}

/* save InputBuf to current station */
void save_input_to_station(void) {
    if (CurLine == 0) {
        strncpy(Line1[CurSta], InputBuf, 16);
        Line1[CurSta][16] = '\0';
    } else {
        strncpy(Line2[CurSta], InputBuf, 16);
        Line2[CurSta][16] = '\0';
    }
}

/* main key handling loop (no nested functions) */
void handle_key(uchar k) {
    if (k == 0xFF) return;
    if (!EditFlag) {
        switch (k) {
            case 0: /* key 0: switch line */
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
                /* arrival beep */
                beep(100);
                break;
            case 3: /* enter edit mode */
                EditFlag = 1;
                InputIdx = 0;
                memset(InputBuf, 0, sizeof(InputBuf));
                beep(30);
                break;
            default:
                /* other keys ignored in normal mode */
                break;
        }
    } else {
        /* in edit mode: keys map to characters, '*' (index 14) -> backspace, '#' (15) -> save */
        if (k <= 11) {
            /* digits and A/B etc mapped in keymap */
            if (InputIdx < 16) {
                InputBuf[InputIdx++] = keymap[k];
                InputBuf[InputIdx] = '\0';
                beep(20);
            }
        } else if (k == 14) { /* '*' as backspace */
            if (InputIdx > 0) {
                InputIdx--;
                InputBuf[InputIdx] = '\0';
                beep(20);
            }
        } else if (k == 15) { /* '#' save and exit */
            save_input_to_station();
            EditFlag = 0;
            InputIdx = 0;
            memset(InputBuf, 0, sizeof(InputBuf));
            beep(100);
        } else {
            /* keys 12,13 (C,D) insert letters if desired */
            if ((k == 12 || k == 13) && InputIdx < 16) {
                InputBuf[InputIdx++] = keymap[k];
                InputBuf[InputIdx] = '\0';
                beep(20);
            }
        }
    }
}

void main(void) {
    uchar k;
    /* init pins: set P3 rows high (1) and leave cols as inputs (upper nibble) */
    P3 = 0xF0 | 0x0F; /* rows high, cols read as 1 until pulled low */
    BEEP = 0;
    lcd_init();
    refresh_lcd();

    while (1) {
        k = get_key();
        if (k != 0xFF) {
            handle_key(k);
            refresh_lcd();
        }
        /* small idle delay to reduce CPU use */
        delay_ms(10);
    }
}
