/*
 * demo.c — a guided tour of libvecdb.
 *
 * Build with `make demo` and run with `./demo`.
 * A small "knowledge base" of documents is created, stored with
 * vectors + arbitrary payloads + text, then queried in three ways:
 * semantic (vector) search, keyword (BM25) search and hybrid search.
 */
#include "vecdb.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* A toy "embedding model".                                            */
/* Each document is embedded as a bag of (topic, weight) pairs, hashed  */
/* into 64 dimensions and L2-normalised.  Semantically similar words     */
/* share dimensions, so nearest neighbours really are related documents. */
/* ------------------------------------------------------------------ */
#define DEMO_DIM 64

typedef struct { const char *word; float w; } bagword;

static unsigned hash_word(const char *w)
{
    unsigned h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)w; *p; p++)
        h = (h ^ *p) * 16777619u;
    return h;
}

static void embed(const char *text, float *out)
{
    static const struct { const char *word; const char *topic; } synonyms[] = {
        { "database", "storage" }, { "databases", "storage" }, { "storage", "storage" },
        { "index", "storage" },     { "indexes", "storage" },   { "vector", "storage" },
        { "vectors", "storage" },   { "embedding", "storage" }, { "embeddings", "storage" },
        { "query", "storage" },     { "search", "storage" },
        { "vehicle", "car" },       { "vehicles", "car" },      { "car", "car" },
        { "cars", "car" },          { "automobile", "car" },    { "engine", "car" },
        { "driving", "car" },       { "wheel", "car" },
        { "fruit", "food" },        { "fruits", "food" },       { "apple", "food" },
        { "apples", "food" },       { "banana", "food" },       { "recipe", "food" },
        { "cooking", "food" },      { "kitchen", "food" },      { "delicious", "food" },
        { "planet", "space" },      { "planets", "space" },     { "space", "space" },
        { "star", "space" },        { "stars", "space" },       { "galaxy", "space" },
        { "orbit", "space" },       { "gravity", "space" },     { "nasa", "space" },
        { "rocket", "space" },
    };

    memset(out, 0, DEMO_DIM * sizeof(float));
    char copy[1024];
    snprintf(copy, sizeof(copy), "%s", text);

    for (char *tok = strtok(copy, " ,.\n\t"); tok; tok = strtok(NULL, " ,.\n\t")) {
        /* map the token to a topic bucket unless it is a direct match */
        const char *topic = tok;
        for (size_t i = 0; i < sizeof(synonyms) / sizeof(synonyms[0]); i++) {
            if (strcmp(synonyms[i].word, tok) == 0) { topic = synonyms[i].topic; break; }
        }
        unsigned h = hash_word(topic);
        out[h % DEMO_DIM] += 1.0f;
        out[(h / DEMO_DIM) % DEMO_DIM] += 0.5f;
    }

    float norm = 0.0f;
    for (size_t i = 0; i < DEMO_DIM; i++) norm += out[i] * out[i];
    norm = sqrtf(norm);
    if (norm > 0.0f)
        for (size_t i = 0; i < DEMO_DIM; i++) out[i] /= norm;
}

/* ------------------------------------------------------------------ */
static void print_hits(const char *label, vecdb_match *m, size_t n)
{
    printf("  %s\n", label);
    if (n == 0) { printf("    (no hits)\n"); return; }
    for (size_t i = 0; i < n; i++) {
        printf("    %.4f  #%-3llu  %s\n", m[i].score, (unsigned long long)m[i].id,
               m[i].text ? m[i].text : "(no text)");
    }
}

