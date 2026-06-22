/*
 * main.c
 * Keil C51 source for AT89C51 (12MHz) driving ST7920 128x64 in serial mode.
 * Features: two lines of 8 stops, switch line, next/prev station, edit station via simple input (Edit cycles through preset names), beep on arrival.
 * Uses include/hzk16.h STOP_BITMAPS[16][32] for 16x16 bitmaps.
 */

#include <reg51.h>
#include <intrins.h>
#include "../include/hzk16.h"

// MCU / crystal
// AT89C51 @ 12MHz

// ST7920 serial pins (bit-banged)
sbit ST_SCLK = P1^0; // SCLK
sbit ST_SDIN  = P1^1; // MOSI
sbit ST_RS    = P1^2; // RS (1:data,0:cmd)
sbit BEEP     = P3^4;

// Keys mapped to P3.0..P3.3 (active low when pressed)
#define KEY_NEXT   0x01 // P3.0
#define KEY_PREV   0x02 // P3.1
#define KEY_SWITCH 0x04 // P3.2
#define KEY_EDIT   0x08 // P3.3

// simple delay (tuned roughly for 12MHz)
void delay_ms(unsigned int ms) {
    unsigned int i,j;
    for(i=0;i<ms;i++) for(j=0;j<120;j++);
}

void delay_us(unsigned int t) {
    unsigned int i,j;
    for(i=0;i<t;i++) for(j=0;j<2;j++);
}

void st_write_byte(unsigned char b) {
    unsigned char i;
    for(i=0;i<8;i++) {
        ST_SCLK = 0;
        ST_SDIN = (b & 0x80) ? 1 : 0;
        b <<= 1;
        delay_us(2);
        ST_SCLK = 1;
        delay_us(2);
    }
}

// send command (0xF8 + high nibble, low nibble)
void st_write_cmd(unsigned char cmd) {
    ST_RS = 0; // command
    delay_us(2);
    st_write_byte(0xF8);
    st_write_byte(cmd & 0xF0);
    st_write_byte((cmd<<4) & 0xF0);
    delay_us(50);
}

// send data
void st_write_data(unsigned char dat) {
    ST_RS = 1; // data
    delay_us(2);
    st_write_byte(0xFA);
    st_write_byte(dat & 0xF0);
    st_write_byte((dat<<4) & 0xF0);
    delay_us(50);
}

void st_init() {
    ST_SCLK = 0;
    ST_SDIN = 0;
    ST_RS = 0;
    delay_ms(50);
    st_write_cmd(0x30); // basic instruction
    delay_ms(5);
    st_write_cmd(0x0C); // display on
    delay_ms(5);
    st_write_cmd(0x01); // clear
    delay_ms(5);
}

// clear whole graphic ram area by writing zeros
void st_clear_graphic(void) {
    unsigned int addr;
    unsigned char page, col;
    // ST7920 graphic area: 8 pages of 128 bytes each (approximation)
    for(page=0; page<8; page++) {
        for(col=0; col<128; col++) {
            // set graphic address: 0x80 + addr (we use addr = page*128 + col)
            st_write_cmd(0x80 | (col & 0x3F));
            st_write_data(0x00);
        }
    }
}

// draw a 16x16 bitmap at pixel position (x,y). bmp is 32 bytes: for col=0..15, [2*col] top8, [2*col+1] bot8
void st_draw_16x16(unsigned char x, unsigned char y, const unsigned char *bmp) {
    unsigned char col;
    unsigned int base_addr;
    unsigned char top, bot;
    unsigned char page;
    // y must be multiple of 8 for this simplified writer (0,8,16,24,32,40,48,56)
    for(col=0; col<16; col++) {
        top = bmp[col*2];
        bot = bmp[col*2+1];
        // write top byte at (x+col, y)
        base_addr = (y/8)*128 + (x + col);
        st_write_cmd(0x80 | (base_addr & 0x3F));
        st_write_data(top);
        // write bottom byte at (x+col, y+8)
        base_addr = ((y/8)+1)*128 + (x + col);
        st_write_cmd(0x80 | (base_addr & 0x3F));
        st_write_data(bot);
    }
}

// show station name (single 16x16 char) at top or bottom row
void show_stop(unsigned char xpos, unsigned char ypos, unsigned char stop_index) {
    if(stop_index >= 16) return;
    st_draw_16x16(xpos, ypos, STOP_BITMAPS[stop_index]);
}

// simple beep
void beep(void) {
    unsigned int i;
    for(i=0;i<2000;i++) {
        BEEP = 0;
        delay_us(50);
        BEEP = 1;
        delay_us(50);
    }
    BEEP = 0;
}

// station indices
const unsigned char line1_idx[8] = {0,1,2,3,4,5,6,7};
const unsigned char line2_idx[8] = {0,8,9,10,11,12,13,7};

unsigned char currentLine = 1;
unsigned char currentStop = 0;

// read keys (P3 bits), return mask with bits 0..3 representing keys
unsigned char read_keys(void) {
    unsigned char p = P3 & 0x0F; // read lower nibble
    // active low: pressed -> 0; convert to mask where bit=1 means pressed
    unsigned char mask = 0;
    if(!(p & 0x01)) mask |= KEY_NEXT;
    if(!(p & 0x02)) mask |= KEY_PREV;
    if(!(p & 0x04)) mask |= KEY_SWITCH;
    if(!(p & 0x08)) mask |= KEY_EDIT;
    return mask;
}

// debounce and wait for release
unsigned char wait_key_press(void) {
    unsigned char k;
    while(1) {
        k = read_keys();
        if(k) {
            delay_ms(20);
            if(read_keys() == k) break;
        }
    }
    // wait release
    while(read_keys()) ;
    delay_ms(20);
    return k;
}

// simple edit: cycling through preset alternatives for current stop (placeholder)
void edit_station(void) {
    // For quick demo, when Edit pressed we advance current stop name index by +1 within available STOP_BITMAPS
    unsigned char idx = (currentLine==1)? line1_idx[currentStop]: line2_idx[currentStop];
    idx = (idx + 1) % 14; // cycle among available bitmaps
    if(currentLine==1) {
        ((unsigned char*)line1_idx)[currentStop] = idx; // note: writing to const for demo; better to use non-const in full impl
    } else {
        ((unsigned char*)line2_idx)[currentStop] = idx;
    }
}

void KeyScan(void) {
    unsigned char k = read_keys();
    if(k) {
        unsigned char pressed = wait_key_press();
        if(pressed & KEY_NEXT) {
            if(currentStop < 7) currentStop++;
            beep();
        } else if(pressed & KEY_PREV) {
            if(currentStop > 0) currentStop--;
            beep();
        } else if(pressed & KEY_SWITCH) {
            currentLine = (currentLine==1)?2:1;
            currentStop = 0;
            beep();
        } else if(pressed & KEY_EDIT) {
            edit_station();
            beep();
        }
        // After action, small delay
        delay_ms(50);
    }
}

void main(void) {
    st_init();
    st_clear_graphic();
    while(1) {
        unsigned char idx_cur, idx_next;
        if(currentLine==1) {
            idx_cur = line1_idx[currentStop];
            idx_next = line1_idx[(currentStop<7)?(currentStop+1):7];
        } else {
            idx_cur = line2_idx[currentStop];
            idx_next = line2_idx[(currentStop<7)?(currentStop+1):7];
        }
        // show current at x=8,y=0 and next at x=8,y=32
        show_stop(8, 0, idx_cur);
        show_stop(8, 32, idx_next);
        KeyScan();
    }
}
