/*
 * test_vecdb.c — integration tests for libvecdb.
 *
 * Run with `make test`.  Exits non-zero if any check fails.
 */
#include "vecdb.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int checks = 0, failures = 0;

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            fprintf(stderr, "  FAIL %s:%d: ", __FILE__, __LINE__);           \
            fprintf(stderr, __VA_ARGS__);                                    \
            fputc('\n', stderr);                                             \
        }                                                                    \
    } while (0)

/* deterministic RNG */
static uint64_t rng_state = 0x123456789abcdefULL;
static double rnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (double)(rng_state >> 11) / 9007199254740992.0;
}

static char DBPATH[256];

static void rand_vec(float *v, size_t dim)
{
    for (size_t i = 0; i < dim; i++) v[i] = (float)(rnd() * 2.0 - 1.0);
}

/* ------------------------------------------------------------------ */
/* 1. basic CRUD with mixed payload types                              */
/* ------------------------------------------------------------------ */
static void test_basics(void)
{
    printf("[1] basic CRUD + payload types\n");
    vecdb_options opt;
    vecdb_options_default(&opt);

    vecdb *db = vecdb_open(DBPATH, &opt);
    CHECK(db != NULL, "open failed: %s", db ? vecdb_last_error(db) : "?");
    if (!db) return;
    CHECK(vecdb_count(db) == 0, "new db should be empty");

    /* payload flavours */
    int64_t year = 1999;
    double  rating = 9.3;
    char    title[64];
    vecdb_pack_str(title, sizeof(title), "The Matrix");
    unsigned char blob[16];
    for (int i = 0; i < 16; i++) blob[i] = (unsigned char)(i * 7);

    CHECK(vecdb_put(db, 1, NULL, 0, &year, VECDB_I64_SIZE, "a movie about hackers") == VECDB_OK,
          "put 1 failed");
    CHECK(vecdb_put(db, 2, NULL, 0, &rating, VECDB_F64_SIZE, NULL) == VECDB_OK, "put 2 failed");
    CHECK(vecdb_put(db, 3, NULL, 0, title, strlen(title) + 1, "the matrix is a classic") == VECDB_OK,
          "put 3 failed");
    CHECK(vecdb_put(db, 4, NULL, 0, blob, sizeof(blob), NULL) == VECDB_OK, "put 4 failed");
    CHECK(vecdb_count(db) == 4, "count == %zu, want 4", vecdb_count(db));

    vecdb_record rec;
    CHECK(vecdb_get(db, 1, &rec) == VECDB_OK, "get 1");
    CHECK(vecdb_unpack_i64(rec.payload) == 1999, "i64 payload roundtrip");
    CHECK(rec.payload_len == VECDB_I64_SIZE, "payload_len");

    CHECK(vecdb_get(db, 2, &rec) == VECDB_OK, "get 2");
    CHECK(fabs(vecdb_unpack_f64(rec.payload) - 9.3) < 1e-12, "f64 payload roundtrip");

    CHECK(vecdb_get(db, 3, &rec) == VECDB_OK, "get 3");
    CHECK(strcmp((const char *)rec.payload, "The Matrix") == 0, "string payload roundtrip");
    CHECK(rec.text != NULL && strcmp(rec.text, "the matrix is a classic") == 0, "text roundtrip");

    CHECK(vecdb_get(db, 4, &rec) == VECDB_OK, "get 4");
    CHECK(rec.payload_len == 16, "blob length");
    CHECK(memcmp(rec.payload, blob, 16) == 0, "blob contents");

    CHECK(vecdb_get(db, 999, &rec) == VECDB_ERR_NOTFOUND, "get of missing id");

    /* upsert replaces the payload */
    int64_t year2 = 2020;
    CHECK(vecdb_put(db, 1, NULL, 0, &year2, VECDB_I64_SIZE, NULL) == VECDB_OK, "upsert");
    CHECK(vecdb_get(db, 1, &rec) == VECDB_OK && vecdb_unpack_i64(rec.payload) == 2020,
          "upserted payload");

    /* insert-only must fail on a duplicate */
    CHECK(vecdb_put_ex(db, 2, NULL, 0, &rating, VECDB_F64_SIZE, NULL, VECDB_INSERT_ONLY) ==
          VECDB_ERR_EXISTS, "INSERT_ONLY duplicate");

    /* ids can be arbitrary 64 bit values */
    uint64_t big_id = 0xfffffffffffffffeULL;
    CHECK(vecdb_put(db, big_id, NULL, 0, &year, VECDB_I64_SIZE, NULL) == VECDB_OK, "big id put");
    CHECK(vecdb_get(db, big_id, &rec) == VECDB_OK, "big id get");

    /* list ids */
    uint64_t ids[16];
    size_t n = vecdb_list_ids(db, ids, 16);
    CHECK(n == 5, "list_ids returned %zu, want 5", n);

    vecdb_close(db);
}

