"""Dump a JSR-184 .m3g file per the spec (little-endian): meshes, their
node transforms, vertex buffers and raw vertex extents."""
import struct, sys, zlib

data = open(sys.argv[1], 'rb').read()
assert data[:12] == b'\xabJSR184\xbb\r\n\x1a\n'
p = 12
objects = [None]  # index 0 = null
TYPES = {0: 'Header', 3: 'Appearance', 4: 'Background', 5: 'Camera', 6: 'CompositingMode',
         8: 'PolygonMode', 9: 'Group', 10: 'Image2D', 11: 'TriStrip', 12: 'Light', 13: 'Material',
         14: 'Mesh', 17: 'Texture2D', 20: 'VertexArray', 21: 'VertexBuffer', 22: 'World'}


class R:
    def __init__(self, b):
        self.b, self.p = b, 0
    def u8(self):
        v = self.b[self.p]; self.p += 1; return v
    def u16(self):
        v = struct.unpack_from('<H', self.b, self.p)[0]; self.p += 2; return v
    def i16(self):
        v = struct.unpack_from('<h', self.b, self.p)[0]; self.p += 2; return v
    def u32(self):
        v = struct.unpack_from('<I', self.b, self.p)[0]; self.p += 4; return v
    def i32(self):
        v = struct.unpack_from('<i', self.b, self.p)[0]; self.p += 4; return v
    def f(self):
        v = struct.unpack_from('<f', self.b, self.p)[0]; self.p += 4; return v
    def rest(self):
        return len(self.b) - self.p


def obj3d(r):
    uid = r.u32()
    for _ in range(r.u32()):
        r.u32()
    for _ in range(r.u32()):
        r.u32(); n = r.u32(); r.p += n
    return uid


def transformable(r):
    t = {}
    if r.u8():
        t['T'] = (r.f(), r.f(), r.f()); t['S'] = (r.f(), r.f(), r.f())
        t['R'] = (r.f(), r.f(), r.f(), r.f())
    if r.u8():
        t['M'] = [r.f() for _ in range(16)]
    return t


def node(r):
    t = transformable(r)
    r.u8(); r.u8(); r.u8(); r.u32()
    if r.u8():
        r.u8(); r.u8(); r.u32(); r.u32()
    return t


while p < len(data):
    comp = data[p]; total, uncomp = struct.unpack_from('<II', data, p + 1)
    body = data[p + 9:p + total - 4]
    if comp == 1:
        body = zlib.decompress(body)
    p += total
    q = 0
    while q < len(body):
        typ = body[q]; ln = struct.unpack_from('<I', body, q + 1)[0]
        ob = body[q + 5:q + 5 + ln]; q += 5 + ln
        r = R(ob)
        info = {'type': TYPES.get(typ, typ), 'len': ln}
        try:
            if typ == 20:
                info['uid'] = obj3d(r)
                cs, cc, enc, vc = r.u8(), r.u8(), r.u8(), r.u16()
                vals = []
                for i in range(vc * cc):
                    vals.append(struct.unpack('b', bytes([r.u8()]))[0] if cs == 1 else r.i16())
                if enc == 1:  # deltas
                    for i in range(cc, len(vals)):
                        vals[i] += vals[i - cc]
                        if cs == 1:
                            vals[i] = (vals[i] + 128) % 256 - 128
                        else:
                            vals[i] = (vals[i] + 32768) % 65536 - 32768
                info.update(cs=cs, cc=cc, enc=enc, vc=vc, leftover=r.rest())
                if cc >= 2:
                    ext = [(min(vals[k::cc]), max(vals[k::cc])) for k in range(cc)]
                    info['ext'] = ext
            elif typ == 21:
                info['uid'] = obj3d(r)
                r.u32()
                info['pos'] = r.u32(); info['bias'] = (r.f(), r.f(), r.f()); info['scale'] = r.f()
                info['norm'] = r.u32(); info['col'] = r.u32()
                info['leftover_before_tex'] = r.rest()
            elif typ == 14:
                info['uid'] = obj3d(r)
                info['xf'] = node(r)
                info['vb'] = r.u32(); n = r.u32()
                info['subs'] = [(r.u32(), r.u32()) for _ in range(n)]
                info['leftover'] = r.rest()
            elif typ in (9, 22):
                info['uid'] = obj3d(r)
                info['xf'] = node(r)
                info['children'] = [r.u32() for _ in range(r.u32())]
                info['leftover'] = r.rest()
        except Exception as e:
            info['ERR'] = repr(e)
        objects.append(info)

for i, o in enumerate(objects):
    if o is None:
        continue
    if o['type'] in ('Mesh', 'Group', 'World'):
        print(i, o)
        if o['type'] == 'Mesh':
            vb = objects[o['vb']]
            va = objects[vb['pos']] if vb.get('pos') else None
            print('    VB', o['vb'], {k: vb[k] for k in ('uid', 'pos', 'bias', 'scale', 'leftover_before_tex')})
            if va:
                print('    POS', vb['pos'], {k: va.get(k) for k in ('cs', 'cc', 'enc', 'vc', 'ext', 'leftover')})
