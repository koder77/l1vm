package vecdb;

/**
 * A vector had the wrong length, or the database has no dimension yet
 * ({@code VECDB_ERR_DIM} / {@code VECDB_ERR_NODIM}).
 */
public class DimensionMismatchException extends VecDbException {

    private static final long serialVersionUID = 1L;

    public DimensionMismatchException(String message, int status) {
        super(message, status);
    }
}
