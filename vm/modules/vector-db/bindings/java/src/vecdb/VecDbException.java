package vecdb;

/**
 * Base class for every error thrown by the vecdb Java bindings.
 *
 * <p>{@link #status()} carries the raw {@code VECDB_*} status code where one
 * applies, or {@code 0} for client-side errors such as using a closed
 * database.
 */
public class VecDbException extends RuntimeException {

    private static final long serialVersionUID = 1L;

    private final int status;

    public VecDbException(String message) {
        this(message, 0);
    }

    public VecDbException(String message, int status) {
        super(message);
        this.status = status;
    }

    /** The raw {@code VECDB_*} status code, or {@code 0}. */
    public int status() {
        return status;
    }
}
