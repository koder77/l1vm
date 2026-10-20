"""
vecdb — a Chroma/Milvus-style embedded vector + full-text database.

This module is a thin ctypes wrapper around the `libvecdb.so` C library.
It gives you an embedded database that stores arbitrary payloads alongside
dense vectors and free text, and retrieves them by:

  * nearest-neighbour **vector search**
  * **full-text search** (BM25)
  * **hybrid search** that blends both

Quick start
-----------

    from vecdb import VecDB

    db = VecDB("/tmp/notes.db", dim=3, metric="cosine")
    db.add(1, vector=[1, 0, 0], text="a cat sat on the mat",
           payload={"kind": "animal"})
    db.add(2, vector=[0, 1, 0], text="a dog barked at the postman",
           payload={"kind": "animal"})

    for hit in db.search(vector=[0.9, 0.1, 0], k=2):
        print(hit.score, hit.id, hit.text, hit.payload)

    for hit in db.search_text("postman", k=1):
        print(hit.id)

There is also a `Collection`-ish convenience layer (`VecDB`) and a
lower-level `Client` that mirrors the C API closely.
"""

from __future__ import annotations

import ctypes
import ctypes.util
import json
import os
import struct
from ctypes import (
    POINTER,
    Structure,
    c_char_p,
    c_float,
    c_int,
    c_size_t,
    c_uint,
    c_uint64,
    c_void_p,
)
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple, Union

__all__ = [
    "VecDB",
    "Client",
    "Match",
    "Record",
    "VecDBError",
    "NotFoundError",
    "AlreadyExistsError",
    "DimensionMismatchError",
    "find_library",
    "__version__",
]

__version__ = "1.0.0"

# --------------------------------------------------------------------------
# status codes (must match vecdb.h)
# --------------------------------------------------------------------------
VECDB_OK = 0
VECDB_ERR_IO = -1
VECDB_ERR_NOMEM = -2
VECDB_ERR_INVALID = -3
VECDB_ERR_NOTFOUND = -4
VECDB_ERR_EXISTS = -5
VECDB_ERR_DIM = -6
VECDB_ERR_CORRUPT = -7
VECDB_ERR_NODIM = -8

VECDB_METRIC_L2 = 0
VECDB_METRIC_IP = 1
VECDB_METRIC_COSINE = 2

VECDB_INDEX_FLAT = 0
VECDB_INDEX_HNSW = 1

VECDB_INSERT_ONLY = 1 << 0
VECDB_SYNC = 1 << 1

_METRICS = {"l2": VECDB_METRIC_L2, "ip": VECDB_METRIC_IP, "cosine": VECDB_METRIC_COSINE}
_INDEXES = {"flat": VECDB_INDEX_FLAT, "hnsw": VECDB_INDEX_HNSW}


# --------------------------------------------------------------------------
# errors
# --------------------------------------------------------------------------
class VecDBError(Exception):
    """Base class for every error raised by this module."""


class NotFoundError(VecDBError):
    """The requested id does not exist."""


class AlreadyExistsError(VecDBError):
    """An insert-only write hit an existing id."""


class DimensionMismatchError(VecDBError):
    """A vector had the wrong length."""


_ERRORS = {
    VECDB_ERR_IO: VecDBError,
    VECDB_ERR_NOMEM: MemoryError,
    VECDB_ERR_INVALID: VecDBError,
    VECDB_ERR_NOTFOUND: NotFoundError,
    VECDB_ERR_EXISTS: AlreadyExistsError,
    VECDB_ERR_DIM: DimensionMismatchError,
    VECDB_ERR_NODIM: DimensionMismatchError,
    VECDB_ERR_CORRUPT: VecDBError,
}


# --------------------------------------------------------------------------
# C structures
# --------------------------------------------------------------------------
class _Options(Structure):
    _fields_ = [
        ("dim", c_size_t),
        ("metric", c_int),
        ("index", c_int),
        ("hnsw_m", c_size_t),
        ("hnsw_ef_construction", c_size_t),
        ("hnsw_ef_search", c_size_t),
        ("seed", c_uint64),
    ]


class _CRecord(Structure):
    _fields_ = [
        ("id", c_uint64),
        ("vec", POINTER(c_float)),
        ("payload", c_void_p),
        ("payload_len", c_size_t),
        ("text", c_char_p),
    ]


