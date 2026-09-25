package org.xml.sax;

/** An error at a place in the document (JSR 172). */
public class SAXParseException extends SAXException {
    private String publicId;
    private String systemId;
    private int lineNumber = -1;
    private int columnNumber = -1;

    public SAXParseException(String message, Locator locator) {
        super(message);
        init(locator);
    }

    public SAXParseException(String message, Locator locator, Exception e) {
        super(message, e);
        init(locator);
    }

    public SAXParseException(String message, String publicId, String systemId,
                             int lineNumber, int columnNumber) {
        this(message, publicId, systemId, lineNumber, columnNumber, null);
    }

    public SAXParseException(String message, String publicId, String systemId,
                             int lineNumber, int columnNumber, Exception e) {
        super(message, e);
        this.publicId = publicId;
        this.systemId = systemId;
        this.lineNumber = lineNumber;
        this.columnNumber = columnNumber;
    }

    private void init(Locator locator) {
        if (locator != null) {
            publicId = locator.getPublicId();
            systemId = locator.getSystemId();
            lineNumber = locator.getLineNumber();
            columnNumber = locator.getColumnNumber();
        }
    }

    public String getPublicId() {
        return publicId;
    }

    public String getSystemId() {
        return systemId;
    }

    public int getLineNumber() {
        return lineNumber;
    }

    public int getColumnNumber() {
        return columnNumber;
    }
}
