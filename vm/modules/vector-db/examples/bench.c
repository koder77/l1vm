/*
 * bench.c — throughput / latency benchmark for libvecdb.
 *
 * Build: make bench
 * Usage: ./bench [n_records] [dim] [--hnsw]
 *
 * Measures insert throughput and query latency for FLAT and HNSW
 * vector search, BM25 text search and hybrid search.
 */
#include "vecdb.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint64_t rng_state = 88172645463325252ULL;
static float frnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (float)((double)(rng_state >> 11) / 9007199254740992.0 * 2.0 - 1.0);
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static const char *WORDS[] = {
    "vector", "database", "embedding", "search", "index", "query", "engine",
    "storage", "ranking", "document", "cluster", "similarity", "graph", "score",
    "payload", "record", "collection", "token", "matrix", "distance"
};

int main(int argc, char **argv)
{
    size_t N = 50000, dim = 128;
    int use_hnsw = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hnsw") == 0) use_hnsw = 1;
        else if (atof(argv[i]) > 0) {
            if (N == 50000) N = (size_t)atoll(argv[i]);
            else dim = (size_t)atoll(argv[i]);
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "/tmp/vecdb_bench_%ld.db", (long)getpid());
    unlink(path);

    vecdb_options opt;
    vecdb_options_default(&opt);
    opt.dim = dim;
    opt.metric = VECDB_METRIC_COSINE;
    opt.index = use_hnsw ? VECDB_INDEX_HNSW : VECDB_INDEX_FLAT;

    vecdb *db = vecdb_open(path, &opt);
    if (!db) { fprintf(stderr, "open failed\n"); return 1; }

    printf("vecdb benchmark: %zu records, dim=%zu, index=%s\n",
           N, dim, use_hnsw ? "HNSW" : "FLAT");

    /* ---- inserts ---- */
    float *vec = (float *)malloc(dim * sizeof(float));
    double t0 = now_sec();
    for (size_t i = 0; i < N; i++) {
        for (size_t d = 0; d < dim; d++) vec[d] = frnd();
        char text[256];
        int nt = 8 + (int)(i % 12);
        size_t off = 0;
        for (int w = 0; w < nt && off < sizeof(text) - 16; w++) {
            off += (size_t)snprintf(text + off, sizeof(text) - off, "%s ",
                                    WORDS[(i * 7 + (size_t)w * 3) % (sizeof(WORDS) / sizeof(WORDS[0]))]);
        }
        uint64_t tag = i;
        vecdb_put(db, i + 1, vec, dim, &tag, sizeof(tag), text);
    }
    double t1 = now_sec();
    printf("  insert      : %8.0f rec/s  (%.3f s total)\n", N / (t1 - t0), t1 - t0);

    /* ---- flush ---- */
    t0 = now_sec();
    vecdb_flush(db);
    t1 = now_sec();
    printf("  flush       : %8.0f rec/s  (%.3f s total)\n", N / (t1 - t0), t1 - t0);

    /* ---- vector search ---- */
    enum { Q = 2000 };
    vecdb_match hits[10];
    size_t found = 0;
    t0 = now_sec();
    for (int q = 0; q < Q; q++) {
        for (size_t d = 0; d < dim; d++) vec[d] = frnd();
        found += vecdb_search_vector(db, vec, dim, 10, hits, 10);
    }
    t1 = now_sec();
    printf("  vector top10: %8.0f q/s     (%.3f ms/query, %zu hits)\n",
           Q / (t1 - t0), (t1 - t0) / Q * 1000.0, found);

    /* ---- text search ---- */
    found = 0;
    t0 = now_sec();
    for (int q = 0; q < Q; q++) {
        char qbuf[64];
        snprintf(qbuf, sizeof(qbuf), "%s %s", WORDS[(q * 5) % 20], WORDS[(q * 11) % 20]);
        found += vecdb_search_text(db, qbuf, 10, hits, 10);
    }
    t1 = now_sec();
    printf("  text  top10 : %8.0f q/s     (%.3f ms/query, %zu hits)\n",
           Q / (t1 - t0), (t1 - t0) / Q * 1000.0, found);

    /* ---- hybrid search ---- */
    t0 = now_sec();
    for (int q = 0; q < Q; q++) {
        for (size_t d = 0; d < dim; d++) vec[d] = frnd();
        char qbuf[64];
        snprintf(qbuf, sizeof(qbuf), "%s", WORDS[(q * 3) % 20]);
        vecdb_search_hybrid(db, vec, dim, qbuf, 10, 0.5f, hits, 10);
    }
    t1 = now_sec();
    printf("  hybrid top10: %8.0f q/s     (%.3f ms/query)\n",
           Q / (t1 - t0), (t1 - t0) / Q * 1000.0);

    /* ---- delete + compact ---- */
    t0 = now_sec();
    for (size_t i = 0; i < N; i += 2) vecdb_delete(db, i + 1);
    t1 = now_sec();
    printf("  delete half : %8.0f rec/s  (%.3f s total)\n", (N / 2) / (t1 - t0), t1 - t0);

    t0 = now_sec();
    vecdb_compact(db);
    t1 = now_sec();
    printf("  compact     : %8.0f rec/s  (%.3f s total, %zu records left)\n",
           vecdb_count(db) / (t1 - t0), t1 - t0, vecdb_count(db));

    vecdb_close(db);
    unlink(path);
    free(vec);
    return 0;
}