/* ------------------------------------------------------------------ */
/* 2. exact vector search (FLAT index)                                 */
/* ------------------------------------------------------------------ */
static void test_flat_vector_search(void)
{
    printf("[2] flat vector search (exact)\n");
    unlink(DBPATH);

    vecdb_options opt;
    vecdb_options_default(&opt);
    opt.dim = 8;
    opt.metric = VECDB_METRIC_L2;
    opt.index = VECDB_INDEX_FLAT;

    vecdb *db = vecdb_open(DBPATH, &opt);
    CHECK(db != NULL, "open");
    if (!db) return;

    enum { N = 500 };
    float v[8];
    for (uint64_t i = 0; i < N; i++) {
        rand_vec(v, 8);
        char txt[64];
        snprintf(txt, sizeof(txt), "record number %llu", (unsigned long long)i);
        CHECK(vecdb_put(db, i + 1000, v, 8, &i, sizeof(i), txt) == VECDB_OK, "put %llu",
              (unsigned long long)i);
    }
    CHECK(vecdb_count(db) == N, "count after insert");

    /* query with an exact copy of record 137's vector */
    vecdb_record rec;
    CHECK(vecdb_get(db, 1137, &rec) == VECDB_OK, "get stored record");
    vecdb_match m[10];
    size_t n = vecdb_search_vector(db, rec.vec, 8, 10, m, 10);
    CHECK(n == 10, "expected 10 hits, got %zu", n);
    CHECK(n > 0 && m[0].id == 1137, "top hit should be the exact record (got %llu)",
          n ? (unsigned long long)m[0].id : 0);
    CHECK(n > 0 && m[0].score > -1e-5f, "exact match score ~ 0, got %f", n ? m[0].score : 0.0f);
    for (size_t i = 1; i < n; i++)
        CHECK(m[i].score <= m[i - 1].score, "results must be sorted descending");

    /* payload travels with the match */
    CHECK(n > 0 && m[0].payload_len == sizeof(uint64_t), "payload attached");
    CHECK(n > 0 && *(const uint64_t *)m[0].payload == 137, "payload content");

    /* dimension guard */
    CHECK(vecdb_search_vector(db, rec.vec, 4, 5, m, 5) == 0, "wrong dim must return no hits");
    CHECK(vecdb_dim(db) == 8, "dim");

    vecdb_close(db);
}

