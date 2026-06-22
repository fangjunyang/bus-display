README.md

Bus Display Project

This repository contains the AT89C51 (12MHz) bus-stop display project with ST7920 128x64 LCD (serial mode). Files:
- src/main.c        : Keil C51 source (uses include/hzk16.h)
- include/hzk16.h   : placeholder HZK16 arrays and station mapping (auto-generated later)
- proteus/README_PROTEUS.txt : instructions how to open Proteus project (model: use built-in ST7920)
- tools/hzk16_convert.py : Python script to convert a full hzk16 binary into a header with only required characters

Notes:
- I will generate full HZK16 C arrays once you confirm or provide hzk16 binary. For now include/hzk16.h contains placeholders so you can build and test the project skeleton.
- Proteus: use built-in ST7920/12864 graphic LCD in your Proteus 8 Professional; instructions are in proteus/README_PROTEUS.txt

How I will proceed next:
- Replace placeholder hzk16 arrays with real 16x16 bitmaps generated from a standard hzk16 binary (I will add these arrays to include/hzk16.h)
- Add complete Keil project files and Proteus .dsn/.pdsprj
- Create a ZIP release in the repo
