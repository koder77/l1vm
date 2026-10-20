package vecdb;

/** Distance metric used by vector search. Mirrors {@code vecdb_metric}. */
public enum Metric {
    /** Negated euclidean distance; an exact match scores {@code 0.0}. */
    L2(0, "l2"),
    /** Inner product. */
    IP(1, "ip"),
    /** Cosine similarity in {@code [-1, 1]}. */
    COSINE(2, "cosine");

    private final int code;
    private final String label;

    Metric(int code, String label) {
        this.code = code;
        this.label = label;
    }

    /** The C enum value. */
    public int code() {
        return code;
    }

    /** The lowercase name used by the other bindings ({@code "l2"}, ...). */
    public String label() {
        return label;
    }

    /** Look a metric up by code, defaulting to {@link #L2}. */
    public static Metric of(int code) {
        for (Metric m : values()) {
            if (m.code == code) {
                return m;
            }
        }
        return L2;
    }

    /** Case-insensitive lookup by name, for {@code "l2"} / {@code "ip"} / {@code "cosine"}. */
    public static Metric parse(String name) {
        if (name == null) {
            return L2;
        }
        String want = name.trim().toLowerCase();
        for (Metric m : values()) {
            if (m.label.equals(want)) {
                return m;
            }
        }
        throw new IllegalArgumentException("metric must be one of l2, ip, cosine (got " + name + ")");
    }

    @Override
    public String toString() {
        return label;
    }
}