class _CMatch(Structure):
    _fields_ = [
        ("id", c_uint64),
        ("score", c_float),
        ("payload", c_void_p),
        ("payload_len", c_size_t),
        ("text", c_char_p),
    ]


# --------------------------------------------------------------------------
# library loading
# --------------------------------------------------------------------------
def find_library(path: Optional[str] = None) -> str:
    """Locate libvecdb.so.

    Search order: an explicit `path`, the VECDB_LIBRARY environment
    variable, this module's directory and each of its ancestors
    (so `make` in the repository root is found), then the system loader.
    """
    candidates: List[str] = []
    if path:
        candidates.append(path)
    env = os.environ.get("VECDB_LIBRARY")
    if env:
        candidates.append(env)
    names = ("libvecdb.so", "libvecdb.dylib", "vecdb.dll")
    here = os.path.dirname(os.path.abspath(__file__))
    d = here
    for _ in range(5):  # module dir, then up to 4 ancestors (repo root)
        for name in names:
            candidates.append(os.path.join(d, name))
        parent = os.path.dirname(d)
        if parent == d:
            break
        d = parent
    for cand in candidates:
        if cand and os.path.exists(cand):
            return os.path.abspath(cand)
    found = ctypes.util.find_library("vecdb")
    if found:
        return found
    raise OSError(
        "libvecdb.so was not found. Build it with `make` and either place it "
        "next to vecdb.py, add its directory to LD_LIBRARY_PATH, pass "
        "VecDB(library=...) or set the VECDB_LIBRARY environment variable."
    )


def _load(library: Optional[str]) -> ctypes.CDLL:
    lib = ctypes.CDLL(find_library(library))
    P, S, U, I, F = c_void_p, c_char_p, c_size_t, c_int, c_float

    lib.vecdb_options_default.argtypes = [POINTER(_Options)]
    lib.vecdb_options_default.restype = None

    lib.vecdb_open.argtypes = [S, POINTER(_Options)]
    lib.vecdb_open.restype = P

    lib.vecdb_close.argtypes = [P]
    lib.vecdb_close.restype = I
    lib.vecdb_flush.argtypes = [P]
    lib.vecdb_flush.restype = I
    lib.vecdb_compact.argtypes = [P]
    lib.vecdb_compact.restype = I

    lib.vecdb_count.argtypes = [P]
    lib.vecdb_count.restype = U
    lib.vecdb_capacity.argtypes = [P]
    lib.vecdb_capacity.restype = U
    lib.vecdb_dim.argtypes = [P]
    lib.vecdb_dim.restype = U
    lib.vecdb_metric_of.argtypes = [P]
    lib.vecdb_metric_of.restype = I
    lib.vecdb_index_of.argtypes = [P]
    lib.vecdb_index_of.restype = I
    lib.vecdb_path.argtypes = [P]
    lib.vecdb_path.restype = S
    lib.vecdb_last_error.argtypes = [P]
    lib.vecdb_last_error.restype = S
    lib.vecdb_strerror.argtypes = [I]
    lib.vecdb_strerror.restype = S
    lib.vecdb_set_ef_search.argtypes = [P, U]
    lib.vecdb_set_ef_search.restype = I

    lib.vecdb_put.argtypes = [P, c_uint64, POINTER(F), U, P, U, S]
    lib.vecdb_put.restype = I
    lib.vecdb_put_ex.argtypes = [P, c_uint64, POINTER(F), U, P, U, S, c_uint]
    lib.vecdb_put_ex.restype = I
    lib.vecdb_get.argtypes = [P, c_uint64, POINTER(_CRecord)]
    lib.vecdb_get.restype = I
    lib.vecdb_delete.argtypes = [P, c_uint64]
    lib.vecdb_delete.restype = I

    lib.vecdb_search_vector.argtypes = [P, POINTER(F), U, U, POINTER(_CMatch), U]
    lib.vecdb_search_vector.restype = U
    lib.vecdb_search_text.argtypes = [P, S, U, POINTER(_CMatch), U]
    lib.vecdb_search_text.restype = U
    lib.vecdb_search_hybrid.argtypes = [P, POINTER(F), U, S, U, F, POINTER(_CMatch), U]
    lib.vecdb_search_hybrid.restype = U

    lib.vecdb_list_ids.argtypes = [P, POINTER(c_uint64), U]
    lib.vecdb_list_ids.restype = U

    lib.vecdb_pack_i64.argtypes = [P, ctypes.c_int64]
    lib.vecdb_unpack_i64.argtypes = [P]
    lib.vecdb_unpack_i64.restype = ctypes.c_int64
    lib.vecdb_pack_f64.argtypes = [P, ctypes.c_double]
    lib.vecdb_unpack_f64.argtypes = [P]
    lib.vecdb_unpack_f64.restype = ctypes.c_double
    lib.vecdb_pack_f32.argtypes = [P, F]
    lib.vecdb_unpack_f32.argtypes = [P]
    lib.vecdb_unpack_f32.restype = F

    return lib


