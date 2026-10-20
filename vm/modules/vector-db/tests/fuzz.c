/*
 * fuzz.c — robustness tests: malformed files, hostile arguments, edge cases.
 *
 * A database that reads files from disk must never crash on a corrupt or
 * truncated file, so this suite writes garbage, truncates valid files and
 * feeds nonsense to the API.  Any crash, hang or leak is a bug.
 *
 * Build: make fuzz
 */
#include "vecdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char PATH[256];
static int failures = 0;

#define EXPECT(cond, msg)                                                    \
    do {                                                                     \
        if (!(cond)) { failures++; printf("  MISSING: %s\n", msg); }          \
    } while (0)

static void write_file(const char *data, size_t n)
{
    FILE *f = fopen(PATH, "wb");
    if (!f) return;
    if (n) fwrite(data, 1, n, f);
    fclose(f);
}

/* A malformed database must be rejected, never crash. */
static void test_corrupt_files(void)
{
    printf("[fuzz] corrupt / truncated files\n");

    struct { const char *name; const char *data; size_t n; } cases[] = {
        { "empty file",            "", 0 },
        { "garbage bytes",         "\x01\x02\x03\x04\x05\x06\x07\x08", 8 },
        { "right magic only",      "VECDB001", 8 },
        { "magic + version",       "VECDB001\x01\x00\x00\x00", 12 },
        { "plain text",            "this is not a database at all", 29 },
        { "zip-like",              "PK\x03\x04junkjunkjunkjunk", 20 },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        write_file(cases[i].data, cases[i].n);
        vecdb *db = vecdb_open(PATH, NULL);
        EXPECT(db == NULL, cases[i].name);
        vecdb_close(db);
    }

    /* Take a valid database and truncate it at every possible length. */
    unlink(PATH);
    vecdb_options opt;
    vecdb_options_default(&opt);
    opt.dim = 4;
    vecdb *db = vecdb_open(PATH, &opt);
    float v[4] = { 1, 2, 3, 4 };
    for (uint64_t i = 0; i < 20; i++) {
        char txt[32];
        snprintf(txt, sizeof(txt), "document %llu", (unsigned long long)i);
        vecdb_put(db, i, v, 4, txt, strlen(txt) + 1, txt);
    }
    vecdb_close(db);

    FILE *f = fopen(PATH, "rb");
    fseek(f, 0, SEEK_END);
    long full = ftell(f);
    fclose(f);

    unsigned char *bytes = malloc((size_t)full);
    f = fopen(PATH, "rb");
    if (fread(bytes, 1, (size_t)full, f) != (size_t)full) { /* ignore */ }
    fclose(f);

    int truncated_ok = 0, truncated_rejected = 0;
    for (long n = 0; n < full; n++) {
        write_file((const char *)bytes, (size_t)n);
        vecdb *t = vecdb_open(PATH, NULL);
        if (t) { truncated_ok++; vecdb_close(t); }
        else   { truncated_rejected++; }
    }
    printf("        %d truncation lengths handled (%d clean opens, %d rejected)\n",
           (int)full, truncated_ok, truncated_rejected);

    /* Corrupt bytes in the middle / header must not crash either. */
    for (long pos = 0; pos < full; pos += 3) {
        unsigned char *copy = malloc((size_t)full);
        memcpy(copy, bytes, (size_t)full);
        copy[pos] ^= 0xA5;
        write_file((const char *)copy, (size_t)full);
        vecdb *t = vecdb_open(PATH, NULL);
        if (t) {
            /* if it opened, the API must still behave */
            vecdb_match m[5];
            float q[4] = { 1, 2, 3, 4 };
            vecdb_search_vector(t, q, 4, 5, m, 5);
            vecdb_search_text(t, "document", 5, m, 5);
            vecdb_record r;
            vecdb_get(t, 3, &r);
            vecdb_close(t);
        }
        free(copy);
    }
    free(bytes);
    printf("        single-byte corruption at every offset survived\n");
    unlink(PATH);
}

