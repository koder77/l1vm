'use strict';
/**
 * vecdb — a Chroma/Milvus-style embedded vector + full-text database.
 *
 * A thin koffi wrapper around the `libvecdb` C library. It gives you an
 * embedded database that stores arbitrary payloads alongside dense vectors
 * and free text, and retrieves them by:
 *
 *   * nearest-neighbour **vector search**
 *   * **full-text search** (BM25)
 *   * **hybrid search** that blends both
 *
 * Quick start
 * -----------
 *
 *     const { VecDB } = require('vecdb');
 *
 *     const db = new VecDB('/tmp/notes.db', { dim: 3, metric: 'cosine' });
 *     db.add(1, { vector: [1, 0, 0], text: 'a cat sat on the mat',
 *                 payload: { kind: 'animal' } });
 *     db.add(2, { vector: [0, 1, 0], text: 'a dog barked at the postman',
 *                 payload: { kind: 'animal' } });
 *
 *     for (const hit of db.search([0.9, 0.1, 0], 2))
 *         console.log(hit.score, hit.id, hit.text, hit.payload);
 *
 *     for (const hit of db.searchText('postman', 1))
 *         console.log(hit.id);
 *
 *     db.close();
 *
 * The module finds `libvecdb.so` next to itself, in any ancestor directory
 * (so a plain `make` in the repository root is enough), via
 * `VECDB_LIBRARY`, or on the system loader path.
 */

const fs = require('fs');
const path = require('path');
const koffi = require('koffi');

const VERSION = '1.0.0';

// --------------------------------------------------------------------------
// status codes (must match vecdb.h)
// --------------------------------------------------------------------------
const VECDB_OK = 0;
const VECDB_ERR_IO = -1;
const VECDB_ERR_NOMEM = -2;
const VECDB_ERR_INVALID = -3;
const VECDB_ERR_NOTFOUND = -4;
const VECDB_ERR_EXISTS = -5;
const VECDB_ERR_DIM = -6;
const VECDB_ERR_CORRUPT = -7;
const VECDB_ERR_NODIM = -8;

const METRICS = Object.freeze({ l2: 0, ip: 1, cosine: 2 });
const INDEXES = Object.freeze({ flat: 0, hnsw: 1 });

const INSERT_ONLY = 1 << 0;
const SYNC = 1 << 1;

// --------------------------------------------------------------------------
// errors
// --------------------------------------------------------------------------
class VecDBError extends Error {
    /** Base class for every error thrown by this module. */
    constructor(message, code) {
        super(message);
        this.name = 'VecDBError';
        this.code = code === undefined ? null : code;
    }
}

class NotFoundError extends VecDBError {
    /** The requested id does not exist. */
}

class AlreadyExistsError extends VecDBError {
    /** An insert-only write hit an existing id. */
}

class DimensionMismatchError extends VecDBError {
    /** A vector had the wrong length. */
}

const ERROR_TYPES = Object.freeze({
    [VECDB_ERR_NOTFOUND]: NotFoundError,
    [VECDB_ERR_EXISTS]: AlreadyExistsError,
    [VECDB_ERR_DIM]: DimensionMismatchError,
    [VECDB_ERR_NODIM]: DimensionMismatchError,
});

// --------------------------------------------------------------------------
// payload codec — JSON first, with a tiny \x00VDB tag for the two cases
// JSON cannot round-trip losslessly: raw bytes and numbers that must keep
// their exact 64-bit width. Mirrors the Python bindings.
// --------------------------------------------------------------------------
const TAG = Buffer.from([0x00, 0x56, 0x44, 0x42]); // "\0VDB"
const BLOB_TAG = Buffer.concat([TAG, Buffer.from('b')]);
const INT_TAG = Buffer.concat([TAG, Buffer.from('i')]);
const FLOAT_TAG = Buffer.concat([TAG, Buffer.from('f')]);

