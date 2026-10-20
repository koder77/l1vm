/*
 * vecdb — an embedded vector + full-text database for C.
 *
 * A small, dependency-free (libc + pthreads only) document database that
 * stores arbitrary payloads, dense float vectors and free text, and can
 * retrieve them by id, by nearest-neighbour vector search, by BM25
 * full-text search, or by a hybrid of both.
 *
 * All records live in a single file.  The file is written atomically
 * (write to <path>.tmp, fsync, rename), so a crash never corrupts an
 * existing database.
 *
 * Scores returned by the search functions are "higher is better":
 *   - metrics L2 / IP / COSINE report a similarity (for L2 that is the
 *     negated euclidean distance, so an exact match scores 0.0).
 *   - full-text search reports an Okapi BM25 score.
 *   - hybrid search reports a blended, min-max normalised score in [0,1].
 *
 * License: MIT.
 */
#ifndef VECDB_H
#define VECDB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Status codes: 0 on success, negative on error.                      */
/* ------------------------------------------------------------------ */
typedef enum {
    VECDB_OK           = 0,
    VECDB_ERR_IO       = -1,  /* file could not be read/written         */
    VECDB_ERR_NOMEM    = -2,  /* out of memory                          */
    VECDB_ERR_INVALID  = -3,  /* bad argument / malformed record        */
    VECDB_ERR_NOTFOUND = -4,  /* id does not exist                      */
    VECDB_ERR_EXISTS   = -5,  /* INSERT_ONLY and the id already exists  */
    VECDB_ERR_DIM      = -6,  /* vector dimension mismatch              */
    VECDB_ERR_CORRUPT  = -7,  /* database file is not a vecdb file      */
    VECDB_ERR_NODIM    = -8   /* vector operation on a dimension-less db*/
} vecdb_status;

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */
typedef enum {
    VECDB_METRIC_L2     = 0,  /* negated euclidean distance (default)   */
    VECDB_METRIC_IP     = 1,  /* inner product                          */
    VECDB_METRIC_COSINE = 2   /* cosine similarity                      */
} vecdb_metric;

typedef enum {
    VECDB_INDEX_FLAT = 0,     /* exact brute-force scan (default)       */
    VECDB_INDEX_HNSW = 1      /* approximate HNSW graph index           */
} vecdb_index;

/* Flags for vecdb_put_ex(). */
enum {
    VECDB_INSERT_ONLY = 1u << 0, /* fail with VECDB_ERR_EXISTS if id exists */
    VECDB_SYNC        = 1u << 1  /* flush to disk before returning          */
};

typedef struct {
    size_t       dim;                 /* vector length, 0 = learn from 1st put  */
    vecdb_metric metric;              /* distance metric                        */
    vecdb_index  index;               /* index structure for vectors            */
    /* HNSW tuning, 0 = use the defaults (M=16, efConstruction=100, efSearch=100).
     * Larger efConstruction buys recall at a roughly linear cost in build
     * time; 100 is a good default, 200-400 for hard datasets. */
    size_t       hnsw_m;
    size_t       hnsw_ef_construction;
    size_t       hnsw_ef_search;
    uint64_t     seed;                /* RNG seed for HNSW (0 = default)        */
} vecdb_options;

/* Fill `opt` with the library defaults. */
void vecdb_options_default(vecdb_options *opt);

/* ------------------------------------------------------------------ */
/* Handles and result types                                            */
/* ------------------------------------------------------------------ */
typedef struct vecdb vecdb;

/* A record as returned by vecdb_get().  All pointers are borrowed and
 * stay valid until the next mutation (put/delete/compact/flush/close)
 * on the same database handle. */
typedef struct {
    uint64_t     id;
    const float *vec;          /* NULL if the record has no vector        */
    const void  *payload;      /* NULL if the record has no payload       */
    size_t       payload_len;
    const char  *text;         /* NULL if the record has no text          */
} vecdb_record;

/* One search hit.  Same pointer-lifetime rules as vecdb_record. */
typedef struct {
    uint64_t    id;
    float       score;         /* higher is better, see header comment    */
    const void *payload;
    size_t      payload_len;
    const char *text;
} vecdb_match;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/* Open (or create) a database.  If `opt` is NULL the defaults are used.
 * Pass opt->dim = 0 to infer the dimension from the first stored vector.
 * The metric and index type are persisted in the file and win over the
 * values supplied in `opt` for an existing database. */
vecdb *vecdb_open(const char *path, const vecdb_options *opt);

/* Flush pending changes, then close.  Returns VECDB_OK even if a final
 * flush fails only because the handle was already closed. NULL is safe. */
int vecdb_close(vecdb *db);

/* Persist the database atomically. */
int vecdb_flush(vecdb *db);

