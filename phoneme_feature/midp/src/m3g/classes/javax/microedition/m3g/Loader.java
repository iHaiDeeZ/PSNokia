package javax.microedition.m3g;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Vector;
import java.util.Hashtable;

/**
 * Parser for the JSR184 (.m3g) binary file format. Scoped to what real
 * bundled MIDlets (Tower Bloxx) actually need: static, unlit, untextured-
 * beyond-simple-replace scene graphs (Group/World/Mesh/Camera/Background/
 * Appearance/Material/Texture2D/Image2D/VertexArray/VertexBuffer/
 * TriangleStripArray). Object types this port doesn't render
 * (AnimationController/AnimationTrack/CompositingMode/Fog/PolygonMode/
 * Light/KeyframeSequence/Sprite3D/MorphingMesh/SkinnedMesh) are still
 * structurally parsed (their ObjectIndex slot is reserved so later
 * references resolve correctly) but their payload is skipped, not
 * decoded - safe because every ObjectChunk self-declares its own byte
 * length, so an unparsed/partially-understood chunk can always be
 * skipped to its declared end without derailing the rest of the file.
 * Every object's field-level parsing below also unconditionally seeks to
 * the chunk's declared end afterward, as a defensive safety net against
 * this implementation getting some field's exact layout slightly wrong -
 * that degrades to "this one object's data may be wrong" rather than
 * "the whole file fails to parse".
 */
public class Loader {

    private static final byte[] MAGIC = {
        (byte) 0xAB, 'J', 'S', 'R', '1', '8', '4', (byte) 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
    };

    private Loader() {
    }

    public static Object3D[] load(String name) throws IOException {
        InputStream in = new Object().getClass().getResourceAsStream(name);
        if (in == null) {
            throw new IOException("resource not found: " + name);
        }
        try {
            return new Parser(in).parse();
        } finally {
            try { in.close(); } catch (IOException e) { }
        }
    }

    private static final class Parser {
        private final DataInputStream in;
        /** Every object parsed so far, in file order. Index 0 is reserved (ObjectIndex 0 == null). */
        private final Vector allObjects = new Vector();
        private final Vector roots = new Vector();
        private Hashtable userIDMap = new Hashtable();

        Parser(InputStream in) {
            this.in = new DataInputStream(in);
            allObjects.addElement(null); // index 0 = null reference
        }

        Object3D[] parse() throws IOException {
            byte[] magic = new byte[12];
            in.readFully(magic);
            for (int i = 0; i < 12; i++) {
                if (magic[i] != MAGIC[i]) {
                    throw new IOException("not an M3G file");
                }
            }
            while (true) {
                int b = readSection();
                if (b < 0) {
                    break;
                }
            }
            Object3D[] result = new Object3D[roots.size()];
            for (int i = 0; i < result.length; i++) {
                result[i] = (Object3D) roots.elementAt(i);
            }
            // Attach the userID map to every parsed World so World.find() works.
            for (int i = 0; i < allObjects.size(); i++) {
                Object o = allObjects.elementAt(i);
                if (o instanceof World) {
                    ((World) o).byUserID = userIDMap;
                }
            }
            return result;
        }

        /** Returns -1 at end of stream, otherwise the number of chunks read in this section. */
        private int readSection() throws IOException {
            int compressionScheme;
            try {
                compressionScheme = in.readUnsignedByte();
            } catch (java.io.EOFException e) {
                return -1;
            }
            long totalSectionLength = readU32();
            long uncompressedLength = readU32();
            long objectBytes = totalSectionLength - 9 - 4; // minus this 9-byte header and the trailing 4-byte CRC

            byte[] payload;
            if (compressionScheme == 0) {
                payload = new byte[(int) objectBytes];
                in.readFully(payload);
            } else {
                // Compressed sections are not supported - skip the payload (its objects
                // will simply be missing) rather than fail the whole file.
                skipBytes(objectBytes);
                readU32(); // CRC
                return 0;
            }
            readU32(); // CRC, not verified

            DataInputStream sec = new DataInputStream(new java.io.ByteArrayInputStream(payload));
            int count = 0;
            while (sec.available() > 0) {
                readObjectChunk(sec);
                count++;
            }
            return count;
        }