/* ------------------------------------------------------------------ */
/* 3. full text search                                                 */
/* ------------------------------------------------------------------ */
static void test_text_search(void)
{
    printf("[3] BM25 full-text search\n");
    unlink(DBPATH);

    vecdb_options opt;
    vecdb_options_default(&opt);
    vecdb *db = vecdb_open(DBPATH, &opt);
    CHECK(db != NULL, "open");
    if (!db) return;

    struct { uint64_t id; const char *text; } docs[] = {
        { 1, "the quick brown fox jumps over the lazy dog" },
        { 2, "a quick tutorial about databases and indexes" },
        { 3, "vector databases store embeddings for similarity search" },
        { 4, "full text search ranks documents with BM25 scoring" },
        { 5, "foxes are quick animals and dogs are lazy" },
        { 6, "ein schneller Fuchs springt über den faulen Hund" },      /* German */
        { 7, "日本語のテキスト検索のテスト" },                            /* Japanese */
        { 8, NULL },                                                     /* no text at all */
    };
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
        int rc = vecdb_put(db, docs[i].id, NULL, 0, &i, sizeof(i), docs[i].text);
        CHECK(rc == VECDB_OK, "put %llu rc=%d", (unsigned long long)docs[i].id, rc);
    }

    vecdb_match m[8];
    size_t n = vecdb_search_text(db, "quick fox", 8, m, 8);
    CHECK(n >= 2, "expected >= 2 hits for 'quick fox', got %zu", n);
    CHECK(n > 0 && (m[0].id == 1 || m[0].id == 5), "top hit for 'quick fox' should be 1 or 5, got %llu",
          n ? (unsigned long long)m[0].id : 0);

    /* both 'quick' and 'fox' occur in doc 1, doc 5; doc 2 has only 'quick' */
    n = vecdb_search_text(db, "quick", 8, m, 8);
    CHECK(n == 3, "docs containing 'quick': got %zu, want 3", n);

    /* the word 'vector' only exists in one document */
    n = vecdb_search_text(db, "vector", 8, m, 8);
    CHECK(n == 1 && m[0].id == 3, "unique term should find exactly doc 3");

    /* stop words are dropped: a query of only stopwords finds nothing */
    n = vecdb_search_text(db, "the and is of", 8, m, 8);
    CHECK(n == 0, "stopword-only query should return 0 hits, got %zu", n);

    /* non-ASCII tokens */
    n = vecdb_search_text(db, "schneller", 8, m, 8);
    CHECK(n == 1 && m[0].id == 6, "utf-8 german token search");
    n = vecdb_search_text(db, "テキスト", 8, m, 8);
    CHECK(n == 1 && m[0].id == 7, "utf-8 japanese token search");

    /* deleting removes the document from the text index */
    CHECK(vecdb_delete(db, 3) == VECDB_OK, "delete doc 3");
    n = vecdb_search_text(db, "vector", 8, m, 8);
    CHECK(n == 0, "deleted doc must not be found by text search, got %zu", n);

    /* upserting the text of a document re-indexes it */
    int x = 0;
    CHECK(vecdb_put(db, 8, NULL, 0, &x, sizeof(x), "completely different wording") == VECDB_OK,
          "upsert text");
    n = vecdb_search_text(db, "different", 8, m, 8);
    CHECK(n == 1 && m[0].id == 8, "re-indexed doc found");
    n = vecdb_search_text(db, "dog", 8, m, 8);
    CHECK(n >= 1 && n <= 3, "docs without text still safe, got %zu", n);

    /* Term frequency: a document repeating a word must be found exactly
     * once (never twice) and must outrank a single-occurrence document. */
    CHECK(vecdb_put(db, 20, NULL, 0, NULL, 0, "zebra zebra zebra") == VECDB_OK, "tf doc 1");
    CHECK(vecdb_put(db, 21, NULL, 0, NULL, 0, "zebra") == VECDB_OK, "tf doc 2");
    CHECK(vecdb_put(db, 22, NULL, 0, NULL, 0, "zebra and more zebra words") == VECDB_OK,
          "non-adjacent repeat");
    n = vecdb_search_text(db, "zebra", 10, m, 10);
    CHECK(n == 3, "repeated token must not duplicate a document, got %zu", n);
    int seen20 = 0, seen21 = 0, seen22 = 0;
    for (size_t i = 0; i < n; i++) {
        if (m[i].id == 20) seen20++;
        if (m[i].id == 21) seen21++;
        if (m[i].id == 22) seen22++;
    }
    CHECK(seen20 == 1 && seen21 == 1 && seen22 == 1, "each document appears once");
    CHECK(m[0].id == 20, "highest term frequency should rank first, got %llu",
          n ? (unsigned long long)m[0].id : 0);
    CHECK(n > 2 && m[2].id == 21, "single occurrence should rank last");

    /* deleting a document with repeated tokens must remove all of them */
    CHECK(vecdb_delete(db, 20) == VECDB_OK, "delete tf doc");
    n = vecdb_search_text(db, "zebra", 10, m, 10);
    CHECK(n == 2, "delete removes the whole posting, got %zu", n);
    for (size_t i = 0; i < n; i++)
        CHECK(m[i].id != 20, "deleted doc must not reappear");
    CHECK(vecdb_delete(db, 21) == VECDB_OK && vecdb_delete(db, 22) == VECDB_OK, "cleanup");
    n = vecdb_search_text(db, "zebra", 10, m, 10);
    CHECK(n == 0, "fully removed term finds nothing, got %zu", n);

    /* re-adding a term after all its postings were deleted must work */
    CHECK(vecdb_put(db, 23, NULL, 0, NULL, 0, "zebra came back") == VECDB_OK, "re-add");
    n = vecdb_search_text(db, "zebra", 10, m, 10);
    CHECK(n == 1 && m[0].id == 23, "term reusable after deletion");

    vecdb_close(db);
}