const INT64_MIN = -(2n ** 63n);
const INT64_MAX = 2n ** 63n - 1n;
const SAFE_MAX = BigInt(Number.MAX_SAFE_INTEGER);
const SAFE_MIN = -SAFE_MAX;

function bigToNum(v) {
    return v >= SAFE_MIN && v <= SAFE_MAX ? Number(v) : v;
}

function asBuffer(payload) {
    if (Buffer.isBuffer(payload)) return payload;
    if (payload instanceof Uint8Array) {
        return Buffer.from(payload.buffer, payload.byteOffset, payload.byteLength);
    }
    if (payload instanceof ArrayBuffer) return Buffer.from(payload);
    throw new TypeError('payload bytes must be a Buffer, Uint8Array or ArrayBuffer');
}

/**
 * Turn a JS value into the bytes stored in the payload blob.
 *
 *   * null / undefined   -> no payload
 *   * Buffer/Uint8Array  -> b"\x00VDBb" + raw bytes
 *   * string / boolean   -> UTF-8 JSON
 *   * integer number     -> b"\x00VDBi" + 8 byte little-endian signed
 *   * other number       -> b"\x00VDBf" + 8 byte little-endian IEEE-754
 *   * bigint             -> b"\x00VDBi" + 8 byte little-endian signed
 *   * object / array     -> UTF-8 JSON
 *
 * Throws when the value cannot be represented (functions, symbols,
 * circular or BigInt-carrying structures).
 */
function encodePayload(value) {
    if (value === null || value === undefined) return null;
    if (value instanceof Uint8Array || value instanceof ArrayBuffer) {
        return Buffer.concat([BLOB_TAG, asBuffer(value)]);
    }
    if (typeof value === 'string' || typeof value === 'boolean') {
        return Buffer.from(JSON.stringify(value), 'utf8');
    }
    if (typeof value === 'number') {
        if (Number.isInteger(value)) {
            const v = BigInt(value);
            if (v >= INT64_MIN && v <= INT64_MAX) return tagInt(v);
        }
        return tagFloat(value);
    }
    if (typeof value === 'bigint') {
        if (value < INT64_MIN || value > INT64_MAX) {
            throw new RangeError('bigint payload does not fit in a signed 64-bit integer');
        }
        return tagInt(value);
    }
    const json = JSON.stringify(value);
    if (json === undefined) {
        throw new TypeError(`cannot encode payload of type ${typeof value}`);
    }
    return Buffer.from(json, 'utf8');
}

function tagInt(v) {
    const b = Buffer.allocUnsafe(8);
    b.writeBigInt64LE(v, 0);
    return Buffer.concat([INT_TAG, b]);
}

function tagFloat(v) {
    const b = Buffer.allocUnsafe(8);
    b.writeDoubleLE(v, 0);
    return Buffer.concat([FLOAT_TAG, b]);
}

/**
 * Best-effort inverse of {@link encodePayload}. The payload blob is
 * untyped, so this guesses for data written by other clients: tagged
 * values are recognised exactly, foreign payloads fall back to JSON, then
 * to fixed-width numbers, then to text, then to raw bytes. Use
 * `Match.rawPayload` / `Record.rawPayload` for the exact bytes.
 *
 * Returns `null` for an absent payload and a `Uint8Array` for opaque
 * binary data.
 */
