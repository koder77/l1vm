package vecdb;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Smoke test for the Java bindings (FFM over {@code libvecdb.so}).
 *
 * <p>Run with {@code ./run-tests.sh}, or by hand:
 *
 * <pre>{@code
 * javac --release 21 --enable-preview -d build/classes $(find src -name '*.java')
 * java --enable-preview --enable-native-access=ALL-UNNAMED \
 *      -cp build/classes vecdb.VecDbTest
 * }</pre>
 *
 * It needs the shared library built ({@code make} in the repository root);
 * the loader finds it relative to this module, the working directory, or via
 * {@code VECDB_LIBRARY}.
 */
public final class VecDbTest {

    private static int checks;
    private static int failures;
    private static final List<String> failed = new ArrayList<>();

    public static void main(String[] args) throws Exception {
        basicVector();
        textSearch();
        hybrid();
        payloadCodec();
        getDeleteErrors();
        dimensionMismatch();
        putManyListIds();
        persistence();
        closedAndDefaults();
        threads();

        System.out.println();
        System.out.printf("java bindings: %d checks, %d failures%n", checks, failures);
        if (failures > 0) {
            for (String f : failed) {
                System.out.println("  FAILED: " + f);
            }
            System.exit(1);
        }
    }

    // -- tiny assertion helpers -------------------------------------------

    private static void check(boolean ok, String what) {
        checks++;
        if (!ok) {
            failures++;
            failed.add(what);
            System.out.println("FAIL: " + what);
        }
    }

    private static void eq(Object got, Object want, String what) {
        boolean ok;
        if (got instanceof float[] && want instanceof float[]) {
            ok = Arrays.equals((float[]) got, (float[]) want);
        } else if (got instanceof byte[] && want instanceof byte[]) {
            ok = Arrays.equals((byte[]) got, (byte[]) want);
        } else {
            ok = got == null ? want == null : got.equals(want);
        }
        check(ok, what + " (got " + render(got) + ", want " + render(want) + ")");
    }

    private static void feq(float got, float want, float eps, String what) {
        check(Math.abs(got - want) <= eps, what + " (got " + got + ", want " + want + ")");
    }

    private static String render(Object o) {
        if (o instanceof float[]) {
            return Arrays.toString((float[]) o);
        }
        if (o instanceof byte[]) {
            return Arrays.toString((byte[]) o);
        }
        return String.valueOf(o);
    }

    private static Path tempDir() throws IOException {
        return Files.createTempDirectory("vecdb-java-");
    }

    private static VecDb open(Path dir, String name, Options opts) {
        VecDb db = opts == null ? VecDb.open(dir.resolve(name).toString())
                : VecDb.open(dir.resolve(name).toString(), opts);
        return db;
    }

    // -- tests -------------------------------------------------------------

    private static void basicVector() throws IOException {
        Path dir = tempDir();
        try (VecDb db = open(dir, "v.db",
                Options.builder().dim(3).metric(Metric.COSINE).build())) {
            db.add(1, RecordSpec.of(new float[]{1, 0, 0})
                    .textValue("a cat sat on the mat")
                    .payload(Map.of("kind", "animal")));
            db.add(2, RecordSpec.of(new float[]{0, 1, 0})
                    .textValue("a dog barked at the postman")
                    .payload(Map.of("kind", "animal")));
            db.add(3, RecordSpec.of(new float[]{0, 0, 1}).textValue("a fish swam in the sea"));

            eq(db.count(), 3L, "count");
            eq(db.dim(), 3L, "dim");
            eq(db.metric(), Metric.COSINE, "metric");
            eq(db.index(), Index.FLAT, "index");

            Match[] hits = db.search(new float[]{0.95f, 0.05f, 0f}, 3);
            eq(hits.length, 3, "search hit count");
            eq(hits[0].id(), 1L, "nearest neighbour id");
            check(hits[0].score() > hits[1].score(), "scores descending");
            eq(hits[0].text(), "a cat sat on the mat", "match text");
            // payload round-trips through the JSON codec
            eq(hits[0].rawPayload(), Payload.encode(Map.of("kind", "animal")), "raw payload bytes");

            Match fish = null;
            for (Match m : hits) {
                if (m.id() == 3) {
                    fish = m;
                }
            }
            check(fish != null && fish.rawPayload() == null, "record 3 has no payload");
        }
    }

    private static void textSearch() throws IOException {
        Path dir = tempDir();
        try (VecDb db = open(dir, "t.db", null)) {
            db.add(1, RecordSpec.text("the quick brown fox jumps over the lazy dog"));
            db.add(2, RecordSpec.text("a packet of postman biscuits"));
            db.add(3, RecordSpec.text("foxes and hounds"));

            // BM25 has no stemming: 'foxes' is its own token.
            eq(db.searchText("fox", 5).length, 1, "fox hit count");
            eq(db.searchText("fox", 5)[0].id(), 1L, "fox id");
            eq(db.searchText("foxes", 5)[0].id(), 3L, "foxes id");
            eq(db.searchText("postman", 5)[0].id(), 2L, "postman id");
            eq(db.searchText("   ", 5).length, 0, "blank query");
        }
    }

