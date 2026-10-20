//! Integration tests for the Rust bindings.
//!
//! Run with:  cargo test
//!
//! They need the shared library built (`make` in the repository root);
//! `build.rs` locates it relative to this crate.

use std::sync::atomic::{AtomicU64, Ordering};

use vecdb::{Index, Metric, Options, VecDb};

/// Unique temp directory per call, removed on drop. Avoids a `tempfile`
/// dependency while keeping tests parallel-safe.
struct TempDir(std::path::PathBuf);

impl TempDir {
    fn new() -> TempDir {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        let n = COUNTER.fetch_add(1, Ordering::Relaxed);
        let dir = std::env::temp_dir().join(format!(
            "vecdb-rs-{}-{}-{}",
            std::process::id(),
            n,
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        std::fs::create_dir_all(&dir).unwrap();
        TempDir(dir)
    }

    fn path(&self, name: &str) -> std::path::PathBuf {
        self.0.join(name)
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.0);
    }
}

/// Open a database in a fresh temp dir with the given options.
fn open(name: &str, options: &Options) -> VecDb {
    VecDb::open_with(TempDir::new().path(name), options).expect("open")
}

#[test]
fn basic_vector() {
    let db = open(
        "v.db",
        &Options {
            dim: 3,
            metric: Metric::Cosine,
            ..Options::default()
        },
    );
    db.put(
        1,
        &[1.0, 0.0, 0.0],
        br#"{"kind":"animal"}"#,
        "a cat sat on the mat",
    )
    .unwrap();
    db.put(
        2,
        &[0.0, 1.0, 0.0],
        br#"{"kind":"animal"}"#,
        "a dog barked at the postman",
    )
    .unwrap();
    db.put(3, &[0.0, 0.0, 1.0], b"", "a fish swam in the sea")
        .unwrap();

    assert_eq!(db.count(), 3);
    assert_eq!(db.dim(), 3);
    assert_eq!(db.metric(), Metric::Cosine);
    assert_eq!(db.index(), Index::Flat);

    let hits = db.search_vector(&[0.95, 0.05, 0.0], 3).unwrap();
    assert_eq!(hits.len(), 3, "expected three hits");
    assert_eq!(hits[0].id, 1, "nearest neighbour should be id 1");
    assert!(
        hits[0].score > hits[1].score,
        "scores not descending: {} then {}",
        hits[0].score,
        hits[1].score
    );
    assert_eq!(
        hits[0].payload.as_deref(),
        Some(&br#"{"kind":"animal"}"#[..])
    );
    assert_eq!(hits[0].text.as_deref(), Some("a cat sat on the mat"));
    // no payload stored on record 3
    let fish = hits.iter().find(|h| h.id == 3).unwrap();
    assert_eq!(fish.payload, None);
}

#[test]
fn text_search() {
    let db = open("t.db", &Options::default());
    db.put(1, &[], b"", "the quick brown fox jumps over the lazy dog")
        .unwrap();
    db.put(2, &[], b"", "a packet of postman biscuits").unwrap();
    db.put(3, &[], b"", "foxes and hounds").unwrap();

    // BM25 has no stemming: 'foxes' is its own token.
    for (query, want) in [("fox", 1u64), ("foxes", 3), ("postman", 2)] {
        let hits = db.search_text(query, 5).unwrap();
        assert_eq!(hits.len(), 1, "query {query:?} returned {hits:?}");
        assert_eq!(hits[0].id, want, "query {query:?}");
    }
    assert!(db.search_text("   ", 5).unwrap().is_empty());
}

#[test]
fn hybrid() {
    let db = open(
        "h.db",
        &Options {
            dim: 2,
            metric: Metric::L2,
            ..Options::default()
        },
    );
    db.put(1, &[0.0, 0.0], b"", "red apple fruit").unwrap();
    db.put(2, &[10.0, 10.0], b"", "blue car vehicle").unwrap();

    let hits = db
        .search_hybrid(Some(&[0.1, 0.1]), Some("apple"), 2, 0.5)
        .unwrap();
    assert_eq!(hits.len(), 2);
    assert_eq!(hits[0].id, 1, "hybrid should prefer the matching doc");

    // alpha extremes collapse to a single modality
    let hits = db
        .search_hybrid(Some(&[9.9, 9.9]), Some("zzz"), 1, 1.0)
        .unwrap();
    assert_eq!(hits.len(), 1);
    assert_eq!(hits[0].id, 2, "alpha=1 should be pure vector search");

    let hits = db.search_hybrid(None, Some("apple"), 1, 0.0).unwrap();
    assert_eq!(hits.len(), 1);
    assert_eq!(hits[0].id, 1, "alpha=0 should be pure text search");
}

#[test]
fn get_delete_and_errors() {
    let db = open(
        "g.db",
        &Options {
            dim: 2,
            ..Options::default()
        },
    );
    db.put(7, &[1.0, 2.0], b"", "hello").unwrap();

    let rec = db.get(7).unwrap();
    assert_eq!(rec.id, 7);
    assert_eq!(rec.text.as_deref(), Some("hello"));
    assert_eq!(rec.vector.as_deref(), Some(&[1.0f32, 2.0][..]));

    db.delete(7).unwrap();
    assert_eq!(db.count(), 0);
    assert!(db.get(7).unwrap_err().is_not_found());
    assert!(db.delete(7).unwrap_err().is_not_found());

    db.put(9, &[0.0, 0.0], b"", "").unwrap();
    let err = db
        .put_with_flags(9, &[0.0, 0.0], b"", "", vecdb::PutFlags::INSERT_ONLY)
        .unwrap_err();
    assert!(err.is_already_exists(), "got {err}");
}

#[test]
fn dimension_mismatch() {
    let db = open(
        "d.db",
        &Options {
            dim: 3,
            ..Options::default()
        },
    );
    assert!(db
        .put(1, &[1.0, 2.0], b"", "")
        .unwrap_err()
        .is_dimension_mismatch());
    db.put(1, &[1.0, 2.0, 3.0], b"", "").unwrap();
    assert!(db
        .search_vector(&[1.0], 1)
        .unwrap_err()
        .is_dimension_mismatch());
}

#[test]
fn put_many_and_list_ids() {
    let db = open(
        "m.db",
        &Options {
            dim: 2,
            ..Options::default()
        },
    );
    for i in 0..50u64 {
        db.put(i, &[i as f32, 0.0], &i.to_le_bytes(), "doc number")
            .unwrap();
    }
    assert_eq!(db.count(), 50);

    let ids = db.list_ids().unwrap();
    let distinct: std::collections::HashSet<u64> = ids.iter().copied().collect();
    assert_eq!(distinct.len(), 50, "ListIDs should cover 50 distinct ids");

    let evens: Vec<u64> = (0..50).step_by(2).collect();
    assert_eq!(db.delete_many(&evens), 25);
    assert_eq!(db.count(), 25);
}

#[test]
fn persistence() {
    let dir = TempDir::new();
    let path = dir.path("persist.db");

    let db = VecDb::open_with(
        &path,
        &Options {
            dim: 2,
            index: Index::Hnsw,
            metric: Metric::InnerProduct,
            ..Options::default()
        },
    )
    .unwrap();
    for i in 0..20u64 {
        db.put(i, &[i as f32, 1.0], &i.to_le_bytes(), "text")
            .unwrap();
    }
    db.flush().unwrap();
    db.close().unwrap();

    let db = VecDb::open(&path).unwrap();
    assert_eq!(db.count(), 20);
    assert_eq!(db.index(), Index::Hnsw, "index type is persisted");
    assert_eq!(db.metric(), Metric::InnerProduct, "metric is persisted");

    let rec = db.get(13).unwrap();
    assert_eq!(rec.vector.as_deref(), Some(&[13.0f32, 1.0][..]));
    assert_eq!(rec.payload.as_deref(), Some(&13u64.to_le_bytes()[..]));

    db.compact().unwrap();
    assert_eq!(db.count(), 20);

    // Orphan temp dir is cleaned up when `dir` drops.
    assert!(path.exists());
}

#[test]
fn defaults_and_attributes() {
    let opts = Options::default();
    assert_eq!(opts.dim, 0);
    assert_eq!(opts.metric, Metric::L2);
    assert_eq!(opts.index, Index::Flat);
    assert_eq!(opts.hnsw_m, 16);
    assert_eq!(opts.hnsw_ef_construction, 100);
    assert_eq!(opts.hnsw_ef_search, 100);
    assert_eq!(opts.seed, 42);

    assert_eq!(Metric::L2.to_string(), "l2");
    assert_eq!(Metric::InnerProduct.to_string(), "ip");
    assert_eq!(Metric::Cosine.to_string(), "cosine");
    assert_eq!(Index::Flat.to_string(), "flat");
    assert_eq!(Index::Hnsw.to_string(), "hnsw");

    let flags = vecdb::PutFlags::INSERT_ONLY | vecdb::PutFlags::SYNC;
    assert_ne!(flags, vecdb::PutFlags::NONE);

    let db = open(
        "c.db",
        &Options {
            dim: 1,
            ..Options::default()
        },
    );
    let path = db.path();
    assert!(path.ends_with("c.db"), "path = {path}");
    // A fresh database reports no dimension until the first vector.
    assert_eq!(db.dim(), 1);
}

#[test]
fn shutdown_and_ownership() {
    let dir = TempDir::new();
    let path = dir.path("s.db");

    {
        let db = VecDb::open_with(
            &path,
            &Options {
                dim: 2,
                ..Options::default()
            },
        )
        .unwrap();
        db.put(1, &[1.0, 2.0], b"", "keep").unwrap();
        // Dropped without an explicit close: the file is flushed anyway.
    }

    let db = VecDb::open(&path).unwrap();
    assert_eq!(db.count(), 1);
    assert_eq!(db.get(1).unwrap().text.as_deref(), Some("keep"));
}

#[test]
fn shared_handle_across_threads() {
    let db = std::sync::Arc::new(open(
        "x.db",
        &Options {
            dim: 2,
            ..Options::default()
        },
    ));
    db.put(0, &[0.0, 0.0], b"", "seed").unwrap();

    let mut handles = Vec::new();
    for t in 0..4u64 {
        let db = std::sync::Arc::clone(&db);
        handles.push(std::thread::spawn(move || {
            for i in 0..25u64 {
                let id = t * 100 + i + 1;
                db.put(id, &[id as f32, 1.0], b"", "worker").unwrap();
            }
        }));
    }
    for h in handles {
        h.join().unwrap();
    }
    assert_eq!(db.count(), 101);
}