        private void readObjectChunk(DataInputStream sec) throws IOException {
            int type = sec.readUnsignedByte();
            long length = readU32(sec);
            byte[] data = new byte[(int) length];
            sec.readFully(data);
            DataInputStream body = new DataInputStream(new java.io.ByteArrayInputStream(data));

            Object obj = null;
            try {
                obj = parseObject(type, body);
            } catch (Exception e) {
                System.out.println("M3G_CHUNK_PARSE_EXC type=" + type + " " + e);
            }
            if (obj instanceof Object3D && ((Object3D) obj).userID != 0) {
                userIDMap.put(new Integer(((Object3D) obj).userID), obj);
            }
            allObjects.addElement(obj);
            if (obj instanceof World) {
                roots.addElement(obj);
            }
        }

        // ---- per-type parsing ----

        /** Returns an Object3D for most types, or a raw int[] (flattened triangle indices) for TriangleStripArray. */
        private Object parseObject(int type, DataInputStream b) throws IOException {
            switch (type) {
                case 0: // Header - no common Object3D fields
                    return null;
                case 9: return parseGroup(b, new Group());
                case 22: return parseWorld(b);
                case 14: return parseMesh(b);
                case 5: return parseCamera(b);
                case 4: return parseBackground(b);
                case 3: return parseAppearance(b);
                case 13: return parseMaterial(b);
                case 17: return parseTexture2D(b);
                case 10: return parseImage2D(b);
                case 20: return parseVertexArray(b);
                case 21: return parseVertexBuffer(b);
                case 11: return parseTriangleStripArray(b);
                default:
                    return null; // not modeled - structurally skipped by the caller's length-based read
            }
        }

        private void readObject3DHeader(DataInputStream b, Object3D o) throws IOException {
            o.userID = (int) readU32(b);
            long animTrackCount = readU32(b);
            long userParamCount = -1;
            for (long i = 0; i < animTrackCount; i++) {
                readU32(b); // ObjectIndex, unresolved - animation not modeled
            }
            userParamCount = readU32(b);
            for (long i = 0; i < userParamCount; i++) {
                readU32(b); // parameterID
                long byteCount = readU32(b);
                skipBytes(b, byteCount);
            }
        }

        private void readTransformableHeader(DataInputStream b, Node n) throws IOException {
            boolean hasComponentTransform = readBoolean(b);
            if (hasComponentTransform) {
                float tx = readFloat(b), ty = readFloat(b), tz = readFloat(b);
                float sx = readFloat(b), sy = readFloat(b), sz = readFloat(b);
                float angle = readFloat(b);
                float ax = readFloat(b), ay = readFloat(b), az = readFloat(b);
                n.transform.setIdentity();
                n.transform.postTranslate(tx, ty, tz);
                n.transform.postRotate(angle, ax, ay, az);
                float[] m = new float[16];
                n.transform.get(m);
                m[0] *= sx; m[5] *= sy; m[10] *= sz;
                n.transform.set(m);
            }
            boolean hasGeneralTransform = readBoolean(b);
            if (hasGeneralTransform) {
                float[] m = new float[16];
                for (int i = 0; i < 16; i++) {
                    m[i] = readFloat(b);
                }
                // General transform is column-major per spec; this port's Transform
                // is row-major, so transpose on the way in.
                float[] rowMajor = new float[16];
                for (int r = 0; r < 4; r++) {
                    for (int c = 0; c < 4; c++) {
                        rowMajor[r * 4 + c] = m[c * 4 + r];
                    }
                }
                n.transform.set(rowMajor);
            }
        }

