package vecdb;

import java.util.Arrays;
import java.util.Objects;

/**
 * A full record, as returned by {@link VecDb#get}.
 *
 * <p>{@code vector} is {@code null} when the record has no vector.
 */
public final class Record {

    private final long id;
    private final float[] vector;
    private final Object payload;
    private final byte[] rawPayload;
    private final String text;

    Record(long id, float[] vector, Object payload, byte[] rawPayload, String text) {
        this.id = id;
        this.vector = vector;
        this.payload = payload;
        this.rawPayload = rawPayload;
        this.text = text;
    }

    /** The record id. */
    public long id() {
        return id;
    }

    /** The stored vector, or {@code null}. */
    public float[] vector() {
        return vector == null ? null : vector.clone();
    }

    /**
     * The decoded payload, or {@code null} when the record has none.
     *
     * @see Payload#decode
     */
    public Object payload() {
        return payload;
    }

    /** The exact payload bytes, or {@code null} when the record has none. */
    public byte[] rawPayload() {
        return rawPayload;
    }

    /** The stored text, or {@code null}. */
    public String text() {
        return text;
    }

    @Override
    public String toString() {
        String v = vector == null ? "null" : "<" + vector.length + " floats>";
        return "Record(id=" + id + ", vector=" + v
                + ", text=" + Match.quote(text) + ", payload=" + Payload.json(payload) + ")";
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof Record)) {
            return false;
        }
        Record other = (Record) o;
        return id == other.id
                && Arrays.equals(vector, other.vector)
                && Objects.equals(text, other.text)
                && Arrays.equals(rawPayload, other.rawPayload);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, Arrays.hashCode(vector), text, Arrays.hashCode(rawPayload));
    }
}
