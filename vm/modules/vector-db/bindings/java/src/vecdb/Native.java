package vecdb;

import java.io.File;
import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

/**
 * Raw cgo-free FFI layer over {@code libvecdb}, using the Java 21+ Foreign
 * Function &amp; Memory API ({@code java.lang.foreign}).
 *
 * <p>Everything here works on raw {@link MemorySegment}s and status codes;
 * {@link VecDb} is the safe, idiomatic wrapper. Downcall handles are bound
 * once per loaded library.
 *
 * <p>Requires {@code --enable-native-access=ALL-UNNAMED} (a warning is
 * printed otherwise) and, on JDK 21, {@code --enable-preview}.
 */
final class Native {

    private Native() {
    }

    // -- status codes (must match vecdb.h) ---------------------------------
    static final int OK = 0;
    static final int ERR_IO = -1;
    static final int ERR_NOMEM = -2;
    static final int ERR_INVALID = -3;
    static final int ERR_NOTFOUND = -4;
    static final int ERR_EXISTS = -5;
    static final int ERR_DIM = -6;
    static final int ERR_CORRUPT = -7;
    static final int ERR_NODIM = -8;

    static final int INSERT_ONLY = 1;
    static final int SYNC = 1 << 1;

    // -- struct layouts ----------------------------------------------------
    static final MemoryLayout OPTIONS_LAYOUT = MemoryLayout.structLayout(
            ValueLayout.JAVA_LONG.withName("dim"),
            ValueLayout.JAVA_INT.withName("metric"),
            ValueLayout.JAVA_INT.withName("index"),
            ValueLayout.JAVA_LONG.withName("hnsw_m"),
            ValueLayout.JAVA_LONG.withName("hnsw_ef_construction"),
            ValueLayout.JAVA_LONG.withName("hnsw_ef_search"),
            ValueLayout.JAVA_LONG.withName("seed"));

    static final MemoryLayout RECORD_LAYOUT = MemoryLayout.structLayout(
            ValueLayout.JAVA_LONG.withName("id"),
            ValueLayout.ADDRESS.withName("vec"),
            ValueLayout.ADDRESS.withName("payload"),
            ValueLayout.JAVA_LONG.withName("payload_len"),
            ValueLayout.ADDRESS.withName("text"));

    static final MemoryLayout MATCH_LAYOUT = MemoryLayout.structLayout(
            ValueLayout.JAVA_LONG.withName("id"),
            ValueLayout.JAVA_FLOAT.withName("score"),
            MemoryLayout.paddingLayout(4),
            ValueLayout.ADDRESS.withName("payload"),
            ValueLayout.JAVA_LONG.withName("payload_len"),
            ValueLayout.ADDRESS.withName("text"));

    static final long MATCH_SIZE = MATCH_LAYOUT.byteSize();

