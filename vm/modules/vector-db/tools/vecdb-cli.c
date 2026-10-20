/*
 * vecdb-cli — a small command line front end for libvecdb.
 *
 * Usage:
 *   vecdb-cli create  <db> [--dim N] [--metric l2|ip|cosine] [--index flat|hnsw]
 *   vecdb-cli put     <db> [--id N] [--vector "1,2,3"] [--text "..."] [--payload "..."]
 *                          [--payload-file F] [--no-upsert] [--sync]
 *   vecdb-cli get     <db> <id> [--json]
 *   vecdb-cli del     <db> <id>
 *   vecdb-cli search  <db> --vector "1,2,3" [--k N] [--json]
 *   vecdb-cli text    <db> --text "query" [--k N] [--json]
 *   vecdb-cli hybrid  <db> [--vector "1,2,3"] [--text "query"] [--k N]
 *                          [--alpha A] [--json]
 *   vecdb-cli info    <db> [--json]
 *   vecdb-cli list    <db> [--json]
 *   vecdb-cli dump    <db> [--json]
 *   vecdb-cli compact <db>
 *
 * Vectors are comma- or space-separated floats.  The payload is an opaque
 * blob: the CLI stores the literal bytes you pass and prints them as text
 * (or as a JSON byte array with --json), mirroring the C API.
 */

#include "vecdb.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
static const char *prog = "vecdb-cli";

static void usage(FILE *f)
{
    fprintf(f,
        "usage: %s <command> <db> [options]\n"
        "\n"
        "commands:\n"
        "  create  <db> [--dim N] [--metric l2|ip|cosine] [--index flat|hnsw]\n"
        "  put     <db> [--id N] [--vector \"1,2,3\"] [--text S]\n"
        "               [--payload S | --payload-file F] [--no-upsert] [--sync]\n"
        "  get     <db> <id> [--json]\n"
        "  del     <db> <id>\n"
        "  search  <db> --vector \"1,2,3\"  [--k N] [--json]\n"
        "  text    <db> --text S           [--k N] [--json]\n"
        "  hybrid  <db> [--vector V] [--text S] [--k N] [--alpha A] [--json]\n"
        "  info    <db> [--json]\n"
        "  list    <db> [--json]\n"
        "  dump    <db> [--json]\n"
        "  compact <db>\n"
        "\n"
        "When --id is omitted for `put`, the next free id is chosen.\n",
        prog);
}

static int die(const char *what)
{
    fprintf(stderr, "%s: %s\n", prog, what);
    return 2;
}

static int die_db(vecdb *db, const char *what)
{
    const char *msg = db ? vecdb_last_error(db) : NULL;
    fprintf(stderr, "%s: %s%s%s\n", prog, what,
            msg && *msg ? ": " : "", msg && *msg ? msg : "");
    return 1;
}

/* ------------------------------------------------------------------ */
/* Parsing helpers                                                     */
/* ------------------------------------------------------------------ */
typedef struct {
    float  *v;
    size_t  n;
} fvec;

static int parse_vector(const char *s, fvec *out)
{
    out->v = NULL;
    out->n = 0;
    if (!s) return 0;

    size_t cap = 8, n = 0;
    float *v = malloc(cap * sizeof(float));
    if (!v) return -1;

    const char *p = s;
    while (*p) {
        while (*p == ' ' || *p == ',' || *p == '[' || *p == ']' || *p == '\t')
            p++;
        if (!*p) break;
        errno = 0;
        char *end = NULL;
        float x = strtof(p, &end);
        if (end == p) { free(v); return -1; }
        if (n == cap) {
            cap *= 2;
            float *nv = realloc(v, cap * sizeof(float));
            if (!nv) { free(v); return -1; }
            v = nv;
        }
        v[n++] = x;
        p = end;
    }
    out->v = v;
    out->n = n;
    return 0;
}

static int metric_from(const char *s, vecdb_metric *m)
{
    if (!s || strcmp(s, "l2") == 0)   { *m = VECDB_METRIC_L2;     return 0; }
    if (strcmp(s, "ip") == 0)         { *m = VECDB_METRIC_IP;     return 0; }
    if (strcmp(s, "cosine") == 0)     { *m = VECDB_METRIC_COSINE; return 0; }
    return -1;
}

