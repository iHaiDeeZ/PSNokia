package org.xml.sax;

/** A SAX error or warning (JSR 172). */
public class SAXException extends Exception {
    private Exception exception;

    public SAXException() {
    }

    public SAXException(String message) {
        super(message);
    }

    public SAXException(Exception e) {
        exception = e;
    }

    public SAXException(String message, Exception e) {
        super(message);
        exception = e;
    }

    public String getMessage() {
        String message = super.getMessage();
        return message == null && exception != null ? exception.getMessage() : message;
    }

    public Exception getException() {
        return exception;
    }

    public String toString() {
        return exception != null ? exception.toString() : super.toString();
    }
}
