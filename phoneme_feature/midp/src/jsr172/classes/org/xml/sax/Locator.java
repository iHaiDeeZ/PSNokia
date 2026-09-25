package org.xml.sax;

/** Where in the document an event comes from (JSR 172). */
public interface Locator {
    String getPublicId();
    String getSystemId();
    int getLineNumber();
    int getColumnNumber();
}
