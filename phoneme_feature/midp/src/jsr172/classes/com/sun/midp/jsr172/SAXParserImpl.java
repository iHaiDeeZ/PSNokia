package com.sun.midp.jsr172;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;
import java.util.Hashtable;
import java.util.Vector;

import javax.xml.parsers.SAXParser;

import org.xml.sax.InputSource;
import org.xml.sax.Locator;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.helpers.DefaultHandler;

/**
 * A small non-validating XML parser for the JSR 172 SAX API. It reads the
 * whole document, decodes it (UTF-8, UTF-16 with or without a byte order
 * mark, ISO-8859-1 and US-ASCII) and reports elements, attributes, text,
 * CDATA and processing instructions. The DOCTYPE is skipped; only the
 * predefined entities and character references are expanded.
 */
class SAXParserImpl extends SAXParser implements Locator {
    private static final String XMLNS_URI = "http://www.w3.org/2000/xmlns/";
    private static final String XML_URI = "http://www.w3.org/XML/1998/namespace";

    private final boolean namespaceAware;
    private final boolean namespacePrefixes;
    private final boolean validating;

    // The document being parsed
    private char[] text;
    private int pos;
    private int end;
    private int line;
    private int lineStart;
    private String systemId;
    private String publicId;
    private DefaultHandler handler;

    private final AttributesImpl attributes = new AttributesImpl();
    private final StringBuffer buffer = new StringBuffer();
    // Namespace declarations: prefix -> Vector of URIs (innermost last)
    private final Hashtable namespaces = new Hashtable();

    SAXParserImpl(boolean namespaceAware, boolean namespacePrefixes, boolean validating) {
        this.namespaceAware = namespaceAware;
        this.namespacePrefixes = namespacePrefixes;
        this.validating = validating;
    }

    public boolean isNamespaceAware() {
        return namespaceAware;
    }

    public boolean isValidating() {
        return validating;
    }

    public void parse(InputSource source, DefaultHandler dh) throws SAXException, IOException {
        if (source == null) {
            throw new IllegalArgumentException("InputSource is null");
        }
        handler = dh != null ? dh : new DefaultHandler();
        systemId = source.getSystemId();
        publicId = source.getPublicId();
        if (source.getCharacterStream() != null) {
            text = readAll(source.getCharacterStream());
        } else if (source.getByteStream() != null) {
            text = decode(readAll(source.getByteStream()), source.getEncoding());
        } else {
            throw new IOException("No input in the InputSource");
        }
        end = text.length;
        pos = 0;
        line = 1;
        lineStart = 0;
        namespaces.clear();
        declared.removeAllElements();
        try {
            handler.setDocumentLocator(this);
            handler.startDocument();
            parseDocument();
            handler.endDocument();
        } finally {
            text = null;
            handler = null;
        }
    }

    // --- Locator ---------------------------------------------------------

    public String getPublicId() {
        return publicId;
    }

    public String getSystemId() {
        return systemId;
    }

    public int getLineNumber() {
        return line;
    }

    public int getColumnNumber() {
        return pos - lineStart + 1;
    }

    // --- Input -----------------------------------------------------------

