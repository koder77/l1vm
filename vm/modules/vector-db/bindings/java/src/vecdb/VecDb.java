package vecdb;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.util.ArrayList;
import java.util.List;

/**
 * An embedded vector + full-text database.
 *
 * <p>A thin wrapper around the {@code libvecdb} C library. Records carry an
 * opaque payload, an optional float vector and optional free text, and can be
 * retrieved by id, by nearest-neighbour vector search (exact FLAT or
 * approximate HNSW), by BM25 full-text search, or by a hybrid of both.
 * Scores are higher-is-better everywhere.
 *
 * <pre>{@code
 * try (VecDb db = VecDb.open("/tmp/notes.db",
 *         Options.builder().dim(3).metric(Metric.COSINE).build())) {
 *     db.add(1, RecordSpec.of(new float[]{1, 0, 0})
 *             .text("a cat sat on the mat")
 *             .payload(Map.of("kind", "animal")));
 *
 *     for (Match hit : db.search(new float[]{0.9f, 0.1f, 0f}, 5)) {
 *         System.out.println(hit.score() + "  #" + hit.id() + "  " + hit.text());
 *     }
 * }
 * }</pre>
 *
 * <p>{@code VecDb} is {@link AutoCloseable}; a phaser-based {@link
 * java.lang.ref.Cleaner cleaner} closes handles the caller forgot about. The
 * underlying library serialises writers with a rwlock, so a single handle is
 * safe to share between threads for reads; open one handle per thread if you
 * write concurrently.
 */
public final class VecDb implements AutoCloseable {

    /** A client-side cap so we never ask the C layer for a silly allocation. */
    public static final int MAX_PAYLOAD = 1 << 30;

    /** Version of the bindings. */
    public static final String VERSION = "1.0.0";

    private static final java.lang.ref.Cleaner CLEANER = java.lang.ref.Cleaner.create();

    private final Native api;
    private final CloseAction cleanup;
    private final java.lang.ref.Cleaner.Cleanable cleanable;

    /** Shared, non-capturing teardown run by {@link #close()} or the cleaner. */
    private static final class CloseAction implements Runnable {
        private final Native api;
        private volatile MemorySegment handle;

        CloseAction(Native api, MemorySegment handle) {
            this.api = api;
            this.handle = handle;
        }

        @Override
        public void run() {
            MemorySegment h = handle;
            handle = null;
            if (h != null && h.address() != 0) {
                try {
                    api.close(h);
                } catch (RuntimeException ignored) {
                    // nothing sensible to do during cleanup
                }
            }
        }
    }

    private VecDb(Native api, MemorySegment handle) {
        this.api = api;
        this.cleanup = new CloseAction(api, handle);
        this.cleanable = CLEANER.register(this, cleanup);
    }

    // -- construction ------------------------------------------------------

    /** Open (or create) a database with the library defaults. */
    public static VecDb open(String path) {
        return open(path, Options.defaults());
    }

    /** Open (or create) a database. Pass {@code dim = 0} to learn the
     * dimension from the first stored vector. */
    public static VecDb open(String path, Options options) {
        if (path == null || path.isEmpty()) {
            throw new IllegalArgumentException("path must be a non-empty string");
        }
        Options opts = options == null ? Options.defaults() : options;
        Native api = Native.get(null);
        MemorySegment handle = api.open(path, opts);
        if (handle == null) {
            throw new VecDbException(
                    "could not open " + path + " (missing, corrupt, or dimension mismatch)");
        }
        return new VecDb(api, handle);
    }

    /** Open with an explicit {@code libvecdb} path (rarely needed). */
    public static VecDb open(String path, Options options, String libraryPath) {
        if (path == null || path.isEmpty()) {
            throw new IllegalArgumentException("path must be a non-empty string");
        }
        Options opts = options == null ? Options.defaults() : options;
        Native api = Native.get(libraryPath);
        MemorySegment handle = api.open(path, opts);
        if (handle == null) {
            throw new VecDbException(
                    "could not open " + path + " (missing, corrupt, or dimension mismatch)");
        }
        return new VecDb(api, handle);
    }

