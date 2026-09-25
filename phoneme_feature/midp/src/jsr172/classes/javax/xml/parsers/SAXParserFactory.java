package javax.xml.parsers;

import org.xml.sax.SAXException;
import org.xml.sax.SAXNotRecognizedException;
import org.xml.sax.SAXNotSupportedException;

/** Creates SAX parsers (JSR 172). */
public abstract class SAXParserFactory {
    private static final String NAMESPACES = "http://xml.org/sax/features/namespaces";
    private static final String NAMESPACE_PREFIXES =
        "http://xml.org/sax/features/namespace-prefixes";

    private boolean namespaceAware;
    private boolean namespacePrefixes;
    private boolean validating;

    protected SAXParserFactory() {
    }

    public static SAXParserFactory newInstance() {
        return new com.sun.midp.jsr172.SAXParserFactoryImpl();
    }

    public abstract SAXParser newSAXParser()
        throws ParserConfigurationException, SAXException;

    public void setNamespaceAware(boolean awareness) {
        namespaceAware = awareness;
    }

    public boolean isNamespaceAware() {
        return namespaceAware;
    }

    /** The parser does not validate; the setting is only recorded. */
    public void setValidating(boolean validating) {
        this.validating = validating;
    }

    public boolean isValidating() {
        return validating;
    }

    public void setFeature(String name, boolean value)
            throws ParserConfigurationException, SAXNotRecognizedException,
                   SAXNotSupportedException {
        if (NAMESPACES.equals(name)) {
            namespaceAware = value;
        } else if (NAMESPACE_PREFIXES.equals(name)) {
            namespacePrefixes = value;
        } else {
            throw new SAXNotRecognizedException(name);
        }
    }

    public boolean getFeature(String name)
            throws ParserConfigurationException, SAXNotRecognizedException,
                   SAXNotSupportedException {
        if (NAMESPACES.equals(name)) {
            return namespaceAware;
        } else if (NAMESPACE_PREFIXES.equals(name)) {
            return namespacePrefixes;
        }
        throw new SAXNotRecognizedException(name);
    }
}
