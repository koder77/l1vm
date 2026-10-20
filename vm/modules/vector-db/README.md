# vecdb

Generated with the help of opencode AI.
The code is GPL licensed.

An embedded **vector + full-text database** for C, shipped as a shared
library (`libvecdb.so`). It stores arbitrary payloads alongside dense float
vectors and free text, and retrieves them three ways:

* **vector search** — exact (FLAT) or approximate (HNSW)
* **full-text search** — Okapi BM25 over an inverted index
* **hybrid search** — a weighted blend of both

It is **dependency-free**: libc and pthreads only, no external libraries, no
build system beyond `make`. Everything lives in a single file that is written
atomically, so a crash never corrupts an existing database. Think of it as a
small, embeddable Chroma/Milvus you can `dlopen` or link into a C, C++ or
Python program.

```
make            # builds libvecdb.so
make test       # C integration suite   (5500+ checks)
make fuzz       # corrupt-input robustness suite
make demo       # guided C tour
make bench      # micro-benchmarks
make cpp_demo   # C++ wrapper tour
make pytest     # Python bindings suite
make jstest     # JavaScript (Node) bindings suite
make gotest     # Go bindings suite
make rusttest   # Rust bindings suite
make javatest   # Java (JDK 21+ FFM) bindings suite
make cli        # builds the vecdb-cli shell tool
```

## Highlights

| | |
|---|---|
| Language | C11, POSIX + pthreads only |
| Artifact | `libvecdb.so` (X). Also builds a CLI, C++ header, Python/JS/Go/Rust/Java bindings |
| Storage | one self-describing file, atomic save (tmp + fsync + rename) |
| Metrics | L2, inner product, cosine |
| Indices | FLAT (exact) and HNSW (approximate) |
| Text | inverted index with BM25 ranking, stopword stripping, CJK-aware tokenizer |
| Concurrency | `pthread` rwlock; many readers or one writer; thread-local error strings |
| Data | opaque payload blobs — **any** bytes you like (typed codecs included) |
| Scores | *higher is better* everywhere (L2 reports negated distance) |

---

## 1. The C API

Include `vecdb.h` and link with `-lvecdb`.

```c
#include "vecdb.h"

vecdb_options opt;
vecdb_options_default(&opt);
opt.dim    = 384;
opt.metric = VECDB_METRIC_COSINE;
opt.index  = VECDB_INDEX_HNSW;

vecdb *db = vecdb_open("notes.db", &opt);
```

### Lifecycle

| function | purpose |
|---|---|
| `vecdb *vecdb_open(const char *path, const vecdb_options *opt)` | open or create; `opt` may be `NULL` for defaults |
| `int vecdb_close(vecdb *db)` | flush and close (safe on `NULL`, safe twice) |
| `int vecdb_flush(vecdb *db)` | write the file atomically |
| `int vecdb_compact(vecdb *db)` | drop tombstones and rebuild indices |

### Introspection

| function | returns |
|---|---|
| `vecdb_count` | live records |
| `vecdb_capacity` | slots in use, including tombstones |
| `vecdb_dim` | vector length (0 until the first vector is stored) |
| `vecdb_metric_of` / `vecdb_index_of` | the persisted enums |
| `vecdb_path` | the file path |
| `vecdb_list_ids(db, out, cap)` | up to `cap` live ids; pass `NULL,0` to count |
| `vecdb_set_ef_search(db, ef)` | HNSW query breadth (recall vs speed) |
| `vecdb_strerror(code)` / `vecdb_last_error(db)` | human readable errors |

### Writing

```c
float v[384] = { /* ... */ };
int rc = vecdb_put(db, /*id=*/42, v, 384,
                   /*payload=*/"any bytes", /*payload_len=*/9,
                   /*text=*/"a short article about databases");
```

* Any of vector / payload / text may be omitted (`NULL`).
* The **first** vector stored fixes `vecdb_dim()` unless `opt.dim` was set.
* `vecdb_put_ex(..., flags)` adds `VECDB_INSERT_ONLY` (fail if the id exists)
  and `VECDB_SYNC` (flush before returning).
* `vecdb_get(db, id, &rec)` fetches one record; pointers are borrowed until
  the next mutation.
* `vecdb_delete(db, id)` removes a record (a tombstone remains until compact).

### Searching