    // -- lifecycle ---------------------------------------------------------

    /** Flush and close. Safe to call more than once. */
    @Override
    public void close() {
        cleanable.clean();
    }

    /** True once {@link #close()} has run. */
    public boolean isClosed() {
        return cleanup.handle == null;
    }

    /** Persist the database atomically (write to tmp, fsync, rename). */
    public void flush() {
        check(api.flush(require()));
    }

    /** Drop deleted records and rebuild the indexes. */
    public void compact() {
        check(api.compact(require()));
    }

    /** HNSW search breadth: larger means better recall, slower queries.
     * Fails when the database is not HNSW-indexed. */
    public void setEfSearch(long ef) {
        check(api.setEfSearch(require(), ef));
    }

    // -- introspection -----------------------------------------------------

    /** The database file path. */
    public String path() {
        return api.path(require());
    }

    /** Number of live records. */
    public long count() {
        return api.count(require());
    }

    /** Slots in use, including tombstones from deleted records. */
    public long capacity() {
        return api.capacity(require());
    }

    /** Vector length, or {@code 0} if no vector has been stored yet. */
    public long dim() {
        return api.dim(require());
    }

    /** The distance metric (persisted with the database). */
    public Metric metric() {
        return Metric.of(api.metricOf(require()));
    }

    /** The index type (persisted with the database). */
    public Index index() {
        return Index.of(api.indexOf(require()));
    }

    /** Every live id, in unspecified order. */
    public long[] ids() {
        return api.listIds(require());
    }

    /** The resolved path of the underlying library. */
    public String libraryPath() {
        return api.libraryPath();
    }

    // -- writes ------------------------------------------------------------

    /**
     * Store a record (upsert). Any of vector, payload and text may be empty.
     * The first vector stored fixes the database dimension unless a dimension
     * was passed at open time.
     */
    public void add(long id, RecordSpec record) {
        put(id, record, false);
    }

    /** Store a record with {@code INSERT_ONLY} semantics. */
    public void insert(long id, RecordSpec record) {
        put(id, record, true);
    }

    private void put(long id, RecordSpec record, boolean insertOnly) {
        RecordSpec spec = record == null ? RecordSpec.empty() : record;
        byte[] blob = Payload.encode(spec.payload);
        if (blob != null && blob.length > MAX_PAYLOAD) {
            throw new IllegalArgumentException("payload is too large (" + blob.length + " bytes)");
        }
        int flags = (insertOnly ? Native.INSERT_ONLY : 0) | (spec.sync ? Native.SYNC : 0);
        check(api.put(require(), id, spec.vector, blob, spec.text, flags));
    }

    /**
     * Store several records, returning how many were written.
     *
     * <p>When {@code sync} is set, a single flush happens after the last
     * record.
     */
    public int addAll(List<IdRecord> records, boolean insertOnly, boolean sync) {
        if (records == null || records.isEmpty()) {
            return 0;
        }
        int written = 0;
        for (int i = 0; i < records.size(); i++) {
            IdRecord ir = records.get(i);
            boolean last = sync && i == records.size() - 1;
            RecordSpec spec = ir.record.withSync(last);
            put(ir.id, spec, insertOnly);
            written++;
        }
        return written;
    }

    /** Convenience wrapper pairing an id with a {@link RecordSpec}. */
    public static final class IdRecord {
        public final long id;
        public final RecordSpec record;

        public IdRecord(long id, RecordSpec record) {
            this.id = id;
            this.record = record == null ? RecordSpec.empty() : record;
        }

        public static IdRecord of(long id, RecordSpec record) {
            return new IdRecord(id, record);
        }
    }

    /** Fetch one record. Throws {@link NotFoundException} if absent. */
    public Record get(long id) {
        Native.RecordView view = api.get(require(), id);
        return new Record(view.id, view.vector, Payload.decode(view.rawPayload),
                view.rawPayload, view.text);
    }

    /** Remove a record. Throws {@link NotFoundException} if absent. */
    public void delete(long id) {
        check(api.delete(require(), id));
    }

