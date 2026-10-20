package vecdb;

import java.util.List;
import java.util.Objects;

/**
 * One search hit, as returned by {@link VecDb#search}, {@link
 * VecDb#searchText} and {@link VecDb#searchHybrid}.
 *
 * <p>{@code score} is higher-is-better everywhere: for {@link Metric#L2} it
 * is the negated euclidean distance (an exact match scores {@code 0.0}), for
 * {@link Metric#IP} the dot product, for {@link Metric#COSINE} the cosine
 * similarity in {@code [-1, 1]}, and for full-text search an Okapi BM25
 * score.
 */
public final class Match {

    private final long id;
    private final float score;
    private final Object payload;
    private final byte[] rawPayload;
    private final String text;

    Match(long id, float score, Object payload, byte[] rawPayload, String text) {
        this.id = id;
        this.score = score;
        this.payload = payload;
        this.rawPayload = rawPayload;
        this.text = text;
    }

    /** The record id. */
    public long id() {
        return id;
    }

    /** The similarity / BM25 / blended score; higher is better. */
    public float score() {
        return score;
    }

    /**
     * The decoded payload, or {@code null} when the record has none.
     *
     * @see Payload#decode
     */
    public Object payload() {
        return payload;
    }

    /**
     * The exact payload bytes, or {@code null} when the record has none.
     * Unlike {@link #payload()} this is never a guess.
     */
    public byte[] rawPayload() {
        return rawPayload;
    }

    /** The stored text, or {@code null}. */
    public String text() {
        return text;
    }

    @Override
    public String toString() {
        return "Match(id=" + id + ", score=" + score
                + ", text=" + quote(text) + ", payload=" + Payload.json(payload) + ")";
    }

    static String quote(String s) {
        return s == null ? "null" : '"' + s + '"';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof Match)) {
            return false;
        }
        Match other = (Match) o;
        return id == other.id
                && Float.compare(score, other.score) == 0
                && Objects.equals(text, other.text);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, score, text);
    }

    /** Convenience view of {@link #rawPayload()} as an unmodifiable list. */
    List<Byte> payloadBytes() {
        return Payload.asList(rawPayload);
    }
}