```c
vecdb_match hits[10];

/* nearest neighbours */
size_t n = vecdb_search_vector(db, query, 384, /*k=*/10, hits, 10);

/* BM25 keyword search */
n = vecdb_search_text(db, "full text search", 10, hits, 10);

/* hybrid: alpha weights the vector signal (1 = vector, 0 = text) */
n = vecdb_search_hybrid(db, query, 384, "databases", 10, /*alpha=*/0.6f, hits, 10);

for (size_t i = 0; i < n; i++)
    printf("%.4f  #%llu  %s\n", hits[i].score,
           (unsigned long long)hits[i].id,
           hits[i].text ? hits[i].text : "");
```

Each search writes at most `min(k, cap)` hits in descending score order and
returns how many it wrote.

### Status codes

```
VECDB_OK 0
VECDB_ERR_IO       -1   file read/write failure
VECDB_ERR_NOMEM    -2
VECDB_ERR_INVALID  -3   bad argument / malformed record
VECDB_ERR_NOTFOUND -4
VECDB_ERR_EXISTS   -5   INSERT_ONLY and the id already exists
VECDB_ERR_DIM      -6   vector dimension mismatch
VECDB_ERR_CORRUPT  -7   not a vecdb file
VECDB_ERR_NODIM    -8   vector op on a dimension-less database
```

### Payload helpers

The payload is an opaque blob, so the library ships little-endian fixed-width
codecs for the common cases:

```c
char buf[16];
vecdb_pack_i64(buf, 2024);                 /* 8 bytes */
vecdb_pack_f32(buf + 8, 0.5f);             /* 4 bytes */
int64_t year = vecdb_unpack_i64(buf);
```

`vecdb_pack_u64`, `vecdb_pack_f64`, `vecdb_unpack_u64`, `vecdb_unpack_f64`,
`vecdb_unpack_f32` and `vecdb_pack_str` complete the set.

---

## 2. Building and linking

```sh
make                     # -> libvecdb.so
cc my_app.c -Iinclude -L. -lvecdb -Wl,-rpath,'$ORIGIN'
```

`src/vecdb.map` is a linker version script: only the public API is exported,
so internal symbols can never collide with your program.

The header is C and C++ safe (`extern "C"` guards included).

---

## 3. C++ wrapper (`include/vecdb.hpp`)

Header-only, C++17, RAII handles and exceptions. Namespace `vdb`.

```cpp
#include "vecdb.hpp"

vdb::Database db("notes.db", {.dim = 3, .metric = vdb::Cosine, .index = vdb::Hnsw});

db.put(1, {1, 0, 0}, vdb::encode(std::string("animal")), "a cat sat on the mat");
db.put(2, {0, 1, 0}, vdb::encode(std::string("animal")), "a dog barked");

for (const auto &hit : db.search({1, 0, 0}, 5))
    std::cout << hit.id << " " << hit.score
              << " " << hit.as<std::string>() << "\n";   // typed payload
```

* `vdb::Database` flushes and closes itself on destruction, is move-only.
* `put`, `get`, `search`, `search_text`, `search_hybrid`, `erase`, `flush`,
  `compact`, `ids`, `size`, `dim`, `metric`, `index` mirror the C API but use
  `std::vector`, `std::string` and `std::string_view`.
* Errors are thrown as `vdb::Error` and subclasses (`NotFound`,
  `AlreadyExists`, `DimensionMismatch`, `CorruptDatabase`, `IoError`,
  `InvalidArgument`), each carrying `code()`.
* `vdb::encode(value)` / `record.as<T>()` give you a typed payload codec for
  strings, integers, floats and trivially-copyable structs.

See `examples/cpp_demo.cpp` (`make cpp_demo`).

---

## 4. Python bindings (`bindings/python/vecdb.py`)

Pure `ctypes` — no build step, no dependencies. The module finds
`libvecdb.so` next to itself, in any ancestor directory (so a plain `make` in
the repo root is enough), via `VECDB_LIBRARY`, or on the system loader path.

```python
from vecdb import VecDB

db = VecDB("notes.db", dim=3, metric="cosine", index="hnsw")
db.add(1, vector=[1, 0, 0], text="a cat sat on the mat", payload={"kind": "animal"})
db.add(2, vector=[0, 1, 0], text="a dog barked at the postman", payload={"kind": "animal"})

for hit in db.search([0.9, 0.1, 0], k=2):
    print(hit.score, hit.id, hit.text, hit.payload)

for hit in db.search_text("postman", k=1):
    print(hit.id)

for hit in db.search_hybrid(vector=[0.9, 0.1, 0], text="mat", k=2, alpha=0.5):
    print(hit.id, hit.score)

db.flush(); db.close()
```