# --------------------------------------------------------------------------
# result objects
# --------------------------------------------------------------------------
_PAYLOAD_STRING_LIMIT = 64 * 1024


class Match:
    """One search hit.

    Attributes:
        id:          the record id
        score:       similarity / BM25 / blended score, higher is better
        payload:     the decoded payload (as passed to ``add``)
        text:        the stored text, or None
    """

    __slots__ = ("id", "score", "payload", "text", "raw_payload")

    def __init__(self, id: int, score: float, payload: Any, text: Optional[str],
                 raw_payload: Optional[bytes] = None):
        self.id = id
        self.score = score
        self.payload = payload
        self.text = text
        self.raw_payload = raw_payload

    def __repr__(self) -> str:
        return (f"Match(id={self.id}, score={self.score:.4f}, "
                f"text={self.text!r}, payload={self.payload!r})")


class Record:
    """A full record as returned by :meth:`VecDB.get`."""

    __slots__ = ("id", "vector", "payload", "text", "raw_payload")

    def __init__(self, id: int, vector: Optional[List[float]], payload: Any,
                 text: Optional[str], raw_payload: Optional[bytes] = None):
        self.id = id
        self.vector = vector
        self.payload = payload
        self.text = text
        self.raw_payload = raw_payload

    def __repr__(self) -> str:
        v = None if self.vector is None else f"<{len(self.vector)} floats>"
        return (f"Record(id={self.id}, vector={v}, text={self.text!r}, "
                f"payload={self.payload!r})")


# --------------------------------------------------------------------------
# payload codec
# --------------------------------------------------------------------------
#: Small self-describing tags for the payloads JSON cannot round-trip.
_TAG = b"\x00VDB"
_BLOB_TAG = _TAG + b"b"
_INT_TAG = _TAG + b"i"
_FLOAT_TAG = _TAG + b"f"


def _encode_payload(payload: Any) -> Optional[bytes]:
    """Turn a Python value into the bytes stored in the payload blob.

    Serialisation keeps plain JSON readable by non-Python clients, with a
    private ``\\x00VDB`` prefix only for the two cases JSON cannot express
    losslessly: raw ``bytes`` and the ``int``/``float`` distinction.

      * None            -> NULL payload
      * bytes/bytearray -> b"\\x00VDBb" + raw bytes
      * str             -> UTF-8 JSON string
      * int             -> b"\\x00VDBi" + 8 byte little-endian signed
      * float           -> b"\\x00VDBf" + 8 byte little-endian IEEE-754
      * bool/dict/list  -> UTF-8 JSON
      * anything else   -> UTF-8 JSON (with a fallback to repr())
    """
    if payload is None:
        return None
    if isinstance(payload, bytes):
        return _BLOB_TAG + payload
    if isinstance(payload, bytearray):
        return _BLOB_TAG + bytes(payload)
    if isinstance(payload, bool):
        # bool is an int subclass; JSON keeps the intent
        return json.dumps(payload).encode("utf-8")
    if isinstance(payload, int):
        return _INT_TAG + struct.pack("<q", payload)
    if isinstance(payload, float):
        return _FLOAT_TAG + struct.pack("<d", payload)
    try:
        return json.dumps(payload).encode("utf-8")
    except (TypeError, ValueError):
        return repr(payload).encode("utf-8")