/* Drop deleted records and rebuild the indexes. */
int vecdb_compact(vecdb *db);

/* ------------------------------------------------------------------ */
/* Introspection                                                       */
/* ------------------------------------------------------------------ */
size_t vecdb_count(const vecdb *db);            /* live records           */
size_t vecdb_capacity(const vecdb *db);         /* slots incl. tombstones */
size_t vecdb_dim(const vecdb *db);              /* 0 if not yet known     */
vecdb_metric vecdb_metric_of(const vecdb *db);
vecdb_index  vecdb_index_of(const vecdb *db);
const char  *vecdb_path(const vecdb *db);

/* Tune the HNSW search breadth at runtime (bigger = better recall, slower).
 * Returns VECDB_OK, or VECDB_ERR_INVALID when the db is not HNSW-indexed. */
int vecdb_set_ef_search(vecdb *db, size_t ef);

/* Short human readable description of a status code. */
const char *vecdb_strerror(int status);

/* Message describing the most recent error on this handle. */
const char *vecdb_last_error(const vecdb *db);

/* List live ids into `out` (up to `cap`).  Returns the number written.
 * Pass out=NULL/cap=0 to query the number of live records. */
size_t vecdb_list_ids(vecdb *db, uint64_t *out, size_t cap);

/* ------------------------------------------------------------------ */
/* Writing data                                                        */
/* ------------------------------------------------------------------ */

/* Store a record (upsert).  Any of vec/payload/text may be NULL.
 *   vec            dim floats, or NULL for a text/payload-only record.
 *                  The first vector defines the database dimension.
 *   dim            number of floats in `vec` (ignored when vec == NULL).
 *   payload_len    length of the opaque payload in bytes.
 *   text           NUL terminated UTF-8/ASCII text, indexed for BM25 search.
 * Returns VECDB_OK, VECDB_ERR_DIM on a vector size mismatch, or
 * VECDB_ERR_INVALID when vec == NULL but dim != 0. */
int vecdb_put(vecdb *db, uint64_t id,
              const float *vec, size_t dim,
              const void *payload, size_t payload_len,
              const char *text);

/* Same, with flags (VECDB_INSERT_ONLY, VECDB_SYNC). */
int vecdb_put_ex(vecdb *db, uint64_t id,
                 const float *vec, size_t dim,
                 const void *payload, size_t payload_len,
                 const char *text, unsigned flags);

/* Fetch one record.  Returns VECDB_OK or VECDB_ERR_NOTFOUND. */
int vecdb_get(vecdb *db, uint64_t id, vecdb_record *out);

/* Delete a record.  Returns VECDB_OK, or VECDB_ERR_NOTFOUND. */
int vecdb_delete(vecdb *db, uint64_t id);

/* ------------------------------------------------------------------ */
/* Searching                                                           */
/* ------------------------------------------------------------------ */

/* k-nearest-neighbour vector search.  `dim` must equal vecdb_dim().
 * Writes at most min(k, cap) hits to `out` (descending score order) and
 * returns the number written. */
size_t vecdb_search_vector(vecdb *db, const float *query, size_t dim, size_t k,
                           vecdb_match *out, size_t cap);

/* Okapi BM25 full-text search. */
size_t vecdb_search_text(vecdb *db, const char *query, size_t k,
                         vecdb_match *out, size_t cap);

/* Hybrid search: min-max normalises the vector and text candidate scores
 * and blends them as  alpha*vector + (1-alpha)*text.
 * alpha == 1 behaves like vector search, alpha == 0 like text search.
 * Either query may be NULL to fall back to the other modality. */
size_t vecdb_search_hybrid(vecdb *db, const float *vec_query, size_t dim,
                           const char *text_query, size_t k, float alpha,
                           vecdb_match *out, size_t cap);

/* ------------------------------------------------------------------ */
/* Payload helpers — little-endian fixed width codecs                  */
/* ------------------------------------------------------------------ */
void        vecdb_pack_i64(void *dst, int64_t v);
int64_t     vecdb_unpack_i64(const void *src);
void        vecdb_pack_u64(void *dst, uint64_t v);
uint64_t    vecdb_unpack_u64(const void *src);
void        vecdb_pack_f64(void *dst, double v);
double      vecdb_unpack_f64(const void *src);
void        vecdb_pack_f32(void *dst, float v);
float       vecdb_unpack_f32(const void *src);

/* Copy `s` (including its NUL) into buf of bufcap bytes.
 * Returns buf, or NULL when it does not fit. */
char       *vecdb_pack_str(void *buf, size_t bufcap, const char *s);

#define VECDB_I64_SIZE 8
#define VECDB_U64_SIZE 8
#define VECDB_F64_SIZE 8
#define VECDB_F32_SIZE 4

#ifdef __cplusplus
}
#endif

#endif /* VECDB_H */
