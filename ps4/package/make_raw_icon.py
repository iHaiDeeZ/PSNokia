# make_raw_icon.py <icon.png> <out.raw>: the game icon for the PSNokia menu,
# 256x256, as two little-endian 32-bit sizes followed by 0xAARRGGBB pixels
# (the menu has no PNG decoder).
import struct, sys
from PIL import Image
src, out = sys.argv[1:3]
im = Image.open(src).convert('RGBA').resize((256, 256), Image.LANCZOS)
data = bytearray(struct.pack('<II', 256, 256))
for r, g, b, a in im.getdata():
    data += struct.pack('<I', (a << 24) | (r << 16) | (g << 8) | b)
open(out, 'wb').write(data)