function decodePayload(raw) {
    if (raw === null || raw === undefined) return null;
    const buf = asBuffer(raw);
    if (buf.length === 0) return new Uint8Array(0);

    if (buf.length >= TAG.length && buf.subarray(0, TAG.length).equals(TAG)) {
        const kind = buf[TAG.length];
        if (kind === 0x62 /* b */) {
            return new Uint8Array(buf.subarray(BLOB_TAG.length));
        }
        if (kind === 0x69 /* i */ && buf.length === INT_TAG.length + 8) {
            return bigToNum(buf.readBigInt64LE(INT_TAG.length));
        }
        if (kind === 0x66 /* f */ && buf.length === FLOAT_TAG.length + 8) {
            return buf.readDoubleLE(FLOAT_TAG.length);
        }
    }

    let text = null;
    try {
        text = new TextDecoder('utf-8', { fatal: true }).decode(buf);
    } catch {
        text = null;
    }

    if (text !== null) {
        const stripped = text.trimStart();
        if ('{["'.includes(stripped[0]) || stripped === 'true' || stripped === 'false' || stripped === 'null') {
            try {
                return JSON.parse(text);
            } catch {
                /* fall through */
            }
        }
    }
    if (buf.length === 8) return bigToNum(buf.readBigInt64LE(0));
    if (buf.length === 4) return buf.readFloatLE(0);
    if (text !== null) return text;
    return new Uint8Array(buf);
}

// --------------------------------------------------------------------------
// library loading
// --------------------------------------------------------------------------

/**
 * Locate libvecdb. Search order: an explicit `path`, the VECDB_LIBRARY
 * environment variable, this module's directory and each of its ancestors
 * (so `make` in the repository root is found), then the system loader.
 */
function findLibrary(explicit) {
    if (explicit) {
        if (!fs.existsSync(explicit)) {
            throw new VecDBError(`library not found: ${explicit}`);
        }
        return path.resolve(explicit);
    }
    const candidates = [];
    if (process.env.VECDB_LIBRARY) candidates.push(process.env.VECDB_LIBRARY);
    const names = ['libvecdb.so', 'libvecdb.dylib', 'vecdb.dll'];
    let dir = __dirname;
    for (let i = 0; i < 5; i++) {
        for (const name of names) candidates.push(path.join(dir, name));
        const parent = path.dirname(dir);
        if (parent === dir) break;
        dir = parent;
    }
    for (const cand of candidates) {
        if (cand && fs.existsSync(cand)) return path.resolve(cand);
    }
    return 'libvecdb.so'; // let the system loader have a go
}

// --------------------------------------------------------------------------
// FFI types — declared once, globally, they do not depend on the library
// --------------------------------------------------------------------------
const tVecdb = koffi.opaque('vecdb');
const tOptions = koffi.struct('vecdb_options', {
    dim: 'size_t',
    metric: 'int',
    index: 'int',
    hnsw_m: 'size_t',
    hnsw_ef_construction: 'size_t',
    hnsw_ef_search: 'size_t',
    seed: 'uint64_t',
});
const tRecord = koffi.struct('vecdb_record', {
    id: 'uint64_t',
    vec: 'const float *',
    payload: 'const void *',
    payload_len: 'size_t',
    text: 'const char *',
});
const tMatch = koffi.struct('vecdb_match', {
    id: 'uint64_t',
    score: 'float',
    payload: 'const void *',
    payload_len: 'size_t',
    text: 'const char *',
});
const MATCH_SIZE = koffi.sizeof(tMatch);