    private static void hybrid() throws IOException {
        Path dir = tempDir();
        try (VecDb db = open(dir, "h.db",
                Options.builder().dim(2).metric(Metric.L2).build())) {
            db.add(1, RecordSpec.of(new float[]{0, 0}).textValue("red apple fruit"));
            db.add(2, RecordSpec.of(new float[]{10, 10}).textValue("blue car vehicle"));

            Match[] hits = db.searchHybrid(new float[]{0.1f, 0.1f}, "apple", 2, 0.5f);
            eq(hits.length, 2, "hybrid hit count");
            eq(hits[0].id(), 1L, "hybrid prefers the matching doc");

            Match[] pure = db.searchHybrid(new float[]{9.9f, 9.9f}, "zzz", 1, 1.0f);
            eq(pure.length, 1, "alpha=1 hit count");
            eq(pure[0].id(), 2L, "alpha=1 is pure vector search");

            Match[] textOnly = db.searchHybrid(null, "apple", 1, 0.0f);
            eq(textOnly.length, 1, "alpha=0 hit count");
            eq(textOnly[0].id(), 1L, "alpha=0 is pure text search");

            // dispatcher
            eq(db.query(new float[]{9.9f, 9.9f}, null, 1, 0.5f)[0].id(), 2L, "query(vector)");
            eq(db.query(null, "apple", 1, 0.5f)[0].id(), 1L, "query(text)");
            eq(db.query(new float[]{0.1f, 0.1f}, "apple", 1, 0.5f)[0].id(), 1L, "query(both)");
        }
    }

    private static void payloadCodec() throws IOException {
        // codec, independent of the database
        eq(Payload.decode(Payload.encode((byte) 42)), 42L, "byte -> Long");
        eq(Payload.decode(Payload.encode(42)), 42L, "int -> Long");
        eq(Payload.decode(Payload.encode(1L << 40)), 1L << 40, "long -> Long");
        eq(Payload.decode(Payload.encode(3.5)), 3.5, "double -> Double");
        eq(Payload.decode(Payload.encode(3.5f)), 3.5, "float -> Double");
        eq(Payload.decode(Payload.encode(true)), "true", "boolean -> json");
        eq(Payload.decode(Payload.encode("a string")), "\"a string\"", "string -> json");
        eq(Payload.decode(Payload.encode(new byte[]{1, 2, 3})), new byte[]{1, 2, 3}, "bytes -> byte[]");
        eq(Payload.decode(null), null, "null -> null");
        eq(Payload.decode(new byte[0]), new byte[0], "empty -> empty");

        Map<String, Object> nested = new LinkedHashMap<>();
        nested.put("a", 1);
        nested.put("b", Arrays.<Object>asList(2, 3));
        eq(Payload.decode(Payload.encode(nested)), "{\"a\":1,\"b\":[2,3]}", "map -> json");

        // round-trip through the database
        Path dir = tempDir();
        try (VecDb db = open(dir, "p.db", null)) {
            Map<Long, Object> cases = new LinkedHashMap<>();
            cases.put(100L, 42L);
            cases.put(101L, 3.5);
            cases.put(102L, "a string");
            cases.put(103L, new byte[]{1, 2, 3});
            cases.put(105L, true);
            cases.put(106L, 1L << 40);
            for (Map.Entry<Long, Object> e : cases.entrySet()) {
                db.add(e.getKey(), RecordSpec.withPayload(e.getValue()));
            }
            for (Map.Entry<Long, Object> e : cases.entrySet()) {
                Record rec = db.get(e.getKey());
                eq(rec.payload(), Payload.decode(Payload.encode(e.getValue())),
                        "payload round-trip id " + e.getKey());
            }
        }
    }

    private static void getDeleteErrors() throws IOException {
        Path dir = tempDir();
        try (VecDb db = open(dir, "g.db", Options.builder().dim(2).build())) {
            db.add(7, RecordSpec.of(new float[]{1, 2}).textValue("hello"));
            Record rec = db.get(7);
            eq(rec.id(), 7L, "get id");
            eq(rec.text(), "hello", "get text");
            eq(rec.vector(), new float[]{1, 2}, "get vector");

            db.delete(7);
            eq(db.count(), 0L, "count after delete");

            boolean notFoundOnGet = false;
            try {
                db.get(7);
            } catch (NotFoundException e) {
                notFoundOnGet = true;
            }
            check(notFoundOnGet, "get(missing) throws NotFoundException");

            boolean notFoundOnDelete = false;
            try {
                db.delete(7);
            } catch (NotFoundException e) {
                notFoundOnDelete = true;
            }
            check(notFoundOnDelete, "delete(missing) throws NotFoundException");

            db.add(9, RecordSpec.of(new float[]{0, 0}));
            boolean exists = false;
            try {
                db.insert(9, RecordSpec.of(new float[]{0, 0}));
            } catch (AlreadyExistsException e) {
                exists = true;
            }
            check(exists, "duplicate insert throws AlreadyExistsException");
        }
    }

