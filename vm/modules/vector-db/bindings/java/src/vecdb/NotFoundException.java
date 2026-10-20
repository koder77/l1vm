package vecdb;

/** The requested id does not exist ({@code VECDB_ERR_NOTFOUND}). */
public class NotFoundException extends VecDbException {

    private static final long serialVersionUID = 1L;

    public NotFoundException(String message, int status) {
        super(message, status);
    }
}