        private void readNodeHeader(DataInputStream b, Node n) throws IOException {
            readBoolean(b); // enableRendering
            readBoolean(b); // enablePicking
            b.readUnsignedByte(); // alphaFactor
            readU32(b); // scope
            boolean hasAlignment = readBoolean(b);
            if (hasAlignment) {
                b.readUnsignedByte(); // zTarget
                b.readUnsignedByte(); // yTarget
                readU32(b); // zReference
                readU32(b); // yReference
            }
        }

        private Node readNodeCommon(DataInputStream b, Node n) throws IOException {
            readObject3DHeader(b, n);
            readTransformableHeader(b, n);
            readNodeHeader(b, n);
            return n;
        }

        private Group parseGroup(DataInputStream b, Group g) throws IOException {
            readNodeCommon(b, g);
            long childCount = readU32(b);
            for (long i = 0; i < childCount; i++) {
                int idx = (int) readU32(b);
                Object o = resolve(idx);
                if (o instanceof Node) {
                    g.addChild((Node) o);
                }
            }
            return g;
        }

        private World parseWorld(DataInputStream b) throws IOException {
            World w = new World();
            parseGroup(b, w);
            int bgIdx = (int) readU32(b);
            int camIdx = (int) readU32(b);
            Object bg = resolve(bgIdx);
            Object cam = resolve(camIdx);
            if (bg instanceof Background) w.background = (Background) bg;
            if (cam instanceof Camera) w.activeCamera = (Camera) cam;
            return w;
        }

        private Mesh parseMesh(DataInputStream b) throws IOException {
            Mesh m = new Mesh(null, new int[0][], new Appearance[0]);
            readNodeCommon(b, m);
            int vbIdx = (int) readU32(b);
            Object vb = resolve(vbIdx);
            m.vertices = (vb instanceof VertexBuffer) ? (VertexBuffer) vb : null;
            long submeshCount = readU32(b);
            if (submeshCount < 0 || submeshCount > b.available()) {
                submeshCount = 0; // field layout mismatch - same defensive fallback as elsewhere
            }
            int[][] tris = new int[(int) submeshCount][];
            Appearance[] apps = new Appearance[(int) submeshCount];
            for (int i = 0; i < submeshCount; i++) {
                int ibIdx = (int) readU32(b);
                int apIdx = (int) readU32(b);
                Object ib = resolve(ibIdx);
                Object ap = resolve(apIdx);
                tris[i] = (ib instanceof int[]) ? (int[]) ib : new int[0];
                apps[i] = (ap instanceof Appearance) ? (Appearance) ap : new Appearance();
            }
            m.submeshTriangles = tris;
            m.appearances = apps;
            return m;
        }

        private Camera parseCamera(DataInputStream b) throws IOException {
            Camera c = new Camera();
            readNodeCommon(b, c);
            int projType = b.readUnsignedByte();
            if (projType == Camera.GENERIC) {
                for (int i = 0; i < 16; i++) readFloat(b);
            } else {
                float fovyOrNear = readFloat(b);
                float aspectOrFar = readFloat(b);
                float nearOrLeft = readFloat(b);
                float farOrRight = readFloat(b);
                if (projType == Camera.PERSPECTIVE) {
                    c.setPerspective(fovyOrNear, aspectOrFar, nearOrLeft, farOrRight);
                }
                // PARALLEL not modeled - not used by real bundled MIDlets tested so far.
            }
            return c;
        }

        private Background parseBackground(DataInputStream b) throws IOException {
            Background bg = new Background();
            readObject3DHeader(b, bg);
            int r = b.readUnsignedByte(), g = b.readUnsignedByte(), bl = b.readUnsignedByte(), a = b.readUnsignedByte();
            bg.color = (a << 24) | (r << 16) | (g << 8) | bl;
            readU32(b); // backgroundImage ObjectIndex - image backgrounds not modeled
            b.readUnsignedByte(); // backgroundImageModeX
            b.readUnsignedByte(); // backgroundImageModeY
            int cx = readS32(b), cy = readS32(b), cw = readS32(b), ch = readS32(b);
            bg.colorClearEnable = readBoolean(b);
            bg.depthClearEnable = readBoolean(b);
            return bg;
        }