    private static void dimensionMismatch() throws IOException {
        Path dir = tempDir();
        try (VecDb db = open(dir, "d.db", Options.builder().dim(3).build())) {
            boolean dimOnPut = false;
            try {
                db.add(1, RecordSpec.of(new float[]{1, 2}));
            } catch (DimensionMismatchException e) {
                dimOnPut = true;
            }
            check(dimOnPut, "wrong-dim put throws DimensionMismatchException");

            db.add(1, RecordSpec.of(new float[]{1, 2, 3}));

            boolean dimOnSearch = false;
            try {
                db.search(new float[]{1}, 1);
            } catch (DimensionMismatchException e) {
                dimOnSearch = true;
            }
            check(dimOnSearch, "wrong-dim search throws DimensionMismatchException");
        }
    }

    private static void putManyListIds() throws IOException {
        Path dir = tempDir();
        try (VecDb db = open(dir, "m.db", Options.builder().dim(2).build())) {
            List<VecDb.IdRecord> batch = new ArrayList<>();
            for (int i = 0; i < 50; i++) {
                batch.add(VecDb.IdRecord.of(i,
                        RecordSpec.of(new float[]{i, 0}).textValue("doc number").payload(i)));
            }
            eq(db.addAll(batch, false, false), 50, "addAll count");
            eq(db.count(), 50L, "count after addAll");

            long[] ids = db.ids();
            Set<Long> distinct = new HashSet<>();
            for (long id : ids) {
                distinct.add(id);
            }
            eq(distinct.size(), 50, "ids() distinct count");

            long[] evens = new long[25];
            for (int i = 0; i < 25; i++) {
                evens[i] = i * 2L;
            }
            eq(db.deleteAll(evens), 25, "deleteAll removed");
            eq(db.count(), 25L, "count after deleteAll");

            // payload of an odd id survived the batch write
            eq(db.get(11).payload(), 11L, "batched payload");
        }
    }

    private static void persistence() throws IOException {
        Path dir = tempDir();
        Path path = dir.resolve("persist.db");
        try (VecDb db = VecDb.open(path.toString(),
                Options.builder().dim(2).index(Index.HNSW).metric(Metric.IP).build())) {
            for (int i = 0; i < 20; i++) {
                db.add(i, RecordSpec.of(new float[]{i, 1}).textValue("text").payload((long) i));
            }
            db.flush();
        }

        try (VecDb db = VecDb.open(path.toString())) {
            eq(db.count(), 20L, "reopened count");
            eq(db.index(), Index.HNSW, "index persisted");
            eq(db.metric(), Metric.IP, "metric persisted");
            Record rec = db.get(13);
            eq(rec.vector(), new float[]{13, 1}, "reloaded vector");
            eq(rec.payload(), 13L, "reloaded payload");
            db.compact();
            eq(db.count(), 20L, "count after compact");
            db.setEfSearch(200);
        }
    }

    private static void closedAndDefaults() throws IOException {
        Options defaults = Options.defaults();
        eq(defaults.dim, 0L, "default dim");
        eq(defaults.metric, Metric.L2, "default metric");
        eq(defaults.index, Index.FLAT, "default index");
        eq(defaults.hnswM, 16L, "default hnsw M");
        eq(defaults.hnswEfConstruction, 100L, "default efConstruction");
        eq(defaults.hnswEfSearch, 100L, "default efSearch");
        eq(defaults.seed, 42L, "default seed");

        eq(Metric.parse("cosine"), Metric.COSINE, "parse cosine");
        eq(Metric.parse("IP"), Metric.IP, "parse ip");
        eq(Index.parse("hnsw"), Index.HNSW, "parse hnsw");
        eq(Metric.COSINE.toString(), "cosine", "metric label");

        Path dir = tempDir();
        VecDb db = open(dir, "c.db", Options.builder().dim(1).build());
        check(db.path().endsWith("c.db"), "path ends with c.db");
        db.close();
        db.close(); // second close is a no-op
        check(db.isClosed(), "isClosed after close");

        boolean closedError = false;
        try {
            db.add(1, RecordSpec.of(new float[]{1}));
        } catch (VecDbException e) {
            closedError = true;
        }
        check(closedError, "use after close throws VecDbException");
    }

    private static void threads() throws Exception {
        Path dir = tempDir();
        try (VecDb db = open(dir, "x.db", Options.builder().dim(2).build())) {
            db.add(0, RecordSpec.of(new float[]{0, 0}).textValue("seed"));
            Thread[] workers = new Thread[4];
            for (int t = 0; t < workers.length; t++) {
                final int tt = t;
                workers[t] = new Thread(() -> {
                    for (int i = 0; i < 25; i++) {
                        long id = tt * 100L + i + 1;
                        db.add(id, RecordSpec.of(new float[]{id, 1}));
                    }
                });
                workers[t].start();
            }
            for (Thread w : workers) {
                w.join();
            }
            eq(db.count(), 101L, "count after concurrent writes");
        }
    }
}