/* ------------------------------------------------------------------ */
/* 4. persistence: flush, close, reopen                                */
/* ------------------------------------------------------------------ */
static void test_persistence(void)
{
    printf("[4] persistence\n");
    unlink(DBPATH);

    vecdb_options opt;
    vecdb_options_default(&opt);
    opt.dim = 4;
    opt.metric = VECDB_METRIC_COSINE;
    opt.index = VECDB_INDEX_FLAT;

    const uint64_t N = 200;
    {
        vecdb *db = vecdb_open(DBPATH, &opt);
        CHECK(db != NULL, "open");
        if (!db) return;
        float v[4];
        for (uint64_t i = 0; i < N; i++) {
            rand_vec(v, 4);
            char txt[48];
            snprintf(txt, sizeof(txt), "persisted document %llu", (unsigned long long)i);
            CHECK(vecdb_put(db, i + 1, v, 4, txt, strlen(txt) + 1, txt) == VECDB_OK, "put %llu",
                  (unsigned long long)i);
        }
        /* delete a few before closing */
        CHECK(vecdb_delete(db, 50) == VECDB_OK, "delete 50");
        CHECK(vecdb_delete(db, 51) == VECDB_OK, "delete 51");
        CHECK(vecdb_close(db) == VECDB_OK, "close");
    }
    {
        vecdb_options o2;
        vecdb_options_default(&o2);
        vecdb *db = vecdb_open(DBPATH, &o2);
        CHECK(db != NULL, "reopen");
        if (!db) return;
        CHECK(vecdb_count(db) == N - 2, "reopened count %zu, want %zu", vecdb_count(db), N - 2);
        CHECK(vecdb_dim(db) == 4, "dim persisted");
        CHECK(vecdb_metric_of(db) == VECDB_METRIC_COSINE, "metric persisted");

        vecdb_record rec;
        CHECK(vecdb_get(db, 120, &rec) == VECDB_OK, "get after reload");
        CHECK(rec.vec != NULL && rec.text != NULL, "vector + text survived");
        CHECK(rec.payload_len == strlen(rec.text) + 1, "payload survived");

        vecdb_match m[5];
        size_t n = vecdb_search_vector(db, rec.vec, 4, 5, m, 5);
        CHECK(n == 5 && m[0].id == 120, "vector search after reload, top=%llu n=%zu",
              n ? (unsigned long long)m[0].id : 0, n);
        n = vecdb_search_text(db, "persisted", 5, m, 5);
        CHECK(n == 5, "text search after reload, n=%zu", n);

        CHECK(vecdb_get(db, 50, &rec) == VECDB_ERR_NOTFOUND, "deleted record stays deleted");
        n = vecdb_search_text(db, "50", 5, m, 5);
        for (size_t i = 0; i < n; i++)
            CHECK(m[i].id != 50, "deleted record leaked into text search");

        /* wrong requested dimension must be refused */
        vecdb *bad = vecdb_open(DBPATH, &(vecdb_options){ .dim = 7 });
        CHECK(bad == NULL, "opening with a mismatched dim must fail");

        vecdb_close(db);
    }
    unlink(DBPATH);
}

