package javax.xml.parsers;

/** The parser factory cannot be created (JSR 172). */
public class FactoryConfigurationError extends Error {
    private Exception exception;

    public FactoryConfigurationError() {
    }

    public FactoryConfigurationError(String msg) {
        super(msg);
    }

    public FactoryConfigurationError(Exception e) {
        super(e.toString());
        exception = e;
    }

    public FactoryConfigurationError(Exception e, String msg) {
        super(msg);
        exception = e;
    }

    public String getMessage() {
        String message = super.getMessage();
        return message == null && exception != null ? exception.getMessage() : message;
    }

    public Exception getException() {
        return exception;
    }
}