        private static int diagAppearanceDump = 0;
        private static int diagImage2DDump = 0;

        private Appearance parseAppearance(DataInputStream bOrig) throws IOException {
            Appearance ap = new Appearance();
            readObject3DHeader(bOrig, ap);
            int remaining = bOrig.available();
            byte[] rest = new byte[remaining];
            bOrig.readFully(rest);
            if (remaining != 25 && diagAppearanceDump < 20) {
                diagAppearanceDump++;
                StringBuffer hex = new StringBuffer();
                for (int i = 0; i < rest.length && i < 40; i++) {
                    int v = rest[i] & 0xFF;
                    if (v < 16) hex.append('0');
                    hex.append(Integer.toHexString(v)).append(' ');
                }
                System.out.println("M3G_APPEARANCE_RAW_ANOMALY remaining=" + remaining + " bytes=" + hex);
            }
            DataInputStream b = new DataInputStream(new java.io.ByteArrayInputStream(rest));
            b.readByte(); // layer (signed byte, range -63..63)
            readU32(b); // compositingMode
            readU32(b); // fog
            readU32(b); // polygonMode
            int matIdx = (int) readU32(b);
            Object mat = resolve(matIdx);
            if (mat instanceof Material) ap.material = (Material) mat;
            /*
             * There is no explicit texture-unit count field: confirmed via
             * two raw hex dumps (M3G_APPEARANCE_RAW/_ANOMALY) that some real
             * chunks end after exactly 1 more ObjectIndex field (21 bytes
             * total) and others after exactly 2 (25 bytes total), with zero
             * bytes left over either way. So the real rule is simply "read
             * one ObjectIndex-sized texture slot at a time until the chunk's
             * own declared length runs out" (matching this whole parser's
             * declared per-object self-bounding design), not a fixed count
             * of 8 (the original assumption, which overran every chunk by 6
             * slots/24 bytes and threw EOFException on all of them) nor a
             * fixed count of 2 (an intermediate guess that also turned out
             * wrong once a real 1-texture-slot chunk showed up). The 8-slot
             * cap below is just a safety bound against a corrupt chunk
             * running away, not the real per-object limit.
             */
            for (int i = 0; i < 8 && b.available() >= 4; i++) {
                int texIdx = (int) readU32(b);
                Object tex = resolve(texIdx);
                if (i < ap.textures.length && tex instanceof Texture2D) {
                    ap.textures[i] = (Texture2D) tex;
                }
            }
            return ap;
        }

        private Material parseMaterial(DataInputStream b) throws IOException {
            Material m = new Material();
            readObject3DHeader(b, m);
            // Not decoded further - real bundled MIDlets call setMaterial(null) anyway,
            // so exact ambient/diffuse/specular/emissive/shininess values don't matter
            // here; the caller's defensive seek-to-chunk-end handles the remaining bytes.
            return m;
        }

        private Texture2D parseTexture2D(DataInputStream b) throws IOException {
            Node dummy = new Node(); // reuse the Transformable-header reader without a real Node subtype
            readObject3DHeader(b, dummy);
            readTransformableHeader(b, dummy);
            int imgIdx = (int) readU32(b);
            Object img = resolve(imgIdx);
            Texture2D t = new Texture2D(img instanceof Image2D ? (Image2D) img : null);
            b.readUnsignedByte(); b.readUnsignedByte(); b.readUnsignedByte(); // blendColor RGB
            t.blending = b.readUnsignedByte();
            t.wrapS = b.readUnsignedByte();
            t.wrapT = b.readUnsignedByte();
            b.readUnsignedByte(); // levelFilter
            b.readUnsignedByte(); // imageFilter
            return t;
        }