* `VecDB(path, dim=0, metric="l2", index="flat", ...)`, usable as a context
  manager.
* `add / add_many / get / delete / delete_many / ids / search /
  search_vector / search_text / search_hybrid / query / flush / compact`.
* Payloads are encoded JSON-first, so dicts, lists, strings, bools and numbers
  round-trip; raw `bytes`, plain `int`s and `float`s use a tiny `\x00VDB`
  tag so they survive exactly. `Match.raw_payload` always gives the exact
  bytes. Data written by other clients still decodes on a best-effort basis.
* Errors map to `VecDBError`, `NotFoundError`, `AlreadyExistsError`,
  `DimensionMismatchError`.
* `Match` carries `id`, `score`, `payload`, `text`; `Record` adds `vector`.

Run the suite with `make pytest` (46 checks).

---

## 5. JavaScript bindings (`bindings/javascript/`)

Node's FFI via [koffi](https://koffi.dev). `npm install` pulls the single
dependency; the module finds `libvecdb.so` next to itself, in any ancestor
directory, or via `VECDB_LIBRARY`.

```js
const { VecDb } = require("vecdb");

const db = VecDb.open("notes.db", { dim: 3, metric: "cosine", index: "hnsw" });
db.put(1, [1, 0, 0], { kind: "animal" }, "a cat sat on the mat");
db.put(2, [0, 1, 0], { kind: "animal" }, "a dog barked at the postman");

for (const hit of db.search([0.9, 0.1, 0], 2)) {
  console.log(hit.score, hit.id, hit.text, hit.payload);
}
for (const hit of db.searchText("postman", 1)) console.log(hit.id);
for (const hit of db.searchHybrid([0.9, 0.1, 0], "mat", 2, 0.5)) console.log(hit.id);

db.flush();
db.close();
```

* `VecDb.open(path, { dim, metric, index, ... })`; also `using db = ...`
  (`Symbol.dispose`) and a `FinalizationRegistry` safety net.
* `put / putMany / get / delete / deleteMany / ids / search / searchVector /
  searchText / searchHybrid / flush / compact / setEfSearch`, plus `count`,
  `dim`, `metric`, `index`, `path`, `capacity`.
* Payloads use the same JSON-first `\x00VDB` codec as Python, so dicts,
  arrays, strings, bools, ints, floats and raw `bytes` round-trip; BigInt is
  handled for integer tags.
* Errors are `VecDbError` with `NotFoundError`, `AlreadyExistsError`,
  `DimensionMismatchError` subclasses.

Run the suite with `make jstest` (50 checks).

---

## 6. Go bindings (`bindings/go/`)

`cgo` over `libvecdb.so`; the cgo directives locate the library relative to
the package, so a plain `make` in the repo root is enough. Module path
`github.com/koder77/l1vm/vm/modules/vector-db/bindings/go`.

```go
db, err := vecdb.Open("notes.db", &vecdb.Options{Dim: 3, Metric: vecdb.MetricCosine})
if err != nil { log.Fatal(err) }
defer db.Close()

_ = db.Put(1, []float32{1, 0, 0}, []byte(`{"kind":"animal"}`), "a cat sat on the mat")
_ = db.Put(2, []float32{0, 1, 0}, []byte(`{"kind":"animal"}`), "a dog barked")

hits, _ := db.SearchVector([]float32{0.9, 0.1, 0}, 2)
for _, h := range hits { fmt.Println(h.Score, h.ID, h.Text) }
```

* `Open / Put / PutEx / Get / Delete / DeleteMany / ListIDs / SearchVector /
  SearchText / SearchHybrid / Flush / Compact / SetEfSearch / Close`.
* `EncodePayload` / `DecodePayload` mirror the Python `\x00VDB` payload codec;
  payloads may equally be raw `[]byte`.
* Errors are the `*Error` type; use `IsNotFound`, `IsAlreadyExists`,
  `IsDimensionMismatch`, `IsIO`, `IsInvalid`. After `Close` every method
  returns `ErrClosed`.