function bind(lib) {
    return {
        options_default: lib.func('void vecdb_options_default(_Inout_ vecdb_options *opt)'),
        open: lib.func('vecdb *vecdb_open(const char *path, const vecdb_options *opt)'),
        close: lib.func('int vecdb_close(vecdb *db)'),
        flush: lib.func('int vecdb_flush(vecdb *db)'),
        compact: lib.func('int vecdb_compact(vecdb *db)'),

        count: lib.func('size_t vecdb_count(const vecdb *db)'),
        capacity: lib.func('size_t vecdb_capacity(const vecdb *db)'),
        dim: lib.func('size_t vecdb_dim(const vecdb *db)'),
        metric_of: lib.func('int vecdb_metric_of(const vecdb *db)'),
        index_of: lib.func('int vecdb_index_of(const vecdb *db)'),
        path_of: lib.func('const char *vecdb_path(const vecdb *db)'),
        set_ef_search: lib.func('int vecdb_set_ef_search(vecdb *db, size_t ef)'),
        strerror: lib.func('const char *vecdb_strerror(int status)'),
        last_error: lib.func('const char *vecdb_last_error(const vecdb *db)'),
        list_ids: lib.func('size_t vecdb_list_ids(vecdb *db, uint64_t *out, size_t cap)'),

        put_ex: lib.func('int vecdb_put_ex(vecdb *db, uint64_t id, const float *vec, '
            + 'size_t dim, const void *payload, size_t payload_len, '
            + 'const char *text, unsigned int flags)'),
        get: lib.func('int vecdb_get(vecdb *db, uint64_t id, _Out_ vecdb_record *out)'),
        delete: lib.func('int vecdb_delete(vecdb *db, uint64_t id)'),

        search_vector: lib.func('size_t vecdb_search_vector(vecdb *db, const float *query, '
            + 'size_t dim, size_t k, vecdb_match *out, size_t cap)'),
        search_text: lib.func('size_t vecdb_search_text(vecdb *db, const char *query, '
            + 'size_t k, vecdb_match *out, size_t cap)'),
        search_hybrid: lib.func('size_t vecdb_search_hybrid(vecdb *db, const float *vec_query, '
            + 'size_t dim, const char *text_query, size_t k, float alpha, '
            + 'vecdb_match *out, size_t cap)'),
    };
}

const _apiCache = new Map();

function loadApi(libraryPath) {
    const resolved = findLibrary(libraryPath);
    if (_apiCache.has(resolved)) return _apiCache.get(resolved);
    let lib;
    try {
        lib = koffi.load(resolved);
    } catch (err) {
        throw new VecDBError(
            `could not load libvecdb (${resolved}): ${err.message}. `
            + 'Build it with `make` and either place it next to this module, '
            + 'add its directory to LD_LIBRARY_PATH, or set VECDB_LIBRARY.',
        );
    }
    const api = bind(lib);
    _apiCache.set(resolved, api);
    return api;
}

// --------------------------------------------------------------------------
// result objects
// --------------------------------------------------------------------------

class Match {
    /**
     * One search hit.
     *
     * @property {number} id       the record id
     * @property {number} score    similarity / BM25 / blended score,
     *                             higher is better
     * @property {*} payload       the decoded payload (as passed to `add`)
     * @property {string|null} text the stored text, or null
     * @property {Uint8Array|null} rawPayload the exact payload bytes
     */
    constructor(id, score, payload, text, rawPayload) {
        this.id = id;
        this.score = score;
        this.payload = payload;
        this.text = text;
        this.rawPayload = rawPayload;
    }

    toString() {
        return `Match(id=${this.id}, score=${this.score.toFixed(4)}, `
            + `text=${JSON.stringify(this.text)}, payload=${safeJson(this.payload)})`;
    }
}

class Record {
    /**
     * A full record as returned by {@link VecDB#get}.
     *
     * @property {number} id       the record id
     * @property {number[]|null} vector   the stored vector, or null
     * @property {*} payload       the decoded payload
     * @property {string|null} text       the stored text, or null
     * @property {Uint8Array|null} rawPayload the exact payload bytes
     */
    constructor(id, vector, payload, text, rawPayload) {
        this.id = id;
        this.vector = vector;
        this.payload = payload;
        this.text = text;
        this.rawPayload = rawPayload;
    }

    toString() {
        const v = this.vector === null ? 'null' : `<${this.vector.length} floats>`;
        return `Record(id=${this.id}, vector=${v}, `
            + `text=${JSON.stringify(this.text)}, payload=${safeJson(this.payload)})`;
    }
}

function safeJson(value) {
    try {
        return JSON.stringify(value, (_k, v) => (typeof v === 'bigint' ? String(v) : v));
    } catch {
        return String(value);
    }
}

