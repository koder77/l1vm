/*
 * db.c — record storage, id map, persistence and the public API.
 *
 * Layout of a vecdb file (all integers little endian):
 *
 *   char     magic[8]      "VECDB001"
 *   uint32   version       1
 *   uint32   dim           vector length, 0 = not set
 *   uint32   metric        vecdb_metric
 *   uint32   index         vecdb_index
 *   uint64   count         number of live records
 *   record[count]:
 *     uint64 id
 *     uint32 flags         bit0 vector, bit1 payload, bit2 text
 *     float  vec[dim]      when bit0
 *     uint32 payload_len + payload bytes   when bit1
 *     uint32 text_len    + text bytes      when bit2
 *
 * Saving is atomic: the data goes to "<path>.tmp", is fsync'ed and then
 * rename()d over the real file.
 */
#include "internal.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VECDB_FILE_MAGIC   "VECDB001"
#define VECDB_FILE_VERSION 1u

#define REC_FLAG_VEC     1u
#define REC_FLAG_PAYLOAD 2u
#define REC_FLAG_TEXT    4u

#define MAX_DIM ((size_t)1 << 20)
#define MAX_BLOB ((size_t)1 << 31)

/* ------------------------------------------------------------------ */
/* structures                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    uint64_t id;
    float   *vec;
    void    *payload;
    size_t   payload_len;
    char    *text;
} record_t;

/* id -> slot hash map (open addressing, linear probing, tombstones) */
typedef struct {
    uint64_t *keys;
    uint32_t *vals;
    uint8_t  *states;   /* 0 empty, 1 used, 2 tombstone */
    size_t    cap, count;
} idmap_t;

struct vecdb {
    pthread_rwlock_t lock;
    pthread_mutex_t  errmu;   /* guards err[] only */

    char        *path;
    size_t       dim;
    vecdb_metric metric;
    vecdb_index  index;
    size_t       hnsw_m, hnsw_efc, hnsw_ef;
    uint64_t     seed;

    record_t *recs;
    uint8_t  *alive;
    size_t    nrecs, cap_recs;

    idmap_t  map;
    text_index_t *text;
    hnsw_t       *hnsw;

    int  dirty;
    char err[256];
};

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */
static int fail(vecdb *db, int status, const char *fmt, ...)
{
    if (db) {
        va_list ap;
        pthread_mutex_lock(&db->errmu);
        va_start(ap, fmt);
        vsnprintf(db->err, sizeof(db->err), fmt, ap);
        va_end(ap);
        pthread_mutex_unlock(&db->errmu);
    }
    return status;
}

static void clear_err(vecdb *db)
{
    pthread_mutex_lock(&db->errmu);
    db->err[0] = '\0';
    pthread_mutex_unlock(&db->errmu);
}

static int has_err(const vecdb *db)
{
    int r;
    pthread_mutex_lock((pthread_mutex_t *)&db->errmu);
    r = db->err[0] != '\0';
    pthread_mutex_unlock((pthread_mutex_t *)&db->errmu);
    return r;
}

float vecdb_sim(vecdb_metric metric, const float *a, const float *b, size_t dim)
{
    double sum = 0.0;
    if (metric == VECDB_METRIC_IP) {
        for (size_t i = 0; i < dim; i++) sum += (double)a[i] * (double)b[i];
        return (float)sum;
    }
    if (metric == VECDB_METRIC_COSINE) {
        double dot = 0.0, na = 0.0, nb = 0.0;
        for (size_t i = 0; i < dim; i++) {
            dot += (double)a[i] * (double)b[i];
            na  += (double)a[i] * (double)a[i];
            nb  += (double)b[i] * (double)b[i];
        }
        if (na <= 0.0 || nb <= 0.0) return 0.0f;
        return (float)(dot / (sqrt(na) * sqrt(nb)));
    }
    for (size_t i = 0; i < dim; i++) {
        double d = (double)a[i] - (double)b[i];
        sum += d * d;
    }
    return (float)(-sqrt(sum));
}

/* Hot-path variant used by the HNSW inner loop.  Only the relative order
 * of scores matters here, so accumulation happens in float and the metric
 * switch is hoisted out of the loop.  The loops are written so that GCC
 * auto-vectorises them (typically 8-16 floats per AVX/FMA instruction). */
float vecdb_sim_rank(vecdb_metric metric, const float *a, const float *b, size_t dim)
{
#if defined(__GNUC__) && !defined(VECDB_NO_VECTORIZE)
    typedef float v8sf __attribute__((vector_size(32)));

    size_t i = 0;
    if (metric == VECDB_METRIC_L2) {
        v8sf acc = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (; i + 8 <= dim; i += 8) {
            v8sf x = *(const v8sf *)(a + i);
            v8sf y = *(const v8sf *)(b + i);
            v8sf d = x - y;
            acc += d * d;
        }
        float s = acc[0] + acc[1] + acc[2] + acc[3] +
                  acc[4] + acc[5] + acc[6] + acc[7];
        for (; i < dim; i++) { float d = a[i] - b[i]; s += d * d; }
        return -sqrtf(s);
    }
    if (metric == VECDB_METRIC_IP) {
        v8sf acc = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (; i + 8 <= dim; i += 8) {
            v8sf x = *(const v8sf *)(a + i);
            v8sf y = *(const v8sf *)(b + i);
            acc += x * y;
        }
        float s = acc[0] + acc[1] + acc[2] + acc[3] +
                  acc[4] + acc[5] + acc[6] + acc[7];
        for (; i < dim; i++) s += a[i] * b[i];
        return s;
    }
    /* cosine */
    {
        v8sf dot = { 0, 0, 0, 0, 0, 0, 0, 0 };
        v8sf na  = { 0, 0, 0, 0, 0, 0, 0, 0 };
        v8sf nb  = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (; i + 8 <= dim; i += 8) {
            v8sf x = *(const v8sf *)(a + i);
            v8sf y = *(const v8sf *)(b + i);
            dot += x * y;
            na  += x * x;
            nb  += y * y;
        }
        float d = dot[0] + dot[1] + dot[2] + dot[3] +
                  dot[4] + dot[5] + dot[6] + dot[7];
        float x2 = na[0] + na[1] + na[2] + na[3] + na[4] + na[5] + na[6] + na[7];
        float y2 = nb[0] + nb[1] + nb[2] + nb[3] + nb[4] + nb[5] + nb[6] + nb[7];
        for (; i < dim; i++) { d += a[i] * b[i]; x2 += a[i] * a[i]; y2 += b[i] * b[i]; }
        if (x2 <= 0.0f || y2 <= 0.0f) return 0.0f;
        return d / (sqrtf(x2) * sqrtf(y2));
    }
#else
    if (metric == VECDB_METRIC_L2) {
        float sum = 0.0f;
        for (size_t i = 0; i < dim; i++) { float d = a[i] - b[i]; sum += d * d; }
        return -sqrtf(sum);
    }
    if (metric == VECDB_METRIC_IP) {
        float sum = 0.0f;
        for (size_t i = 0; i < dim; i++) sum += a[i] * b[i];
        return sum;
    }
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    if (na <= 0.0f || nb <= 0.0f) return 0.0f;
    return dot / (sqrtf(na) * sqrtf(nb));
#endif
}

