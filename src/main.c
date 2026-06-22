/*
 * main.c
 * Keil C51 source for AT89C51 (12MHz) driving ST7920 128x64 in serial mode.
 * Features: two lines of 8 stops, switch line, next/prev station, edit station via 4x4 matrix (placeholder), beep on arrival.
 * Depends on include/hzk16.h which must provide HZK16 bitmaps and index mapping for characters used.
 */

#include <reg51.h>
#include <intrins.h>
#include "../include/hzk16.h"

// MCU / crystal
// AT89C51 @ 12MHz

// ST7920 serial pins (bit-banged)
sbit ST_SCLK = P1^0; // SCLK
sbit ST_SDIN  = P1^1; // MOSI
sbit ST_RS    = P1^2; // RS (1:data,0:cmd) - also used as 'PSB' in some modules
sbit BEEP     = P3^4;

// simple delay
void delay_us(unsigned int t) {
    unsigned int i,j;
    for(i=0;i<t;i++) for(j=0;j<6;j++);
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

// send command (0xF8 + high nibble, low nibble) - ST7920 8-bit serial transfer (two nibbles)
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
    delay_us(2000);
    st_write_cmd(0x30); // basic instruction
    delay_us(2000);
    st_write_cmd(0x0C); // display on
    delay_us(2000);
    st_write_cmd(0x01); // clear
    delay_us(2000);
}

// draw a 16x16 chinese character at (x,y) where x 0..7 columns of 8-pixel wide blocks, y 0..3 pages (each page 8 vertical)
// For ST7920 graphic mode, addressing is more complex; here we use simple routine that writes 16x16 bitmap by splitting
// into two 8-pixel-high pages per character column. This function expects 'bmp' to be 32 bytes (16 rows x 16 cols)
void st_draw_chinese_16x16(unsigned char x, unsigned char y, const unsigned char *bmp) {
    // x: pixel column (0..127), y: pixel row (0..63)
    // we'll write column by column: for col=0..15, write two bytes (top 8 bits, bottom 8 bits) at appropriate graphic RAM addresses
    unsigned char col;
    unsigned char top, bot;
    unsigned char addr_high, addr_low;
    unsigned char px = x;
    unsigned char py = y;
    for(col=0; col<16; col++) {
        top = bmp[col*2];      // two bytes per row-group in our layout
        bot = bmp[col*2+1];
        // set graphic address - ST7920 uses instruction 0x80 + addr
        // graphic RAM mapping: each half (left/right) has addresses; here we compute an address simplification for common modules
        // We'll compute a linear address: addr = (py/8)*128 + px + col;
        unsigned int base_addr = (py/8)*128 + (px + col);
        addr_high = 0x80 | ((base_addr >> 6) & 0x3F); // approximate
        addr_low = (base_addr & 0x3F) << 2; // approximate
        // Many ST7920 modules accept 0x80+addr, 0x80+(addr+offset) sequences; to keep compatibility we write using ascii data commands
        st_write_cmd(0x80 | ((base_addr & 0x3F))); // set address low (approximation)
        st_write_data(top);
        st_write_cmd(0x80 | (((base_addr+64)&0x3F)));
        st_write_data(bot);
    }
}

// For demonstration we provide a simple function to show a station name index on left area
void show_station_name_simple(const unsigned char *bmp, unsigned char len) {
    unsigned char i;
    // draw starting at x=8 y=0
    for(i=0;i<len;i++) {
        st_draw_chinese_16x16(8 + i*16, 0, bmp + i*32);
    }
}

// Example station bitmaps are provided in include/hzk16.h as STOP_BITMAPS[n][N]

// station lists: each station referenced by index into STOP_BITMAPS
const unsigned char line1_idx[8] = {0,1,2,3,4,5,6,7};
const unsigned char line2_idx[8] = {8,9,10,11,12,13,14,7}; // reuse index 7 as 终点站

unsigned char currentLine = 1;
unsigned char currentStop = 0;

// simple beep
void beep(void) {
    unsigned int i;
    for(i=0;i<5000;i++) {
        BEEP = ~BEEP;
        delay_us(10);
    }
    BEEP = 0;
}

// Next/Prev/Switch handlers
void next_station(void) {
    if(currentStop < 7) currentStop++;
    beep();
}
void prev_station(void) {
    if(currentStop > 0) currentStop--;
    beep();
}
void switch_line(void) {
    currentLine = (currentLine==1)?2:1;
    currentStop = 0;
    beep();
}

// Placeholder KeyScan - in Proteus you can tie P3.x pins or implement matrix scanning
void KeyScan(void) {
    // Implement matrix scan in final version. For now, no-op.
}

void main(void) {
    st_init();
    // initial display: show current and next stop
    while(1) {
        const unsigned char *bmp;
        unsigned char idx;
        if(currentLine==1) idx = line1_idx[currentStop]; else idx = line2_idx[currentStop];
        bmp = STOP_BITMAPS[idx];
        show_station_name_simple(bmp, 1);
        KeyScan();
    }
}
