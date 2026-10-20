# vecdb — Java bindings

Java bindings for [`libvecdb`](../..), an embedded vector + full-text
database, built on the **Foreign Function & Memory API**
(`java.lang.foreign`) — no JNI, no `jextract`, no dependencies.

```java
import vecdb.*;

try (VecDb db = VecDb.open("/tmp/notes.db",
        Options.builder().dim(3).metric(Metric.COSINE).build())) {

    db.add(1, RecordSpec.of(new float[]{1, 0, 0})
            .textValue("a cat sat on the mat"));
    db.add(2, RecordSpec.of(new float[]{0, 1, 0})
            .textValue("a dog barked at the postman"));

    for (Match hit : db.search(new float[]{0.9f, 0.1f, 0f}, 5)) {
        System.out.println(hit.score() + "  #" + hit.id() + "  " + hit.text());
    }

    for (Match hit : db.searchText("postman", 5)) {
        System.out.println(hit.id());
    }

    for (Match hit : db.searchHybrid(new float[]{0.9f, 0.1f, 0f}, "mat", 5, 0.5f)) {
        System.out.println(hit.id() + "  " + hit.score());
    }
}
```

## Requirements

* **JDK 21 or newer.** The FFM API is a preview feature on JDK 21, so compile
  and run with `--enable-preview`; also pass
  `--enable-native-access=ALL-UNNAMED` to silence the native-access warning.
  On a release where FFM is final the flags become unnecessary.
* **`libvecdb.so`** built with `make` in the repository root.

## Building and running

`run-tests.sh` compiles everything and runs the suite:

```sh
./run-tests.sh
```

By hand:

```sh
javac --release 21 --enable-preview -d build/classes $(find src -name '*.java')
java --enable-preview --enable-native-access=ALL-UNNAMED \
     -cp build/classes vecdb.VecDbTest
```

The loader finds `libvecdb` next to the bindings, in an ancestor of the
current working directory, on `java.library.path`, or wherever
`VECDB_LIBRARY` points.

## API

| type | purpose |
|---|---|
| `VecDb` | the database handle; `AutoCloseable`, cleaned up by a `Cleaner` if you forget |
| `Options` / `Options.Builder` | open-time config: `dim`, `metric`, `index`, HNSW tuning, `seed` |
| `Metric` | `L2`, `IP`, `COSINE` (with `parse("cosine")` and `label()`) |
| `Index` | `FLAT`, `HNSW` |
| `RecordSpec` | a record to store: vector, text, payload |
| `Record` | a full record from `get`: `id`, `vector`, `payload`, `rawPayload`, `text` |
| `Match` | a search hit: `id`, `score`, `payload`, `rawPayload`, `text` |
| `Payload` | the JSON-first `\x00VDB` payload codec |
| `VecDbException` | base error; `NotFoundException`, `AlreadyExistsException`, `DimensionMismatchException` |

### `VecDb`

* **Lifecycle** — `open(path[, options[, libraryPath]])`, `close`, `flush`,
  `compact`, `setEfSearch`, `isClosed`.
* **Introspection** — `path`, `count`, `capacity`, `dim`, `metric`, `index`,
  `ids`, `libraryPath`.
* **Writes** — `add` (upsert), `insert` (insert-only), `addAll`,
  `get`, `delete`, `deleteAll`.
* **Searches** — `search`, `searchText`, `searchHybrid`, and the `query`
  dispatcher that picks a modality from the arguments it is given.

`RecordSpec` is a small fluent builder:

```java
RecordSpec.of(embedding)
        .textValue("hello")
        .payload(Map.of("kind", "greeting"))
        .sync(true);
```

## Payloads

Payloads are opaque bytes in the C API. `Payload` gives them structure
without forcing a schema on other clients:

| Java value | stored as |
|---|---|
| `null` | no payload |
| `byte[]`, `ByteBuffer` | `b"\x00VDBb"` + raw bytes |
| `String`, `Boolean` | UTF-8 JSON |
| `Byte`/`Short`/`Integer`/`Long`/`BigInteger` | `b"\x00VDBi"` + 8 byte little-endian signed |
| `Float`, `Double` | `b"\x00VDBf"` + 8 byte little-endian IEEE-754 |
| `Map`, `List`, arrays | UTF-8 JSON |

`Payload.decode` recognises values written by this codec exactly. Java has no
built-in JSON parser, so a foreign JSON payload comes back as its **UTF-8
text** rather than a parsed tree, and other payloads fall back to fixed-width
numbers, then text, then raw bytes. Use `Match.rawPayload()` / `Record.rawPayload()`
when you need the exact bytes, or bring your own JSON library for full object
round-tripping.

This matches the Python, JavaScript, Go and Rust bindings byte for byte.

## Concurrency

`VecDb` wraps a single C handle. The library serialises writers with a rwlock,
so concurrent use of one handle is safe (see the `threads` test), but a single
handle is still a single serialisation point — open one handle per thread if
you want parallel writes.

## Tests

`./run-tests.sh` runs 77 checks covering basic vector search, BM25 text search,
hybrid blending, the payload codec, error mapping, dimension mismatch, batch
writes and `ids()`, HNSW persistence and reopen, close/default behaviour, and
concurrent writers.

From the repository root, `make javatest` does the same thing.
