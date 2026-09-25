package org.xml.sax;

import java.io.InputStream;
import java.io.Reader;

/** Where a document comes from (JSR 172). */
public class InputSource {
    private String publicId;
    private String systemId;
    private InputStream byteStream;
    private Reader characterStream;
    private String encoding;

    public InputSource() {
    }

    public InputSource(String systemId) {
        this.systemId = systemId;
    }

    public InputSource(InputStream byteStream) {
        this.byteStream = byteStream;
    }

    public InputSource(Reader characterStream) {
        this.characterStream = characterStream;
    }

    public void setPublicId(String publicId) {
        this.publicId = publicId;
    }

    public String getPublicId() {
        return publicId;
    }

    public void setSystemId(String systemId) {
        this.systemId = systemId;
    }

    public String getSystemId() {
        return systemId;
    }

    public void setByteStream(InputStream byteStream) {
        this.byteStream = byteStream;
    }

    public InputStream getByteStream() {
        return byteStream;
    }

    public void setEncoding(String encoding) {
        this.encoding = encoding;
    }

    public String getEncoding() {
        return encoding;
    }

    public void setCharacterStream(Reader characterStream) {
        this.characterStream = characterStream;
    }

    public Reader getCharacterStream() {
        return characterStream;
    }
}
