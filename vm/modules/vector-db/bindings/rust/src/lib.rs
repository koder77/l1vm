//! Rust bindings for **vecdb**, an embedded vector + full-text database
//! written in C and shipped as `libvecdb`.
//!
//! A vecdb database is a single file that is written atomically, so a
//! crash never corrupts it. Records carry an opaque payload blob
//! (`Vec<u8>`), an optional float vector and optional free text, and can
//! be retrieved by id, by nearest-neighbour vector search (exact FLAT or
//! approximate HNSW), by BM25 full-text search, or by a hybrid blend of
//! both. Scores are higher-is-better everywhere (L2 reports the negated
//! distance).
//!
//! ```no_run
//! use vecdb::{Metric, Options, VecDb};
//!
//! let db = VecDb::open_with("notes.db", &Options {
//!     dim: 3,
//!     metric: Metric::Cosine,
//!     ..Options::default()
//! })?;
//!
//! db.put(1, &[1.0, 0.0, 0.0], br#"{"kind":"animal"}"#, "a cat sat on the mat")?;
//! db.put(2, &[0.0, 1.0, 0.0], br#"{"kind":"animal"}"#, "a dog barked")?;
//!
//! for hit in db.search_vector(&[0.9, 0.1, 0.0], 5)? {
//!     println!("{:.4}  #{}  {}", hit.score, hit.id, hit.text.unwrap_or_default());
//! }
//! # Ok::<(), vecdb::Error>(())
//! ```
//!
//! The database handle is safe to share between threads (`Send + Sync`):
//! the C library serialises writers with a rwlock, so concurrent writers
//! on one handle are serialised — open one `VecDb` per thread if you need
//! parallel writes.
//!
//! Payloads are opaque bytes, like in the C API. Anything that
//! serialises to bytes works; for JSON payloads use `serde_json` on
//! `Record::payload` directly.
//!
//! Build requirements: `libvecdb.so` must be built first (`make` in the
//! repository root). `build.rs` locates it relative to this crate, or
//! wherever `VECDB_LIB_DIR` points at build time.

pub mod ffi;

use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::{c_int, c_uint, c_void};
use std::path::Path;
use std::ptr::NonNull;

/// Version of the bindings.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");

/// Distance metric used by vector search.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Metric {
    /// Negated euclidean distance (an exact match scores `0.0`).
    L2,
    /// Inner product.
    InnerProduct,
    /// Cosine similarity in `[-1, 1]`.
    Cosine,
}

impl Metric {
    fn to_c(self) -> c_int {
        match self {
            Metric::L2 => ffi::VECDB_METRIC_L2,
            Metric::InnerProduct => ffi::VECDB_METRIC_IP,
            Metric::Cosine => ffi::VECDB_METRIC_COSINE,
        }
    }

    fn from_c(v: c_int) -> Metric {
        match v {
            ffi::VECDB_METRIC_IP => Metric::InnerProduct,
            ffi::VECDB_METRIC_COSINE => Metric::Cosine,
            _ => Metric::L2,
        }
    }
}

impl fmt::Display for Metric {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Metric::L2 => "l2",
            Metric::InnerProduct => "ip",
            Metric::Cosine => "cosine",
        })
    }
}

/// Vector index structure.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Index {
    /// Exact brute-force scan.
    Flat,
    /// Approximate HNSW graph.
    Hnsw,
}

impl Index {
    fn to_c(self) -> c_int {
        match self {
            Index::Flat => ffi::VECDB_INDEX_FLAT,
            Index::Hnsw => ffi::VECDB_INDEX_HNSW,
        }
    }

    fn from_c(v: c_int) -> Index {
        match v {
            ffi::VECDB_INDEX_HNSW => Index::Hnsw,
            _ => Index::Flat,
        }
    }
}

impl fmt::Display for Index {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Index::Flat => "flat",
            Index::Hnsw => "hnsw",
        })
    }
}

/// Open-time configuration. `Default` gives the library defaults:
/// dimension learned from the first stored vector, L2 metric, FLAT
/// index, HNSW `M=16` / `ef_construction=100` / `ef_search=100`,
/// seed 42.
///
/// The metric and index type are persisted in the file and win over the
/// values supplied here when opening an existing database.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Options {
    /// Vector length; `0` learns the dimension from the first put.
    pub dim: usize,
    /// Distance metric.
    pub metric: Metric,
    /// Index structure.
    pub index: Index,
    /// HNSW `M`; `0` uses the library default (16).
    pub hnsw_m: usize,
    /// HNSW build breadth; `0` uses the library default (100).
    pub hnsw_ef_construction: usize,
    /// HNSW query breadth; `0` uses the library default (100).
    pub hnsw_ef_search: usize,
    /// RNG seed for HNSW; `0` uses the library default (42).
    pub seed: u64,
}

