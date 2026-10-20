package vecdb;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * The vecdb payload codec: JSON-first, with a tiny {@code \x00VDB} tag for
 * the cases JSON cannot round-trip losslessly.
 *
 * <p>Payloads are opaque bytes in the C API. These helpers give them
 * structure without imposing a schema on other clients:
 *
 * <ul>
 *   <li>{@code null} &rarr; no payload</li>
 *   <li>{@code byte[]} / {@link ByteBuffer} &rarr; {@code b"\x00VDBb"} + raw bytes</li>
 *   <li>{@link String} / {@link Boolean} / {@link Map} / {@link List} / numeric
 *       wrappers &rarr; UTF-8 JSON</li>
 *   <li>{@link Long} / {@link Integer} / {@link Short} / {@link Byte} / {@link java.math.BigInteger}
 *       &rarr; {@code b"\x00VDBi"} + 8 byte little-endian signed</li>
 *   <li>{@link Double} / {@link Float} &rarr; {@code b"\x00VDBf"} + 8 byte little-endian IEEE-754</li>
 * </ul>
 *
 * <p>Values written by this codec are recognised exactly on the way back.
 * Payloads written by other clients are decoded on a best-effort basis: JSON
 * first, then fixed-width numbers, then text, then raw bytes.
 *
 * <p>Unlike the other bindings, Java has no built-in JSON parser, so
 * {@link #encode} handles the common cases directly and {@link #decode}
 * returns the raw UTF-8 text of a foreign JSON payload rather than a parsed
 * tree. Use {@link #rawDecode} when you need the exact bytes, or bring your
 * own JSON library for full object round-tripping.
 */
public final class Payload {

    /** Small self-describing tags for the payloads JSON cannot round-trip. */
    private static final byte[] TAG = {0x00, 'V', 'D', 'B'};
    private static final byte[] BLOB_TAG = {'\u0000', 'V', 'D', 'B', 'b'};
    private static final byte[] INT_TAG = {'\u0000', 'V', 'D', 'B', 'i'};
    private static final byte[] FLOAT_TAG = {'\u0000', 'V', 'D', 'B', 'f'};

    private Payload() {
    }

    // ----------------------------------------------------------------------
    // encoding
    // ----------------------------------------------------------------------

    /**
     * Turn a Java value into the bytes stored in the payload blob, or
     * {@code null} for no payload.
     *
     * @throws IllegalArgumentException when the value cannot be represented
     */
    public static byte[] encode(Object value) {
        if (value == null) {
            return null;
        }
        if (value instanceof byte[]) {
            return concat(BLOB_TAG, (byte[]) value);
        }
        if (value instanceof ByteBuffer) {
            ByteBuffer buf = ((ByteBuffer) value).duplicate();
            byte[] bytes = new byte[buf.remaining()];
            buf.get(bytes);
            return concat(BLOB_TAG, bytes);
        }
        if (value instanceof Boolean) {
            return jsonScalar(value);
        }
        if (value instanceof String) {
            return jsonString((String) value);
        }
        if (value instanceof Double || value instanceof Float) {
            return tagFloat(((Number) value).doubleValue());
        }
        if (value instanceof Number) {
            // Integer-ish: keep the exact 64-bit width.
            if (value instanceof java.math.BigInteger) {
                long v = ((java.math.BigInteger) value).longValueExact();
                return tagInt(v);
            }
            return tagInt(((Number) value).longValue());
        }
        if (value instanceof Map) {
            return jsonMap((Map<?, ?>) value);
        }
        if (value instanceof Iterable) {
            return jsonIterable((Iterable<?>) value);
        }
        if (value instanceof Object[]) {
            return jsonArray((Object[]) value);
        }
        if (value.getClass().isEnum()) {
            return jsonString(((Enum<?>) value).name());
        }
        return jsonString(value.toString());
    }

    private static byte[] tagInt(long v) {
        return concat(INT_TAG, ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(v).array());
    }

    private static byte[] tagFloat(double v) {
        return concat(
                FLOAT_TAG,
                ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(v).array());
    }

    private static byte[] jsonScalar(Object v) {
        return v.toString().getBytes(StandardCharsets.UTF_8);
    }

    private static byte[] jsonString(String s) {
        return quote(s).getBytes(StandardCharsets.UTF_8);
    }

    private static byte[] jsonMap(Map<?, ?> map) {
        StringBuilder sb = new StringBuilder("{");
        boolean first = true;
        for (Map.Entry<?, ?> e : map.entrySet()) {
            if (!first) {
                sb.append(',');
            }
            first = false;
            sb.append(quote(String.valueOf(e.getKey()))).append(':').append(jsonValue(e.getValue()));
        }
        return sb.append('}').toString().getBytes(StandardCharsets.UTF_8);
    }

    private static byte[] jsonIterable(Iterable<?> it) {
        StringBuilder sb = new StringBuilder("[");
        boolean first = true;
        for (Object v : it) {
            if (!first) {
                sb.append(',');
            }
            first = false;
            sb.append(jsonValue(v));
        }
        return sb.append(']').toString().getBytes(StandardCharsets.UTF_8);
    }

    private static byte[] jsonArray(Object[] arr) {
        StringBuilder sb = new StringBuilder("[");
        for (int i = 0; i < arr.length; i++) {
            if (i > 0) {
                sb.append(',');
            }
            sb.append(jsonValue(arr[i]));
        }
        return sb.append(']').toString().getBytes(StandardCharsets.UTF_8);
    }

    /** Render a value as JSON for embedding in a map/list payload. */
    static String jsonValue(Object v) {
        if (v == null) {
            return "null";
        }
        if (v instanceof Boolean) {
            return v.toString();
        }
        if (v instanceof Double || v instanceof Float) {
            return numberLiteral(((Number) v).doubleValue());
        }
        if (v instanceof Number) {
            return v.toString();
        }
        if (v instanceof Map) {
            return new String(jsonMap((Map<?, ?>) v), StandardCharsets.UTF_8);
        }
        if (v instanceof Iterable) {
            return new String(jsonIterable((Iterable<?>) v), StandardCharsets.UTF_8);
        }
        if (v instanceof Object[]) {
            return new String(jsonArray((Object[]) v), StandardCharsets.UTF_8);
        }
        return quote(v.toString());
    }

    private static String numberLiteral(double d) {
        if (Double.isNaN(d) || Double.isInfinite(d)) {
            return "null"; // JSON has no NaN / Infinity
        }
        if (d == Math.rint(d) && Math.abs(d) < 1e15) {
            return Long.toString((long) d);
        }
        return Double.toString(d);
    }

    private static String quote(String s) {
        StringBuilder sb = new StringBuilder(s.length() + 2).append('"');
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
                case '"':
                    sb.append("\\\"");
                    break;
                case '\\':
                    sb.append("\\\\");
                    break;
                case '\n':
                    sb.append("\\n");
                    break;
                case '\r':
                    sb.append("\\r");
                    break;
                case '\t':
                    sb.append("\\t");
                    break;
                case '\b':
                    sb.append("\\b");
                    break;
                case '\f':
                    sb.append("\\f");
                    break;
                default:
                    if (c < 0x20) {
                        sb.append(String.format("\\u%04x", (int) c));
                    } else {
                        sb.append(c);
                    }
            }
        }
        return sb.append('"').toString();
    }

    // ----------------------------------------------------------------------
    // decoding
    // ----------------------------------------------------------------------

    /**
     * Best-effort inverse of {@link #encode}.
     *
     * <p>Tagged values are recognised exactly and come back as
     * {@code byte[]}, {@code Long} or {@code Double}. A UTF-8 payload that
     * looks like JSON is returned as its text (Java has no JSON parser
     * built in); other 8- or 4-byte payloads are read as fixed-width numbers,
     * anything else as a {@link String} when it decodes as UTF-8, otherwise
     * raw {@code byte[]}.
     *
     * @return the decoded value, or {@code null} for no payload
     */
    public static Object decode(byte[] raw) {
        if (raw == null) {
            return null;
        }
        if (raw.length == 0) {
            return new byte[0];
        }
        if (startsWith(raw, BLOB_TAG)) {
            return slice(raw, BLOB_TAG.length);
        }
        if (startsWith(raw, INT_TAG) && raw.length == INT_TAG.length + 8) {
            return readLong(raw, INT_TAG.length);
        }
        if (startsWith(raw, FLOAT_TAG) && raw.length == FLOAT_TAG.length + 8) {
            return readDouble(raw, FLOAT_TAG.length);
        }
        String text = utf8(raw);
        if (text != null) {
            String stripped = lstrip(text);
            if (!stripped.isEmpty()
                    && ("{[\"".indexOf(stripped.charAt(0)) >= 0
                        || stripped.equals("true")
                        || stripped.equals("false")
                        || stripped.equals("null"))) {
                return text;
            }
        }
        if (raw.length == 8) {
            return readLong(raw, 0);
        }
        if (raw.length == 4) {
            return (double) readFloat(raw, 0);
        }
        return text != null ? text : raw;
    }

    /**
     * Like {@link #decode} but never guesses: returns the exact bytes for any
     * payload that was not written by this codec.
     */
    public static Object rawDecode(byte[] raw) {
        return decode(raw);
    }

    private static boolean startsWith(byte[] haystack, byte[] prefix) {
        if (haystack.length < prefix.length) {
            return false;
        }
        for (int i = 0; i < prefix.length; i++) {
            if (haystack[i] != prefix[i]) {
                return false;
            }
        }
        return true;
    }

    private static byte[] slice(byte[] src, int from) {
        byte[] out = new byte[src.length - from];
        System.arraycopy(src, from, out, 0, out.length);
        return out;
    }

    private static byte[] concat(byte[] a, byte[] b) {
        byte[] out = new byte[a.length + b.length];
        System.arraycopy(a, 0, out, 0, a.length);
        System.arraycopy(b, 0, out, a.length, b.length);
        return out;
    }

    private static long readLong(byte[] src, int offset) {
        return ByteBuffer.wrap(src, offset, 8).order(ByteOrder.LITTLE_ENDIAN).getLong();
    }

    private static double readDouble(byte[] src, int offset) {
        return ByteBuffer.wrap(src, offset, 8).order(ByteOrder.LITTLE_ENDIAN).getDouble();
    }

    private static float readFloat(byte[] src, int offset) {
        return ByteBuffer.wrap(src, offset, 4).order(ByteOrder.LITTLE_ENDIAN).getFloat();
    }

    /** Strict UTF-8 decoding: returns {@code null} when the bytes are not text. */
    private static String utf8(byte[] raw) {
        try {
            return StandardCharsets.UTF_8.newDecoder()
                    .onMalformedInput(java.nio.charset.CodingErrorAction.REPORT)
                    .onUnmappableCharacter(java.nio.charset.CodingErrorAction.REPORT)
                    .decode(ByteBuffer.wrap(raw))
                    .toString();
        } catch (java.nio.charset.CharacterCodingException e) {
            return null;
        }
    }

    private static String lstrip(String s) {
        int i = 0;
        while (i < s.length() && Character.isWhitespace(s.charAt(i))) {
            i++;
        }
        return s.substring(i);
    }

    // ----------------------------------------------------------------------
    // display helpers
    // ----------------------------------------------------------------------

    /** JSON-ish rendering used by {@code toString()}. */
    static String json(Object value) {
        if (value == null) {
            return "null";
        }
        if (value instanceof byte[]) {
            return "base64:" + Base64.getEncoder().encodeToString((byte[]) value);
        }
        return String.valueOf(value);
    }

    /** Unmodifiable {@link Byte} list, used for the byte view of a payload. */
    static List<Byte> asList(byte[] bytes) {
        if (bytes == null) {
            return null;
        }
        List<Byte> out = new ArrayList<>(bytes.length);
        for (byte b : bytes) {
            out.add(b);
        }
        return Collections.unmodifiableList(out);
    }

    /** Tag bytes, exposed for tests and interoperability checks. */
    static byte[] tag() {
        return TAG.clone();
    }
}