static int index_from(const char *s, vecdb_index *i)
{
    if (!s || strcmp(s, "flat") == 0) { *i = VECDB_INDEX_FLAT; return 0; }
    if (strcmp(s, "hnsw") == 0)       { *i = VECDB_INDEX_HNSW; return 0; }
    return -1;
}

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)sz ? (size_t)sz : 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    *len = got;
    return buf;
}

/* ------------------------------------------------------------------ */
/* Output helpers                                                      */
/* ------------------------------------------------------------------ */
static void print_bytes(const void *p, size_t n)
{
    const unsigned char *b = p;
    for (size_t i = 0; i < n; i++)
        putchar((b[i] >= 32 && b[i] < 127) || b[i] == '\n' || b[i] == '\t'
                    ? b[i] : '.');
}

static void json_bytes(const void *p, size_t n)
{
    const unsigned char *b = p;
    printf("[");
    for (size_t i = 0; i < n; i++)
        printf("%s%u", i ? "," : "", (unsigned)b[i]);
    printf("]");
}

static void json_string(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout);  break;
        case '\r': fputs("\\r", stdout);  break;
        case '\t': fputs("\\t", stdout);  break;
        default:
            if (*p < 32) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

static void print_vector(const float *v, size_t n)
{
    printf("[");
    for (size_t i = 0; i < n; i++)
        printf("%s%g", i ? "," : "", v ? v[i] : 0.0);
    printf("]");
}

static void print_matches(vecdb *db, const vecdb_match *m, size_t n, int json)
{
    size_t dim = vecdb_dim(db);
    if (json) {
        printf("[\n");
        for (size_t i = 0; i < n; i++) {
            printf("  {\"id\": %llu, \"score\": %.6g, \"text\": ",
                   (unsigned long long)m[i].id, m[i].score);
            if (m[i].text) json_string(m[i].text); else fputs("null", stdout);
            printf(", \"payload\": ");
            if (m[i].payload && m[i].payload_len)
                json_bytes(m[i].payload, m[i].payload_len);
            else fputs("null", stdout);
            printf("}%s\n", i + 1 < n ? "," : "");
        }
        printf("]\n");
        (void)dim;
    } else {
        for (size_t i = 0; i < n; i++) {
            printf("%.6g  #%-6llu  ", m[i].score, (unsigned long long)m[i].id);
            if (m[i].text) printf("%s", m[i].text);
            if (m[i].payload && m[i].payload_len) {
                printf("  [payload ");
                print_bytes(m[i].payload, m[i].payload_len);
                printf("]");
            }
            putchar('\n');
        }
    }
}

/* ------------------------------------------------------------------ */
/* Commands                                                            */
/* ------------------------------------------------------------------ */
static vecdb *open_db(const char *path, int create, vecdb_options *opt_in)
{
    vecdb_options o;
    if (opt_in) {
        o = *opt_in;
    } else {
        vecdb_options_default(&o);
    }
    if (!create) {
        FILE *f = fopen(path, "rb");
        if (!f) {
            fprintf(stderr, "%s: cannot open '%s': %s\n", prog, path,
                    strerror(errno));
            return NULL;
        }
        fclose(f);
    } else {
        FILE *f = fopen(path, "rb");
        if (!f && errno != ENOENT) {
            fprintf(stderr, "%s: cannot open '%s': %s\n", prog, path,
                    strerror(errno));
            return NULL;
        }
        if (f)
            fclose(f);
    }
    vecdb *db = vecdb_open(path, &o);
    if (!db) {
        fprintf(stderr, "%s: cannot open '%s': not a vecdb file or corrupt\n",
                prog, path);
    }
    return db;
}

static int cmd_create(int argc, char **argv)
{
    if (argc < 1) return die("create needs a <db>");
    const char *path = argv[0];
    vecdb_options o;
    vecdb_options_default(&o);
    o.dim = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dim") == 0 && i + 1 < argc) {
            o.dim = strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--metric") == 0 && i + 1 < argc) {
            if (metric_from(argv[++i], &o.metric) < 0) return die("bad --metric");
        } else if (strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            if (index_from(argv[++i], &o.index) < 0) return die("bad --index");
        } else {
            return die("unknown option to create");
        }
    }

    vecdb *db = vecdb_open(path, &o);
    if (!db) return die_db(NULL, "cannot create database");
    vecdb_flush(db);
    printf("created %s (dim=%zu)\n", vecdb_path(db), vecdb_dim(db));
    vecdb_close(db);
    return 0;
}