impl Default for Options {
    fn default() -> Options {
        let mut c = ffi::vecdb_options {
            dim: 0,
            metric: 0,
            index: 0,
            hnsw_m: 0,
            hnsw_ef_construction: 0,
            hnsw_ef_search: 0,
            seed: 0,
        };
        unsafe { ffi::vecdb_options_default(&mut c) };
        Options {
            dim: c.dim,
            metric: Metric::from_c(c.metric),
            index: Index::from_c(c.index),
            hnsw_m: c.hnsw_m,
            hnsw_ef_construction: c.hnsw_ef_construction,
            hnsw_ef_search: c.hnsw_ef_search,
            seed: c.seed,
        }
    }
}

impl Options {
    fn to_c(self) -> ffi::vecdb_options {
        ffi::vecdb_options {
            dim: self.dim,
            metric: self.metric.to_c(),
            index: self.index.to_c(),
            hnsw_m: self.hnsw_m,
            hnsw_ef_construction: self.hnsw_ef_construction,
            hnsw_ef_search: self.hnsw_ef_search,
            seed: self.seed,
        }
    }
}

/// Flags for [`VecDb::put_with_flags`].
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct PutFlags(c_uint);

impl PutFlags {
    /// Regular upsert.
    pub const NONE: PutFlags = PutFlags(0);
    /// Fail with [`Error::is_already_exists`] if the id already exists.
    pub const INSERT_ONLY: PutFlags = PutFlags(ffi::VECDB_INSERT_ONLY);
    /// Flush to disk before returning.
    pub const SYNC: PutFlags = PutFlags(ffi::VECDB_SYNC);

    /// Combine flags: `PutFlags::INSERT_ONLY | PutFlags::SYNC`.
    pub fn union(self, other: PutFlags) -> PutFlags {
        PutFlags(self.0 | other.0)
    }
}

impl std::ops::BitOr for PutFlags {
    type Output = PutFlags;

    fn bitor(self, other: PutFlags) -> PutFlags {
        self.union(other)
    }
}

/// An error reported by the library (or by the bindings).
///
/// `status` is one of the `VECDB_*` status codes; `message` is a human
/// readable description including the library's own detail string when
/// one was available.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Error {
    /// The raw status code (0 is success and never appears here).
    pub status: c_int,
    /// Human readable description.
    pub message: String,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message)
    }
}

impl std::error::Error for Error {}

/// Result alias used throughout the crate.
pub type Result<T> = std::result::Result<T, Error>;

impl Error {
    /// Build an error from a status code, using `vecdb_strerror`.
    pub fn from_status(status: c_int) -> Error {
        Error {
            status,
            message: strerror(status),
        }
    }

    fn with_detail(status: c_int, db: *mut ffi::vecdb) -> Error {
        let mut error = Error::from_status(status);
        if !db.is_null() {
            unsafe {
                let detail = CStr::from_ptr(ffi::vecdb_last_error(db));
                if !detail.to_bytes().is_empty() {
                    error.message.push_str(": ");
                    error.message.push_str(&detail.to_string_lossy());
                }
            }
        }
        error
    }

    /// The requested id does not exist.
    pub fn is_not_found(&self) -> bool {
        self.status == ffi::VECDB_ERR_NOTFOUND
    }

    /// An insert-only write hit an existing id.
    pub fn is_already_exists(&self) -> bool {
        self.status == ffi::VECDB_ERR_EXISTS
    }

    /// A vector had the wrong length, or the database has no dimension.
    pub fn is_dimension_mismatch(&self) -> bool {
        self.status == ffi::VECDB_ERR_DIM || self.status == ffi::VECDB_ERR_NODIM
    }

    /// The file could not be read or written.
    pub fn is_io(&self) -> bool {
        self.status == ffi::VECDB_ERR_IO
    }

    /// A bad argument or malformed record.
    pub fn is_invalid(&self) -> bool {
        self.status == ffi::VECDB_ERR_INVALID
    }
}

