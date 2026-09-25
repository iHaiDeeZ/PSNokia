# make_icon.py <jar> <out.png> <title>: 512x512 PS4 icon from the MIDlet icon.
# Exits with status 1 when the jar has no icon, so make_game.sh uses the
# PSNokia icon instead.
import io, re, sys, zipfile
from PIL import Image
jar, out, title = sys.argv[1:4]
z = zipfile.ZipFile(jar)
manifest = z.read('META-INF/MANIFEST.MF').decode('latin-1')
# Long manifest lines continue on the next line, which starts with a space
manifest = re.sub(r'\r?\n ', '', manifest)
candidates = []
m = re.search(r'MIDlet-1:\s*[^,]*,\s*([^,\r\n]*),', manifest)
if m:
    candidates.append(m.group(1))
m = re.search(r'MIDlet-Icon:\s*([^\r\n]*)', manifest)
if m:
    candidates.append(m.group(1))
candidates += ['icon.png', 'icons/icon.png']
icon = None
source = None
for candidate in candidates:
    candidate = candidate.strip().lstrip('/')
    if not candidate:
        continue
    try:
        icon = Image.open(io.BytesIO(z.read(candidate))).convert('RGBA')
        source = candidate
        break
    except Exception:
        pass
if icon is None:
    print('no icon in the jar')
    sys.exit(1)
tile = Image.new('RGB', (512, 512), (24, 28, 40))
if max(icon.size) >= 96:
    # Detailed icons of later phones: smooth scaling to about 400px
    scale = 400 / max(icon.size)
    icon = icon.resize((round(icon.size[0] * scale), round(icon.size[1] * scale)),
                       Image.LANCZOS)
else:
    # Small pixel-art icons: a whole-number scale keeps them crisp
    scale = 360 // max(icon.size)
    icon = icon.resize((icon.size[0] * scale, icon.size[1] * scale), Image.NEAREST)
tile.paste(icon, ((512 - icon.size[0]) // 2, (512 - icon.size[1]) // 2), icon)
tile.save(out)
print('icon from', source, icon.size)
