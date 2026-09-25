package com.sun.midp.jsr172;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

import org.xml.sax.SAXException;

/** The SAXParserFactory that SAXParserFactory.newInstance() returns. */
public class SAXParserFactoryImpl extends SAXParserFactory {
    public SAXParser newSAXParser() throws ParserConfigurationException, SAXException {
        boolean prefixes = getFeature("http://xml.org/sax/features/namespace-prefixes");
        return new SAXParserImpl(isNamespaceAware(), prefixes, isValidating());
    }
}