// Nicer console.log(new Match(...)) output.
const inspectCustom = Symbol.for('nodejs.util.inspect.custom');
Match.prototype[inspectCustom] = function () { return this.toString(); };
Record.prototype[inspectCustom] = function () { return this.toString(); };

// --------------------------------------------------------------------------
// helpers
// --------------------------------------------------------------------------
function toNum(v) {
    return typeof v === 'bigint' ? Number(v) : v;
}

function textOrNull(v) {
    if (v === null || v === undefined || v === 0n || v === 0) return null;
    if (typeof v === 'string') return v;
    return koffi.decode(v, 'const char *') || null;
}

function readBytes(ptr, len) {
    if (ptr === null || ptr === undefined || ptr === 0n || ptr === 0) return null;
    const n = toNum(len) || 0;
    if (n <= 0) return null;
    const view = new Uint8Array(koffi.view(ptr, n));
    return new Uint8Array(view); // copy: the pointer is only borrowed
}

function vectorArg(vector) {
    if (vector === null || vector === undefined) return { vec: null, dim: 0 };
    if (typeof vector === 'string' || vector instanceof Uint8Array || vector instanceof ArrayBuffer) {
        throw new TypeError('vector must be an array of numbers, not bytes');
    }
    const seq = Array.from(vector); // throws on non-iterables
    for (const v of seq) {
        if (typeof v !== 'number') throw new TypeError('vector elements must be numbers');
    }
    if (seq.length === 0) throw new TypeError('vector must not be empty');
    return { vec: Float32Array.from(seq), dim: seq.length };
}

function decodeMatches(buf, n) {
    if (!n) return [];
    const hits = koffi.decode(buf, tMatch, n);
    return hits.map((m) => {
        const raw = readBytes(m.payload, m.payload_len);
        return new Match(toNum(m.id), m.score, decodePayload(raw), textOrNull(m.text), raw);
    });
}

// Close handles the user forgot about (mirrors __del__ in the Python
// bindings). The callback only ever sees plain data, never the object.
const _finalizer = typeof FinalizationRegistry === 'function'
    ? new FinalizationRegistry(({ api, handle }) => {
        try {
            api.close(handle);
        } catch {
            /* nothing sensible to do during GC */
        }
    })
    : null;

// --------------------------------------------------------------------------
// the database
// --------------------------------------------------------------------------

class VecDB {
    /**
     * An embedded vector + full-text database.
     *
     * Synchronous, single-threaded use — the C library serialises writers
     * with a rwlock, so open one handle per thread if you write
     * concurrently.
     *
     * @example
     * const db = new VecDB('notes.db', { dim: 384, metric: 'cosine', index: 'hnsw' });
     * db.add(42, { vector: embedding, text: 'hello', payload: { k: 1 } });
     * const hits = db.search(query, 5);
     *
     * @param {string} filePath  database file (created if missing)
     * @param {object} [options]
     * @param {number} [options.dim=0]  vector length; 0 = learn from the
     *   first stored vector
     * @param {'l2'|'ip'|'cosine'} [options.metric='l2']
     * @param {'flat'|'hnsw'} [options.index='flat']
     * @param {number} [options.hnswM]         HNSW M (library default 16)
     * @param {number} [options.hnswEfConstruction]  (default 100)
     * @param {number} [options.hnswEfSearch]        (default 100)
     * @param {number|bigint} [options.seed]  RNG seed for HNSW (default 42)
     * @param {string} [options.library]      explicit libvecdb path
     */
    constructor(filePath, options = {}) {
        if (typeof filePath !== 'string' || filePath.length === 0) {
            throw new TypeError('path must be a non-empty string');
        }
        const metric = String(options.metric === undefined ? 'l2' : options.metric).toLowerCase();
        const index = String(options.index === undefined ? 'flat' : options.index).toLowerCase();
        if (!(metric in METRICS)) {
            throw new RangeError(`metric must be one of ${Object.keys(METRICS).join(', ')}`);
        }
        if (!(index in INDEXES)) {
            throw new RangeError(`index must be one of ${Object.keys(INDEXES).join(', ')}`);
        }
        const dim = options.dim === undefined ? 0 : options.dim;
        if (!Number.isInteger(dim) || dim < 0) {
            throw new RangeError('dim must be a non-negative integer');
        }

        const api = loadApi(options.library);
        const opt = {};
        api.options_default(opt);
        opt.dim = dim;
        opt.metric = METRICS[metric];
        opt.index = INDEXES[index];
        const hnswM = pick(options, 'hnswM', 'hnsw_m');
        const hnswEfConstruction = pick(options, 'hnswEfConstruction', 'hnsw_ef_construction');
        const hnswEfSearch = pick(options, 'hnswEfSearch', 'hnsw_ef_search');
        if (hnswM !== undefined) opt.hnsw_m = hnswM;
        if (hnswEfConstruction !== undefined) opt.hnsw_ef_construction = hnswEfConstruction;
        if (hnswEfSearch !== undefined) opt.hnsw_ef_search = hnswEfSearch;
        if (options.seed !== undefined) opt.seed = options.seed;

        const handle = api.open(filePath, opt);
        if (!handle) {
            throw new VecDBError(
                `could not open ${JSON.stringify(filePath)} `
                + '(missing, corrupt, or dimension mismatch)',
            );
        }

        this._api = api;
        this._handle = handle;
        if (_finalizer) _finalizer.register(this, { api, handle }, this);
    }

