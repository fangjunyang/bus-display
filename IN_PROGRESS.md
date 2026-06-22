IN_PROGRESS.md

Status: URGENT - Option A (6h) started

Work started: generating HZK16 16x16 bitmaps for required Chinese station names and embedding them into include/hzk16.h; implementing Keil C51 source changes; building .hex; creating Proteus 128x64 (ST7920) .dsn simulation file.

ETA: within 6 hours from 2026-06-22T10:50Z (approx)

Planned deliverables in this commit cycle:
- include/hzk16.h : real 16x16 C arrays for characters used in the two routes
- src/main.c     : finalized C code with simplified 4-key input (Next, Prev, Switch, Edit), ST7920 serial driver, beep on arrival
- keil/          : Keil project files and generated .hex
- proteus/       : Proteus .dsn/.pdsprj (using built-in GRAPHIC LCD 128X64 - ST7920) and instructions
- diagrams/      : Program flowchart and system block diagram (PNG)
- release/       : ZIP built and attached as GitHub Release when ready

Notes:
- I will embed only the bitmaps required for the station names (not full HZK16) to minimize ROM size.
- If you need the full HZK16 file included, tell me and I will add it in a later commit.