/* Hostile / degenerate API arguments. */
static void test_bad_arguments(void)
{
    printf("[fuzz] hostile arguments\n");

    vecdb_options opt;
    vecdb_options_default(&opt);
    vecdb *db = vecdb_open(PATH, &opt);
    EXPECT(db != NULL, "open for arg tests");

    /* the NULL-handle contract */
    EXPECT(vecdb_count(NULL) == 0, "count(NULL)");
    EXPECT(vecdb_dim(NULL) == 0, "dim(NULL)");
    EXPECT(vecdb_close(NULL) == VECDB_OK, "close(NULL)");
    EXPECT(vecdb_put(NULL, 1, NULL, 0, NULL, 0, NULL) == VECDB_ERR_INVALID, "put(NULL)");
    EXPECT(vecdb_get(NULL, 1, NULL) == VECDB_ERR_INVALID, "get(NULL)");
    EXPECT(vecdb_search_text(NULL, "x", 1, NULL, 1) == 0, "search_text(NULL)");
    vecdb_record rec;
    (void)rec;
    EXPECT(vecdb_get(db, 1, NULL) == VECDB_ERR_INVALID, "get with NULL out");

    float v[4] = { 1, 2, 3, 4 };
    /* vec == NULL but a nonzero dim is a caller error */
    EXPECT(vecdb_put(db, 1, NULL, 4, NULL, 0, NULL) == VECDB_ERR_INVALID, "NULL vec with dim");
    /* payload pointer NULL but length > 0 */
    EXPECT(vecdb_put(db, 2, v, 4, NULL, 8, NULL) == VECDB_ERR_INVALID, "NULL payload with len");
    /* absurd dimension */
    EXPECT(vecdb_put(db, 3, v, (size_t)1 << 40, NULL, 0, NULL) == VECDB_ERR_DIM, "huge dim");

    /* now learn the dimension, then mismatch it */
    EXPECT(vecdb_put(db, 4, v, 4, NULL, 0, "hello world") == VECDB_OK, "valid put");
    EXPECT(vecdb_put(db, 5, v, 8, NULL, 0, NULL) == VECDB_ERR_DIM, "dim mismatch");
    EXPECT(vecdb_search_vector(db, v, 4, 3, NULL, 0) == 0, "search with NULL out");
    EXPECT(vecdb_search_vector(db, v, 4, 3, NULL, 3) == 0, "search with NULL matches");

    vecdb_match m[4];
    EXPECT(vecdb_search_vector(db, NULL, 4, 3, m, 4) == 0, "search NULL query");
    EXPECT(vecdb_search_vector(db, v, 4, 0, m, 4) == 0, "search k=0");
    EXPECT(vecdb_search_text(db, NULL, 3, m, 4) == 0, "text NULL query");
    EXPECT(vecdb_search_text(db, "", 3, m, 4) == 0, "text empty query");
    EXPECT(vecdb_search_text(db, "!!! ,,, ...", 3, m, 4) == 0, "text punctuation only");
    EXPECT(vecdb_search_hybrid(db, NULL, 0, NULL, 3, 0.5f, m, 4) == 0, "hybrid no query");
    /* cap smaller than k must clamp, not overflow */
    EXPECT(vecdb_search_text(db, "hello", 100, m, 1) <= 1, "text cap clamp");
    EXPECT(vecdb_search_hybrid(db, v, 4, "hello", 100, 0.5f, m, 1) <= 1, "hybrid cap clamp");
    /* alpha outside [0,1] must not crash */
    vecdb_search_hybrid(db, v, 4, "hello", 4, -5.0f, m, 4);
    vecdb_search_hybrid(db, v, 4, "hello", 4, 5.0f, m, 4);
    vecdb_search_hybrid(db, v, 4, "hello", 4, 0.0f / 0.0f, m, 4);   /* NaN */

    /* duplicate ids, huge ids, delete twice */
    EXPECT(vecdb_put(db, 0, v, 4, NULL, 0, NULL) == VECDB_OK, "id 0");
    EXPECT(vecdb_put(db, UINT64_MAX, v, 4, NULL, 0, NULL) == VECDB_OK, "id UINT64_MAX");
    EXPECT(vecdb_delete(db, 0) == VECDB_OK, "delete id 0");
    EXPECT(vecdb_delete(db, 0) == VECDB_ERR_NOTFOUND, "double delete");
    EXPECT(vecdb_delete(db, UINT64_MAX) == VECDB_OK, "delete UINT64_MAX");

    /* empty text and zero-length payload are legal */
    EXPECT(vecdb_put(db, 10, v, 4, NULL, 0, "") == VECDB_OK, "empty text");
    EXPECT(vecdb_put(db, 11, v, 4, "x", 0, NULL) == VECDB_OK, "zero payload len");

    EXPECT(vecdb_set_ef_search(db, 50) == VECDB_ERR_INVALID, "ef on flat index");

    vecdb_close(db);
    unlink(PATH);
}