static size_t next_pow2(size_t n)
{
    size_t p = 8;
    while (p < n) p *= 2;
    return p;
}

/* ------------------------------------------------------------------ */
/* id map                                                              */
/* ------------------------------------------------------------------ */
static uint64_t hash_id(uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

static void idmap_free(idmap_t *m)
{
    free(m->keys);
    free(m->vals);
    free(m->states);
    memset(m, 0, sizeof(*m));
}

static int idmap_init(idmap_t *m, size_t hint)
{
    memset(m, 0, sizeof(*m));
    m->cap = next_pow2(hint * 2 + 16);
    m->keys = (uint64_t *)malloc(m->cap * sizeof(uint64_t));
    m->vals = (uint32_t *)malloc(m->cap * sizeof(uint32_t));
    m->states = (uint8_t *)calloc(m->cap, 1);
    if (!m->keys || !m->vals || !m->states) {
        idmap_free(m);
        return -1;
    }
    return 0;
}

static int idmap_grow(idmap_t *m)
{
    size_t ncap = m->cap ? m->cap * 2 : 64;
    idmap_t n;
    if (idmap_init(&n, ncap / 2) != 0) return -1;
    for (size_t i = 0; i < m->cap; i++) {
        if (m->states[i] != 1) continue;
        size_t mask = n.cap - 1;
        size_t j = (size_t)hash_id(m->keys[i]) & mask;
        while (n.states[j] == 1) j = (j + 1) & mask;
        n.keys[j] = m->keys[i];
        n.vals[j] = m->vals[i];
        n.states[j] = 1;
        n.count++;
    }
    idmap_free(m);
    *m = n;
    return 0;
}

static long idmap_get(const idmap_t *m, uint64_t id)
{
    if (m->cap == 0) return -1;
    size_t mask = m->cap - 1;
    size_t i = (size_t)hash_id(id) & mask;
    for (size_t p = 0; p < m->cap; p++) {
        if (m->states[i] == 0) return -1;
        if (m->states[i] == 1 && m->keys[i] == id) return (long)m->vals[i];
        i = (i + 1) & mask;
    }
    return -1;
}

static int idmap_put(idmap_t *m, uint64_t id, uint32_t slot)
{
    if (m->cap == 0 || (m->count + 1) * 4 > m->cap * 3) {
        if (idmap_grow(m) != 0) return -1;
    }
    size_t mask = m->cap - 1;
    size_t i = (size_t)hash_id(id) & mask;
    size_t tomb = (size_t)-1;
    for (;;) {
        if (m->states[i] == 0) {
            size_t at = (tomb != (size_t)-1) ? tomb : i;
            m->keys[at] = id;
            m->vals[at] = slot;
            m->states[at] = 1;
            m->count++;
            return 0;
        }
        if (m->states[i] == 1) {
            if (m->keys[i] == id) { m->vals[i] = slot; return 0; }
        } else if (tomb == (size_t)-1) {
            tomb = i;
        }
        i = (i + 1) & mask;
    }
}

static void idmap_del(idmap_t *m, uint64_t id)
{
    if (m->cap == 0) return;
    size_t mask = m->cap - 1;
    size_t i = (size_t)hash_id(id) & mask;
    for (size_t p = 0; p < m->cap; p++) {
        if (m->states[i] == 0) return;
        if (m->states[i] == 1 && m->keys[i] == id) {
            m->states[i] = 2;
            m->count--;
            return;
        }
        i = (i + 1) & mask;
    }
}

/* ------------------------------------------------------------------ */
/* top-k selection helpers (flat scan / hybrid merge)                  */
/* ------------------------------------------------------------------ */
typedef struct {
    float    score;
    uint32_t slot;
} hitpair_t;

typedef struct {
    hitpair_t *a;
    size_t     n, cap;
} topk_t;

static int topk_init(topk_t *t, size_t k)
{
    if (k == 0) k = 1;
    t->a = (hitpair_t *)malloc(k * sizeof(hitpair_t));
    if (!t->a) return -1;
    t->n = 0;
    t->cap = k;
    return 0;
}

static void topk_free(topk_t *t)
{
    free(t->a);
    t->a = NULL;
    t->n = t->cap = 0;
}

/* min-heap on score: index 0 is the smallest (worst) element */
static void topk_offer(topk_t *t, float score, uint32_t slot)
{
    if (t->n < t->cap) {
        size_t i = t->n++;
        t->a[i].score = score;
        t->a[i].slot = slot;
        while (i > 0) {
            size_t p = (i - 1) / 2;
            if (t->a[i].score < t->a[p].score) {
                hitpair_t tmp = t->a[i]; t->a[i] = t->a[p]; t->a[p] = tmp;
                i = p;
            } else break;
        }
        return;
    }
    if (score <= t->a[0].score) return;
    t->a[0].score = score;
    t->a[0].slot = slot;
    size_t i = 0;
    for (;;) {
        size_t l = 2 * i + 1, r = l + 1, best = i;
        if (l < t->n && t->a[l].score < t->a[best].score) best = l;
        if (r < t->n && t->a[r].score < t->a[best].score) best = r;
        if (best == i) break;
        hitpair_t tmp = t->a[i]; t->a[i] = t->a[best]; t->a[best] = tmp;
        i = best;
    }
}

static int cmp_score_desc(const void *x, const void *y)
{
    const hitpair_t *a = (const hitpair_t *)x, *b = (const hitpair_t *)y;
    if (a->score < b->score) return 1;
    if (a->score > b->score) return -1;
    return 0;
}

static size_t topk_finish(topk_t *t, uint32_t *slots, float *scores)
{
    size_t n = t->n;
    qsort(t->a, n, sizeof(hitpair_t), cmp_score_desc);
    for (size_t i = 0; i < n; i++) {
        slots[i] = t->a[i].slot;
        scores[i] = t->a[i].score;
    }
    topk_free(t);
    return n;
}

/* ------------------------------------------------------------------ */
/* record storage                                                      */
/* ------------------------------------------------------------------ */
static int recs_reserve(vecdb *db, size_t need)
{
    if (need <= db->cap_recs) return 0;
    size_t ncap = db->cap_recs ? db->cap_recs : 64;
    while (ncap < need) ncap *= 2;

    record_t *nr = (record_t *)realloc(db->recs, ncap * sizeof(record_t));
    if (!nr) return -1;
    db->recs = nr;

    uint8_t *na = (uint8_t *)realloc(db->alive, ncap);
    if (!na) return -1;
    memset(na + db->cap_recs, 0, ncap - db->cap_recs);
    db->alive = na;

    db->cap_recs = ncap;
    return 0;
}

static void record_clear(record_t *r)
{
    free(r->vec);
    free(r->payload);
    free(r->text);
    memset(r, 0, sizeof(*r));
}

/* ------------------------------------------------------------------ */
/* index management                                                    */
/* ------------------------------------------------------------------ */
static void db_clear_indexes(vecdb *db)
{
    text_index_free(db->text);
    db->text = NULL;
    hnsw_free(db->hnsw);
    db->hnsw = NULL;
}

static int db_build_indexes(vecdb *db)
{
    db_clear_indexes(db);
    db->text = text_index_new();
    if (!db->text) return VECDB_ERR_NOMEM;
    if (db->index == VECDB_INDEX_HNSW && db->dim > 0) {
        db->hnsw = hnsw_new(db->dim, db->hnsw_m, db->hnsw_efc, db->hnsw_ef,
                            db->metric, db->seed);
        if (!db->hnsw) return VECDB_ERR_NOMEM;
    }
    return VECDB_OK;
}

/* Add one vector to the vector index, creating the HNSW graph lazily so
 * that a database can learn its dimension from the first record. */
static int db_index_add_vec(vecdb *db, uint32_t slot, const float *vec)
{
    if (db->index != VECDB_INDEX_HNSW) return VECDB_OK;  /* flat: nothing */
    if (!db->hnsw) {
        db->hnsw = hnsw_new(db->dim, db->hnsw_m, db->hnsw_efc, db->hnsw_ef,
                            db->metric, db->seed);
        if (!db->hnsw) return VECDB_ERR_NOMEM;
    }
    return hnsw_add(db->hnsw, slot, vec);
}

/* ------------------------------------------------------------------ */
/* write path (caller holds the write lock)                            */
/* ------------------------------------------------------------------ */
static int put_locked(vecdb *db, uint64_t id,
                      const float *vec, size_t dim,
                      const void *payload, size_t payload_len,
                      const char *text, unsigned flags)
{
    if (vec) {
        if (dim == 0 || dim > MAX_DIM) return fail(db, VECDB_ERR_DIM, "invalid dimension %zu", dim);
        if (db->dim == 0) db->dim = dim;
        else if (dim != db->dim)
            return fail(db, VECDB_ERR_DIM, "dimension mismatch: db has %zu, got %zu",
                        db->dim, dim);
    } else if (dim != 0) {
        return fail(db, VECDB_ERR_INVALID, "vec is NULL but dim is %zu", dim);
    }
    if (payload_len > MAX_BLOB || (payload_len && !payload))
        return fail(db, VECDB_ERR_INVALID, "invalid payload");
    if (text && strlen(text) > MAX_BLOB)
        return fail(db, VECDB_ERR_INVALID, "text too large");

    long found = idmap_get(&db->map, id);
    uint32_t slot;

    if (found >= 0) {
        if (flags & VECDB_INSERT_ONLY)
            return fail(db, VECDB_ERR_EXISTS, "id %llu already exists", (unsigned long long)id);
        slot = (uint32_t)found;
        record_t *old = &db->recs[slot];
        if (old->text) text_index_remove(db->text, slot);
        if (old->vec && db->hnsw) hnsw_remove(db->hnsw, slot);
        record_clear(old);
    } else {
        if (recs_reserve(db, db->nrecs + 1) != 0)
            return fail(db, VECDB_ERR_NOMEM, "out of memory growing record table");
        slot = (uint32_t)db->nrecs++;
        db->alive[slot] = 1;
        memset(&db->recs[slot], 0, sizeof(record_t));
        if (idmap_put(&db->map, id, slot) != 0) {
            db->nrecs--;
            db->alive[slot] = 0;
            return fail(db, VECDB_ERR_NOMEM, "out of memory growing id map");
        }
    }

    record_t *r = &db->recs[slot];
    r->id = id;

    if (vec) {
        r->vec = (float *)malloc(db->dim * sizeof(float));
        if (!r->vec) return fail(db, VECDB_ERR_NOMEM, "out of memory for vector");
        memcpy(r->vec, vec, db->dim * sizeof(float));
    }
    if (payload && payload_len) {
        r->payload = malloc(payload_len);
        if (!r->payload) return fail(db, VECDB_ERR_NOMEM, "out of memory for payload");
        memcpy(r->payload, payload, payload_len);
        r->payload_len = payload_len;
    }
    if (text && *text) {
        r->text = strdup(text);
        if (!r->text) return fail(db, VECDB_ERR_NOMEM, "out of memory for text");
    }

    if (r->vec) {
        int rc = db_index_add_vec(db, slot, r->vec);
        if (rc != VECDB_OK) return fail(db, rc, "failed to index vector");
    }
    if (r->text) {
        int rc = text_index_add(db->text, slot, r->text);
        if (rc != VECDB_OK) return fail(db, rc, "failed to index text");
    }

    db->dirty = 1;
    if (flags & VECDB_SYNC) return vecdb_flush(db);
    return VECDB_OK;
}

/* ------------------------------------------------------------------ */
/* persistence                                                         */
/* ------------------------------------------------------------------ */
static int fwrite_all(FILE *f, const void *p, size_t n)
{
    return n == 0 || fwrite(p, 1, n, f) == n;
}

static int fread_all(FILE *f, void *p, size_t n)
{
    return n == 0 || fread(p, 1, n, f) == n;
}

static int db_save(vecdb *db)
{
    size_t plen = strlen(db->path) + 5;
    char *tmp = (char *)malloc(plen);
    if (!tmp) return fail(db, VECDB_ERR_NOMEM, "out of memory");
    snprintf(tmp, plen, "%s.tmp", db->path);

    FILE *f = fopen(tmp, "wb");
    if (!f) {
        int rc = fail(db, VECDB_ERR_IO, "cannot create %s: %s", tmp, strerror(errno));
        free(tmp);
        return rc;
    }

    uint32_t version = VECDB_FILE_VERSION;
    uint32_t dim = (uint32_t)db->dim;
    uint32_t metric = (uint32_t)db->metric;
    uint32_t index = (uint32_t)db->index;
    uint64_t count = (uint64_t)db->map.count;

    int ok = 1;
    ok &= fwrite_all(f, VECDB_FILE_MAGIC, 8);
    ok &= fwrite_all(f, &version, 4);
    ok &= fwrite_all(f, &dim, 4);
    ok &= fwrite_all(f, &metric, 4);
    ok &= fwrite_all(f, &index, 4);
    ok &= fwrite_all(f, &count, 8);

    for (size_t i = 0; ok && i < db->nrecs; i++) {
        if (!db->alive[i]) continue;
        const record_t *r = &db->recs[i];
        uint32_t flags = 0;
        if (r->vec) flags |= REC_FLAG_VEC;
        if (r->payload && r->payload_len) flags |= REC_FLAG_PAYLOAD;
        if (r->text) flags |= REC_FLAG_TEXT;
        ok &= fwrite_all(f, &r->id, 8);
        ok &= fwrite_all(f, &flags, 4);
        if (r->vec) ok &= fwrite_all(f, r->vec, db->dim * sizeof(float));
        if (r->payload) {
            uint32_t n = (uint32_t)r->payload_len;
            ok &= fwrite_all(f, &n, 4);
            ok &= fwrite_all(f, r->payload, r->payload_len);
        }
        if (r->text) {
            uint32_t n = (uint32_t)strlen(r->text);
            ok &= fwrite_all(f, &n, 4);
            ok &= fwrite_all(f, r->text, n);
        }
    }

    if (ok) ok = fflush(f) == 0;
    if (ok) ok = fsync(fileno(f)) == 0;
    if (fclose(f) != 0) ok = 0;

    if (!ok) {
        remove(tmp);
        free(tmp);
        return fail(db, VECDB_ERR_IO, "failed writing %s", db->path);
    }
    if (rename(tmp, db->path) != 0) {
        int rc = fail(db, VECDB_ERR_IO, "failed to rename %s: %s", tmp, strerror(errno));
        remove(tmp);
        free(tmp);
        return rc;
    }
    free(tmp);
    db->dirty = 0;
    return VECDB_OK;
}

static int db_load(vecdb *db)
{
    FILE *f = fopen(db->path, "rb");
    if (!f) return fail(db, VECDB_ERR_IO, "cannot open %s: %s", db->path, strerror(errno));

    char magic[8];
    uint32_t version, dim, metric, index;
    uint64_t count;

    if (!fread_all(f, magic, 8) || memcmp(magic, VECDB_FILE_MAGIC, 8) != 0) {
        fclose(f);
        return fail(db, VECDB_ERR_CORRUPT, "%s is not a vecdb database", db->path);
    }
    if (!fread_all(f, &version, 4) || version != VECDB_FILE_VERSION) {
        fclose(f);
        return fail(db, VECDB_ERR_CORRUPT, "%s has an unsupported version", db->path);
    }
    if (!fread_all(f, &dim, 4) || !fread_all(f, &metric, 4) ||
        !fread_all(f, &index, 4) || !fread_all(f, &count, 8)) {
        fclose(f);
        return fail(db, VECDB_ERR_CORRUPT, "%s is truncated", db->path);
    }
    if (dim > MAX_DIM || metric > VECDB_METRIC_COSINE || index > VECDB_INDEX_HNSW) {
        fclose(f);
        return fail(db, VECDB_ERR_CORRUPT, "%s has an invalid header", db->path);
    }
    if (db->dim != 0 && db->dim != dim) {
        fclose(f);
        return fail(db, VECDB_ERR_DIM, "%s has dimension %u, requested %zu",
                    db->path, dim, db->dim);
    }

    /* reject absurd record counts (min record size is 12 bytes) */
    long fsize = 0;
    if (fseek(f, 0, SEEK_END) == 0) fsize = ftell(f);
    if (fsize >= 0 && count > (uint64_t)fsize / 12 + 1) {
        fclose(f);
        return fail(db, VECDB_ERR_CORRUPT, "%s has an invalid record count", db->path);
    }
    fseek(f, 32, SEEK_SET);   /* 8 magic + 4 version + 4 dim + 4 metric + 4 index + 8 count */

    db->dim = dim;
    db->metric = (vecdb_metric)metric;
    db->index = (vecdb_index)index;

    int rc = db_build_indexes(db);
    if (rc != VECDB_OK) { fclose(f); return fail(db, rc, "out of memory"); }

    for (uint64_t i = 0; i < count; i++) {
        uint64_t id;
        uint32_t flags;
        if (!fread_all(f, &id, 8) || !fread_all(f, &flags, 4)) {
            fclose(f);
            return fail(db, VECDB_ERR_CORRUPT, "%s is truncated", db->path);
        }

        float *vec = NULL;
        void  *payload = NULL;
        size_t payload_len = 0;
        char  *text = NULL;

        if (flags & REC_FLAG_VEC) {
            vec = (float *)malloc(dim * sizeof(float));
            if (!vec || !fread_all(f, vec, dim * sizeof(float))) {
                free(vec); fclose(f);
                return fail(db, VECDB_ERR_CORRUPT, "%s is truncated", db->path);
            }
        }
        if (flags & REC_FLAG_PAYLOAD) {
            uint32_t n;
            if (!fread_all(f, &n, 4) || n > MAX_BLOB ||
                (fsize >= 0 && (long)n > fsize)) {
                free(vec); fclose(f);
                return fail(db, VECDB_ERR_CORRUPT, "%s has an invalid payload", db->path);
            }
            payload = malloc(n ? n : 1);
            if (!payload || !fread_all(f, payload, n)) {
                free(vec); free(payload); fclose(f);
                return fail(db, VECDB_ERR_CORRUPT, "%s is truncated", db->path);
            }
            payload_len = n;
        }
        if (flags & REC_FLAG_TEXT) {
            uint32_t n;
            if (!fread_all(f, &n, 4) || n > MAX_BLOB ||
                (fsize >= 0 && (long)n > fsize)) {
                free(vec); free(payload); fclose(f);
                return fail(db, VECDB_ERR_CORRUPT, "%s has an invalid text", db->path);
            }
            text = (char *)malloc((size_t)n + 1);
            if (!text || !fread_all(f, text, n)) {
                free(vec); free(payload); free(text); fclose(f);
                return fail(db, VECDB_ERR_CORRUPT, "%s is truncated", db->path);
            }
            text[n] = '\0';
        }

        rc = put_locked(db, id, vec, dim, payload, payload_len, text, 0);
        free(vec);
        free(payload);
        free(text);
        if (rc != VECDB_OK) { fclose(f); return rc; }
    }

    fclose(f);
    db->dirty = 0;
    return VECDB_OK;
}

/* ------------------------------------------------------------------ */
/* search internals (caller holds at least the read lock)              */
/* ------------------------------------------------------------------ */
static size_t do_search_vector(vecdb *db, const float *q, size_t k,
                               uint32_t *slots, float *scores)
{
    if (db->index == VECDB_INDEX_HNSW && db->hnsw)
        return hnsw_search(db->hnsw, q, k, slots, scores, db->alive);

    topk_t tk;
    if (topk_init(&tk, k) != 0) return 0;
    for (size_t i = 0; i < db->nrecs; i++) {
        if (!db->alive[i] || !db->recs[i].vec) continue;
        float s = vecdb_sim_rank(db->metric, q, db->recs[i].vec, db->dim);
        topk_offer(&tk, s, (uint32_t)i);
    }
    return topk_finish(&tk, slots, scores);
}

static size_t do_search_text(vecdb *db, const char *q, size_t k,
                             uint32_t *slots, float *scores)
{
    return text_index_search(db->text, q, k, slots, scores, db->alive, db->nrecs);
}

static void fill_matches(vecdb *db, size_t n,
                         const uint32_t *slots, const float *scores,
                         vecdb_match *out, size_t cap)
{
    if (n > cap) n = cap;
    for (size_t i = 0; i < n; i++) {
        const record_t *r = &db->recs[slots[i]];
        out[i].id = r->id;
        out[i].score = scores[i];
        out[i].payload = r->payload;
        out[i].payload_len = r->payload_len;
        out[i].text = r->text;
    }
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */
void vecdb_options_default(vecdb_options *opt)
{
    if (!opt) return;
    opt->dim = 0;
    opt->metric = VECDB_METRIC_L2;
    opt->index = VECDB_INDEX_FLAT;
    opt->hnsw_m = 16;
    opt->hnsw_ef_construction = 100;
    opt->hnsw_ef_search = 100;
    opt->seed = 42;
}

static void db_free_contents(vecdb *db);

vecdb *vecdb_open(const char *path, const vecdb_options *opt)
{
    if (!path || !*path) return NULL;

    vecdb_options d;
    vecdb_options_default(&d);
    if (opt) d = *opt;
    if (d.hnsw_m == 0) d.hnsw_m = 16;
    if (d.hnsw_ef_construction == 0) d.hnsw_ef_construction = 100;
    if (d.hnsw_ef_search == 0) d.hnsw_ef_search = 100;

    vecdb *db = (vecdb *)calloc(1, sizeof(vecdb));
    if (!db) return NULL;

    pthread_rwlock_init(&db->lock, NULL);
    pthread_mutex_init(&db->errmu, NULL);
    db->path = strdup(path);
    db->dim = d.dim;
    db->metric = d.metric;
    db->index = d.index;
    db->hnsw_m = d.hnsw_m;
    db->hnsw_efc = d.hnsw_ef_construction;
    db->hnsw_ef = d.hnsw_ef_search;
    db->seed = d.seed ? d.seed : 42;

    if (!db->path || idmap_init(&db->map, 64) != 0) {
        free(db->path);
        pthread_mutex_destroy(&db->errmu);
        pthread_rwlock_destroy(&db->lock);
        free(db);
        return NULL;
    }
    if (db_build_indexes(db) != VECDB_OK) {
        db_free_contents(db);
        free(db->path);
        pthread_mutex_destroy(&db->errmu);
        pthread_rwlock_destroy(&db->lock);
        free(db);
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        if (db_load(db) != VECDB_OK) {
            /* db_load may have inserted records before hitting the bad
             * one, so tear the partial state down completely. */
            db_free_contents(db);
            free(db->path);
            pthread_mutex_destroy(&db->errmu);
            pthread_rwlock_destroy(&db->lock);
            free(db);
            return NULL;
        }
    } else {
        db->dirty = 1;   /* create the file on close/flush */
    }
    return db;
}

static void db_free_contents(vecdb *db)
{
    for (size_t i = 0; i < db->nrecs; i++) record_clear(&db->recs[i]);
    free(db->recs);
    db->recs = NULL;
    free(db->alive);
    db->alive = NULL;
    db->nrecs = db->cap_recs = 0;
    db_clear_indexes(db);
    idmap_free(&db->map);
}

static void db_release(vecdb *db)
{
    db_free_contents(db);
    free(db->path);
    pthread_mutex_destroy(&db->errmu);
    pthread_rwlock_destroy(&db->lock);
    free(db);
}

int vecdb_flush(vecdb *db)
{
    if (!db) return VECDB_ERR_INVALID;
    pthread_rwlock_wrlock(&db->lock);
    int rc = db->dirty ? db_save(db) : VECDB_OK;
    if (rc != VECDB_OK && !has_err(db))
        fail(db, VECDB_ERR_IO, "flush failed");
    pthread_rwlock_unlock(&db->lock);
    return rc;
}

int vecdb_close(vecdb *db)
{
    if (!db) return VECDB_OK;
    pthread_rwlock_wrlock(&db->lock);
    int rc = VECDB_OK;
    if (db->dirty) rc = db_save(db);
    pthread_rwlock_unlock(&db->lock);
    db_release(db);
    return rc;
}

int vecdb_compact(vecdb *db)
{
    if (!db) return VECDB_ERR_INVALID;
    pthread_rwlock_wrlock(&db->lock);

    size_t n = db->map.count;
    record_t *nr = n ? (record_t *)calloc(n, sizeof(record_t)) : NULL;
    uint8_t  *na = n ? (uint8_t *)malloc(n) : NULL;
    idmap_t   nm;
    text_index_t *nt = NULL;
    hnsw_t *nh = NULL;
    int rc = VECDB_OK;

    if ((n && (!nr || !na)) || idmap_init(&nm, n + 8) != 0) {
        rc = fail(db, VECDB_ERR_NOMEM, "out of memory during compaction");
        goto done;
    }
    nt = text_index_new();
    if (!nt) { rc = fail(db, VECDB_ERR_NOMEM, "out of memory"); goto done; }
    if (db->index == VECDB_INDEX_HNSW && db->dim > 0 && n > 0) {
        nh = hnsw_new(db->dim, db->hnsw_m, db->hnsw_efc, db->hnsw_ef, db->metric, db->seed);
        if (!nh) { rc = fail(db, VECDB_ERR_NOMEM, "out of memory"); goto done; }
    }

    size_t j = 0;
    for (size_t i = 0; i < db->nrecs; i++) {
        if (!db->alive[i]) continue;
        const record_t *src = &db->recs[i];
        record_t *dst = &nr[j];
        dst->id = src->id;
        if (src->vec) {
            dst->vec = (float *)malloc(db->dim * sizeof(float));
            if (!dst->vec) { rc = fail(db, VECDB_ERR_NOMEM, "out of memory"); goto done; }
            memcpy(dst->vec, src->vec, db->dim * sizeof(float));
        }
        if (src->payload && src->payload_len) {
            dst->payload = malloc(src->payload_len);
            if (!dst->payload) { rc = fail(db, VECDB_ERR_NOMEM, "out of memory"); goto done; }
            memcpy(dst->payload, src->payload, src->payload_len);
            dst->payload_len = src->payload_len;
        }
        if (src->text) {
            dst->text = strdup(src->text);
            if (!dst->text) { rc = fail(db, VECDB_ERR_NOMEM, "out of memory"); goto done; }
        }
        na[j] = 1;
        if (idmap_put(&nm, dst->id, (uint32_t)j) != 0) {
            rc = fail(db, VECDB_ERR_NOMEM, "out of memory"); goto done;
        }
        if (dst->vec && nh && hnsw_add(nh, (uint32_t)j, dst->vec) != VECDB_OK) {
            rc = fail(db, VECDB_ERR_NOMEM, "out of memory"); goto done;
        }
        if (dst->text && text_index_add(nt, (uint32_t)j, dst->text) != VECDB_OK) {
            rc = fail(db, VECDB_ERR_NOMEM, "out of memory"); goto done;
        }
        j++;
    }

    /* success: swap the old state out */
    for (size_t i = 0; i < db->nrecs; i++) record_clear(&db->recs[i]);
    free(db->recs);
    free(db->alive);
    db_clear_indexes(db);
    idmap_free(&db->map);

    db->recs = nr;
    db->alive = na;
    db->nrecs = j;
    db->cap_recs = n;
    db->map = nm;
    db->text = nt;
    db->hnsw = nh;
    db->dirty = 1;
    pthread_rwlock_unlock(&db->lock);
    return VECDB_OK;

done:
    if (nr) {
        for (size_t i = 0; i < n; i++) record_clear(&nr[i]);
        free(nr);
    }
    free(na);
    idmap_free(&nm);
    text_index_free(nt);
    hnsw_free(nh);
    pthread_rwlock_unlock(&db->lock);
    return rc;
}

/* ------------------------------------------------------------------ */
/* introspection                                                       */
/* ------------------------------------------------------------------ */
size_t vecdb_count(const vecdb *db)
{
    return db ? db->map.count : 0;
}

size_t vecdb_capacity(const vecdb *db)
{
    return db ? db->nrecs : 0;
}

size_t vecdb_dim(const vecdb *db)
{
    return db ? db->dim : 0;
}

vecdb_metric vecdb_metric_of(const vecdb *db)
{
    return db ? db->metric : VECDB_METRIC_L2;
}

vecdb_index vecdb_index_of(const vecdb *db)
{
    return db ? db->index : VECDB_INDEX_FLAT;
}

const char *vecdb_path(const vecdb *db)
{
    return db ? db->path : NULL;
}

int vecdb_set_ef_search(vecdb *db, size_t ef)
{
    if (!db || db->index != VECDB_INDEX_HNSW || !db->hnsw)
        return VECDB_ERR_INVALID;
    pthread_rwlock_wrlock(&db->lock);
    hnsw_set_ef(db->hnsw, ef);
    pthread_rwlock_unlock(&db->lock);
    return VECDB_OK;
}

const char *vecdb_strerror(int status)
{
    switch (status) {
    case VECDB_OK:           return "ok";
    case VECDB_ERR_IO:       return "I/O error";
    case VECDB_ERR_NOMEM:    return "out of memory";
    case VECDB_ERR_INVALID:  return "invalid argument";
    case VECDB_ERR_NOTFOUND: return "not found";
    case VECDB_ERR_EXISTS:   return "already exists";
    case VECDB_ERR_DIM:      return "dimension mismatch";
    case VECDB_ERR_CORRUPT:  return "corrupt database";
    case VECDB_ERR_NODIM:    return "no vector dimension set";
    default:                 return "unknown error";
    }
}

const char *vecdb_last_error(const vecdb *db)
{
    /* thread-local copy: safe to hold onto across threads */
    static __thread char buf[256];
    if (!db) return "";
    pthread_mutex_lock((pthread_mutex_t *)&db->errmu);
    memcpy(buf, db->err, sizeof(buf));
    pthread_mutex_unlock((pthread_mutex_t *)&db->errmu);
    return buf[0] ? buf : "";
}

size_t vecdb_list_ids(vecdb *db, uint64_t *out, size_t cap)
{
    if (!db) return 0;
    pthread_rwlock_rdlock(&db->lock);
    if (!out || cap == 0) {
        size_t n = db->map.count;
        pthread_rwlock_unlock(&db->lock);
        return n;
    }
    size_t n = 0;
    for (size_t i = 0; i < db->nrecs && n < cap; i++) {
        if (db->alive[i]) out[n++] = db->recs[i].id;
    }
    pthread_rwlock_unlock(&db->lock);
    return n;
}

/* ------------------------------------------------------------------ */
/* writing                                                             */
/* ------------------------------------------------------------------ */
int vecdb_put_ex(vecdb *db, uint64_t id,
                 const float *vec, size_t dim,
                 const void *payload, size_t payload_len,
                 const char *text, unsigned flags)
{
    if (!db) return VECDB_ERR_INVALID;
    pthread_rwlock_wrlock(&db->lock);
    clear_err(db);
    int rc = put_locked(db, id, vec, dim, payload, payload_len, text, flags);
    pthread_rwlock_unlock(&db->lock);
    return rc;
}

int vecdb_put(vecdb *db, uint64_t id,
              const float *vec, size_t dim,
              const void *payload, size_t payload_len,
              const char *text)
{
    return vecdb_put_ex(db, id, vec, dim, payload, payload_len, text, 0);
}

int vecdb_get(vecdb *db, uint64_t id, vecdb_record *out)
{
    if (!db || !out) return VECDB_ERR_INVALID;
    pthread_rwlock_rdlock(&db->lock);
    long slot = idmap_get(&db->map, id);
    if (slot < 0) {
        pthread_rwlock_unlock(&db->lock);
        return VECDB_ERR_NOTFOUND;
    }
    const record_t *r = &db->recs[slot];
    out->id = r->id;
    out->vec = r->vec;
    out->payload = r->payload;
    out->payload_len = r->payload_len;
    out->text = r->text;
    pthread_rwlock_unlock(&db->lock);
    return VECDB_OK;
}

int vecdb_delete(vecdb *db, uint64_t id)
{
    if (!db) return VECDB_ERR_INVALID;
    pthread_rwlock_wrlock(&db->lock);
    long slot = idmap_get(&db->map, id);
    if (slot < 0) {
        pthread_rwlock_unlock(&db->lock);
        return VECDB_ERR_NOTFOUND;
    }
    record_t *r = &db->recs[slot];
    if (r->text) text_index_remove(db->text, (uint32_t)slot);
    if (r->vec && db->hnsw) hnsw_remove(db->hnsw, (uint32_t)slot);
    record_clear(r);
    db->alive[slot] = 0;
    idmap_del(&db->map, id);
    db->dirty = 1;
    pthread_rwlock_unlock(&db->lock);
    return VECDB_OK;
}

/* ------------------------------------------------------------------ */
/* searching                                                           */
/* ------------------------------------------------------------------ */
size_t vecdb_search_vector(vecdb *db, const float *query, size_t dim, size_t k,
                           vecdb_match *out, size_t cap)
{
    if (!db || !query || !out || cap == 0) return 0;
    pthread_rwlock_rdlock(&db->lock);
    clear_err(db);
    if (db->dim == 0) {
        fail(db, VECDB_ERR_NODIM, "database has no vector dimension");
        pthread_rwlock_unlock(&db->lock);
        return 0;
    }
    if (dim != db->dim) {
        fail(db, VECDB_ERR_DIM, "query dimension %zu != %zu", dim, db->dim);
        pthread_rwlock_unlock(&db->lock);
        return 0;
    }
    if (k > cap) k = cap;
    if (k == 0) {
        pthread_rwlock_unlock(&db->lock);
        return 0;
    }
    uint32_t *slots = (uint32_t *)malloc(k * sizeof(uint32_t));
    float    *scores = (float *)malloc(k * sizeof(float));
    size_t n = 0;
    if (slots && scores)
        n = do_search_vector(db, query, k, slots, scores);
    else
        fail(db, VECDB_ERR_NOMEM, "out of memory");
    if (n) fill_matches(db, n, slots, scores, out, cap);
    free(slots);
    free(scores);
    pthread_rwlock_unlock(&db->lock);
    return n;
}

size_t vecdb_search_text(vecdb *db, const char *query, size_t k,
                         vecdb_match *out, size_t cap)
{
    if (!db || !query || !out || cap == 0) return 0;
    pthread_rwlock_rdlock(&db->lock);
    clear_err(db);
    if (k > cap) k = cap;
    if (k == 0) {
        pthread_rwlock_unlock(&db->lock);
        return 0;
    }
    uint32_t *slots = (uint32_t *)malloc(k * sizeof(uint32_t));
    float    *scores = (float *)malloc(k * sizeof(float));
    size_t n = 0;
    if (slots && scores)
        n = do_search_text(db, query, k, slots, scores);
    else
        fail(db, VECDB_ERR_NOMEM, "out of memory");
    if (n) fill_matches(db, n, slots, scores, out, cap);
    free(slots);
    free(scores);
    pthread_rwlock_unlock(&db->lock);
    return n;
}

/* min-max normalise `scores[0..n)` in place (guards against n == 1). */
static void normalize_minmax(float *scores, size_t n)
{
    if (n == 0) return;
    float lo = scores[0], hi = scores[0];
    for (size_t i = 1; i < n; i++) {
        if (scores[i] < lo) lo = scores[i];
        if (scores[i] > hi) hi = scores[i];
    }
    float range = hi - lo;
    if (range <= 0.0f) {
        for (size_t i = 0; i < n; i++) scores[i] = 1.0f;
        return;
    }
    for (size_t i = 0; i < n; i++) scores[i] = (scores[i] - lo) / range;
}

size_t vecdb_search_hybrid(vecdb *db, const float *vec_query, size_t dim,
                           const char *text_query, size_t k, float alpha,
                           vecdb_match *out, size_t cap)
{
    if (!db || !out || cap == 0) return 0;
    if (!vec_query && !text_query) return 0;
    if (k > cap) k = cap;
    if (k == 0) {
        pthread_rwlock_unlock(&db->lock);
        return 0;
    }

    pthread_rwlock_rdlock(&db->lock);
    clear_err(db);

    size_t cand = k * 4 + 8;
    uint32_t *vslots = (uint32_t *)malloc(cand * sizeof(uint32_t));
    uint32_t *tslots = (uint32_t *)malloc(cand * sizeof(uint32_t));
    float    *vs = (float *)malloc(cand * sizeof(float));
    float    *ts = (float *)malloc(cand * sizeof(float));
    size_t nslots = db->nrecs ? db->nrecs : 1;
    float   *acc  = (float *)calloc(nslots, sizeof(float));
    uint8_t *seen = (uint8_t *)calloc(nslots, 1);
    uint32_t *list = (uint32_t *)malloc(nslots * sizeof(uint32_t));

    if (!vslots || !tslots || !vs || !ts || !acc || !seen || !list) {
        fail(db, VECDB_ERR_NOMEM, "out of memory");
        free(vslots); free(tslots); free(vs); free(ts); free(acc); free(seen); free(list);
        pthread_rwlock_unlock(&db->lock);
        return 0;
    }

    size_t nv = 0, nt = 0;
    if (vec_query && db->dim > 0 && dim == db->dim)
        nv = do_search_vector(db, vec_query, cand, vslots, vs);
    else if (vec_query && dim != db->dim)
        fail(db, VECDB_ERR_DIM, "query dimension %zu != %zu", dim, db->dim);

    if (text_query)
        nt = do_search_text(db, text_query, cand, tslots, ts);

    normalize_minmax(vs, nv);
    normalize_minmax(ts, nt);

    size_t nlist = 0;
    for (size_t i = 0; i < nv; i++) {
        uint32_t s = vslots[i];
        if (s >= nslots) continue;
        if (!seen[s]) { seen[s] = 1; list[nlist++] = s; }
        acc[s] += alpha * vs[i];
    }
    for (size_t i = 0; i < nt; i++) {
        uint32_t s = tslots[i];
        if (s >= nslots) continue;
        if (!seen[s]) { seen[s] = 1; list[nlist++] = s; }
        acc[s] += (1.0f - alpha) * ts[i];
    }

    topk_t tk;
    size_t n = 0;
    if (topk_init(&tk, k) == 0) {
        for (size_t i = 0; i < nlist; i++)
            topk_offer(&tk, acc[list[i]], list[i]);
        uint32_t *s_slots = (uint32_t *)malloc(k * sizeof(uint32_t));
        float    *s_scores = (float *)malloc(k * sizeof(float));
        if (s_slots && s_scores) {
            n = topk_finish(&tk, s_slots, s_scores);
            if (n) fill_matches(db, n, s_slots, s_scores, out, cap);
        } else {
            topk_free(&tk);
            fail(db, VECDB_ERR_NOMEM, "out of memory");
        }
        free(s_slots);
        free(s_scores);
    }

    free(vslots); free(tslots); free(vs); free(ts);
    free(acc); free(seen); free(list);
    pthread_rwlock_unlock(&db->lock);
    return n;
}

/* ------------------------------------------------------------------ */
/* payload helpers                                                     */
/* ------------------------------------------------------------------ */
void vecdb_pack_i64(void *dst, int64_t v)
{
    uint64_t u = (uint64_t)v;
    unsigned char *p = (unsigned char *)dst;
    for (int i = 0; i < 8; i++) p[i] = (unsigned char)(u >> (8 * i));
}

int64_t vecdb_unpack_i64(const void *src)
{
    const unsigned char *p = (const unsigned char *)src;
    uint64_t u = 0;
    for (int i = 0; i < 8; i++) u |= (uint64_t)p[i] << (8 * i);
    return (int64_t)u;
}

void vecdb_pack_u64(void *dst, uint64_t v) { vecdb_pack_i64(dst, (int64_t)v); }
uint64_t vecdb_unpack_u64(const void *src) { return (uint64_t)vecdb_unpack_i64(src); }

void vecdb_pack_f64(void *dst, double v)
{
    uint64_t u;
    memcpy(&u, &v, 8);
    vecdb_pack_i64(dst, (int64_t)u);
}

double vecdb_unpack_f64(const void *src)
{
    uint64_t u = (uint64_t)vecdb_unpack_i64(src);
    double v;
    memcpy(&v, &u, 8);
    return v;
}

void vecdb_pack_f32(void *dst, float v)
{
    uint32_t u;
    memcpy(&u, &v, 4);
    unsigned char *p = (unsigned char *)dst;
    for (int i = 0; i < 4; i++) p[i] = (unsigned char)(u >> (8 * i));
}

float vecdb_unpack_f32(const void *src)
{
    const unsigned char *p = (const unsigned char *)src;
    uint32_t u = 0;
    for (int i = 0; i < 4; i++) u |= (uint32_t)p[i] << (8 * i);
    float v;
    memcpy(&v, &u, 4);
    return v;
}

char *vecdb_pack_str(void *buf, size_t bufcap, const char *s)
{
    if (!buf || !s) return NULL;
    size_t n = strlen(s) + 1;
    if (n > bufcap) return NULL;
    memcpy(buf, s, n);
    return (char *)buf;
}