    private static final long RECORD_ID = RECORD_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("id"));
    private static final long RECORD_VEC = RECORD_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("vec"));
    private static final long RECORD_PAYLOAD = RECORD_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("payload"));
    private static final long RECORD_PAYLOAD_LEN = RECORD_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("payload_len"));
    private static final long RECORD_TEXT = RECORD_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("text"));

    private static final long MATCH_ID = MATCH_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("id"));
    private static final long MATCH_SCORE = MATCH_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("score"));
    private static final long MATCH_PAYLOAD = MATCH_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("payload"));
    private static final long MATCH_PAYLOAD_LEN = MATCH_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("payload_len"));
    private static final long MATCH_TEXT = MATCH_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("text"));

    // -- function handles --------------------------------------------------
    private MethodHandle fnOptionsDefault;
    private MethodHandle fnOpen;
    private MethodHandle fnClose;
    private MethodHandle fnFlush;
    private MethodHandle fnCompact;
    private MethodHandle fnCount;
    private MethodHandle fnCapacity;
    private MethodHandle fnDim;
    private MethodHandle fnMetricOf;
    private MethodHandle fnIndexOf;
    private MethodHandle fnPath;
    private MethodHandle fnSetEfSearch;
    private MethodHandle fnStrerror;
    private MethodHandle fnLastError;
    private MethodHandle fnListIds;
    private MethodHandle fnPutEx;
    private MethodHandle fnGet;
    private MethodHandle fnDelete;
    private MethodHandle fnSearchVector;
    private MethodHandle fnSearchText;
    private MethodHandle fnSearchHybrid;

    private static Native instance;
    private static String instancePath;

    /** Load (once per library path) and bind the library. */
    static synchronized Native get(String explicitPath) {
        String resolved = findLibrary(explicitPath);
        if (instance != null && resolved.equals(instancePath)) {
            return instance;
        }
        try {
            instance = new Native(resolved);
            instancePath = resolved;
        } catch (RuntimeException e) {
            throw new VecDbException(
                    "could not load libvecdb (" + resolved + "): " + e.getMessage()
                    + ". Build it with `make` and either place it next to the bindings, "
                    + "add its directory to LD_LIBRARY_PATH, or point VECDB_LIBRARY at it.");
        }
        return instance;
    }

    private String libraryPath;

    private Native(String resolvedPath) {
        this.libraryPath = resolvedPath;
        Linker linker = Linker.nativeLinker();
        Arena arena = Arena.ofShared();
        SymbolLookup lib = SymbolLookup.libraryLookup(Paths.get(resolvedPath), arena);

        this.fnOptionsDefault = linker.downcallHandle(
                find(lib, "vecdb_options_default"), FunctionDescriptor.ofVoid(ValueLayout.ADDRESS));
        this.fnOpen = linker.downcallHandle(
                find(lib, "vecdb_open"),
                FunctionDescriptor.of(ValueLayout.ADDRESS, ValueLayout.ADDRESS, ValueLayout.ADDRESS));
        this.fnClose = linker.downcallHandle(
                find(lib, "vecdb_close"), FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS));
        this.fnFlush = linker.downcallHandle(
                find(lib, "vecdb_flush"), FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS));
        this.fnCompact = linker.downcallHandle(
                find(lib, "vecdb_compact"), FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS));
        this.fnCount = linker.downcallHandle(
                find(lib, "vecdb_count"), FunctionDescriptor.of(ValueLayout.JAVA_LONG, ValueLayout.ADDRESS));
        this.fnCapacity = linker.downcallHandle(
                find(lib, "vecdb_capacity"), FunctionDescriptor.of(ValueLayout.JAVA_LONG, ValueLayout.ADDRESS));
        this.fnDim = linker.downcallHandle(
                find(lib, "vecdb_dim"), FunctionDescriptor.of(ValueLayout.JAVA_LONG, ValueLayout.ADDRESS));
        this.fnMetricOf = linker.downcallHandle(
                find(lib, "vecdb_metric_of"), FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS));
        this.fnIndexOf = linker.downcallHandle(
                find(lib, "vecdb_index_of"), FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS));
        this.fnPath = linker.downcallHandle(
                find(lib, "vecdb_path"), FunctionDescriptor.of(ValueLayout.ADDRESS, ValueLayout.ADDRESS));
        this.fnSetEfSearch = linker.downcallHandle(
                find(lib, "vecdb_set_ef_search"),
                FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG));
        this.fnStrerror = linker.downcallHandle(
                find(lib, "vecdb_strerror"),
                FunctionDescriptor.of(ValueLayout.ADDRESS, ValueLayout.JAVA_INT));
        this.fnLastError = linker.downcallHandle(
                find(lib, "vecdb_last_error"), FunctionDescriptor.of(ValueLayout.ADDRESS, ValueLayout.ADDRESS));
        this.fnListIds = linker.downcallHandle(
                find(lib, "vecdb_list_ids"),
                FunctionDescriptor.of(ValueLayout.JAVA_LONG, ValueLayout.ADDRESS, ValueLayout.ADDRESS,
                        ValueLayout.JAVA_LONG));
        this.fnPutEx = linker.downcallHandle(
                find(lib, "vecdb_put_ex"),
                FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG,
                        ValueLayout.ADDRESS, ValueLayout.JAVA_LONG, ValueLayout.ADDRESS,
                        ValueLayout.JAVA_LONG, ValueLayout.ADDRESS, ValueLayout.JAVA_INT));
        this.fnGet = linker.downcallHandle(
                find(lib, "vecdb_get"),
                FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG,
                        ValueLayout.ADDRESS));
        this.fnDelete = linker.downcallHandle(
                find(lib, "vecdb_delete"),
                FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG));
        this.fnSearchVector = linker.downcallHandle(
                find(lib, "vecdb_search_vector"),
                FunctionDescriptor.of(ValueLayout.JAVA_LONG, ValueLayout.ADDRESS, ValueLayout.ADDRESS,
                        ValueLayout.JAVA_LONG, ValueLayout.JAVA_LONG, ValueLayout.ADDRESS,
                        ValueLayout.JAVA_LONG));
        this.fnSearchText = linker.downcallHandle(
                find(lib, "vecdb_search_text"),
                FunctionDescriptor.of(ValueLayout.JAVA_LONG, ValueLayout.ADDRESS, ValueLayout.ADDRESS,
                        ValueLayout.JAVA_LONG, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG));
        this.fnSearchHybrid = linker.downcallHandle(
                find(lib, "vecdb_search_hybrid"),
                FunctionDescriptor.of(ValueLayout.JAVA_LONG, ValueLayout.ADDRESS, ValueLayout.ADDRESS,
                        ValueLayout.JAVA_LONG, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG,
                        ValueLayout.JAVA_FLOAT, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG));
    }

    private static MemorySegment find(SymbolLookup lib, String name) {
        return lib.find(name).orElseThrow(
                () -> new VecDbException("libvecdb is missing the symbol " + name));
    }

    /** The resolved path of the loaded library. */
    String libraryPath() {
        return libraryPath;
    }

    // -- library discovery -------------------------------------------------

    /**
     * Locate {@code libvecdb}. Search order: an explicit path, the
     * {@code VECDB_LIBRARY} environment variable, the current working
     * directory and its ancestors (up to 6 levels), {@code java.library.path},
     * then the bare soname for the system loader.
     */
    static String findLibrary(String explicit) {
        List<String> candidates = new ArrayList<>();
        if (explicit != null && !explicit.isEmpty()) {
            if (!Files.exists(Paths.get(explicit))) {
                throw new VecDbException("library not found: " + explicit);
            }
            return Paths.get(explicit).toAbsolutePath().normalize().toString();
        }
        String fromEnv = System.getenv("VECDB_LIBRARY");
        if (fromEnv != null && !fromEnv.isEmpty()) {
            candidates.add(fromEnv);
        }
        String[] names = {"libvecdb.so", "libvecdb.dylib", "vecdb.dll"};

        // this class's own location, plus each ancestor
        addAncestors(candidates, classLocation(), names);
        // the working directory, plus each ancestor
        addAncestors(candidates, Paths.get("").toAbsolutePath(), names);
        String libPath = System.getProperty("java.library.path");
        if (libPath != null) {
            for (String dir : libPath.split(File.pathSeparator)) {
                if (!dir.isEmpty()) {
                    for (String name : names) {
                        candidates.add(Paths.get(dir, name).toString());
                    }
                }
            }
        }
        for (String cand : candidates) {
            if (cand != null && !cand.isEmpty() && Files.exists(Paths.get(cand))) {
                return Paths.get(cand).toAbsolutePath().normalize().toString();
            }
        }
        return names[0]; // let the system loader have a go
    }

    private static void addAncestors(List<String> out, Path start, String[] names) {
        Path dir = start;
        for (int i = 0; i < 6 && dir != null; i++) {
            for (String name : names) {
                out.add(dir.resolve(name).toString());
            }
            dir = dir.getParent();
        }
    }

    private static Path classLocation() {
        try {
            return Paths.get(Native.class.getProtectionDomain().getCodeSource()
                    .getLocation().toURI());
        } catch (Exception e) {
            return Paths.get("").toAbsolutePath();
        }
    }

    // -- memory helpers ----------------------------------------------------

    /** Read the freshly defaulted options struct. */
    static OptionsDefaults optionsDefaults() {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment seg = arena.allocate(OPTIONS_LAYOUT);
            Native.get(null).fnOptionsDefault.invoke(seg);
            return new OptionsDefaults(
                    seg.get(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("dim"))),
                    seg.get(ValueLayout.JAVA_INT, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("metric"))),
                    seg.get(ValueLayout.JAVA_INT, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("index"))),
                    seg.get(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("hnsw_m"))),
                    seg.get(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("hnsw_ef_construction"))),
                    seg.get(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("hnsw_ef_search"))),
                    seg.get(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("seed"))));
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    /** Plain snapshot of the library defaults. */
    static final class OptionsDefaults {
        final long dim;
        final int metric;
        final int index;
        final long hnswM;
        final long hnswEfConstruction;
        final long hnswEfSearch;
        final long seed;

        OptionsDefaults(long dim, int metric, int index, long hnswM,
                        long hnswEfConstruction, long hnswEfSearch, long seed) {
            this.dim = dim;
            this.metric = metric;
            this.index = index;
            this.hnswM = hnswM;
            this.hnswEfConstruction = hnswEfConstruction;
            this.hnswEfSearch = hnswEfSearch;
            this.seed = seed;
        }
    }

    /** A NUL-terminated UTF-8 segment in {@code arena}, or {@code NULL}. */
    static MemorySegment cstring(Arena arena, String s) {
        if (s == null) {
            return MemorySegment.NULL;
        }
        byte[] bytes = s.getBytes(StandardCharsets.UTF_8);
        MemorySegment seg = arena.allocate(bytes.length + 1L);
        MemorySegment.copy(bytes, 0, seg, ValueLayout.JAVA_BYTE, 0, bytes.length);
        seg.set(ValueLayout.JAVA_BYTE, bytes.length, (byte) 0);
        return seg;
    }

    /** A float array segment in {@code arena}, or {@code NULL} when empty. */
    static MemorySegment floats(Arena arena, float[] values) {
        if (values == null || values.length == 0) {
            return MemorySegment.NULL;
        }
        MemorySegment seg = arena.allocate((long) values.length * 4);
        MemorySegment.copy(values, 0, seg, ValueLayout.JAVA_FLOAT, 0, values.length);
        return seg;
    }

    /** A byte array segment in {@code arena}, or {@code NULL} when empty/null. */
    static MemorySegment bytes(Arena arena, byte[] values) {
        if (values == null || values.length == 0) {
            return MemorySegment.NULL;
        }
        MemorySegment seg = arena.allocate(values.length);
        MemorySegment.copy(values, 0, seg, ValueLayout.JAVA_BYTE, 0, values.length);
        return seg;
    }

    /** Read a NUL-terminated C string, or {@code null} for a NULL pointer. */
    static String cstr(MemorySegment ptr) {
        if (ptr == null || ptr.address() == 0) {
            return null;
        }
        return ptr.reinterpret(Long.MAX_VALUE).getUtf8String(0);
    }

    /** Copy {@code len} bytes out of a borrowed pointer, or {@code null}. */
    static byte[] readBytes(MemorySegment ptr, long len) {
        if (ptr == null || ptr.address() == 0 || len <= 0) {
            return null;
        }
        MemorySegment src = ptr.reinterpret(len);
        byte[] out = new byte[(int) len];
        MemorySegment.copy(src, ValueLayout.JAVA_BYTE, 0, out, 0, (int) len);
        return out;
    }

    private static RuntimeException sneaky(Throwable t) {
        if (t instanceof RuntimeException) {
            return (RuntimeException) t;
        }
        if (t instanceof Error) {
            throw (Error) t;
        }
        return new VecDbException(String.valueOf(t.getMessage()));
    }

    // -- handle-based operations ------------------------------------------

    MemorySegment open(String path, Options opts) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment cpath = cstring(arena, path);
            MemorySegment copts = arena.allocate(OPTIONS_LAYOUT);
            copts.set(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("dim")), opts.dim);
            copts.set(ValueLayout.JAVA_INT, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("metric")), opts.metric.code());
            copts.set(ValueLayout.JAVA_INT, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("index")), opts.index.code());
            copts.set(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("hnsw_m")), opts.hnswM);
            copts.set(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("hnsw_ef_construction")), opts.hnswEfConstruction);
            copts.set(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("hnsw_ef_search")), opts.hnswEfSearch);
            copts.set(ValueLayout.JAVA_LONG, OPTIONS_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("seed")), opts.seed);
            MemorySegment h = (MemorySegment) fnOpen.invoke(cpath, copts);
            return h == null || h.address() == 0 ? null : h;
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    int close(MemorySegment db) {
        return callInt(fnClose, db);
    }

    int flush(MemorySegment db) {
        return callInt(fnFlush, db);
    }

    int compact(MemorySegment db) {
        return callInt(fnCompact, db);
    }

    long count(MemorySegment db) {
        return callLong(fnCount, db);
    }

    long capacity(MemorySegment db) {
        return callLong(fnCapacity, db);
    }

    long dim(MemorySegment db) {
        return callLong(fnDim, db);
    }

    int metricOf(MemorySegment db) {
        return callInt(fnMetricOf, db);
    }

    int indexOf(MemorySegment db) {
        return callInt(fnIndexOf, db);
    }

    String path(MemorySegment db) {
        try {
            return cstr((MemorySegment) fnPath.invoke(db));
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    int setEfSearch(MemorySegment db, long ef) {
        try {
            return (int) fnSetEfSearch.invoke(db, ef);
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    String strerror(int status) {
        try {
            return cstr((MemorySegment) fnStrerror.invoke(status));
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    String lastError(MemorySegment db) {
        try {
            return cstr((MemorySegment) fnLastError.invoke(db));
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    long[] listIds(MemorySegment db) {
        try (Arena arena = Arena.ofConfined()) {
            long n = (long) fnListIds.invoke(db, MemorySegment.NULL, 0L);
            if (n <= 0) {
                return new long[0];
            }
            MemorySegment buf = arena.allocate(n * 8);
            long got = (long) fnListIds.invoke(db, buf, n);
            long[] out = new long[(int) got];
            for (int i = 0; i < got; i++) {
                out[i] = buf.get(ValueLayout.JAVA_LONG, (long) i * 8);
            }
            return out;
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    int put(MemorySegment db, long id, float[] vec, byte[] payload, String text, int flags) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment cvec = floats(arena, vec);
            MemorySegment cpayload = bytes(arena, payload);
            MemorySegment ctext = cstring(arena, text);
            return (int) fnPutEx.invoke(db, id, cvec, (long) (vec == null ? 0 : vec.length),
                    cpayload, (long) (payload == null ? 0 : payload.length), ctext, flags);
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    /** Read one record. Raw pointers are only valid during the call. */
    RecordView get(MemorySegment db, long id) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment rec = arena.allocate(RECORD_LAYOUT);
            int rc = (int) fnGet.invoke(db, id, rec);
            if (rc != OK) {
                throw statusError(rc, db);
            }
            long recId = rec.get(ValueLayout.JAVA_LONG, RECORD_ID);
            MemorySegment vecPtr = rec.get(ValueLayout.ADDRESS, RECORD_VEC);
            MemorySegment payloadPtr = rec.get(ValueLayout.ADDRESS, RECORD_PAYLOAD);
            long payloadLen = rec.get(ValueLayout.JAVA_LONG, RECORD_PAYLOAD_LEN);
            MemorySegment textPtr = rec.get(ValueLayout.ADDRESS, RECORD_TEXT);
            float[] vector = null;
            if (vecPtr != null && vecPtr.address() != 0) {
                long d = dim(db);
                if (d > 0) {
                    MemorySegment vseg = vecPtr.reinterpret(d * 4);
                    vector = new float[(int) d];
                    MemorySegment.copy(vseg, ValueLayout.JAVA_FLOAT, 0, vector, 0, (int) d);
                }
            }
            return new RecordView(recId, vector, readBytes(payloadPtr, payloadLen), cstr(textPtr));
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    int delete(MemorySegment db, long id) {
        try {
            return (int) fnDelete.invoke(db, id);
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    long searchVector(MemorySegment db, float[] query, long k, MemorySegment out, long cap) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment q = floats(arena, query);
            return (long) fnSearchVector.invoke(db, q, (long) query.length, k, out, cap);
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    long searchText(MemorySegment db, String query, long k, MemorySegment out, long cap) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment q = cstring(arena, query);
            return (long) fnSearchText.invoke(db, q, k, out, cap);
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    long searchHybrid(MemorySegment db, float[] vec, String text, long k, float alpha,
                      MemorySegment out, long cap) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment q = floats(arena, vec);
            MemorySegment t = cstring(arena, text);
            long dim = vec == null ? 0 : vec.length;
            return (long) fnSearchHybrid.invoke(db, q, dim, t, k, alpha, out, cap);
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    // -- match decoding ----------------------------------------------------

    /** Materialise the first {@code n} matches of an already-filled buffer. */
    static Match[] decodeMatches(MemorySegment buf, long n) {
        Match[] out = new Match[(int) n];
        for (int i = 0; i < n; i++) {
            long base = (long) i * MATCH_SIZE;
            long id = buf.get(ValueLayout.JAVA_LONG, base + MATCH_ID);
            float score = buf.get(ValueLayout.JAVA_FLOAT, base + MATCH_SCORE);
            MemorySegment payloadPtr = buf.get(ValueLayout.ADDRESS, base + MATCH_PAYLOAD);
            long payloadLen = buf.get(ValueLayout.JAVA_LONG, base + MATCH_PAYLOAD_LEN);
            MemorySegment textPtr = buf.get(ValueLayout.ADDRESS, base + MATCH_TEXT);
            byte[] raw = readBytes(payloadPtr, payloadLen);
            out[i] = new Match(id, score, Payload.decode(raw), raw, cstr(textPtr));
        }
        return out;
    }

    /** Build the error for a non-OK status, with the handle detail attached. */
    VecDbException statusError(int rc, MemorySegment db) {
        String detail = "";
        if (db != null && db.address() != 0) {
            String msg = lastError(db);
            if (msg != null && !msg.isEmpty()) {
                detail = ": " + msg;
            }
        }
        String code = strerror(rc);
        String message = (code == null || code.isEmpty() ? "vecdb error " + rc : code) + detail;
        switch (rc) {
            case ERR_NOTFOUND:
                return new NotFoundException(message, rc);
            case ERR_EXISTS:
                return new AlreadyExistsException(message, rc);
            case ERR_DIM:
            case ERR_NODIM:
                return new DimensionMismatchException(message, rc);
            default:
                return new VecDbException(message, rc);
        }
    }

    /** Borrowed view of a record, copied into safe objects by {@link VecDb}. */
    static final class RecordView {
        final long id;
        final float[] vector;
        final byte[] rawPayload;
        final String text;

        RecordView(long id, float[] vector, byte[] rawPayload, String text) {
            this.id = id;
            this.vector = vector;
            this.rawPayload = rawPayload;
            this.text = text;
        }
    }

    private int callInt(MethodHandle h, MemorySegment db) {
        try {
            return (int) h.invoke(db);
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }

    private long callLong(MethodHandle h, MemorySegment db) {
        try {
            return (long) h.invoke(db);
        } catch (Throwable t) {
            throw sneaky(t);
        }
    }
}
