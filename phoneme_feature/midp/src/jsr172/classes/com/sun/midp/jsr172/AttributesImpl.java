package com.sun.midp.jsr172;

import org.xml.sax.Attributes;

/** The attributes of one element, as parsed. */
class AttributesImpl implements Attributes {
    private String[] data = new String[4 * 8];  // uri, local name, qName, value
    private int length;

    void clear() {
        length = 0;
    }

    void add(String uri, String localName, String qName, String value) {
        if (4 * (length + 1) > data.length) {
            String[] bigger = new String[data.length * 2];
            System.arraycopy(data, 0, bigger, 0, data.length);
            data = bigger;
        }
        data[4 * length] = uri;
        data[4 * length + 1] = localName;
        data[4 * length + 2] = qName;
        data[4 * length + 3] = value;
        length++;
    }

    void setURI(int index, String uri) {
        data[4 * index] = uri;
    }

    public int getLength() {
        return length;
    }

    private String get(int index, int field) {
        return index >= 0 && index < length ? data[4 * index + field] : null;
    }

    public String getURI(int index) {
        return get(index, 0);
    }

    public String getLocalName(int index) {
        return get(index, 1);
    }

    public String getQName(int index) {
        return get(index, 2);
    }

    public String getType(int index) {
        return index >= 0 && index < length ? "CDATA" : null;
    }

    public String getValue(int index) {
        return get(index, 3);
    }

    public int getIndex(String uri, String localName) {
        for (int i = 0; i < length; i++) {
            if (data[4 * i + 1].equals(localName) && data[4 * i].equals(uri)) {
                return i;
            }
        }
        return -1;
    }

    public int getIndex(String qName) {
        for (int i = 0; i < length; i++) {
            if (data[4 * i + 2].equals(qName)) {
                return i;
            }
        }
        return -1;
    }

    public String getType(String uri, String localName) {
        return getType(getIndex(uri, localName));
    }

    public String getType(String qName) {
        return getType(getIndex(qName));
    }

    public String getValue(String uri, String localName) {
        return getValue(getIndex(uri, localName));
    }

    public String getValue(String qName) {
        return getValue(getIndex(qName));
    }
}