static int cmd_put(int argc, char **argv)
{
    if (argc < 1) return die("put needs a <db>");
    const char *path = argv[0];
    const char *vstr = NULL, *text = NULL, *payload = NULL, *pfile = NULL;
    long long id = -1;
    int upsert = 1, sync = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) id = strtoll(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--vector") == 0 && i + 1 < argc) vstr = argv[++i];
        else if (strcmp(argv[i], "--text") == 0 && i + 1 < argc) text = argv[++i];
        else if (strcmp(argv[i], "--payload") == 0 && i + 1 < argc) payload = argv[++i];
        else if (strcmp(argv[i], "--payload-file") == 0 && i + 1 < argc) pfile = argv[++i];
        else if (strcmp(argv[i], "--no-upsert") == 0) upsert = 0;
        else if (strcmp(argv[i], "--sync") == 0) sync = 1;
        else return die("unknown option to put");
    }

    vecdb *db = open_db(path, 1, NULL);
    if (!db) return 1;

    fvec vec;
    if (parse_vector(vstr, &vec) < 0) { vecdb_close(db); return die("bad --vector"); }

    char *filebuf = NULL;
    size_t filelen = 0;
    if (pfile) {
        filebuf = read_file(pfile, &filelen);
        if (!filebuf) { free(vec.v); vecdb_close(db); return die("cannot read --payload-file"); }
        payload = filebuf;
    }
    size_t payload_len = payload ? strlen(payload) : 0;
    if (pfile) payload_len = filelen;

    if (id < 0) {
        /* next free id */
        size_t cap = vecdb_count(db) + 1;
        uint64_t *ids = malloc(cap * sizeof(uint64_t));
        size_t n = vecdb_list_ids(db, ids, cap);
        uint64_t next = 1;
        if (n) {
            uint64_t mx = 0;
            for (size_t i = 0; i < n; i++) if (ids[i] > mx) mx = ids[i];
            next = mx + 1;
        }
        id = (long long)next;
        free(ids);
    }

    unsigned flags = (upsert ? 0 : VECDB_INSERT_ONLY) | (sync ? VECDB_SYNC : 0);
    int rc = vecdb_put_ex(db, (uint64_t)id, vec.v, vec.n,
                          payload, payload_len, text, flags);
    free(vec.v);
    free(filebuf);
    if (rc != VECDB_OK) { int r = die_db(db, "put failed"); vecdb_close(db); return r; }

    printf("stored #%lld\n", id);
    vecdb_close(db);
    return 0;
}

static int cmd_get(int argc, char **argv)
{
    if (argc < 2) return die("get needs <db> <id>");
    int json = 0;
    for (int i = 2; i < argc; i++) if (strcmp(argv[i], "--json") == 0) json = 1;

    vecdb *db = open_db(argv[0], 0, NULL);
    if (!db) return 1;

    vecdb_record r;
    int rc = vecdb_get(db, strtoull(argv[1], NULL, 10), &r);
    if (rc != VECDB_OK) { int e = die_db(db, "get failed"); vecdb_close(db); return e; }

    size_t dim = vecdb_dim(db);
    if (json) {
        printf("{\"id\": %llu, \"vector\": ", (unsigned long long)r.id);
        if (r.vec) print_vector(r.vec, dim); else fputs("null", stdout);
        printf(", \"text\": ");
        if (r.text) json_string(r.text); else fputs("null", stdout);
        printf(", \"payload\": ");
        if (r.payload && r.payload_len) json_bytes(r.payload, r.payload_len);
        else fputs("null", stdout);
        printf("}\n");
    } else {
        printf("id      : %llu\n", (unsigned long long)r.id);
        printf("vector  : ");
        if (r.vec) { print_vector(r.vec, dim); putchar('\n'); }
        else printf("(none)\n");
        printf("text    : %s\n", r.text ? r.text : "(none)");
        printf("payload : ");
        if (r.payload && r.payload_len) {
            print_bytes(r.payload, r.payload_len);
            printf("  (%zu bytes)", r.payload_len);
        } else printf("(none)");
        putchar('\n');
    }
    vecdb_close(db);
    return 0;
}

