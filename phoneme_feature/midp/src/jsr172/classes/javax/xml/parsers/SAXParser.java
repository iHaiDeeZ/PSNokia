package javax.xml.parsers;

import java.io.IOException;
import java.io.InputStream;

import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

/** A SAX parser (JSR 172). */
public abstract class SAXParser {
    protected SAXParser() {
    }

    public void parse(InputStream is, DefaultHandler dh) throws SAXException, IOException {
        if (is == null) {
            throw new IllegalArgumentException("InputStream is null");
        }
        parse(new InputSource(is), dh);
    }

    public abstract void parse(InputSource is, DefaultHandler dh)
        throws SAXException, IOException;

    public abstract boolean isNamespaceAware();

    public abstract boolean isValidating();

    public void reset() {
    }
}