        private Image2D parseImage2D(DataInputStream b) throws IOException {
            Object3D header = new Material();
            readObject3DHeader(b, header);
            /*
             * format is a single Byte, not a UInt32 - confirmed via live
             * diagnostic logging: every observed "format" value decoded as
             * (garbage upper 3 bytes) + 99 in the LOW byte, and 99 is
             * Image2D.RGB, a real valid format constant. Reading 4 bytes
             * here consumed 3 bytes that actually belong to isMutable/width,
             * cascading misalignment through the rest of the object and
             * producing width/height values in the tens of thousands -
             * meaning EVERY texture in the file failed the sane-size check
             * below and fell back to the 1x1 white placeholder. This is why
             * nothing ever appeared textured, no matter what else was fixed.
             */
            int format = b.readUnsignedByte();
            boolean isMutable = readBoolean(b);
            int width = (int) readU32(b);
            int height = (int) readU32(b);
            int[] pixels;
            int bpp = bytesPerPixel(format);
            boolean ok = !isMutable && width > 0 && height > 0 && width <= 2048 && height <= 2048;
            long paletteByteCount = -1, pixelDataByteCount = -1;
            if (ok) {
                try {
                    /*
                     * Real layout (confirmed via hex-dumping real chunks and
                     * matching total byte counts exactly against `avail`,
                     * not just guessed): a palette section (byte length
                     * explicitly given, 0 if unused) comes before the pixel
                     * data, and the pixel data ALSO has its own explicit
                     * byte-length prefix rather than being purely implied by
                     * width*height*bpp - e.g. one real chunk was exactly
                     * [paletteByteCount=54][54 palette bytes][pixelDataByteCount
                     * =4096][4096 index bytes], 4+54+4+4096=4158, matching
                     * that chunk's remaining length with zero left over.
                     * When paletteByteCount>0, pixel data is one BYTE PER
                     * PIXEL (a palette index, 0-based) rather than raw
                     * format-encoded pixels; the palette itself holds
                     * (paletteByteCount / bytesPerPixel(format)) real colors
                     * in the same per-format encoding decodePixels() already
                     * handles.
                     */
                    paletteByteCount = readU32(b);
                    int[] paletteColors = null;
                    if (paletteByteCount < 0 || paletteByteCount > b.available()) {
                        ok = false;
                    } else if (paletteByteCount > 0 && bpp > 0) {
                        byte[] paletteRaw = new byte[(int) paletteByteCount];
                        b.readFully(paletteRaw);
                        int numColors = (int) (paletteByteCount / bpp);
                        paletteColors = decodePixels(format, numColors, 1, paletteRaw);
                    }
                    pixelDataByteCount = ok ? readU32(b) : -1;
                    long expected = paletteColors != null ? (long) width * height : (long) width * height * bpp;
                    if (ok && pixelDataByteCount == expected && pixelDataByteCount > 0 && pixelDataByteCount <= b.available()) {
                        byte[] raw = new byte[(int) pixelDataByteCount];
                        b.readFully(raw);
                        if (paletteColors != null) {
                            pixels = new int[width * height];
                            for (int i = 0; i < pixels.length; i++) {
                                int idx = raw[i] & 0xFF;
                                pixels[i] = (idx < paletteColors.length) ? paletteColors[idx] : 0xFFFFFFFF;
                            }
                        } else {
                            pixels = decodePixels(format, width, height, raw);
                        }
                    } else {
                        ok = false;
                        pixels = null;
                    }
                } catch (IOException e) {
                    ok = false;
                    pixels = null;
                }
            } else {
                pixels = null;
            }
            if (!ok || pixels == null) {
                // Field layout mismatch for this chunk (declared/derived size doesn't fit
                // what's actually left in the chunk) - fall back to a small placeholder
                // rather than risk a huge/garbage allocation. The mesh using this texture
                // will just render untextured instead of hanging or OOMing.
                System.out.println("M3G_IMAGE2D_REJECTED paletteBytes=" + paletteByteCount + " pixelDataBytes=" + pixelDataByteCount);
                width = width > 0 && width <= 2048 ? width : 1;
                height = height > 0 && height <= 2048 ? height : 1;
                pixels = new int[width * height];
                for (int i = 0; i < pixels.length; i++) {
                    pixels[i] = 0xFFFFFFFF;
                }
            }
            Image2D img = new Image2D(format, width, height, pixels);
            img.userID = header.userID;
            return img;
        }