Run the suite with `make gotest` (9 tests).

---

## 7. Rust bindings (`bindings/rust/`)

A hand-written FFI declaration (`src/ffi.rs`) with no dependencies — not even
`bindgen` or `serde`. `build.rs` locates `libvecdb.so` relative to the crate
or via `VECDB_LIB_DIR` and bakes the rpath, so `cargo test` just works after
`make`.

```rust
use vecdb::{Metric, Options, VecDb};

let db = VecDb::open_with("notes.db", &Options {
    dim: 3,
    metric: Metric::Cosine,
    ..Options::default()
})?;

db.put(1, &[1.0, 0.0, 0.0], br#"{"kind":"animal"}"#, "a cat sat on the mat")?;
db.put(2, &[0.0, 1.0, 0.0], br#"{"kind":"animal"}"#, "a dog barked")?;

for hit in db.search_vector(&[0.9, 0.1, 0.0], 2)? {
    println!("{:.4}  #{}  {}", hit.score, hit.id, hit.text.unwrap_or_default());
}
```

* `VecDb::open / open_with`, `put / put_with_flags / get / delete /
  delete_many / list_ids / search_vector / search_text / search_hybrid /
  flush / compact / close / set_ef_search`, plus `count`, `capacity`, `dim`,
  `metric`, `index`, `path`.
* `Metric`, `Index`, `Options`, `PutFlags` (`|`-combinable), `Record`,
  `Match`.
* Payloads are opaque `Vec<u8>` exactly as in C; anything that serialises to
  bytes works (bring `serde_json` for JSON payloads).
* `Error` carries the raw status plus a message and offers
  `is_not_found / is_already_exists / is_dimension_mismatch / is_io /
  is_invalid`.
* `VecDb` is `Send + Sync`, so a handle can be shared across threads (the C
  library serialises writers).

Run the suite with `make rusttest` (10 integration tests + a doc-test).

---

## 8. Java bindings (`bindings/java/`)

Built on the **Foreign Function & Memory API** (`java.lang.foreign`) — no JNI,
no `jextract`, no dependencies. Requires a JDK 21+ (the FFM API is a preview
feature there, hence `--enable-preview`; run with
`--enable-native-access=ALL-UNNAMED`).

```java
import vecdb.*;

try (VecDb db = VecDb.open("notes.db",
        Options.builder().dim(3).metric(Metric.COSINE).build())) {
    db.add(1, RecordSpec.of(new float[]{1, 0, 0}).textValue("a cat sat on the mat"));
    db.add(2, RecordSpec.of(new float[]{0, 1, 0}).textValue("a dog barked"));

    for (Match hit : db.search(new float[]{0.9f, 0.1f, 0f}, 5)) {
        System.out.println(hit.score() + "  #" + hit.id() + "  " + hit.text());
    }
}
```

* `VecDb` is `AutoCloseable` (backed by a `Cleaner`); `open / close / flush /
  compact / setEfSearch / isClosed`.
* `add / insert / addAll / get / delete / deleteAll / ids / search /
  searchText / searchHybrid / query`, plus `path`, `count`, `capacity`, `dim`,
  `metric`, `index`, `libraryPath`.
* `Options.Builder` for open-time config; `RecordSpec` is a fluent record
  builder; `Match` and `Record` carry the decoded payload plus the exact
  `rawPayload()` bytes.
* Errors map to `VecDbException` with `NotFoundException`,
  `AlreadyExistsException`, `DimensionMismatchException`.
* Payloads use the same JSON-first `\x00VDB` codec as the other bindings;
  since Java has no built-in JSON parser, a foreign JSON payload decodes to
  its UTF-8 text — use `rawPayload()` for exact bytes.

Run the suite with `make javatest` (77 checks), or `bindings/java/run-tests.sh`.

---

## 9. Command line tool (`vecdb-cli`)

```
vecdb-cli create  notes.db --dim 3 --metric cosine --index hnsw
vecdb-cli put     notes.db --id 1 --vector "1,0,0" --text "a cat" --payload "animal"
vecdb-cli put     notes.db --vector "0,1,0" --text "a dog"          # auto id
vecdb-cli search  notes.db --vector "0.9,0.1,0" --k 2 --json
vecdb-cli text    notes.db --text "dog" --k 2
vecdb-cli hybrid  notes.db --vector "0.9,0.1,0" --text "cat" --alpha 0.5
vecdb-cli get     notes.db 1 --json
vecdb-cli info    notes.db
vecdb-cli list    notes.db --json
vecdb-cli dump    notes.db --json
vecdb-cli del     notes.db 1
vecdb-cli compact notes.db
```