    // -- lifecycle ---------------------------------------------------------

    /** Flush and close. Safe to call more than once. */
    close() {
        if (!this._handle) return;
        const handle = this._handle;
        this._handle = null;
        if (_finalizer) _finalizer.unregister(this);
        this._api.close(handle);
    }

    /** Persist the database atomically (write to tmp, fsync, rename). */
    flush() {
        this._check(this._api.flush(this._require()));
    }

    /** Drop deleted records and rebuild the indexes. */
    compact() {
        this._check(this._api.compact(this._require()));
    }

    /** HNSW search breadth: larger means better recall, slower queries. */
    setEfSearch(ef) {
        this._check(this._api.set_ef_search(this._require(), ef));
    }

    // -- introspection -----------------------------------------------------

    /** The database file path. */
    get path() {
        return this._str(this._api.path_of(this._require()));
    }

    /** Number of live records. */
    count() {
        return this._api.count(this._require());
    }

    /** Slots in use, including tombstones from deleted records. */
    capacity() {
        return this._api.capacity(this._require());
    }

    /** Vector length, or 0 if no vector has been stored yet. */
    get dim() {
        return this._api.dim(this._require());
    }

    /** One of `'l2'`, `'ip'`, `'cosine'`. */
    get metric() {
        const v = this._api.metric_of(this._require());
        return Object.keys(METRICS).find((k) => METRICS[k] === v) || 'l2';
    }

    /** One of `'flat'`, `'hnsw'`. */
    get index() {
        const v = this._api.index_of(this._require());
        return Object.keys(INDEXES).find((k) => INDEXES[k] === v) || 'flat';
    }

    /** Every live id, in unspecified order. */
    ids() {
        const h = this._require();
        const n = this._api.list_ids(h, null, 0);
        if (n === 0) return [];
        const buf = Buffer.allocUnsafe(n * 8);
        const got = this._api.list_ids(h, buf, n);
        const out = [];
        for (let i = 0; i < got; i++) out.push(Number(buf.readBigUInt64LE(i * 8)));
        return out;
    }

    // -- writes ------------------------------------------------------------