def _decode_payload(raw: Optional[bytes]) -> Any:
    """Best-effort inverse of :func:`_encode_payload`.

    The payload blob is untyped, so this guesses for data written by other
    clients.  Values produced by :func:`_encode_payload` are recognised
    exactly via their tag; foreign payloads fall back to JSON, then to
    fixed-width numbers, then to text, then to raw bytes.  Use
    ``Match.raw_payload`` when you need the exact bytes.
    """
    if raw is None:
        return None
    if len(raw) == 0:
        return b""
    if raw.startswith(_BLOB_TAG):
        return raw[len(_BLOB_TAG):]
    if raw.startswith(_INT_TAG) and len(raw) == len(_INT_TAG) + 8:
        return struct.unpack("<q", raw[len(_INT_TAG):])[0]
    if raw.startswith(_FLOAT_TAG) and len(raw) == len(_FLOAT_TAG) + 8:
        return struct.unpack("<d", raw[len(_FLOAT_TAG):])[0]
    # JSON (only when it can be text at all)
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        text = None
    if text is not None:
        stripped = text.lstrip()
        if stripped[:1] in "{[\"" or stripped in ("true", "false", "null"):
            try:
                return json.loads(text)
            except ValueError:
                pass
    if len(raw) == 8:
        return struct.unpack("<q", raw)[0]
    if len(raw) == 4:
        return struct.unpack("<f", raw)[0]
    if text is not None:
        return text
    return raw


