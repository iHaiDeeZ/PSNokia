package org.xml.sax;

/** A known feature or property that cannot be set (JSR 172). */
public class SAXNotSupportedException extends SAXException {
    public SAXNotSupportedException() {
    }

    public SAXNotSupportedException(String message) {
        super(message);
    }
}