        private static int bytesPerPixel(int format) {
            switch (format) {
                case Image2D.ALPHA: return 1;
                case Image2D.LUMINANCE: return 1;
                case Image2D.LUMINANCE_ALPHA: return 2;
                case Image2D.RGB: return 3;
                case Image2D.RGBA: return 4;
                default: return 4;
            }
        }

        private static int[] decodePixels(int format, int width, int height, byte[] raw) {
            int[] out = new int[width * height];
            int idx = 0;
            for (int i = 0; i < out.length; i++) {
                switch (format) {
                    case Image2D.ALPHA: {
                        int a = raw[idx++] & 0xFF;
                        out[i] = a << 24;
                        break;
                    }
                    case Image2D.LUMINANCE: {
                        int l = raw[idx++] & 0xFF;
                        out[i] = 0xFF000000 | (l << 16) | (l << 8) | l;
                        break;
                    }
                    case Image2D.LUMINANCE_ALPHA: {
                        int l = raw[idx++] & 0xFF;
                        int a = raw[idx++] & 0xFF;
                        out[i] = (a << 24) | (l << 16) | (l << 8) | l;
                        break;
                    }
                    case Image2D.RGB: {
                        int r = raw[idx++] & 0xFF, g = raw[idx++] & 0xFF, bch = raw[idx++] & 0xFF;
                        out[i] = 0xFF000000 | (r << 16) | (g << 8) | bch;
                        break;
                    }
                    case Image2D.RGBA:
                    default: {
                        int r = raw[idx++] & 0xFF, g = raw[idx++] & 0xFF, bch = raw[idx++] & 0xFF, a = raw[idx++] & 0xFF;
                        out[i] = (a << 24) | (r << 16) | (g << 8) | bch;
                        break;
                    }
                }
            }
            return out;
        }

        private VertexArray parseVertexArray(DataInputStream b) throws IOException {
            Object3D header = new Material(); // any concrete Object3D to host the common header
            readObject3DHeader(b, header);
            int componentSize = b.readUnsignedByte();
            int componentCount = b.readUnsignedByte();
            int vertexCount = readU16BE(b);
            int total = vertexCount * componentCount;
            if (total < 0 || total > b.available()) {
                // Field layout mismatch for this chunk - empty array, same defensive
                // fallback used elsewhere in this parser.
                return new VertexArray(0, componentCount == 0 ? 1 : componentCount, new float[0]);
            }
            float[] data = new float[total];
            for (int i = 0; i < data.length; i++) {
                if (componentSize == 1) {
                    data[i] = b.readByte();
                } else {
                    data[i] = readS16(b);
                }
            }
            VertexArray va = new VertexArray(vertexCount, componentCount, data);
            va.userID = header.userID;
            return va;
        }

        private VertexBuffer parseVertexBuffer(DataInputStream b) throws IOException {
            VertexBuffer vb = new VertexBuffer();
            readObject3DHeader(b, vb);
            int r = b.readUnsignedByte(), g = b.readUnsignedByte(), bl = b.readUnsignedByte(), a = b.readUnsignedByte();
            vb.defaultColor = (a << 24) | (r << 16) | (g << 8) | bl;
            int posIdx = (int) readU32(b);
            Object pos = resolve(posIdx);
            float pbx = 0, pby = 0, pbz = 0, psc = 1;
            if (pos != null) {
                pbx = readFloat(b); pby = readFloat(b); pbz = readFloat(b);
                psc = readFloat(b);
            }
            if (pos instanceof VertexArray) {
                vb.positions = applyBiasScale((VertexArray) pos, pbx, pby, pbz, 0, psc);
            }
            int normIdx = (int) readU32(b);
            Object norm = resolve(normIdx);
            if (norm instanceof VertexArray) {
                vb.normals = (VertexArray) norm;
            }
            int colIdx = (int) readU32(b);
            Object col = resolve(colIdx);
            if (col instanceof VertexArray) {
                vb.colors = (VertexArray) col;
            }
            long texArrayCount = readU32(b);
            for (int i = 0; i < texArrayCount; i++) {
                int tcIdx = (int) readU32(b);
                Object tc = resolve(tcIdx);
                float bu = readFloat(b), bv = readFloat(b);
                float su = readFloat(b);
                if (i == 0 && tc instanceof VertexArray) {
                    vb.texCoords = applyBiasScale((VertexArray) tc, bu, bv, 0, 0, su);
                }
            }
            return vb;
        }