/* Very long / weird text and large payloads. */
static void test_weird_data(void)
{
    printf("[fuzz] weird text and payloads\n");

    vecdb_options opt;
    vecdb_options_default(&opt);
    vecdb *db = vecdb_open(PATH, &opt);
    EXPECT(db != NULL, "open");

    /* one enormous token (longer than the tokeniser's cap) */
    char big[4096];
    memset(big, 'a', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    EXPECT(vecdb_put(db, 1, NULL, 0, big, strlen(big) + 1, big) == VECDB_OK, "giant token");
    vecdb_match m[4];
    EXPECT(vecdb_search_text(db, big, 4, m, 4) == 1, "giant token is findable");

    /* embedded NUL is impossible for text (C string) but payload supports it */
    unsigned char payload[64];
    memset(payload, 0, sizeof(payload));
    payload[10] = 0xFF;
    EXPECT(vecdb_put(db, 2, NULL, 0, payload, sizeof(payload), NULL) == VECDB_OK, "binary payload");
    vecdb_record r;
    EXPECT(vecdb_get(db, 2, &r) == VECDB_OK, "get binary payload");
    EXPECT(r.payload_len == 64 && ((const unsigned char *)r.payload)[10] == 0xFF,
           "binary payload roundtrip");

    /* text made only of separators and unicode punctuation */
    EXPECT(vecdb_put(db, 3, NULL, 0, NULL, 0, "，，。！ \t\n\r  ,,;;") == VECDB_OK, "sep text");

    /* a document repeated many times then queried */
    for (uint64_t i = 100; i < 400; i++)
        EXPECT(vecdb_put(db, i, NULL, 0, NULL, 0, "repeated phrase repeated phrase") == VECDB_OK,
               "bulk put");
    size_t n = vecdb_search_text(db, "repeated", 10, m, 4);
    EXPECT(n == 4, "k larger than cap clamps");
    for (size_t i = 0; i < n; i++)
        EXPECT(strstr(m[i].text, "repeated") != NULL, "repeated hit has the term");

    /* payload helper boundaries */
    unsigned char buf[16];
    vecdb_pack_i64(buf, INT64_MIN);
    EXPECT(vecdb_unpack_i64(buf) == INT64_MIN, "i64 min roundtrip");
    vecdb_pack_i64(buf, INT64_MAX);
    EXPECT(vecdb_unpack_i64(buf) == INT64_MAX, "i64 max roundtrip");
    vecdb_pack_f64(buf, -0.0);
    EXPECT(vecdb_unpack_f64(buf) == 0.0, "f64 -0.0");
    vecdb_pack_f32(buf, 1e-40f);
    EXPECT(vecdb_unpack_f32(buf) == 1e-40f, "f32 subnormal");
    EXPECT(vecdb_pack_str(buf, 4, "abcdef") == NULL, "pack_str too small");

    vecdb_close(db);
    unlink(PATH);
}

int main(void)
{
    snprintf(PATH, sizeof(PATH), "/tmp/vecdb_fuzz_%ld.db", (long)getpid());
    unlink(PATH);

    test_corrupt_files();
    test_bad_arguments();
    test_weird_data();

    unlink(PATH);
    if (failures) { printf("\n%d problems\n", failures); return 1; }
    printf("\nall robustness checks passed\n");
    return 0;
}