    /**
     * Store a record (upsert).
     *
     * Any of `vector`, `text` and `payload` may be omitted. The first
     * vector stored fixes the database dimension unless `options.dim` was
     * set at open time. With `upsert: false` an existing id throws
     * {@link AlreadyExistsError}.
     *
     * @param {number} id
     * @param {object} [record]
     * @param {number[]} [record.vector]
     * @param {string} [record.text]
     * @param {*} [record.payload]  any JSON-safe value, or raw bytes
     * @param {boolean} [record.upsert=true]
     * @param {boolean} [record.sync=false]  flush before returning
     */
    add(id, record = {}) {
        const upsert = record.upsert !== false;
        const flags = (upsert ? 0 : INSERT_ONLY) | (record.sync ? SYNC : 0);
        this._put(id, record, flags);
    }

    /**
     * Store several records, returning how many were written.
     *
     * `records` is an iterable of `{ id, vector, text, payload }`. When
     * `sync` is set, a single flush happens after the last record.
     *
     * @param {Iterable<object>} records
     * @param {object} [options]
     * @param {boolean} [options.upsert=true]
     * @param {boolean} [options.sync=false]
     * @returns {number}
     */
    addMany(records, options = {}) {
        const items = Array.isArray(records) ? records : Array.from(records);
        const upsert = options.upsert !== false;
        const flags = upsert ? 0 : INSERT_ONLY;
        let written = 0;
        for (let i = 0; i < items.length; i++) {
            const last = options.sync && i === items.length - 1;
            const { id, ...rest } = items[i];
            this._put(id, rest, flags | (last ? SYNC : 0));
            written++;
        }
        return written;
    }

    /**
     * Fetch one record. Throws {@link NotFoundError} if absent.
     *
     * @returns {Record}
     */
    get(id) {
        const rec = {};
        this._check(this._api.get(this._require(), id, rec));
        const dim = this.dim;
        let vector = null;
        if (rec.vec && rec.vec !== 0n && dim > 0) {
            vector = Array.from(koffi.decode(rec.vec, 'float', dim));
        }
        const raw = readBytes(rec.payload, rec.payload_len);
        return new Record(toNum(rec.id), vector, decodePayload(raw),
            textOrNull(rec.text), raw);
    }

    /**
     * Remove a record. Throws {@link NotFoundError} if absent.
     */
    delete(id) {
        this._check(this._api.delete(this._require(), id));
    }

    /**
     * Remove several records, ignoring ids that do not exist.
     *
     * @returns {number} how many records were actually removed
     */
    deleteMany(ids) {
        const h = this._require();
        let removed = 0;
        for (const id of ids) {
            if (this._api.delete(h, id) === VECDB_OK) removed++;
        }
        return removed;
    }

    // -- searches ----------------------------------------------------------

    /**
     * k nearest neighbours by vector similarity.
     *
     * Scores are higher-is-better: for `l2` an exact match scores 0.0
     * (it is the negated distance), for `ip` it is the dot product and
     * for `cosine` the cosine similarity in [-1, 1].
     *
     * @param {number[]} vector
     * @param {number} [k=10]
     * @returns {Match[]}
     */
    search(vector, k = 10) {
        return this.searchVector(vector, k);
    }

    /** k nearest neighbours by vector similarity. @see search */
    searchVector(vector, k = 10) {
        const h = this._require();
        const { vec, dim } = vectorArg(vector);
        if (!vec) throw new TypeError('a vector is required for vector search');
        if (k <= 0) return [];
        const buf = Buffer.allocUnsafe(k * MATCH_SIZE);
        const n = this._api.search_vector(h, vec, dim, k, buf, k);
        if (n === 0) this._raiseDimensionError();
        return decodeMatches(buf, n);
    }

    /**
     * BM25 full-text search.
     *
     * @param {string} query
     * @param {number} [k=10]
     * @returns {Match[]}
     */
    searchText(query, k = 10) {
        const h = this._require();
        if (k <= 0 || !query) return [];
        const buf = Buffer.allocUnsafe(k * MATCH_SIZE);
        const n = this._api.search_text(h, query, k, buf, k);
        return decodeMatches(buf, n);
    }