/// One stored row as returned by [`VecDb::get`]. `vector`, `payload`
/// and `text` are owned copies.
#[derive(Debug, Clone, PartialEq)]
pub struct Record {
    /// The record id.
    pub id: u64,
    /// The stored vector, if the record has one.
    pub vector: Option<Vec<f32>>,
    /// The opaque payload bytes, if the record has one.
    pub payload: Option<Vec<u8>>,
    /// The stored text, if the record has one.
    pub text: Option<String>,
}

/// One search hit. `payload` and `text` are owned copies.
#[derive(Debug, Clone, PartialEq)]
pub struct Match {
    /// The record id.
    pub id: u64,
    /// Similarity / BM25 / blended score, higher is better.
    pub score: f32,
    /// The opaque payload bytes, if the record has one.
    pub payload: Option<Vec<u8>>,
    /// The stored text, if the record has one.
    pub text: Option<String>,
}

/// An open database handle. Flushed and closed on drop.
pub struct VecDb {
    ptr: NonNull<ffi::vecdb>,
}

// SAFETY: every vecdb operation takes the C-side rwlock, and error
// strings are thread-local, so a handle may move and be shared between
// threads. Close is only reachable through a consuming `close()` or
// `Drop`, so the pointer cannot be freed under another thread's feet
// while a single owner is used.
unsafe impl Send for VecDb {}
unsafe impl Sync for VecDb {}

impl fmt::Debug for VecDb {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("VecDb")
            .field("path", &self.path())
            .field("count", &self.count())
            .field("dim", &self.dim())
            .field("metric", &self.metric())
            .field("index", &self.index())
            .finish()
    }
}

impl Drop for VecDb {
    fn drop(&mut self) {
        // Flushes pending changes; a failure here cannot be reported
        // from a destructor, so call `close()` explicitly if it matters.
        unsafe { ffi::vecdb_close(self.ptr.as_ptr()) };
    }
}

impl VecDb {
    /// Open (or create) a database with the library defaults.
    pub fn open(path: impl AsRef<Path>) -> Result<VecDb> {
        VecDb::open_with(path, &Options::default())
    }

    /// Open (or create) a database. Pass `options.dim = 0` to learn the
    /// dimension from the first stored vector.
    pub fn open_with(path: impl AsRef<Path>, options: &Options) -> Result<VecDb> {
        let cpath = cstring(path.as_ref().to_string_lossy().as_ref())?;
        let copts = options.to_c();
        let ptr = unsafe { ffi::vecdb_open(cpath.as_ptr(), &copts) };
        NonNull::new(ptr)
            .map(|ptr| VecDb { ptr })
            .ok_or_else(|| Error {
                status: ffi::VECDB_ERR_IO,
                message: format!(
                    "could not open {:?} (missing, corrupt, or dimension mismatch)",
                    path.as_ref()
                ),
            })
    }

    /// Flush and close the database. The handle is consumed; later use
    /// of any kind is impossible.
    pub fn close(self) -> Result<()> {
        let ptr = self.ptr;
        std::mem::forget(self);
        check(unsafe { ffi::vecdb_close(ptr.as_ptr()) }, ptr.as_ptr())
    }

    /// Persist the database atomically.
    pub fn flush(&self) -> Result<()> {
        self.check(unsafe { ffi::vecdb_flush(self.ptr.as_ptr()) })
    }

    /// Drop deleted records and rebuild the indexes.
    pub fn compact(&self) -> Result<()> {
        self.check(unsafe { ffi::vecdb_compact(self.ptr.as_ptr()) })
    }

    /// Number of live records.
    pub fn count(&self) -> usize {
        unsafe { ffi::vecdb_count(self.ptr.as_ptr()) }
    }

    /// Slots in use, including tombstones left by deleted records.
    pub fn capacity(&self) -> usize {
        unsafe { ffi::vecdb_capacity(self.ptr.as_ptr()) }
    }

    /// Vector length, or `0` if no vector has been stored yet.
    pub fn dim(&self) -> usize {
        unsafe { ffi::vecdb_dim(self.ptr.as_ptr()) }
    }

    /// The distance metric (persisted with the database).
    pub fn metric(&self) -> Metric {
        Metric::from_c(unsafe { ffi::vecdb_metric_of(self.ptr.as_ptr()) })
    }

    /// The index type (persisted with the database).
    pub fn index(&self) -> Index {
        Index::from_c(unsafe { ffi::vecdb_index_of(self.ptr.as_ptr()) })
    }

    /// The database file path.
    pub fn path(&self) -> String {
        unsafe { cstr(ffi::vecdb_path(self.ptr.as_ptr())) }
    }