# --------------------------------------------------------------------------
# the database
# --------------------------------------------------------------------------
class VecDB:
    """An embedded vector + full-text database.

    The connection is not thread safe for concurrent *writes* through the
    same Python object (the GIL plus the C level rwlock protect the data,
    but a single handle serialises).  Open one handle per thread if you
    write concurrently.

    Example:
        db = VecDB("notes.db", dim=384, metric="cosine", index="hnsw")
        db.add(id=42, vector=embedding, text="hello", payload={"k": 1})
        hits = db.search(vector=q, k=5)
    """

    #: Python-side cap so we never ask the C layer for a silly allocation.
    MAX_PAYLOAD = 1 << 31

    def __init__(
        self,
        path: str,
        dim: int = 0,
        metric: str = "l2",
        index: str = "flat",
        hnsw_m: int = 16,
        hnsw_ef_construction: int = 100,
        hnsw_ef_search: int = 100,
        seed: int = 42,
        library: Optional[str] = None,
    ):
        if metric.lower() not in _METRICS:
            raise ValueError(f"metric must be one of {sorted(_METRICS)}")
        if index.lower() not in _INDEXES:
            raise ValueError(f"index must be one of {sorted(_INDEXES)}")
        if dim < 0:
            raise ValueError("dim must be >= 0")

        self._lib = _load(library)
        self._closed = False

        opt = _Options()
        self._lib.vecdb_options_default(ctypes.byref(opt))
        opt.dim = dim
        opt.metric = _METRICS[metric.lower()]
        opt.index = _INDEXES[index.lower()]
        opt.hnsw_m = hnsw_m
        opt.hnsw_ef_construction = hnsw_ef_construction
        opt.hnsw_ef_search = hnsw_ef_search
        opt.seed = seed

        data = os.fsencode(path)
        self._handle = self._lib.vecdb_open(data, ctypes.byref(opt))
        if not self._handle:
            raise VecDBError(
                f"could not open {path!r} (missing, corrupt, or dimension mismatch)"
            )

    # -- lifecycle ---------------------------------------------------------
    def _require(self) -> int:
        if self._closed or not self._handle:
            raise VecDBError("database is closed")
        return self._handle

    def _check(self, rc: int) -> None:
        if rc == VECDB_OK:
            return
        detail = ""
        if self._handle:
            msg = self._lib.vecdb_last_error(self._handle)
            if msg:
                detail = f": {msg.decode('utf-8', 'replace')}"
        code = self._lib.vecdb_strerror(rc).decode("utf-8", "replace")
        err = _ERRORS.get(rc, VecDBError)
        raise err(f"{code}{detail}")

    def close(self) -> None:
        """Flush and close.  Safe to call more than once."""
        if self._closed or not self._handle:
            return
        self._lib.vecdb_close(self._handle)
        self._handle = None
        self._closed = True

    def flush(self) -> None:
        """Persist to disk atomically."""
        self._check(self._lib.vecdb_flush(self._require()))

    def compact(self) -> None:
        """Drop deleted records and rebuild the indexes."""
        self._check(self._lib.vecdb_compact(self._require()))

    def set_ef_search(self, ef: int) -> None:
        """HNSW search breadth: larger means better recall, slower queries."""
        self._check(self._lib.vecdb_set_ef_search(self._require(), ef))

    def __enter__(self) -> "VecDB":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    # -- introspection -----------------------------------------------------
    @property
    def path(self) -> str:
        return self._lib.vecdb_path(self._require()).decode("utf-8", "replace")

    def __len__(self) -> int:
        return self._lib.vecdb_count(self._require())

    def __bool__(self) -> bool:
        return not self._closed

    def count(self) -> int:
        """Number of live records."""
        return self._lib.vecdb_count(self._require())

    def capacity(self) -> int:
        """Slots in use including tombstones from deleted records."""
        return self._lib.vecdb_capacity(self._require())

    @property
    def dim(self) -> int:
        """Vector length, or 0 if no vector has been stored yet."""
        return self._lib.vecdb_dim(self._require())

    @property
    def metric(self) -> str:
        m = self._lib.vecdb_metric_of(self._require())
        return {v: k for k, v in _METRICS.items()}.get(m, "l2")

    @property
    def index(self) -> str:
        i = self._lib.vecdb_index_of(self._require())
        return {v: k for k, v in _INDEXES.items()}.get(i, "flat")

    def ids(self) -> List[int]:
        """Every live id, in unspecified order."""
        h = self._require()
        n = self._lib.vecdb_list_ids(h, None, 0)
        if n == 0:
            return []
        buf = (c_uint64 * n)()
        got = self._lib.vecdb_list_ids(h, buf, n)
        return [int(buf[i]) for i in range(got)]

    # -- writes ------------------------------------------------------------
    def add(
        self,
        id: int,
        vector: Optional[Sequence[float]] = None,
        text: Optional[str] = None,
        payload: Any = None,
        *,
        upsert: bool = True,
        sync: bool = False,
    ) -> None:
        """Store a record.

        Any of `vector`, `text` and `payload` may be omitted.  The first
        vector that is stored fixes the database dimension unless it was
        set at open time.  With ``upsert=False`` an existing id raises
        :class:`AlreadyExistsError`.
        """
        self.add_many(
            [(id, vector, text, payload)],
            upsert=upsert,
            sync=sync,
        )

    def add_many(
        self,
        records: Iterable[Tuple[int, Optional[Sequence[float]], Optional[str], Any]],
        *,
        upsert: bool = True,
        sync: bool = False,
    ) -> int:
        """Store several records, returning how many were written.

        Each item is ``(id, vector, text, payload)``.  Only one flush
        happens at the end when ``sync`` is set.
        """
        h = self._require()
        flags = 0 if upsert else VECDB_INSERT_ONLY
        n = 0
        items = list(records)
        for pos, (rid, vector, text, payload) in enumerate(items):
            arr, dim, keepalive = self._vector_arg(vector)
            blob, blob_len = self._payload_arg(payload)
            last = sync and pos == len(items) - 1
            rc = self._lib.vecdb_put_ex(
                h,
                int(rid),
                arr,
                dim,
                blob,
                blob_len,
                None if text is None else text.encode("utf-8"),
                flags | (VECDB_SYNC if last else 0),
            )
            self._check(rc)
            n += 1
        return n

    def delete(self, id: int) -> None:
        """Remove a record.  Raises :class:`NotFoundError` if absent."""
        self._check(self._lib.vecdb_delete(self._require(), int(id)))

    def delete_many(self, ids: Iterable[int]) -> int:
        """Remove several records, ignoring ids that do not exist."""
        h = self._require()
        n = 0
        for i in ids:
            if self._lib.vecdb_delete(h, int(i)) == VECDB_OK:
                n += 1
        return n

    def get(self, id: int) -> Record:
        """Fetch one record.  Raises :class:`NotFoundError` if absent."""
        h = self._require()
        rec = _CRecord()
        rc = self._lib.vecdb_get(h, int(id), ctypes.byref(rec))
        self._check(rc)
        dim = self.dim
        vec = None
        if rec.vec:
            vec = [float(rec.vec[i]) for i in range(dim)]
        raw = None
        if rec.payload and rec.payload_len:
            raw = ctypes.string_at(rec.payload, rec.payload_len)
        text = rec.text.decode("utf-8", "replace") if rec.text else None
        return Record(int(rec.id), vec, _decode_payload(raw), text, raw)

    # -- searches ----------------------------------------------------------
    def search(
        self,
        vector: Sequence[float],
        k: int = 10,
        *,
        metric: Optional[str] = None,
    ) -> List[Match]:
        """k nearest neighbours by vector similarity.

        Scores are higher-is-better: for ``l2`` an exact match scores 0.0
        (it is the negated distance), for ``ip`` it is the dot product and
        for ``cosine`` the cosine similarity in [-1, 1].
        """
        del metric  # the metric is fixed per database
        return self.search_vector(vector, k)

    def search_vector(self, vector: Sequence[float], k: int = 10) -> List[Match]:
        h = self._require()
        arr, dim, keepalive = self._vector_arg(vector)
        if arr is None:
            raise ValueError("a vector is required for vector search")
        if k <= 0:
            return []
        buf = (_CMatch * k)()
        n = self._lib.vecdb_search_vector(h, arr, dim, k, buf, k)
        if n == 0:
            msg = self._lib.vecdb_last_error(h)
            if msg:
                # surface dimension errors rather than silently returning []
                code = msg.decode("utf-8", "replace")
                if "dimension" in code:
                    raise DimensionMismatchError(code)
        return self._matches(buf, n)

    def search_text(self, query: str, k: int = 10) -> List[Match]:
        """BM25 full-text search."""
        h = self._require()
        if k <= 0 or not query:
            return []
        buf = (_CMatch * k)()
        n = self._lib.vecdb_search_text(h, query.encode("utf-8"), k, buf, k)
        return self._matches(buf, n)

    def search_hybrid(
        self,
        vector: Optional[Sequence[float]] = None,
        text: Optional[str] = None,
        k: int = 10,
        alpha: float = 0.5,
    ) -> List[Match]:
        """Blend vector and text results.

        ``alpha`` weights the vector signal: 1.0 is pure vector search,
        0.0 pure text search.  Either query may be omitted.
        """
        h = self._require()
        if k <= 0 or (vector is None and not text):
            return []
        arr, dim, keepalive = self._vector_arg(vector)
        buf = (_CMatch * k)()
        n = self._lib.vecdb_search_hybrid(
            h,
            arr,
            dim,
            None if text is None else text.encode("utf-8"),
            k,
            c_float(alpha),
            buf,
            k,
        )
        return self._matches(buf, n)

    def query(
        self,
        text: Optional[str] = None,
        vector: Optional[Sequence[float]] = None,
        k: int = 10,
        alpha: float = 0.5,
    ) -> List[Match]:
        """Convenience wrapper that picks the right search automatically.

        Passing both runs a hybrid search, passing only one runs that
        single-modality search.
        """
        if text is not None and vector is not None:
            return self.search_hybrid(vector, text, k, alpha)
        if vector is not None:
            return self.search_vector(vector, k)
        if text is not None:
            return self.search_text(text, k)
        return []

    # -- helpers -----------------------------------------------------------
    def _matches(self, buf, n: int) -> List[Match]:
        out: List[Match] = []
        for i in range(n):
            m = buf[i]
            raw = None
            if m.payload and m.payload_len:
                raw = ctypes.string_at(m.payload, m.payload_len)
            text = m.text.decode("utf-8", "replace") if m.text else None
            out.append(Match(int(m.id), float(m.score), _decode_payload(raw),
                             text, raw))
        return out

    def _vector_arg(self, vector):
        """Return (float pointer, dim, keepalive) for a Python sequence."""
        if vector is None:
            return None, 0, None
        if isinstance(vector, (bytes, bytearray)):
            raise TypeError("vector must be a sequence of floats, not bytes")
        seq = list(vector)
        for v in seq:
            if not isinstance(v, (int, float)):
                raise TypeError("vector elements must be numbers")
        dim = len(seq)
        if dim == 0:
            raise ValueError("vector must not be empty")
        arr = (c_float * dim)(*[float(v) for v in seq])
        return arr, dim, arr

    def _payload_arg(self, payload):
        """Return (c_void_p, length) for a payload, or (None, 0)."""
        blob = _encode_payload(payload)
        if blob is None:
            return None, 0
        if not blob:
            return None, 0
        if len(blob) > self.MAX_PAYLOAD:
            raise ValueError("payload is too large")
        buf = ctypes.create_string_buffer(bytes(blob), len(blob))
        return ctypes.cast(buf, c_void_p), len(blob)


#: An alias so the module reads naturally in Chroma-flavoured code.
Client = VecDB