static int cmd_del(int argc, char **argv)
{
    if (argc < 2) return die("del needs <db> <id>");
    vecdb *db = open_db(argv[0], 0, NULL);
    if (!db) return 1;
    int rc = vecdb_delete(db, strtoull(argv[1], NULL, 10));
    if (rc != VECDB_OK) { int e = die_db(db, "delete failed"); vecdb_close(db); return e; }
    printf("deleted #%s\n", argv[1]);
    vecdb_close(db);
    return 0;
}

/* shared by search/text/hybrid */
static int do_search(const char *path, const char *vstr, const char *text,
                     long k, double alpha, int json)
{
    vecdb *db = open_db(path, 0, NULL);
    if (!db) return 1;

    fvec vec;
    if (parse_vector(vstr, &vec) < 0) { vecdb_close(db); return die("bad --vector"); }
    if (k <= 0) k = 10;

    vecdb_match *out = malloc((size_t)k * sizeof(vecdb_match));
    if (!out) { free(vec.v); vecdb_close(db); return die("out of memory"); }

    size_t n;
    if (vec.n && text)
        n = vecdb_search_hybrid(db, vec.v, vec.n, text, (size_t)k, (float)alpha, out, (size_t)k);
    else if (vec.n)
        n = vecdb_search_vector(db, vec.v, vec.n, (size_t)k, out, (size_t)k);
    else if (text)
        n = vecdb_search_text(db, text, (size_t)k, out, (size_t)k);
    else { free(out); free(vec.v); vecdb_close(db); return die("nothing to search for"); }

    print_matches(db, out, n, json);
    free(out);
    free(vec.v);
    vecdb_close(db);
    return 0;
}

static int cmd_info(int argc, char **argv)
{
    if (argc < 1) return die("info needs a <db>");
    int json = 0;
    for (int i = 1; i < argc; i++) if (strcmp(argv[i], "--json") == 0) json = 1;

    vecdb *db = open_db(argv[0], 0, NULL);
    if (!db) return 1;

    const char *metric =
        vecdb_metric_of(db) == VECDB_METRIC_L2 ? "l2" :
        vecdb_metric_of(db) == VECDB_METRIC_IP ? "ip" : "cosine";
    const char *index =
        vecdb_index_of(db) == VECDB_INDEX_HNSW ? "hnsw" : "flat";

    if (json) {
        printf("{\"path\": "); json_string(vecdb_path(db));
        printf(", \"count\": %zu, \"capacity\": %zu, \"dim\": %zu, "
               "\"metric\": \"%s\", \"index\": \"%s\"}\n",
               vecdb_count(db), vecdb_capacity(db), vecdb_dim(db), metric, index);
    } else {
        printf("path     : %s\n", vecdb_path(db));
        printf("count    : %zu\n", vecdb_count(db));
        printf("capacity : %zu\n", vecdb_capacity(db));
        printf("dim      : %zu\n", vecdb_dim(db));
        printf("metric   : %s\n", metric);
        printf("index    : %s\n", index);
    }
    vecdb_close(db);
    return 0;
}

static int cmd_list(int argc, char **argv)
{
    if (argc < 1) return die("list needs a <db>");
    int json = 0;
    for (int i = 1; i < argc; i++) if (strcmp(argv[i], "--json") == 0) json = 1;

    vecdb *db = open_db(argv[0], 0, NULL);
    if (!db) return 1;

    size_t n = vecdb_list_ids(db, NULL, 0);
    uint64_t *ids = malloc((n ? n : 1) * sizeof(uint64_t));
    if (!ids) { vecdb_close(db); return die("out of memory"); }
    n = vecdb_list_ids(db, ids, n);
    if (json) {
        putchar('[');
        for (size_t i = 0; i < n; i++)
            printf("%s%llu", i ? ", " : "", (unsigned long long)ids[i]);
        printf("]\n");
    } else {
        for (size_t i = 0; i < n; i++)
            printf("%llu\n", (unsigned long long)ids[i]);
    }
    free(ids);
    vecdb_close(db);
    return 0;
}

