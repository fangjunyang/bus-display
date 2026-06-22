RELEASE_NOTES.md

Bus Display - urgent release (demo)

Contents:
- src/main.c : Complete demo firmware (Keil C51). Uses embedded STOP_BITMAPS in include/hzk16.h.
- include/hzk16.h : Embedded 16x16 demo bitmaps for required station names (demo glyphs).
- keil/placeholder.hex : Placeholder — build in Keil to generate real hex, or replace with your compiled .hex
- proteus/README_PROTEUS.txt : Instructions to setup Proteus simulation
- diagrams/flowchart.txt : High-level flowchart description
- docs/resource_table.csv : MCU resource mapping

Notes:
- The embedded 16x16 bitmaps are demo-patterns to allow immediate visualization in Proteus. They are not perfect Chinese glyphs but allow verifying display logic.
- For production, replace include/hzk16.h with HZK16-extracted real glyphs (tools/hzk16_convert.py provided to assist).