    private static byte[] readAll(InputStream in) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] chunk = new byte[4096];
        int n;
        while ((n = in.read(chunk)) > 0) {
            out.write(chunk, 0, n);
        }
        return out.toByteArray();
    }

    private static char[] readAll(Reader in) throws IOException {
        char[] chars = new char[4096];
        int length = 0;
        int n;
        while ((n = in.read(chars, length, chars.length - length)) > 0) {
            length += n;
            if (length == chars.length) {
                char[] bigger = new char[chars.length * 2];
                System.arraycopy(chars, 0, bigger, 0, length);
                chars = bigger;
            }
        }
        char[] result = new char[length];
        System.arraycopy(chars, 0, result, 0, length);
        return result;
    }

    /** Decodes the document: byte order mark, else declared encoding, else UTF-8. */
    private static char[] decode(byte[] b, String encoding) {
        int start = 0;
        if (b.length >= 2 && (b[0] & 0xff) == 0xfe && (b[1] & 0xff) == 0xff) {
            return utf16(b, 2, true);
        }
        if (b.length >= 2 && (b[0] & 0xff) == 0xff && (b[1] & 0xff) == 0xfe) {
            return utf16(b, 2, false);
        }
        if (b.length >= 3 && (b[0] & 0xff) == 0xef && (b[1] & 0xff) == 0xbb
                && (b[2] & 0xff) == 0xbf) {
            return utf8(b, 3);
        }
        // "<?" in UTF-16 without a byte order mark
        if (b.length >= 4 && b[0] == 0 && b[1] == '<' && b[2] == 0 && b[3] == '?') {
            return utf16(b, 0, true);
        }
        if (b.length >= 4 && b[0] == '<' && b[1] == 0 && b[2] == '?' && b[3] == 0) {
            return utf16(b, 0, false);
        }
        if (encoding == null) {
            encoding = declaredEncoding(b);
        }
        if (encoding != null) {
            String e = encoding.toLowerCase();
            if (e.equals("iso-8859-1") || e.equals("latin1") || e.equals("us-ascii")
                    || e.equals("ascii") || e.equals("windows-1252")
                    || e.equals("cp1252")) {
                char[] chars = new char[b.length];
                for (int i = 0; i < b.length; i++) {
                    chars[i] = (char) (b[i] & 0xff);
                }
                return chars;
            }
            if (e.equals("utf-16") || e.equals("utf-16be")) {
                return utf16(b, start, true);
            }
            if (e.equals("utf-16le")) {
                return utf16(b, start, false);
            }
        }
        return utf8(b, start);
    }

    /** The encoding named in an ASCII <?xml ... encoding="..."?> declaration. */
    private static String declaredEncoding(byte[] b) {
        int limit = Math.min(b.length, 200);
        StringBuffer decl = new StringBuffer();
        for (int i = 0; i < limit && b[i] != '>'; i++) {
            decl.append((char) (b[i] & 0xff));
        }
        String s = decl.toString();
        if (!s.startsWith("<?xml")) {
            return null;
        }
        int i = s.indexOf("encoding");
        if (i < 0) {
            return null;
        }
        int q = i + 8;
        while (q < s.length() && s.charAt(q) != '"' && s.charAt(q) != '\'') {
            q++;
        }
        if (q >= s.length()) {
            return null;
        }
        int close = s.indexOf(s.charAt(q), q + 1);
        return close < 0 ? null : s.substring(q + 1, close).trim();
    }

    private static char[] utf16(byte[] b, int start, boolean bigEndian) {
        int n = (b.length - start) / 2;
        char[] chars = new char[n];
        for (int i = 0; i < n; i++) {
            int hi = b[start + 2 * i] & 0xff;
            int lo = b[start + 2 * i + 1] & 0xff;
            chars[i] = bigEndian ? (char) ((hi << 8) | lo) : (char) ((lo << 8) | hi);
        }
        return chars;
    }

    private static char[] utf8(byte[] b, int start) {
        char[] chars = new char[b.length - start];
        int n = 0;
        int i = start;
        while (i < b.length) {
            int c = b[i++] & 0xff;
            if (c < 0x80) {
                chars[n++] = (char) c;
            } else if ((c & 0xe0) == 0xc0 && i < b.length) {
                chars[n++] = (char) (((c & 0x1f) << 6) | (b[i++] & 0x3f));
            } else if ((c & 0xf0) == 0xe0 && i + 1 < b.length) {
                chars[n++] = (char) (((c & 0x0f) << 12) | ((b[i] & 0x3f) << 6)
                                     | (b[i + 1] & 0x3f));
                i += 2;
            } else if ((c & 0xf8) == 0xf0 && i + 2 < b.length) {
                // Outside the BMP: a surrogate pair
                int cp = ((c & 0x07) << 18) | ((b[i] & 0x3f) << 12)
                         | ((b[i + 1] & 0x3f) << 6) | (b[i + 2] & 0x3f);
                i += 3;
                cp -= 0x10000;
                if (n + 2 > chars.length) {
                    char[] bigger = new char[chars.length + 16];
                    System.arraycopy(chars, 0, bigger, 0, n);
                    chars = bigger;
                }
                chars[n++] = (char) (0xd800 + (cp >> 10));
                chars[n++] = (char) (0xdc00 + (cp & 0x3ff));
            } else {
                chars[n++] = (char) 0xfffd;
            }
        }
        char[] result = new char[n];
        System.arraycopy(chars, 0, result, 0, n);
        return result;
    }

    // --- Scanning --------------------------------------------------------

    private SAXParseException error(String message) {
        return new SAXParseException(message, this);
    }

    private void fatal(String message) throws SAXException {
        SAXParseException e = error(message);
        handler.fatalError(e);
        throw e;
    }

    private static boolean isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    private static boolean isNameChar(char c) {
        return !isSpace(c) && c != '>' && c != '/' && c != '=' && c != '<'
               && c != '"' && c != '\'' && c != '?' && c != '!';
    }

    private boolean startsWith(String s) {
        int n = s.length();
        if (pos + n > end) {
            return false;
        }
        for (int i = 0; i < n; i++) {
            if (text[pos + i] != s.charAt(i)) {
                return false;
            }
        }
        return true;
    }

    /** Moves pos forward by n characters, counting lines. */
    private void advance(int n) {
        for (int i = 0; i < n && pos < end; i++) {
            if (text[pos++] == '\n') {
                line++;
                lineStart = pos;
            }
        }
    }

    private void skipSpace() {
        while (pos < end && isSpace(text[pos])) {
            advance(1);
        }
    }

    /** Moves past the next occurrence of s; returns false at the end. */
    private boolean skipPast(String s) {
        while (pos < end) {
            if (startsWith(s)) {
                advance(s.length());
                return true;
            }
            advance(1);
        }
        return false;
    }

    private String readName() throws SAXException {
        int start = pos;
        while (pos < end && isNameChar(text[pos])) {
            pos++;
        }
        if (pos == start) {
            fatal("Name expected");
        }
        return new String(text, start, pos - start);
    }

    // --- Document --------------------------------------------------------

    private void parseDocument() throws SAXException {
        Vector open = new Vector();   // qNames of open elements
        boolean seenRoot = false;
        while (pos < end) {
            if (text[pos] != '<') {
                readText(open.size() > 0);
            } else if (startsWith("<?")) {
                readProcessingInstruction();
            } else if (startsWith("<!--")) {
                if (!skipPast("-->")) {
                    fatal("Unterminated comment");
                }
            } else if (startsWith("<![CDATA[")) {
                advance(9);
                int start = pos;
                if (!skipPast("]]>")) {
                    fatal("Unterminated CDATA section");
                }
                handler.characters(text, start, pos - 3 - start);
            } else if (startsWith("<!")) {
                skipDeclaration();
            } else if (startsWith("</")) {
                advance(2);
                String qName = readName();
                skipSpace();
                if (pos >= end || text[pos] != '>') {
                    fatal("'>' expected after </" + qName);
                }
                advance(1);
                if (open.size() == 0 || !open.lastElement().equals(qName)) {
                    fatal("Unexpected </" + qName + ">");
                }
                open.removeElementAt(open.size() - 1);
                endElement(qName);
            } else {
                if (open.size() == 0 && seenRoot) {
                    fatal("Content after the root element");
                }
                seenRoot = true;
                advance(1);
                String qName = readName();
                boolean empty = readAttributes();
                startElement(qName);
                if (empty) {
                    endElement(qName);
                } else {
                    open.addElement(qName);
                }
            }
        }
        if (open.size() > 0) {
            fatal("Unclosed element <" + open.lastElement() + ">");
        }
        if (!seenRoot) {
            fatal("No root element");
        }
    }

    /** Skips <!DOCTYPE ...> and other declarations, with any [...] subset. */
    private void skipDeclaration() throws SAXException {
        int depth = 0;
        char quote = 0;
        while (pos < end) {
            char c = text[pos];
            advance(1);
            if (quote != 0) {
                if (c == quote) {
                    quote = 0;
                }
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (c == '[') {
                depth++;
            } else if (c == ']') {
                depth--;
            } else if (c == '>' && depth <= 0) {
                return;
            }
        }
        fatal("Unterminated declaration");
    }

    private void readProcessingInstruction() throws SAXException {
        advance(2);
        String target = readName();
        skipSpace();
        int start = pos;
        if (!skipPast("?>")) {
            fatal("Unterminated processing instruction");
        }
        if (!target.toLowerCase().equals("xml")) {
            handler.processingInstruction(target, new String(text, start, pos - 2 - start));
        }
    }

    /** Text up to the next '<', with references expanded. */
    private void readText(boolean inElement) throws SAXException {
        int start = pos;
        boolean plain = true;
        while (pos < end && text[pos] != '<') {
            if (text[pos] == '&') {
                plain = false;
            }
            advance(1);
        }
        if (!inElement) {
            return;     // whitespace around the root element
        }
        if (plain) {
            handler.characters(text, start, pos - start);
        } else {
            buffer.setLength(0);
            expand(start, pos, false);
            char[] chars = new char[buffer.length()];
            buffer.getChars(0, chars.length, chars, 0);
            handler.characters(chars, 0, chars.length);
        }
    }

    /** Appends text[from, to) to buffer with references expanded. */
    private void expand(int from, int to, boolean attribute) throws SAXException {
        int i = from;
        while (i < to) {
            char c = text[i];
            if (c == '&') {
                int semi = i + 1;
                while (semi < to && text[semi] != ';' && semi - i < 12) {
                    semi++;
                }
                if (semi < to && text[semi] == ';') {
                    String name = new String(text, i + 1, semi - i - 1);
                    int ch = reference(name);
                    if (ch >= 0) {
                        if (ch > 0xffff) {
                            ch -= 0x10000;
                            buffer.append((char) (0xd800 + (ch >> 10)));
                            buffer.append((char) (0xdc00 + (ch & 0x3ff)));
                        } else {
                            buffer.append((char) ch);
                        }
                    } else {
                        // An entity from a DTD we do not read: keep it as text
                        handler.skippedEntity(name);
                        buffer.append('&').append(name).append(';');
                    }
                    i = semi + 1;
                    continue;
                }
                buffer.append(c);
            } else if (attribute && (c == '\t' || c == '\n' || c == '\r')) {
                buffer.append(' ');
            } else {
                buffer.append(c);
            }
            i++;
        }
    }

    /** The character a reference stands for, or -1 if it is unknown. */
    private static int reference(String name) {
        if (name.equals("lt")) {
            return '<';
        } else if (name.equals("gt")) {
            return '>';
        } else if (name.equals("amp")) {
            return '&';
        } else if (name.equals("quot")) {
            return '"';
        } else if (name.equals("apos")) {
            return '\'';
        } else if (name.startsWith("#x") || name.startsWith("#X")) {
            try {
                return Integer.parseInt(name.substring(2), 16);
            } catch (NumberFormatException e) {
                return -1;
            }
        } else if (name.startsWith("#")) {
            try {
                return Integer.parseInt(name.substring(1));
            } catch (NumberFormatException e) {
                return -1;
            }
        }
        return -1;
    }

    /** Reads the attributes of a start tag; returns true for <empty/>. */
    private boolean readAttributes() throws SAXException {
        attributes.clear();
        while (true) {
            skipSpace();
            if (pos >= end) {
                fatal("Unterminated start tag");
            }
            char c = text[pos];
            if (c == '>') {
                advance(1);
                return false;
            }
            if (c == '/') {
                advance(1);
                if (pos >= end || text[pos] != '>') {
                    fatal("'>' expected after '/'");
                }
                advance(1);
                return true;
            }
            String qName = readName();
            skipSpace();
            if (pos >= end || text[pos] != '=') {
                fatal("'=' expected after attribute " + qName);
            }
            advance(1);
            skipSpace();
            if (pos >= end || (text[pos] != '"' && text[pos] != '\'')) {
                fatal("Quoted value expected for attribute " + qName);
            }
            char quote = text[pos];
            advance(1);
            int start = pos;
            while (pos < end && text[pos] != quote) {
                advance(1);
            }
            if (pos >= end) {
                fatal("Unterminated value of attribute " + qName);
            }
            buffer.setLength(0);
            expand(start, pos, true);
            advance(1);
            attributes.add("", localPart(qName), qName, buffer.toString());
        }
    }

    private static String localPart(String qName) {
        int colon = qName.indexOf(':');
        return colon < 0 ? qName : qName.substring(colon + 1);
    }

    private static String prefix(String qName) {
        int colon = qName.indexOf(':');
        return colon < 0 ? "" : qName.substring(0, colon);
    }

    // --- Elements and namespaces -----------------------------------------

    private String namespaceUri(String prefix) {
        if (prefix.equals("xml")) {
            return XML_URI;
        }
        Vector uris = (Vector) namespaces.get(prefix);
        return uris == null || uris.size() == 0 ? "" : (String) uris.lastElement();
    }

    // Prefixes declared by each open element, innermost last
    private final Vector declared = new Vector();

    private void startElement(String qName) throws SAXException {
        if (!namespaceAware) {
            handler.startElement("", localPart(qName), qName, attributes);
            return;
        }
        // Declarations first, so they apply to this element's own names
        Vector prefixes = new Vector();
        for (int i = 0; i < attributes.getLength(); i++) {
            String name = attributes.getQName(i);
            if (name.equals("xmlns") || name.startsWith("xmlns:")) {
                String p = name.length() > 5 ? name.substring(6) : "";
                Vector uris = (Vector) namespaces.get(p);
                if (uris == null) {
                    uris = new Vector();
                    namespaces.put(p, uris);
                }
                uris.addElement(attributes.getValue(i));
                prefixes.addElement(p);
                handler.startPrefixMapping(p, attributes.getValue(i));
            }
        }
        declared.addElement(prefixes);
        AttributesImpl reported = attributes;
        if (!prefixes.isEmpty() && !namespacePrefixes) {
            // Report the attributes without the xmlns declarations
            reported = new AttributesImpl();
            for (int i = 0; i < attributes.getLength(); i++) {
                String name = attributes.getQName(i);
                if (!name.equals("xmlns") && !name.startsWith("xmlns:")) {
                    reported.add("", attributes.getLocalName(i), name, attributes.getValue(i));
                }
            }
        }
        for (int i = 0; i < reported.getLength(); i++) {
            String name = reported.getQName(i);
            String p = prefix(name);
            if (name.equals("xmlns") || p.equals("xmlns")) {
                reported.setURI(i, XMLNS_URI);
            } else if (p.length() > 0) {
                // Unprefixed attributes are in no namespace
                reported.setURI(i, namespaceUri(p));
            }
        }
        handler.startElement(namespaceUri(prefix(qName)), localPart(qName), qName, reported);
    }

    private void endElement(String qName) throws SAXException {
        if (!namespaceAware) {
            handler.endElement("", localPart(qName), qName);
            return;
        }
        handler.endElement(namespaceUri(prefix(qName)), localPart(qName), qName);
        Vector prefixes = (Vector) declared.lastElement();
        declared.removeElementAt(declared.size() - 1);
        for (int i = prefixes.size() - 1; i >= 0; i--) {
            String p = (String) prefixes.elementAt(i);
            Vector uris = (Vector) namespaces.get(p);
            uris.removeElementAt(uris.size() - 1);
            handler.endPrefixMapping(p);
        }
    }
}