int main(void)
{
    char path[256];
    snprintf(path, sizeof(path), "/tmp/vecdb_demo_%ld.db", (long)getpid());
    unlink(path);

    printf("=== vecdb demo ===============================================\n\n");

    /* 1. open a database with cosine similarity over 64-dim vectors */
    vecdb_options opt;
    vecdb_options_default(&opt);
    opt.dim    = DEMO_DIM;
    opt.metric = VECDB_METRIC_COSINE;
    opt.index  = VECDB_INDEX_HNSW;   /* exact (FLAT) works too */
    opt.hnsw_m = 16;

    vecdb *db = vecdb_open(path, &opt);
    if (!db) { fprintf(stderr, "cannot open %s\n", path); return 1; }
    printf("opened %s  (dim=%zu, metric=cosine, index=hnsw)\n\n", vecdb_path(db),
           vecdb_dim(db));

    /* 2. insert documents with text, a vector and a typed payload.
     *    The payload is an opaque blob; here we store a year + a category. */
    static const char *docs[] = {
        "Vector databases store embeddings and support similarity search",
        "A full text index ranks documents with BM25 scoring",
        "An automobile engine converts fuel into motion",
        "Electric cars charge their batteries at home overnight",
        "Apples and bananas are delicious fruits for a healthy recipe",
        "The kitchen was full of the smell of freshly cooked bread",
        "NASA launched a rocket to explore a distant galaxy",
        "The planet Mars has a weaker gravity than Earth",
        "Planets orbit their star once every year",
        "The database query planner chose an index scan",
    };
    const size_t ndocs = sizeof(docs) / sizeof(docs[0]);

    for (size_t i = 0; i < ndocs; i++) {
        float v[DEMO_DIM];
        embed(docs[i], v);

        char payload[64];
        int64_t year = 2018 + (int64_t)(i % 6);
        vecdb_pack_i64(payload, year);
        vecdb_pack_str(payload + VECDB_I64_SIZE,
                       sizeof(payload) - VECDB_I64_SIZE,
                       (i % 3 == 0) ? "storage" : (i % 3 == 1) ? "science" : "life");

        size_t plen = VECDB_I64_SIZE + strlen(payload + VECDB_I64_SIZE) + 1;
        int rc = vecdb_put(db, i + 1, v, DEMO_DIM, payload, plen, docs[i]);
        if (rc != VECDB_OK) {
            fprintf(stderr, "put failed: %s\n", vecdb_last_error(db));
            break;
        }
    }

    printf("stored %zu documents\n\n", vecdb_count(db));

    vecdb_match hits[5];

    /* 3. semantic search: ask about a "car" */
    float q[DEMO_DIM];
    embed("cars and driving", q);
    size_t n = vecdb_search_vector(db, q, DEMO_DIM, 3, hits, 5);
    print_hits("vector search for \"cars and driving\":", hits, n);

    embed("apples recipe cooking", q);
    n = vecdb_search_vector(db, q, DEMO_DIM, 3, hits, 5);
    print_hits("vector search for \"apples recipe cooking\":", hits, n);

    /* 4. keyword search */
    n = vecdb_search_text(db, "index ranking", 3, hits, 5);
    print_hits("BM25 search for \"index ranking\":", hits, n);

    /* 5. hybrid: semantic + keyword, blended 60/40 */
    embed("automobile", q);
    n = vecdb_search_hybrid(db, q, DEMO_DIM, "engine fuel", 3, 0.6f, hits, 5);
    print_hits("hybrid search (0.6) for vector \"automobile\" + text \"engine fuel\":",
               hits, n);

    /* 6. fetch a single record and decode its payload */
    vecdb_record rec;
    if (vecdb_get(db, 7, &rec) == VECDB_OK) {
        printf("\nfetched record 7:\n");
        printf("    text    : %s\n", rec.text);
        printf("    vector  : %zu floats, first = %.4f\n", vecdb_dim(db),
               rec.vec ? rec.vec[0] : 0.0f);
        printf("    payload : year=%lld category=%s\n",
               (long long)vecdb_unpack_i64(rec.payload),
               (const char *)rec.payload + VECDB_I64_SIZE);
    }

    /* 7. delete, then persist */
    printf("\ndeleting record 3 (the automobile engine) ...\n");
    vecdb_delete(db, 3);
    n = vecdb_search_text(db, "automobile engine", 3, hits, 5);
    print_hits("BM25 search after the delete:", hits, n);

    printf("\ncompacting and flushing to disk ...\n");
    vecdb_compact(db);
    vecdb_close(db);

    /* 8. reopen and show that everything survived */
    vecdb *db2 = vecdb_open(path, NULL);
    if (db2) {
        printf("reopened: %zu documents, dim=%zu\n", vecdb_count(db2), vecdb_dim(db2));
        n = vecdb_search_text(db2, "galaxy rocket", 2, hits, 5);
        print_hits("BM25 search after reopen:", hits, n);
        vecdb_close(db2);
    }
    unlink(path);
    printf("\n=== done =====================================================\n");
    return 0;
}