static int cmd_dump(int argc, char **argv)
{
    if (argc < 1) return die("dump needs a <db>");
    int json = 0;
    for (int i = 1; i < argc; i++) if (strcmp(argv[i], "--json") == 0) json = 1;

    vecdb *db = open_db(argv[0], 0, NULL);
    if (!db) return 1;

    size_t cap = vecdb_list_ids(db, NULL, 0);
    uint64_t *ids = malloc((cap ? cap : 1) * sizeof(uint64_t));
    if (!ids) { vecdb_close(db); return die("out of memory"); }
    size_t n = vecdb_list_ids(db, ids, cap);
    size_t dim = vecdb_dim(db);

    if (json) {
        printf("[\n");
        for (size_t i = 0; i < n; i++) {
            vecdb_record r;
            if (vecdb_get(db, ids[i], &r) != VECDB_OK) continue;
            printf("  {\"id\": %llu, \"vector\": ", (unsigned long long)r.id);
            if (r.vec) print_vector(r.vec, dim); else fputs("null", stdout);
            printf(", \"text\": ");
            if (r.text) json_string(r.text); else fputs("null", stdout);
            printf(", \"payload\": ");
            if (r.payload && r.payload_len) json_bytes(r.payload, r.payload_len);
            else fputs("null", stdout);
            printf("}%s\n", i + 1 < n ? "," : "");
        }
        printf("]\n");
    } else {
        for (size_t i = 0; i < n; i++) {
            vecdb_record r;
            if (vecdb_get(db, ids[i], &r) != VECDB_OK) continue;
            printf("#%llu  ", (unsigned long long)r.id);
            if (r.vec) print_vector(r.vec, dim); else fputs("(no vector)", stdout);
            if (r.text) printf("  %s", r.text);
            if (r.payload && r.payload_len) {
                printf("  [");
                print_bytes(r.payload, r.payload_len);
                printf("]");
            }
            putchar('\n');
        }
    }
    free(ids);
    vecdb_close(db);
    return 0;
}

static int cmd_compact(int argc, char **argv)
{
    if (argc < 1) return die("compact needs a <db>");
    vecdb *db = open_db(argv[0], 0, NULL);
    if (!db) return 1;
    if (vecdb_compact(db) != VECDB_OK) { int e = die_db(db, "compact failed"); vecdb_close(db); return e; }
    printf("compacted; %zu records remain\n", vecdb_count(db));
    vecdb_close(db);
    return 0;
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc < 2) { usage(stderr); return 2; }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        return 0;
    }

    const char *cmd = argv[1];
    int rest = argc - 2;
    char **args = argv + 2;

    if (strcmp(cmd, "create") == 0)  return cmd_create(rest, args);
    if (strcmp(cmd, "put") == 0)     return cmd_put(rest, args);
    if (strcmp(cmd, "get") == 0)     return cmd_get(rest, args);
    if (strcmp(cmd, "del") == 0)     return cmd_del(rest, args);
    if (strcmp(cmd, "compact") == 0) return cmd_compact(rest, args);
    if (strcmp(cmd, "info") == 0)    return cmd_info(rest, args);
    if (strcmp(cmd, "list") == 0)    return cmd_list(rest, args);
    if (strcmp(cmd, "dump") == 0)    return cmd_dump(rest, args);

    if (strcmp(cmd, "search") == 0 || strcmp(cmd, "text") == 0 ||
        strcmp(cmd, "hybrid") == 0) {
        if (rest < 1) return die("a <db> is required");
        const char *path = args[0];
        const char *vstr = NULL, *text = NULL;
        long k = 10;
        double alpha = 0.5;
        int json = 0;
        for (int i = 1; i < rest; i++) {
            if (strcmp(args[i], "--vector") == 0 && i + 1 < rest) vstr = args[++i];
            else if (strcmp(args[i], "--text") == 0 && i + 1 < rest) text = args[++i];
            else if (strcmp(args[i], "--k") == 0 && i + 1 < rest) k = strtol(args[++i], NULL, 10);
            else if (strcmp(args[i], "--alpha") == 0 && i + 1 < rest) alpha = strtod(args[++i], NULL);
            else if (strcmp(args[i], "--json") == 0) json = 1;
            else return die("unknown search option");
        }
        if (strcmp(cmd, "text") == 0 && !text) return die("text needs --text");
        if (strcmp(cmd, "search") == 0 && !vstr) return die("search needs --vector");
        return do_search(path, vstr, text, k, alpha, json);
    }

    fprintf(stderr, "%s: unknown command '%s'\n", prog, cmd);
    usage(stderr);
    return 2;
}
