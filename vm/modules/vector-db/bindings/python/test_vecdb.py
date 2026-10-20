#!/usr/bin/env python3
"""Smoke test for the vecdb Python bindings (ctypes over libvecdb.so).

Run with:  python3 bindings/python/test_vecdb.py

It needs the shared library built (`make`) either next to this file, in the
repository root (its parent), on LD_LIBRARY_PATH, or pointed at by the
VECDB_LIBRARY environment variable.
"""

from __future__ import annotations

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from vecdb import (  # noqa: E402
    AlreadyExistsError,
    DimensionMismatchError,
    NotFoundError,
    VecDB,
    VecDBError,
)

_failures = 0
_checks = 0


def check(cond: bool, msg: str) -> None:
    global _checks, _failures
    _checks += 1
    if not cond:
        _failures += 1
        print(f"FAIL: {msg}")


def expect_raises(exc, fn, msg: str) -> None:
    global _checks, _failures
    _checks += 1
    try:
        fn()
    except exc:
        return
    except Exception as e:  # noqa: BLE001
        _failures += 1
        print(f"FAIL: {msg} (got {type(e).__name__}: {e})")
        return
    _failures += 1
    print(f"FAIL: {msg} (no exception raised)")


def tmpdb(name: str) -> str:
    d = tempfile.mkdtemp(prefix="vecdb-py-")
    return os.path.join(d, name)


def test_basic_vector() -> None:
    path = tmpdb("v.db")
    with VecDB(path, dim=3, metric="cosine") as db:
        db.add(1, vector=[1, 0, 0], text="a cat sat on the mat",
               payload={"kind": "animal"})
        db.add(2, vector=[0, 1, 0], text="a dog barked at the postman",
               payload={"kind": "animal"})
        db.add(3, vector=[0, 0, 1], text="a fish swam in the sea")
        check(len(db) == 3, "count after 3 adds")
        check(db.count() == 3, "count() == 3")
        check(db.dim == 3, "dim == 3")
        check(db.metric == "cosine", "metric round-trips")

        hits = db.search([0.95, 0.05, 0], k=3)
        check(len(hits) == 3, "vector search returns 3")
        check(hits[0].id == 1, "nearest neighbour is id 1")
        check(hits[0].score > hits[1].score, "scores descending")
        check(hits[0].payload == {"kind": "animal"}, "dict payload decodes")


def test_text_search() -> None:
    path = tmpdb("t.db")
    db = VecDB(path)
    db.add(1, text="the quick brown fox jumps over the lazy dog")
    db.add(2, text="a packet of postman biscuits")
    db.add(3, text="foxes and hounds")
    hits = db.search_text("fox", k=5)
    check(len(hits) == 1 and hits[0].id == 1, "BM25 matches the exact token 'fox'")
    hits = db.search_text("foxes", k=5)
    check(len(hits) == 1 and hits[0].id == 3, "BM25 has no stemming: 'foxes' is its own token")
    hits = db.search_text("postman", k=5)
    check(len(hits) == 1 and hits[0].id == 2, "postman matched id 2")
    check(db.search_text("", k=5) == [], "empty query yields no hits")
    db.close()


def test_hybrid() -> None:
    path = tmpdb("h.db")
    db = VecDB(path, dim=2, metric="l2")
    db.add(1, vector=[0, 0], text="red apple fruit")
    db.add(2, vector=[10, 10], text="blue car vehicle")
    hits = db.search_hybrid(vector=[0.1, 0.1], text="apple", k=2, alpha=0.5)
    check(len(hits) == 2, "hybrid returns both")
    check(hits[0].id == 1, "hybrid prefers the matching doc")
    # alpha extremes collapse to one modality
    hits = db.search_hybrid(vector=[9.9, 9.9], text="zzz", k=1, alpha=1.0)
    check(hits and hits[0].id == 2, "alpha=1 is pure vector")
    hits = db.search_hybrid(vector=None, text="apple", k=1, alpha=0.0)
    check(hits and hits[0].id == 1, "alpha=0 with text only")
    db.close()


