# make_icon.py <jar> <out.png> <title>: 512x512 PS4 icon from the MIDlet icon
import io, re, sys, zipfile
from PIL import Image
jar, out, title = sys.argv[1:4]
z = zipfile.ZipFile(jar)
manifest = z.read('META-INF/MANIFEST.MF').decode('latin-1')
m = re.search(r'MIDlet-1:\s*[^,]*,\s*([^,]*),', manifest)
name = m.group(1).strip().lstrip('/') if m else ''
icon = None
for candidate in (name, 'icon.png', 'icons/icon.png'):
    try:
        icon = Image.open(io.BytesIO(z.read(candidate))).convert('RGBA')
        break
    except Exception:
        pass
tile = Image.new('RGB', (512, 512), (24, 28, 40))
if icon is not None:
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
print('icon from', name or '(none)', icon.size if icon else '')
