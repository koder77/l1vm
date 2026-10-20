/* Internal declarations for vecdb.  Not part of the public API. */
#ifndef VECDB_INTERNAL_H
#define VECDB_INTERNAL_H

#include "vecdb.h"

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Shared distance helpers (db.c)                                      */
/* ------------------------------------------------------------------ */

/* Similarity between a and b: higher is always better.
 *   L2     -> -euclidean_distance
 *   IP     -> dot product
 *   COSINE -> cosine similarity (0 when a vector is all zeros)         */
float vecdb_sim(vecdb_metric metric, const float *a, const float *b, size_t dim);

/* Same ordering as vecdb_sim but accumulated in float with the metric
 * switch hoisted out of the loop.  Used by the HNSW inner loop, where
 * millions of distance computations are performed per build. */
float vecdb_sim_rank(vecdb_metric metric, const float *a, const float *b, size_t dim);

/* Lowercase ASCII, split on non-alphanumerics, keep UTF-8 bytes >= 0x80
 * as token bytes, drop single-character noise and common stop words.
 * Returns a malloc'd array of malloc'd NUL-terms; the caller frees the
 * strings and the array.  *out_count receives the number of tokens.
 * Returns NULL with *out_count == 0 on allocation failure/empty input. */
char **vecdb_tokenize(const char *text, size_t *out_count);

/* ------------------------------------------------------------------ */
/* Full-text index (text.c)                                            */
/* ------------------------------------------------------------------ */
typedef struct text_index text_index_t;

text_index_t *text_index_new(void);
void          text_index_free(text_index_t *idx);

/* Add/replace the text of a slot.  `slot` indexes the caller's arrays. */
int  text_index_add(text_index_t *idx, uint32_t slot, const char *text);
/* Remove a slot from the index. */
void text_index_remove(text_index_t *idx, uint32_t slot);

/* BM25 search.  Postings whose slot is !alive[slot] are ignored.
 * Writes up to k (slot, score) pairs in descending score order. */
size_t text_index_search(const text_index_t *idx, const char *query, size_t k,
                         uint32_t *out_slots, float *out_scores,
                         const uint8_t *alive, size_t nslots);

/* ------------------------------------------------------------------ */
/* HNSW vector index (hnsw.c)                                          */
/* ------------------------------------------------------------------ */
typedef struct hnsw hnsw_t;

hnsw_t *hnsw_new(size_t dim, size_t m, size_t ef_construction, size_t ef_search,
                 vecdb_metric metric, uint64_t seed);
void    hnsw_free(hnsw_t *h);
int     hnsw_add(hnsw_t *h, uint32_t slot, const float *vec);
void    hnsw_remove(hnsw_t *h, uint32_t slot);
/* Search living nodes; `alive` may be NULL when every slot is alive. */
size_t  hnsw_search(hnsw_t *h, const float *query, size_t k,
                    uint32_t *out_slots, float *out_scores, const uint8_t *alive);
void    hnsw_set_ef(hnsw_t *h, size_t ef);
double  hnsw_avg_level(const hnsw_t *h);

#endif /* VECDB_INTERNAL_H */