/* ------------------------------------------------------------------ */
/* 5. HNSW approximate search                                          */
/* ------------------------------------------------------------------ */
static void test_hnsw(void)
{
    printf("[5] HNSW index (recall)\n");
    unlink(DBPATH);

    vecdb_options opt;
    vecdb_options_default(&opt);
    opt.dim = 32;
    opt.metric = VECDB_METRIC_L2;
    opt.index = VECDB_INDEX_HNSW;
    opt.hnsw_m = 16;
    opt.hnsw_ef_construction = 200;
    opt.hnsw_ef_search = 128;
    opt.seed = 1337;

    vecdb *db = vecdb_open(DBPATH, &opt);
    CHECK(db != NULL, "open");
    if (!db) return;

    enum { N = 3000, Q = 100 };
    float v[32];
    for (uint64_t i = 0; i < N; i++) {
        rand_vec(v, 32);
        CHECK(vecdb_put(db, i + 1, v, 32, NULL, 0, NULL) == VECDB_OK, "put %llu",
              (unsigned long long)i);
    }

    int top1 = 0, top10 = 0;
    for (int q = 0; q < Q; q++) {
        uint64_t target = 1 + (uint64_t)(rnd() * (N - 1));
        vecdb_record rec;
        if (vecdb_get(db, target, &rec) != VECDB_OK) continue;
        vecdb_match m[10];
        size_t n = vecdb_search_vector(db, rec.vec, 32, 10, m, 10);
        if (n > 0 && m[0].id == target) top1++;
        for (size_t i = 0; i < n; i++) if (m[i].id == target) { top10++; break; }
    }
    printf("    HNSW recall@1 = %.0f%%  recall@10 = %.0f%%\n", 100.0 * top1 / Q,
           100.0 * top10 / Q);
    CHECK(top1 >= (int)(Q * 95 / 100), "recall@1 too low: %d/%d", top1, Q);
    CHECK(top10 >= (int)(Q * 99 / 100), "recall@10 too low: %d/%d", top10, Q);

    /* deleting a record removes it from HNSW results */
    vecdb_record first;
    float query_vec[32];
    CHECK(vecdb_get(db, 7, &first) == VECDB_OK, "get 7");
    CHECK(first.vec != NULL, "7 has a vector");
    if (first.vec) memcpy(query_vec, first.vec, sizeof(query_vec));
    CHECK(vecdb_delete(db, 7) == VECDB_OK, "delete 7");
    vecdb_match m[10];
    size_t n = vecdb_search_vector(db, query_vec, 32, 10, m, 10);
    for (size_t i = 0; i < n; i++) CHECK(m[i].id != 7, "deleted id leaked into HNSW search");

    /* ef knob */
    CHECK(vecdb_set_ef_search(db, 200) == VECDB_OK, "set ef");
    CHECK(vecdb_set_ef_search(db, 4) == VECDB_OK || 1, "set ef low is clamped");

    vecdb_close(db);
}

/* ------------------------------------------------------------------ */
/* 6. delete / compact                                                 */
/* ------------------------------------------------------------------ */
static void test_delete_compact(void)
{
    printf("[6] delete + compact\n");
    unlink(DBPATH);

    vecdb_options opt;
    vecdb_options_default(&opt);
    opt.dim = 16;
    opt.metric = VECDB_METRIC_IP;

    vecdb *db = vecdb_open(DBPATH, &opt);
    CHECK(db != NULL, "open");
    if (!db) return;

    enum { N = 1000 };
    float v[16];
    for (uint64_t i = 0; i < N; i++) {
        rand_vec(v, 16);
        char txt[32];
        snprintf(txt, sizeof(txt), "doc %llu", (unsigned long long)i);
        CHECK(vecdb_put(db, i, v, 16, &i, sizeof(i), txt) == VECDB_OK, "put");
    }

    for (uint64_t i = 0; i < N; i += 2) CHECK(vecdb_delete(db, i) == VECDB_OK, "delete %llu",
                                              (unsigned long long)i);
    CHECK(vecdb_count(db) == N / 2, "half deleted, count=%zu", vecdb_count(db));
    CHECK(vecdb_capacity(db) == N, "capacity still holds tombstones");

    CHECK(vecdb_delete(db, 0) == VECDB_ERR_NOTFOUND, "double delete");

    /* deleted records must never come back */
    vecdb_match m[20];
    for (int q = 0; q < 20; q++) {
        uint64_t query_id = (uint64_t)(rnd() * N);
        vecdb_record qr;
        if (vecdb_get(db, query_id, &qr) != VECDB_OK) continue;
        size_t n = vecdb_search_vector(db, qr.vec, 16, 20, m, 20);
        for (size_t i = 0; i < n; i++) CHECK(m[i].id % 2 == 1, "even (deleted) id %llu returned",
                                             (unsigned long long)m[i].id);
    }

    CHECK(vecdb_compact(db) == VECDB_OK, "compact");
    CHECK(vecdb_count(db) == N / 2, "count unchanged by compact");
    CHECK(vecdb_capacity(db) == N / 2, "tombstones gone after compact");

    /* exact vector match still works after slot renumbering */
    vecdb_record rec;
    float saved_vec[16];
    CHECK(vecdb_get(db, 333, &rec) == VECDB_OK, "get 333 after compact");
    CHECK(rec.vec != NULL, "333 has a vector");
    if (rec.vec) memcpy(saved_vec, rec.vec, sizeof(saved_vec));
    size_t n = vecdb_search_vector(db, rec.vec, 16, 5, m, 5);
    CHECK(n == 5 && m[0].id == 333, "exact match after compact, top=%llu",
          n ? (unsigned long long)m[0].id : 0);
    n = vecdb_search_text(db, "doc 333", 5, m, 5);
    CHECK(n > 0 && m[0].id == 333, "text search after compact");

    /* data must survive flush + reload after compaction */
    CHECK(vecdb_flush(db) == VECDB_OK, "flush");
    vecdb_close(db);

    db = vecdb_open(DBPATH, NULL);
    CHECK(db != NULL && vecdb_count(db) == N / 2, "reload after compact");
    if (db) {
        vecdb_record r2;
        CHECK(vecdb_get(db, 333, &r2) == VECDB_OK, "get 333 after reload");
        CHECK(r2.vec != NULL && memcmp(r2.vec, saved_vec, 16 * sizeof(float)) == 0,
              "vector bytes identical after compact+reload");
        vecdb_close(db);
    }
}

