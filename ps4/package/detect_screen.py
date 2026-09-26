# detect_screen.py <jar> [-v]: prints the phone screen a MIDlet was made
# for, "WxH", or "-" for the default (240x320: most later games read the
# screen size and adapt to it).
#
# 1. A size in the manifest (a few games declare one).
# 2. A full-screen image of a large screen's exact size (e.g. a 640x360
#    loading screen).
# 3. The game's code: games made for one screen use its width and height as
#    constants over and over, so the constants pushed with bipush/sipush are
#    counted in every method. Numbers like 360 and 640 are also angles and
#    fixed-point values, and 128 is a bit mask, so only the classic fixed
#    screens are taken from the code, and only when no image is much wider
#    than the screen. 128x128 and 128x160 were the MIDP 1.0 era, without 3D;
#    later games that use 128 a lot are adapting to the screen, not made
#    for it.
import re, struct, sys, zipfile

# Screens a full-screen image can reveal (240x320 is the default anyway)
IMAGE_SCREENS = [(640, 360), (360, 640), (320, 240), (240, 320), (176, 220), (176, 208)]
MIN_USES = 5            # times each of width and height must appear
STRONG_USES = 25        # so often that the images do not matter

MANIFEST_KEYS = ('Nokia-MIDlet-Original-Display-Size', 'Nokia-MIDlet-Target-Display-Size',
                 'Display-Size', 'MIDlet-Display-Size')


def manifest_size(manifest):
    manifest = re.sub(r'\r?\n ', '', manifest)
    for key in MANIFEST_KEYS:
        m = re.search(r'^' + re.escape(key) + r'\s*:\s*(\d+)\s*[,xX*]\s*(\d+)', manifest, re.M)
        if m:
            return int(m.group(1)), int(m.group(2))
    return None


# Bytecode walking: the length of each instruction
def _lengths():
    n = [1] * 256
    for op in (0x10, 0x12, 0x15, 0x16, 0x17, 0x18, 0x19, 0x36, 0x37, 0x38, 0x39,
               0x3a, 0xa9, 0xbc):
        n[op] = 2                               # bipush ldc loads stores ret newarray
    for op in list(range(0x99, 0xa9)) + [0x11, 0x13, 0x14, 0x84, 0xb2, 0xb3, 0xb4,
                                          0xb5, 0xb6, 0xb7, 0xb8, 0xbb, 0xbd, 0xc0,
                                          0xc1, 0xc6, 0xc7]:
        n[op] = 3                               # branches sipush ldc_w iinc fields ...
    n[0xc5] = 4                                 # multianewarray
    for op in (0xb9, 0xba, 0xc8, 0xc9):
        n[op] = 5                               # invokeinterface goto_w jsr_w
    return n


LENGTHS = _lengths()


def code_constants(code, counts):
    i = 0
    while i < len(code):
        op = code[i]
        if op == 0x10:                          # bipush
            v = struct.unpack_from('>b', code, i + 1)[0]
            counts[v] = counts.get(v, 0) + 1
        elif op == 0x11:                        # sipush
            v = struct.unpack_from('>h', code, i + 1)[0]
            counts[v] = counts.get(v, 0) + 1
        if op in (0xaa, 0xab):                  # tableswitch, lookupswitch
            j = (i + 4) & ~3
            if op == 0xaa:
                low, high = struct.unpack_from('>ii', code, j + 4)
                i = j + 12 + 4 * (high - low + 1)
            else:
                pairs = struct.unpack_from('>i', code, j + 4)[0]
                i = j + 8 + 8 * pairs
        elif op == 0xc4:                        # wide
            i += 6 if code[i + 1] == 0x84 else 4
        else:
            i += LENGTHS[op]


def class_constants(data, counts):
    """Adds the bipush/sipush constants of every method in a class file."""
    pos = 8
    count = struct.unpack_from('>H', data, pos)[0]
    pos += 2
    utf8 = {}
    i = 1
    while i < count:
        tag = data[pos]
        if tag == 1:
            length = struct.unpack_from('>H', data, pos + 1)[0]
            utf8[i] = data[pos + 3:pos + 3 + length]
            pos += 3 + length
        elif tag in (3, 4, 9, 10, 11, 12):
            pos += 5
        elif tag in (5, 6):
            pos += 9
            i += 1
        elif tag in (7, 8):
            pos += 3
        else:
            return
        i += 1
    pos += 6                                    # access, this, super
    interfaces = struct.unpack_from('>H', data, pos)[0]
    pos += 2 + 2 * interfaces
    for member in range(2):                     # fields, then methods
        n = struct.unpack_from('>H', data, pos)[0]
        pos += 2
        for _ in range(n):
            pos += 6
            attrs = struct.unpack_from('>H', data, pos)[0]
            pos += 2
            for _ in range(attrs):
                name, length = struct.unpack_from('>HI', data, pos)
                if member == 1 and utf8.get(name) == b'Code':
                    code_length = struct.unpack_from('>I', data, pos + 10)[0]
                    code_constants(data[pos + 14:pos + 14 + code_length], counts)
                pos += 6 + length


def detect(jar, verbose=False):
    z = zipfile.ZipFile(jar)
    try:
        manifest = z.read('META-INF/MANIFEST.MF').decode('latin-1')
    except KeyError:
        manifest = ''
    size = manifest_size(manifest)
    if size:
        return size, 'manifest'
    counts = {}
    images = set()
    uses_3d = False
    for name in z.namelist():
        data = z.read(name)
        if name.endswith('.class'):
            uses_3d = uses_3d or b'javax/microedition/m3g' in data
            try:
                class_constants(data, counts)
            except (struct.error, IndexError):
                pass
        elif data[:8] == b'\x89PNG\r\n\x1a\n' and len(data) >= 24:
            images.add(struct.unpack('>II', data[16:24]))
    widest = max([w for w, h in images] or [0])
    midp1 = re.search(r'^MicroEdition-Profile\s*:\s*MIDP-1', manifest, re.M) is not None
    if verbose:
        print('constants:', {v: counts.get(v, 0) for v in (128, 160, 176, 208, 220, 240, 320, 360, 640)},
              'widest image', widest, 'MIDP-1' if midp1 else 'MIDP-2')
    for screen in IMAGE_SCREENS:
        if screen in images:
            return (None if screen == (240, 320) else screen), 'image'

    def used(w, h):
        return min(counts.get(w, 0), counts.get(h, 0))

    for w, h in ((176, 220), (176, 208)):
        if used(w, h) >= STRONG_USES or (used(w, h) >= MIN_USES and widest <= w * 1.15):
            return (w, h), 'code'
    # No 128-pixel phone had the 3D API (JSR 184)
    if midp1 and not uses_3d:
        for w, h in ((128, 160), (128, 128)):
            if used(w, h) >= MIN_USES and widest <= 160:
                return (w, h), 'code'
    return None, 'default'


if __name__ == '__main__':
    size, how = detect(sys.argv[1], '-v' in sys.argv)
    if '-v' in sys.argv:
        print('from', how)
    print('%dx%d' % size if size else '-')
