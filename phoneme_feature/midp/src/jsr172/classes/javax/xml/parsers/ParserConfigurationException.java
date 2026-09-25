package javax.xml.parsers;

/** A parser with the requested configuration cannot be created (JSR 172). */
public class ParserConfigurationException extends Exception {
    public ParserConfigurationException() {
    }

    public ParserConfigurationException(String msg) {
        super(msg);
    }
}