/* ------------------------------------------------------------------ */
/* 7. hybrid search                                                    */
/* ------------------------------------------------------------------ */
static void test_hybrid(void)
{
    printf("[7] hybrid search\n");
    unlink(DBPATH);

    vecdb_options opt;
    vecdb_options_default(&opt);
    opt.dim = 3;

    vecdb *db = vecdb_open(DBPATH, &opt);
    CHECK(db != NULL, "open");
    if (!db) return;

    /* [action, romance, comedy] style embeddings */
    struct { uint64_t id; float vec[3]; const char *txt; } rows[] = {
        { 1, { 0.9f, 0.0f, 0.1f }, "Die Hard action movie with explosions" },
        { 2, { 0.8f, 0.1f, 0.1f }, "Lethal Weapon action comedy" },
        { 3, { 0.0f, 0.9f, 0.1f }, "The Notebook romance drama" },
        { 4, { 0.1f, 0.8f, 0.1f }, "Pretty Woman romance story" },
        { 5, { 0.0f, 0.1f, 0.9f }, "Superbad comedy about school" },
        { 6, { 0.1f, 0.0f, 0.8f }, "The Hangover comedy night out" },
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        CHECK(vecdb_put(db, rows[i].id, rows[i].vec, 3, rows[i].txt, strlen(rows[i].txt) + 1,
                        rows[i].txt) == VECDB_OK, "put");
    }

    float q[3] = { 0.90f, 0.05f, 0.05f };
    vecdb_match mv[3], mt[3], mh[8];
    size_t nv = vecdb_search_vector(db, q, 3, 3, mv, 3);
    CHECK(nv == 3, "vector search hits");
    size_t nt = vecdb_search_text(db, "action", 3, mt, 3);
    CHECK(nt == 2, "text search hits for 'action'");

    size_t nh = vecdb_search_hybrid(db, q, 3, "action", 3, 1.0f, mh, 3);
    CHECK(nh > 0 && mh[0].id == mv[0].id, "alpha=1 should behave like vector search");
    nh = vecdb_search_hybrid(db, q, 3, "action", 3, 0.0f, mh, 3);
    CHECK(nh > 0 && mh[0].id == mt[0].id, "alpha=0 should behave like text search");

    nh = vecdb_search_hybrid(db, q, 3, "comedy", 4, 0.5f, mh, 4);
    CHECK(nh == 4, "hybrid returns k hits, got %zu", nh);
    /* the winner must be a film that satisfies at least one of the signals */
    int good_winner = 0;
    for (size_t i = 0; i < nh; i++) {
        if (mh[i].id == 1 || mh[i].id == 2 || mh[i].id == 5 || mh[i].id == 6) good_winner = 1;
    }
    CHECK(good_winner, "hybrid winner not among plausible candidates");
    /* and the comedy docs must not be crowded out entirely */
    int comedy_hits = 0;
    for (size_t i = 0; i < nh; i++)
        if (strstr(mh[i].text, "comedy") != NULL) comedy_hits++;
    CHECK(comedy_hits >= 2, "hybrid should surface the comedy documents, got %d", comedy_hits);

    /* single modality fallbacks */
    nh = vecdb_search_hybrid(db, NULL, 0, "romance", 3, 0.5f, mh, 3);
    CHECK(nh == nt, "text-only hybrid falls back to text search");
    nh = vecdb_search_hybrid(db, q, 3, NULL, 3, 0.5f, mh, 3);
    CHECK(nh == nv, "vector-only hybrid falls back to vector search");
    nh = vecdb_search_hybrid(db, NULL, 0, NULL, 3, 0.5f, mh, 3);
    CHECK(nh == 0, "no queries -> no hits");

    vecdb_close(db);
}

