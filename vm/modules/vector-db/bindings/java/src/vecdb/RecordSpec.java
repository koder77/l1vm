package vecdb;

import java.util.Arrays;
import java.util.Map;

/**
 * A record to store: an optional vector, text and payload.
 *
 * <pre>{@code
 * db.add(42, RecordSpec.of(embedding)
 *         .text("hello")
 *         .payload(Map.of("kind", "greeting")));
 * }</pre>
 */
public final class RecordSpec {

    float[] vector;
    String text;
    Object payload;
    boolean sync;

    private RecordSpec() {
    }

    /** An empty record (payload only, or a pure tombstone-style upsert). */
    public static RecordSpec empty() {
        return new RecordSpec();
    }

    /** A record with the given vector. */
    public static RecordSpec of(float[] vector) {
        RecordSpec spec = new RecordSpec();
        spec.vector = vector;
        return spec;
    }

    /** A text-only record. */
    public static RecordSpec text(String text) {
        RecordSpec spec = new RecordSpec();
        spec.text = text;
        return spec;
    }

    /** A payload-only record. */
    public static RecordSpec withPayload(Object payload) {
        RecordSpec spec = new RecordSpec();
        spec.payload = payload;
        return spec;
    }

    /** Set (or clear) the vector. */
    public RecordSpec vector(float[] vector) {
        this.vector = vector;
        return this;
    }

    /** Set (or clear) the text. */
    public RecordSpec textValue(String text) {
        this.text = text;
        return this;
    }

    /** Set (or clear) the payload; encoded with {@link Payload#encode}. */
    public RecordSpec payload(Object payload) {
        this.payload = payload;
        return this;
    }

    /** Flush to disk before this write returns. */
    public RecordSpec sync(boolean sync) {
        this.sync = sync;
        return this;
    }

    RecordSpec withSync(boolean sync) {
        this.sync = sync;
        return this;
    }

    @Override
    public String toString() {
        return "RecordSpec(vector=" + (vector == null ? "null" : Arrays.toString(vector))
                + ", text=" + Match.quote(text) + ", payload=" + Payload.json(payload) + ")";
    }
}
