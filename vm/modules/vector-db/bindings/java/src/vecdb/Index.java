package vecdb;

/** Vector index structure. Mirrors {@code vecdb_index}. */
public enum Index {
    /** Exact brute-force scan. */
    FLAT(0, "flat"),
    /** Approximate HNSW graph. */
    HNSW(1, "hnsw");

    private final int code;
    private final String label;

    Index(int code, String label) {
        this.code = code;
        this.label = label;
    }

    /** The C enum value. */
    public int code() {
        return code;
    }

    /** The lowercase name used by the other bindings ({@code "flat"}, {@code "hnsw"}). */
    public String label() {
        return label;
    }

    /** Look an index up by code, defaulting to {@link #FLAT}. */
    public static Index of(int code) {
        for (Index i : values()) {
            if (i.code == code) {
                return i;
            }
        }
        return FLAT;
    }

    /** Case-insensitive lookup by name, for {@code "flat"} / {@code "hnsw"}. */
    public static Index parse(String name) {
        if (name == null) {
            return FLAT;
        }
        String want = name.trim().toLowerCase();
        for (Index i : values()) {
            if (i.label.equals(want)) {
                return i;
            }
        }
        throw new IllegalArgumentException("index must be one of flat, hnsw (got " + name + ")");
    }

    @Override
    public String toString() {
        return label;
    }
}