    /// Tune the HNSW search breadth at runtime: bigger means better
    /// recall and slower queries. Fails when the database is not
    /// HNSW-indexed.
    pub fn set_ef_search(&self, ef: usize) -> Result<()> {
        self.check(unsafe { ffi::vecdb_set_ef_search(self.ptr.as_ptr(), ef) })
    }

    /// Every live id, in unspecified order.
    pub fn list_ids(&self) -> Result<Vec<u64>> {
        let ptr = self.ptr;
        let db = ptr.as_ptr();
        let n = unsafe { ffi::vecdb_list_ids(db, std::ptr::null_mut(), 0) };
        if n == 0 {
            return Ok(Vec::new());
        }
        let mut ids = vec![0u64; n];
        let got = unsafe { ffi::vecdb_list_ids(db, ids.as_mut_ptr(), n) };
        ids.truncate(got);
        Ok(ids)
    }

    /// Store a record (upsert). Any of `vector`, `payload` and `text`
    /// may be empty. The first vector stored fixes the database
    /// dimension unless [`Options::dim`] was set at open time.
    pub fn put(&self, id: u64, vector: &[f32], payload: &[u8], text: &str) -> Result<()> {
        self.put_with_flags(id, vector, payload, text, PutFlags::NONE)
    }

    /// Store a record with flags ([`PutFlags::INSERT_ONLY`],
    /// [`PutFlags::SYNC`]).
    pub fn put_with_flags(
        &self,
        id: u64,
        vector: &[f32],
        payload: &[u8],
        text: &str,
        flags: PutFlags,
    ) -> Result<()> {
        let ctext = if text.is_empty() {
            None
        } else {
            Some(cstring(text)?)
        };
        let ctext = ctext.as_ref().map_or(std::ptr::null(), |s| s.as_ptr());
        self.check(unsafe {
            ffi::vecdb_put_ex(
                self.ptr.as_ptr(),
                id,
                slice_ptr(vector),
                vector.len(),
                payload.as_ptr() as *const c_void,
                payload.len(),
                ctext,
                flags.0,
            )
        })
    }

    /// Fetch one record. Returns [`Error::is_not_found`] when the id
    /// does not exist.
    pub fn get(&self, id: u64) -> Result<Record> {
        let mut rec = ffi::vecdb_record::default();
        self.check(unsafe { ffi::vecdb_get(self.ptr.as_ptr(), id, &mut rec) })?;
        Ok(unsafe { record_from_c(&rec, self.dim()) })
    }

    /// Remove a record (a tombstone remains until [`VecDb::compact`]).
    /// Returns [`Error::is_not_found`] when the id does not exist.
    pub fn delete(&self, id: u64) -> Result<()> {
        self.check(unsafe { ffi::vecdb_delete(self.ptr.as_ptr(), id) })
    }

    /// Remove several records, skipping ids that do not exist. Returns
    /// how many records were actually removed.
    pub fn delete_many(&self, ids: &[u64]) -> usize {
        let mut removed = 0;
        for &id in ids {
            if unsafe { ffi::vecdb_delete(self.ptr.as_ptr(), id) } == ffi::VECDB_OK {
                removed += 1;
            }
        }
        removed
    }

    /// The `k` nearest neighbours of `vector`. For [`Metric::L2`] an
    /// exact match scores `0.0` (the score is the negated distance), for
    /// [`Metric::InnerProduct`] it is the dot product and for
    /// [`Metric::Cosine`] the cosine similarity in `[-1, 1]`.
    pub fn search_vector(&self, vector: &[f32], k: usize) -> Result<Vec<Match>> {
        self.check_vector(vector)?;
        if k == 0 {
            return Ok(Vec::new());
        }
        let mut out = vec![ffi::vecdb_match::default(); k];
        let n = unsafe {
            ffi::vecdb_search_vector(
                self.ptr.as_ptr(),
                slice_ptr(vector),
                vector.len(),
                k,
                out.as_mut_ptr(),
                k,
            )
        };
        Ok(unsafe { matches_from_c(&out[..n]) })
    }

    /// BM25 full-text search.
    pub fn search_text(&self, query: &str, k: usize) -> Result<Vec<Match>> {
        // A text-only database has no dimension; go straight to the
        // library in that case instead of bailing out.
        if k == 0 || query.trim().is_empty() {
            return Ok(Vec::new());
        }
        let cquery = cstring(query)?;
        let mut out = vec![ffi::vecdb_match::default(); k];
        let n = unsafe {
            ffi::vecdb_search_text(self.ptr.as_ptr(), cquery.as_ptr(), k, out.as_mut_ptr(), k)
        };
        Ok(unsafe { matches_from_c(&out[..n]) })
    }

