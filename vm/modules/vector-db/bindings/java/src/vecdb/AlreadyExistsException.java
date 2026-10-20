package vecdb;

/**
 * An insert-only write hit an existing id ({@code VECDB_ERR_EXISTS}).
 */
public class AlreadyExistsException extends VecDbException {

    private static final long serialVersionUID = 1L;

    public AlreadyExistsException(String message, int status) {
        super(message, status);
    }
}
