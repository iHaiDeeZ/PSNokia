package org.xml.sax;

/** An unknown feature or property name (JSR 172). */
public class SAXNotRecognizedException extends SAXException {
    public SAXNotRecognizedException() {
    }

    public SAXNotRecognizedException(String message) {
        super(message);
    }
}
