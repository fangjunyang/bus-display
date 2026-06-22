# tools/hzk16_convert.py
# Python utility to extract required HZK16 characters and generate a C header used by the project.
# Usage: python hzk16_convert.py hzk16.bin mapping.txt out.h
# mapping.txt: list of GB2312 hex codes (one per line) for characters you need, e.g. "B0A7" (without 0x)

import sys

if len(sys.argv) != 4:
    print('Usage: python hzk16_convert.py hzk16.bin mapping.txt out.h')
    sys.exit(1)

hzk_file = sys.argv[1]
mapping_file = sys.argv[2]
out_file = sys.argv[3]

with open(mapping_file,'r',encoding='utf-8') as f:
    keys = [line.strip() for line in f if line.strip()]

records = []
with open(hzk_file,'rb') as fh:
    for key in keys:
        area = int(key[0:2],16)
        index = int(key[2:4],16)
        offset = ((area - 0xA0 -1) * 94 + (index - 0xA0 -1)) * 32
        fh.seek(offset)
        data = fh.read(32)
        records.append((key,data))

with open(out_file,'w',encoding='utf-8') as fo:
    fo.write('/* Generated header from hzk16 */\n')
    fo.write('#ifndef HZK_AUTO_H\n')
    fo.write('#define HZK_AUTO_H\n\n')
    fo.write('const unsigned char HZK_AUTO[] = {\n')
    for k,d in records:
        fo.write(' /* %s */\n' % k)
        fo.write('    ' + ','.join('0x%02X' % b for b in d) + ',\n')
    fo.write('};\n')
    fo.write('\n#endif\n')

print('Wrote', out_file)