    /**
     * Blend vector and text results.
     *
     * `alpha` weights the vector signal: 1 is pure vector search, 0 pure
     * text search. Either query may be omitted.
     *
     * @param {number[]|null} vector
     * @param {string|null} text
     * @param {number} [k=10]
     * @param {number} [alpha=0.5]
     * @returns {Match[]}
     */
    searchHybrid(vector, text, k = 10, alpha = 0.5) {
        const h = this._require();
        if (k <= 0 || ((vector === null || vector === undefined) && !text)) return [];
        const { vec, dim } = vectorArg(vector);
        const buf = Buffer.allocUnsafe(k * MATCH_SIZE);
        const n = this._api.search_hybrid(h, vec, dim, text || null, k, alpha, buf, k);
        if (n === 0) this._raiseDimensionError();
        return decodeMatches(buf, n);
    }

    /**
     * Convenience dispatcher: passing both queries runs a hybrid search,
     * passing only one runs that single-modality search.
     *
     * @param {object} [query]
     * @param {string} [query.text]
     * @param {number[]} [query.vector]
     * @param {number} [query.k=10]
     * @param {number} [query.alpha=0.5]
     * @returns {Match[]}
     */
    query(query = {}) {
        const { text, vector, k = 10, alpha = 0.5 } = query;
        const hasVector = vector !== null && vector !== undefined;
        if (text !== undefined && text !== null && hasVector) {
            return this.searchHybrid(vector, text, k, alpha);
        }
        if (hasVector) return this.searchVector(vector, k);
        if (text !== undefined && text !== null) return this.searchText(text, k);
        return [];
    }

    // -- internals ---------------------------------------------------------

    _require() {
        if (!this._handle) throw new VecDBError('database is closed');
        return this._handle;
    }

    _check(rc) {
        if (rc === VECDB_OK) return;
        let detail = '';
        if (this._handle) {
            const msg = this._api.last_error(this._handle);
            if (msg) detail = `: ${msg}`;
        }
        const code = this._api.strerror(rc) || `error ${rc}`;
        const Err = ERROR_TYPES[rc] || VecDBError;
        throw new Err(`${code}${detail}`, rc);
    }

    /** Search functions report errors as a 0 result; surface dim errors. */
    _raiseDimensionError() {
        if (!this._handle) return;
        const msg = this._api.last_error(this._handle);
        if (msg && msg.includes('dimension')) {
            throw new DimensionMismatchError(msg, VECDB_ERR_DIM);
        }
    }

    _str(ptr) {
        if (ptr === null || ptr === undefined || ptr === 0n) return '';
        return typeof ptr === 'string' ? ptr : (koffi.decode(ptr, 'const char *') || '');
    }

    _put(id, record, flags) {
        const { vec, dim } = vectorArg(record.vector);
        const blob = encodePayload(record.payload);
        const blobLen = blob ? blob.length : 0;
        if (blobLen > VecDB.MAX_PAYLOAD) {
            throw new RangeError(`payload is too large (${blobLen} bytes)`);
        }
        this._check(this._api.put_ex(
            this._require(), id, vec, dim, blob, blobLen,
            record.text === null || record.text === undefined ? null : record.text,
            flags,
        ));
    }
}

/** Cap so we never ask the C layer for a silly allocation. */
VecDB.MAX_PAYLOAD = 2 ** 31;

// `using db = new VecDB(...)` on runtimes with explicit resource management.
if (typeof Symbol.dispose === 'symbol') {
    VecDB.prototype[Symbol.dispose] = function () { this.close(); };
}

function pick(obj, ...keys) {
    for (const k of keys) {
        if (obj[k] !== undefined) return obj[k];
    }
    return undefined;
}

module.exports = {
    VecDB,
    Match,
    Record,
    VecDBError,
    NotFoundError,
    AlreadyExistsError,
    DimensionMismatchError,
    findLibrary,
    encodePayload,
    decodePayload,
    METRICS,
    INDEXES,
    VERSION,
};