def test_payload_types() -> None:
    path = tmpdb("p.db")
    db = VecDB(path)
    cases = {
        100: 42,
        101: 3.5,
        102: "a string",
        103: b"\x01\x02\x03",
        104: {"a": 1, "b": [2, 3]},
        105: True,
    }
    for rid, val in cases.items():
        db.add(rid, payload=val)
    for rid, val in cases.items():
        rec = db.get(rid)
        check(rec.payload == val, f"payload {rid} round-trips ({val!r} -> {rec.payload!r})")
    db.close()


def test_get_delete_errors() -> None:
    path = tmpdb("g.db")
    db = VecDB(path, dim=2)
    db.add(7, vector=[1, 2], text="hello")
    rec = db.get(7)
    check(rec.id == 7, "get id")
    check(rec.vector == [1.0, 2.0], "get vector")
    check(rec.text == "hello", "get text")
    db.delete(7)
    check(len(db) == 0, "count after delete")
    expect_raises(NotFoundError, lambda: db.get(7), "get missing raises NotFound")
    expect_raises(NotFoundError, lambda: db.delete(7), "delete missing raises NotFound")
    db.add(9, vector=[0, 0])
    expect_raises(AlreadyExistsError,
                  lambda: db.add(9, vector=[0, 0], upsert=False),
                  "duplicate insert-only raises AlreadyExists")
    db.close()


def test_dimension_mismatch() -> None:
    path = tmpdb("d.db")
    db = VecDB(path, dim=3)
    expect_raises(DimensionMismatchError,
                  lambda: db.add(1, vector=[1, 2]),
                  "wrong-dim put raises DimensionMismatch")
    db.add(1, vector=[1, 2, 3])
    check(len(db) == 1, "correct-dim put works")
    db.close()


def test_add_many_and_ids() -> None:
    path = tmpdb("m.db")
    db = VecDB(path, dim=2)
    n = db.add_many([(i, [float(i), 0.0], f"doc number {i}", {"i": i})
                     for i in range(50)])
    check(n == 50, "add_many wrote 50")
    check(len(db) == 50, "count is 50")
    ids = set(db.ids())
    check(ids == set(range(50)), "list_ids covers all")
    removed = db.delete_many(range(0, 50, 2))
    check(removed == 25, "delete_many removed 25")
    check(len(db) == 25, "count after delete_many")
    db.close()


def test_persistence() -> None:
    path = tmpdb("persist.db")
    db = VecDB(path, dim=2, index="hnsw", metric="ip")
    db.add_many([(i, [float(i), 1.0], f"text {i}", i) for i in range(20)])
    db.flush()
    db.close()

    db = VecDB(path)
    check(len(db) == 20, "reopened count")
    check(db.index == "hnsw", "index persisted")
    check(db.metric == "ip", "metric persisted")
    rec = db.get(13)
    check(rec.vector == [13.0, 1.0], "vector survived reload")
    check(rec.payload == 13, "payload survived reload")
    db.compact()
    check(len(db) == 20, "count survives compact")
    db.close()


def test_query_dispatch() -> None:
    path = tmpdb("q.db")
    db = VecDB(path, dim=2)
    db.add(1, vector=[1, 0], text="alpha beta")
    db.add(2, vector=[0, 1], text="gamma delta")
    check(len(db.query(text="alpha", k=1)) == 1, "query text only")
    check(len(db.query(vector=[1, 0], k=2)) == 2, "query vector only")
    check(len(db.query(text="alpha", vector=[1, 0], k=2)) == 2, "query hybrid")
    check(db.query(k=2) == [], "query with nothing returns []")
    db.close()


def main() -> int:
    tests = [
        test_basic_vector,
        test_text_search,
        test_hybrid,
        test_payload_types,
        test_get_delete_errors,
        test_dimension_mismatch,
        test_add_many_and_ids,
        test_persistence,
        test_query_dispatch,
    ]
    for t in tests:
        try:
            t()
        except Exception as e:  # noqa: BLE001
            global _failures
            _failures += 1
            print(f"FAIL: {t.__name__} raised {type(e).__name__}: {e}")
    print(f"\npython bindings: {_checks} checks, {_failures} failures")
    return 1 if _failures else 0


if __name__ == "__main__":
    sys.exit(main())