        /** Positions/texcoords are stored pre-decoded from int component values via (value*scale)+bias, per component. */
        private VertexArray applyBiasScale(VertexArray src, float b0, float b1, float b2, float b3, float scale) {
            float[] out = new float[src.data.length];
            float[] bias = {b0, b1, b2, b3};
            int cc = src.componentCount;
            for (int i = 0; i < src.data.length; i++) {
                out[i] = src.data[i] * scale + bias[i % cc];
            }
            return new VertexArray(src.vertexCount, cc, out);
        }

        private int[] parseTriangleStripArray(DataInputStream bOrig) throws IOException {
            Object3D header = new Material();
            readObject3DHeader(bOrig, header);
            int encoding = bOrig.readUnsignedByte();
            int remaining = bOrig.available();
            byte[] rest = new byte[remaining];
            bOrig.readFully(rest);
            DataInputStream b = new DataInputStream(new java.io.ByteArrayInputStream(rest));
            long firstIndex = 0;
            boolean explicit = (encoding & 0x80) != 0;
            int explicitSize = 0;
            int[] indices;
            if (!explicit) {
                if (encoding == 0) firstIndex = readU32(b);
                else if (encoding == 1) firstIndex = b.readUnsignedByte();
                else if (encoding == 2) firstIndex = readU16(b);
                indices = null; // filled in below once stripLengths (and hence total count) is known
            } else {
                /*
                 * Field order confirmed by hand-decoding a raw hex dump of real
                 * TriangleStripArray chunks (M3G_TRISTRIP_RAW): a UInt32 giving
                 * the TOTAL explicit index count comes first, followed by that
                 * many indices, THEN the strip-length table - the opposite order
                 * from what this parser originally assumed (stripCount+lengths
                 * first, indices last). E.g. one real chunk was exactly
                 * [04 00 00 00][22 23 24 25][01 00 00 00][04 00 00 00] =
                 * indexCount=4, four byte-indices, stripLengthCount=1, one
                 * strip of length 4 - matching the index count exactly.
                 */
                if (encoding == 128) explicitSize = 4;
                else if (encoding == 129) explicitSize = 1;
                else if (encoding == 130) explicitSize = 2;
                long indexCount = readU32(b);
                if (indexCount < 0 || indexCount > 1000000 || indexCount * Math.max(1, explicitSize) > b.available()) {
                    return new int[0];
                }
                indices = new int[(int) indexCount];
                for (int i = 0; i < indexCount; i++) {
                    if (explicitSize == 1) indices[i] = b.readUnsignedByte();
                    else if (explicitSize == 2) indices[i] = readU16(b);
                    else indices[i] = readS32(b);
                }
            }
            long stripCount = readU32(b);
            if (stripCount < 0 || stripCount > b.available()) {
                // Field layout mismatch for this chunk - bail out to an empty triangle
                // list (the caller's defensive design treats a null/empty submesh as
                // "renders nothing" rather than propagating a bad size further).
                return new int[0];
            }
            int[] stripLengths = new int[(int) stripCount];
            int totalVerts = 0;
            for (int i = 0; i < stripCount; i++) {
                stripLengths[i] = readS32(b);
                if (stripLengths[i] < 0 || stripLengths[i] > 100000) {
                    return new int[0];
                }
                totalVerts += stripLengths[i];
            }
            if (totalVerts < 0 || totalVerts > 1000000) {
                return new int[0];
            }
            if (!explicit) {
                indices = new int[totalVerts];
                for (int i = 0; i < totalVerts; i++) {
                    indices[i] = (int) firstIndex + i;
                }
            }
            // explicit case: indices[] was already fully read above, before stripLengths;
            // its length should equal totalVerts (the sum of stripLengths) by construction.
            // Expand triangle strips into a flat triangle list (groups of 3).
            Vector tris = new Vector();
            int pos = 0;
            for (int s = 0; s < stripLengths.length; s++) {
                int len = stripLengths[s];
                for (int i = 0; i + 2 < len; i++) {
                    int i0 = indices[pos + i], i1 = indices[pos + i + 1], i2 = indices[pos + i + 2];
                    if ((i & 1) != 0) { int t = i1; i1 = i2; i2 = t; } // preserve winding on odd triangles
                    tris.addElement(new Integer(i0));
                    tris.addElement(new Integer(i1));
                    tris.addElement(new Integer(i2));
                }
                pos += len;
            }
            int[] result = new int[tris.size()];
            for (int i = 0; i < result.length; i++) {
                result[i] = ((Integer) tris.elementAt(i)).intValue();
            }
            return result;
        }