    /**
     * Remove several records, ignoring ids that do not exist.
     *
     * @return how many records were actually removed
     */
    public int deleteAll(long... ids) {
        MemorySegment h = require();
        int removed = 0;
        for (long id : ids) {
            if (api.delete(h, id) == Native.OK) {
                removed++;
            }
        }
        return removed;
    }

    // -- searches ----------------------------------------------------------

    /** k nearest neighbours by vector similarity. @see #search(float[], int) */
    public Match[] search(float[] vector, int k) {
        MemorySegment h = require();
        if (vector == null || vector.length == 0) {
            throw new IllegalArgumentException("a vector is required for vector search");
        }
        checkVector(vector);
        if (k <= 0) {
            return new Match[0];
        }
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment out = arena.allocate(k * Native.MATCH_SIZE);
            long n = api.searchVector(h, vector, k, out, k);
            return Native.decodeMatches(out, n);
        }
    }

    /** k nearest neighbours, defaulting to {@code k = 10}. */
    public Match[] search(float[] vector) {
        return search(vector, 10);
    }

    /** BM25 full-text search. */
    public Match[] searchText(String query, int k) {
        MemorySegment h = require();
        if (k <= 0 || query == null || query.trim().isEmpty()) {
            return new Match[0];
        }
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment out = arena.allocate(k * Native.MATCH_SIZE);
            long n = api.searchText(h, query, k, out, k);
            return Native.decodeMatches(out, n);
        }
    }

    /** BM25 full-text search, defaulting to {@code k = 10}. */
    public Match[] searchText(String query) {
        return searchText(query, 10);
    }

    /**
     * Blend vector and text results. {@code alpha} weights the vector signal:
     * {@code 1} is pure vector search, {@code 0} pure text search. Either
     * query may be omitted to fall back to the other modality.
     */
    public Match[] searchHybrid(float[] vector, String text, int k, float alpha) {
        MemorySegment h = require();
        boolean hasVector = vector != null && vector.length > 0;
        boolean hasText = text != null && !text.isEmpty();
        if (k <= 0 || (!hasVector && !hasText)) {
            return new Match[0];
        }
        if (hasVector) {
            checkVector(vector);
        }
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment out = arena.allocate(k * Native.MATCH_SIZE);
            long n = api.searchHybrid(h, hasVector ? vector : null, hasText ? text : null,
                    k, alpha, out, k);
            return Native.decodeMatches(out, n);
        }
    }

    /**
     * Convenience dispatcher: passing both queries runs a hybrid search,
     * passing only one runs that single-modality search.
     */
    public Match[] query(float[] vector, String text, int k, float alpha) {
        boolean hasVector = vector != null && vector.length > 0;
        boolean hasText = text != null && !text.isEmpty();
        if (hasVector && hasText) {
            return searchHybrid(vector, text, k, alpha);
        }
        if (hasVector) {
            return search(vector, k);
        }
        if (hasText) {
            return searchText(text, k);
        }
        return new Match[0];
    }

    // -- internals ---------------------------------------------------------

    private MemorySegment require() {
        MemorySegment h = cleanup.handle;
        if (h == null || h.address() == 0) {
            throw new VecDbException("database is closed");
        }
        return h;
    }

    private void check(int rc) {
        if (rc != Native.OK) {
            throw api.statusError(rc, cleanup.handle);
        }
    }

    private void checkVector(float[] vector) {
        long d = dim();
        if (d == 0) {
            throw new DimensionMismatchException("no vector dimension has been established", Native.ERR_NODIM);
        }
        if (vector.length != d) {
            throw new DimensionMismatchException(
                    "query dimension " + vector.length + " != " + d, Native.ERR_DIM);
        }
    }

    /** List the raw byte payloads of a hit array, for tests. */
    static List<byte[]> rawPayloads(Match[] matches) {
        List<byte[]> out = new ArrayList<>(matches.length);
        for (Match m : matches) {
            out.add(m.rawPayload());
        }
        return out;
    }
}