    /// Blend vector and text results. `alpha` weights the vector
    /// signal: `1.0` is pure vector search, `0.0` pure text search.
    /// Either query may be empty to fall back to the other modality.
    pub fn search_hybrid(
        &self,
        vector: Option<&[f32]>,
        text: Option<&str>,
        k: usize,
        alpha: f32,
    ) -> Result<Vec<Match>> {
        let vector = vector.unwrap_or(&[]);
        let text = text.unwrap_or("");
        if !vector.is_empty() {
            self.check_vector(vector)?;
        }
        if k == 0 || (vector.is_empty() && text.is_empty()) {
            return Ok(Vec::new());
        }
        let ctext = if text.is_empty() {
            None
        } else {
            Some(cstring(text)?)
        };
        let ctext = ctext.as_ref().map_or(std::ptr::null(), |s| s.as_ptr());
        let mut out = vec![ffi::vecdb_match::default(); k];
        let n = unsafe {
            ffi::vecdb_search_hybrid(
                self.ptr.as_ptr(),
                slice_ptr(vector),
                vector.len(),
                ctext,
                k,
                alpha,
                out.as_mut_ptr(),
                k,
            )
        };
        Ok(unsafe { matches_from_c(&out[..n]) })
    }

    // -- internals ---------------------------------------------------------

    fn check(&self, rc: c_int) -> Result<()> {
        check(rc, self.ptr.as_ptr())
    }

    /// Validate a query vector up-front: the C search functions only
    /// report errors as an empty result, which is easy to mistake for
    /// "no matches".
    fn check_vector(&self, vector: &[f32]) -> Result<()> {
        let dim = self.dim();
        if dim == 0 {
            return Err(Error::from_status(ffi::VECDB_ERR_NODIM));
        }
        if vector.len() != dim {
            return Err(Error {
                status: ffi::VECDB_ERR_DIM,
                message: format!("query dimension {} != {}", vector.len(), dim),
            });
        }
        Ok(())
    }
}

/// A NULL pointer for an empty slice: vecdb reads `vec == NULL` as "no
/// vector", which is not the same as a zero-length vector.
fn slice_ptr(vector: &[f32]) -> *const f32 {
    if vector.is_empty() {
        std::ptr::null()
    } else {
        vector.as_ptr()
    }
}

fn check(rc: c_int, db: *mut ffi::vecdb) -> Result<()> {
    if rc == ffi::VECDB_OK {
        Ok(())
    } else {
        Err(Error::with_detail(rc, db))
    }
}

unsafe fn record_from_c(rec: &ffi::vecdb_record, dim: usize) -> Record {
    Record {
        id: rec.id,
        vector: if rec.vec.is_null() {
            None
        } else {
            Some(std::slice::from_raw_parts(rec.vec, dim).to_vec())
        },
        payload: copy_bytes(rec.payload, rec.payload_len),
        text: text_from_c(rec.text),
    }
}

unsafe fn matches_from_c(src: &[ffi::vecdb_match]) -> Vec<Match> {
    src.iter()
        .map(|m| Match {
            id: m.id,
            score: m.score,
            payload: copy_bytes(m.payload, m.payload_len),
            text: text_from_c(m.text),
        })
        .collect()
}

unsafe fn copy_bytes(ptr: *const c_void, len: usize) -> Option<Vec<u8>> {
    if ptr.is_null() || len == 0 {
        None
    } else {
        Some(std::slice::from_raw_parts(ptr as *const u8, len).to_vec())
    }
}

unsafe fn text_from_c(ptr: *const std::os::raw::c_char) -> Option<String> {
    if ptr.is_null() {
        None
    } else {
        Some(CStr::from_ptr(ptr).to_string_lossy().into_owned())
    }
}

fn cstr(ptr: *const std::os::raw::c_char) -> String {
    if ptr.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() }
    }
}

fn strerror(status: c_int) -> String {
    format!("vecdb: {}", cstr(unsafe { ffi::vecdb_strerror(status) }))
}

fn cstring(s: &str) -> Result<CString> {
    CString::new(s).map_err(|_| Error::from_status(ffi::VECDB_ERR_INVALID))
}
