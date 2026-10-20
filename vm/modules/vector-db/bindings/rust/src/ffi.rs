//! Raw FFI declarations for `libvecdb`.
//!
//! Hand-written mirror of `include/vecdb.h` — the API is small and
//! stable, so the crate does not need bindgen (and stays free of a
//! libclang build dependency).
//!
//! Constants here must match the C header.

#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_int, c_uint, c_void};

/* status codes, mirroring vecdb_status */
pub const VECDB_OK: c_int = 0;
pub const VECDB_ERR_IO: c_int = -1;
pub const VECDB_ERR_NOMEM: c_int = -2;
pub const VECDB_ERR_INVALID: c_int = -3;
pub const VECDB_ERR_NOTFOUND: c_int = -4;
pub const VECDB_ERR_EXISTS: c_int = -5;
pub const VECDB_ERR_DIM: c_int = -6;
pub const VECDB_ERR_CORRUPT: c_int = -7;
pub const VECDB_ERR_NODIM: c_int = -8;

/* metrics, mirroring vecdb_metric */
pub const VECDB_METRIC_L2: c_int = 0;
pub const VECDB_METRIC_IP: c_int = 1;
pub const VECDB_METRIC_COSINE: c_int = 2;

/* index types, mirroring vecdb_index */
pub const VECDB_INDEX_FLAT: c_int = 0;
pub const VECDB_INDEX_HNSW: c_int = 1;

/* put flags, mirroring the enum in vecdb.h */
pub const VECDB_INSERT_ONLY: c_uint = 1 << 0;
pub const VECDB_SYNC: c_uint = 1 << 1;

#[repr(C)]
pub struct vecdb_options {
    pub dim: usize,
    pub metric: c_int,
    pub index: c_int,
    pub hnsw_m: usize,
    pub hnsw_ef_construction: usize,
    pub hnsw_ef_search: usize,
    pub seed: u64,
}

/// Opaque handle; the definition lives inside libvecdb.
pub enum vecdb {}

#[repr(C)]
#[derive(Debug, Default, Clone, Copy)]
pub struct vecdb_record {
    pub id: u64,
    pub vec: *const f32,
    pub payload: *const c_void,
    pub payload_len: usize,
    pub text: *const c_char,
}

#[repr(C)]
#[derive(Debug, Default, Clone, Copy)]
pub struct vecdb_match {
    pub id: u64,
    pub score: f32,
    pub payload: *const c_void,
    pub payload_len: usize,
    pub text: *const c_char,
}

extern "C" {
    pub fn vecdb_options_default(opt: *mut vecdb_options);

    pub fn vecdb_open(path: *const c_char, opt: *const vecdb_options) -> *mut vecdb;
    pub fn vecdb_close(db: *mut vecdb) -> c_int;
    pub fn vecdb_flush(db: *mut vecdb) -> c_int;
    pub fn vecdb_compact(db: *mut vecdb) -> c_int;

    pub fn vecdb_count(db: *const vecdb) -> usize;
    pub fn vecdb_capacity(db: *const vecdb) -> usize;
    pub fn vecdb_dim(db: *const vecdb) -> usize;
    pub fn vecdb_metric_of(db: *const vecdb) -> c_int;
    pub fn vecdb_index_of(db: *const vecdb) -> c_int;
    pub fn vecdb_path(db: *const vecdb) -> *const c_char;
    pub fn vecdb_set_ef_search(db: *mut vecdb, ef: usize) -> c_int;
    pub fn vecdb_strerror(status: c_int) -> *const c_char;
    pub fn vecdb_last_error(db: *const vecdb) -> *const c_char;
    pub fn vecdb_list_ids(db: *mut vecdb, out: *mut u64, cap: usize) -> usize;

    pub fn vecdb_put(
        db: *mut vecdb,
        id: u64,
        vec: *const f32,
        dim: usize,
        payload: *const c_void,
        payload_len: usize,
        text: *const c_char,
    ) -> c_int;

    pub fn vecdb_put_ex(
        db: *mut vecdb,
        id: u64,
        vec: *const f32,
        dim: usize,
        payload: *const c_void,
        payload_len: usize,
        text: *const c_char,
        flags: c_uint,
    ) -> c_int;

    pub fn vecdb_get(db: *mut vecdb, id: u64, out: *mut vecdb_record) -> c_int;
    pub fn vecdb_delete(db: *mut vecdb, id: u64) -> c_int;

    pub fn vecdb_search_vector(
        db: *mut vecdb,
        query: *const f32,
        dim: usize,
        k: usize,
        out: *mut vecdb_match,
        cap: usize,
    ) -> usize;

    pub fn vecdb_search_text(
        db: *mut vecdb,
        query: *const c_char,
        k: usize,
        out: *mut vecdb_match,
        cap: usize,
    ) -> usize;

    pub fn vecdb_search_hybrid(
        db: *mut vecdb,
        vec_query: *const f32,
        dim: usize,
        text_query: *const c_char,
        k: usize,
        alpha: f32,
        out: *mut vecdb_match,
        cap: usize,
    ) -> usize;
}