        // ---- helpers ----

        private Object resolve(int index) {
            if (index <= 0 || index >= allObjects.size()) {
                return null;
            }
            return allObjects.elementAt(index);
        }

        // The M3G binary format is little-endian throughout (confirmed empirically -
        // interpreting the real header bytes as big-endian, as DataInputStream's own
        // readInt()/readShort()/readFloat() do, produced a >1GB "section length" for
        // a 121KB file; little-endian gives the sane small value instead). All
        // multi-byte reads below therefore assemble bytes manually in LE order
        // rather than using DataInputStream's built-in (big-endian) primitive reads.

        private long readU32() throws IOException { return readU32(in); }
        private long readU32(DataInputStream s) throws IOException {
            int b0 = s.readUnsignedByte();
            int b1 = s.readUnsignedByte();
            int b2 = s.readUnsignedByte();
            int b3 = s.readUnsignedByte();
            return ((long) b3 << 24) | ((long) b2 << 16) | ((long) b1 << 8) | b0;
        }
        private int readS32(DataInputStream s) throws IOException { return (int) readU32(s); }
        private int readU16(DataInputStream s) throws IOException {
            int b0 = s.readUnsignedByte();
            int b1 = s.readUnsignedByte();
            return (b1 << 8) | b0;
        }
        private short readS16(DataInputStream s) throws IOException { return (short) readU16(s); }
        /**
         * VertexArray's vertexCount field is the sole confirmed exception to
         * this format's otherwise-uniform little-endian encoding (empirically
         * found via live diagnostic logging against Tower Bloxx's real .m3g
         * file: every observed vertexCount was a multiple of 256 - e.g. 57344
         * = 0xE000 - with the actually-meaningful small value (8-224, matching
         * real per-submesh vertex counts) sitting in the byte read SECOND,
         * not first). Scoped to just this one field, since every other
         * multi-byte field in this parser (all U32 reads, confirmed via
         * sane userID/chunk-length/stripCount values) is genuinely LE.
         */
        private int readU16BE(DataInputStream s) throws IOException {
            int b0 = s.readUnsignedByte();
            int b1 = s.readUnsignedByte();
            return (b0 << 8) | b1;
        }
        private float readFloat(DataInputStream s) throws IOException {
            return Float.intBitsToFloat(readS32(s));
        }
        private boolean readBoolean(DataInputStream s) throws IOException { return s.readUnsignedByte() != 0; }
        private void skipBytes(long n) throws IOException {
            while (n > 0) {
                long skipped = in.skip(n);
                if (skipped <= 0) break;
                n -= skipped;
            }
        }
        private void skipBytes(DataInputStream s, long n) throws IOException {
            while (n > 0) {
                long skipped = s.skip(n);
                if (skipped <= 0) break;
                n -= skipped;
            }
        }
    }
}