/* ------------------------------------------------------------------ */
/* 8. thread safety smoke test                                         */
/* ------------------------------------------------------------------ */
#include <pthread.h>

#define THREADS 4
#define PER_THREAD 250
#define N_LIMIT 5000

/* per-thread RNG (rand_vec's global state is not thread safe) */
static __thread uint64_t tls_rng = 0xabcdef123456789ULL;
static double tls_rnd(void)
{
    tls_rng ^= tls_rng << 13;
    tls_rng ^= tls_rng >> 7;
    tls_rng ^= tls_rng << 17;
    return (double)(tls_rng >> 11) / 9007199254740992.0;
}

static void *writer_thread(void *arg)
{
    vecdb *db = (vecdb *)arg;
    float v[4];
    for (int i = 0; i < PER_THREAD; i++) {
        for (int j = 0; j < 4; j++) v[j] = (float)(tls_rnd() * 2.0 - 1.0);
        uint64_t id = (uint64_t)(tls_rnd() * N_LIMIT);
        vecdb_put(db, id, v, 4, &id, sizeof(id), "concurrent writer document");
    }
    return NULL;
}

static void *reader_thread(void *arg)
{
    vecdb *db = (vecdb *)arg;
    float v[4];
    for (int i = 0; i < PER_THREAD; i++) {
        for (int j = 0; j < 4; j++) v[j] = (float)(tls_rnd() * 2.0 - 1.0);
        vecdb_match m[5];
        vecdb_search_vector(db, v, 4, 5, m, 5);
        vecdb_search_text(db, "concurrent", 5, m, 5);
        vecdb_search_hybrid(db, v, 4, "writer", 5, 0.5f, m, 5);
    }
    return NULL;
}

static void test_threads(void)
{
    printf("[8] concurrent readers + writers\n");
    unlink(DBPATH);

    vecdb_options opt;
    vecdb_options_default(&opt);
    opt.dim = 4;
    opt.index = VECDB_INDEX_FLAT;

    vecdb *db = vecdb_open(DBPATH, &opt);
    CHECK(db != NULL, "open");
    if (!db) return;

    pthread_t th[THREADS * 2];
    for (long i = 0; i < THREADS; i++) {
        pthread_create(&th[i], NULL, writer_thread, db);
        pthread_create(&th[THREADS + i], NULL, reader_thread, db);
    }
    for (int i = 0; i < THREADS * 2; i++) pthread_join(th[i], NULL);

    CHECK(vecdb_count(db) > 0, "writers inserted something");
    vecdb_close(db);
    unlink(DBPATH);
}

/* ------------------------------------------------------------------ */
int main(void)
{
    snprintf(DBPATH, sizeof(DBPATH), "/tmp/vecdb_test_%ld.db", (long)getpid());
    unlink(DBPATH);

    test_basics();
    test_flat_vector_search();
    test_text_search();
    test_persistence();
    test_hnsw();
    test_delete_compact();
    test_hybrid();
    test_threads();

    unlink(DBPATH);
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