Vectors use comma- or space-separated floats. `--payload-file` reads a payload
blob from disk. `--json` emits machine-readable output (payloads as byte
arrays), which makes piping into `jq` easy.

---

## 10. File format

All integers are little-endian. A file starts with a fixed 32-byte header:

```
offset  size  field
0       8     magic          "VECDB001"
8       4     version        1
12      4     dim            vector length, 0 = not set
16      4     metric         vecdb_metric
20      4     index          vecdb_index
24      8     count          number of live records
32      ...   records
```

Each record:

```
uint64  id
uint32  flags       bit0 vector, bit1 payload, bit2 text
float   vec[dim]    present when bit0
uint32  payload_len ; payload_len bytes   present when bit1
uint32  text_len    ; text_len bytes      present when bit2
```

* The `metric` and `index` in the file win over the values passed to
  `vecdb_open` for an existing database.
* Deleted records are kept as tombstones until `vecdb_compact()`.
* Saving writes `<path>.tmp`, `fsync`s it, then `rename()`s over `<path>`,
  which is atomic on POSIX filesystems.

---

## 11. How it works

* **Storage** — a growable array of records plus an open-addressing id → slot
  map, guarded by a `pthread_rwlock`. Error strings are thread-local so
  concurrent operations never stomp each other.
* **FLAT index** — a brute-force scan with an optional SIMD-friendly
  `vecdb_sim_rank` kernel. Exact, always correct, the default.
* **HNSW** — a hierarchical navigable small-world graph for approximate
  nearest neighbours. Defaults `M=16`, `efConstruction=100`, `efSearch=100`;
  raise `efConstruction` for hard datasets. Recall@10 is measured at 100% on
  the test suite.
* **Text index** — an inverted index with contiguous, slot-sorted posting
  lists. The tokenizer lowercases ASCII, drops stopwords and 1-character
  noise, and indexes CJK / Korean / Hiragana codepoints one token per
  character. Ranking is Okapi BM25 with `k1 = 1.2`, `b = 0.75`.
* **Hybrid** — min-max normalises the vector and text candidate scores, then
  blends them as `alpha * vector + (1 - alpha) * text`.

---

## 12. Testing

| suite | command | scope |
|---|---|---|
| C integration | `make test` | 5507 checks across 8 groups |
| robustness / fuzz | `make fuzz` | 1192 truncations + byte corruption at every offset |
| C++ wrapper | `make cpp_demo` | RAII, typed payloads, exceptions |
| Python | `make pytest` | 46 checks |
| JavaScript | `make jstest` | 50 checks |
| Go | `make gotest` | 9 tests |
| Rust | `make rusttest` | 10 integration tests + doc-test |
| Java | `make javatest` | 77 checks |
| all bindings | `make bindings-test` | pytest + jstest + gotest + rusttest + javatest |
| demo | `make demo` | guided tour |
| benchmarks | `make bench` | insert / query / text / hybrid throughput |

The C suites build and run cleanly under `-fsanitize=address,undefined`
(with leak detection) and `-fsanitize=thread`.

---

## 13. Repository layout

```
include/vecdb.h        public C API
include/vecdb.hpp      header-only C++ wrapper
src/db.c               storage, id map, persistence, public API
src/text.c             tokenizer + BM25 inverted index
src/hnsw.c             HNSW graph index
src/internal.h         shared internal declarations
src/vecdb.map          linker version script (exported symbols)
tests/test_vecdb.c     C integration tests
tests/fuzz.c           corrupt-file / bad-argument robustness tests
examples/demo.c        C guided tour
examples/cpp_demo.cpp  C++ wrapper tour
examples/bench.c       benchmarks
bindings/python/       ctypes bindings + test
bindings/javascript/   koffi (Node FFI) bindings + test
bindings/go/           cgo bindings + test
bindings/rust/         zero-dependency Rust crate + test
bindings/java/         JDK 21+ FFM bindings + test
tools/vecdb-cli.c      command line tool
```

## 14. License

MIT.
