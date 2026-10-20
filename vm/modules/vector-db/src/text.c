/*
 * text.c — tokenizer and inverted index with Okapi BM25 ranking.
 *
 * Each term owns a contiguous, slot-sorted array of postings rather than
 * a linked list.  Contiguous storage is what makes BM25 scoring fast: the
 * scoring loop walks a packed (slot, tf) array with no pointer chasing.
 * Slots within one term's array are unique and ascending, which lets
 * deletion find a posting with a binary search.
 *
 * Every slot also remembers the terms it contributed so that removal is
 * O(tokens of the document) instead of O(vocabulary).  Term strings are
 * owned by the index and live until the index is freed, so re-upserting
 * the same id is cheap.
 *
 * Note that posting arrays grow by memmove on insertion; for the common
 * case of sequentially assigned slots the new posting is appended and the
 * move is a no-op.
 */
#include "internal.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BM25_K1 1.2
#define BM25_B  0.75
#define MAX_TOKEN_LEN 63

typedef struct {
    uint32_t slot;
    uint32_t tf;
} posting_t;

typedef struct {
    char      *term;    /* NULL marks an empty hash bucket */
    posting_t *post;    /* ascending by slot */
    uint32_t   n, cap;
} term_t;

typedef struct {
    uint32_t  term;     /* index into the entry table */
    uint32_t  at;       /* position of this doc's posting in term->post */
} docref_t;

typedef struct {
    docref_t *refs;
    uint32_t  n, cap;
} docterms_t;

struct text_index {
    term_t     *entries;
    size_t      cap, count;
    docterms_t *docs;
    size_t      docs_cap;
    uint32_t   *doclen;
    uint32_t    n_docs;
    uint64_t    total_len;
};

/* ------------------------------------------------------------------ */
/* tokenizer                                                           */
/* ------------------------------------------------------------------ */
static int is_token_char(unsigned char c)
{
    return isalnum(c) != 0 || c >= 0x80;   /* keep UTF-8 sequences intact */
}

/* Decode one UTF-8 codepoint.  Returns its byte length (>= 1).
 * Invalid bytes decode to themselves. */
static size_t utf8_decode(const unsigned char *p, uint32_t *cp)
{
    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) |
              (uint32_t)(p[2] & 0x3F);
        return 3;
    }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
        (p[3] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) |
              ((uint32_t)(p[2] & 0x3F) << 6) | (uint32_t)(p[3] & 0x3F);
        return 4;
    }
    *cp = p[0];
    return 1;
}

/* Languages written without spaces (Japanese, Chinese, Korean) get one
 * token per character — a cheap bigram-free CJK strategy that still gives
 * usable recall for substring queries. */
static int is_cjk(uint32_t cp)
{
    return (cp >= 0x3040 && cp <= 0x30FF) ||     /* hiragana / katakana  */
           (cp >= 0x3400 && cp <= 0x9FFF) ||     /* CJK ext A + unified  */
           (cp >= 0xAC00 && cp <= 0xD7AF) ||     /* hangul syllables    */
           (cp >= 0xF900 && cp <= 0xFAFF) ||     /* compatibility forms  */
           (cp >= 0x20000 && cp <= 0x2FA1F);     /* CJK ext B and beyond */
}

static const char *const STOPWORDS[] = {
    "a", "about", "after", "all", "also", "an", "and", "any", "are", "as", "at",
    "be", "because", "been", "before", "being", "but", "by",
    "can", "could",
    "did", "do", "does", "doing", "during",
    "each", "few", "for", "from", "further",
    "had", "has", "have", "having", "he", "her", "here", "hers", "him", "his",
    "how", "however",
    "i", "if", "in", "into", "is", "it", "its",
    "just",
    "me", "more", "most", "my",
    "no", "nor", "not", "now",
    "of", "off", "on", "once", "only", "or", "other", "our", "out", "over", "own",
    "same", "she", "should", "so", "some", "such",
    "than", "that", "the", "their", "them", "then", "there", "these", "they",
    "this", "those", "through", "to", "too",
    "under", "until", "up", "us",
    "very",
    "was", "we", "were", "what", "when", "where", "which", "while", "who",
    "whom", "why", "will", "with", "would",
    "you", "your"
};

static int is_stopword(const char *w)
{
    for (size_t i = 0; i < sizeof(STOPWORDS) / sizeof(STOPWORDS[0]); i++) {
        if (strcmp(w, STOPWORDS[i]) == 0) return 1;
    }
    return 0;
}

static void free_tokens(char **tokens, size_t n)
{
    if (!tokens) return;
    for (size_t i = 0; i < n; i++) free(tokens[i]);
    free(tokens);
}

/* append strdup(s) to *out[*n] */
static int emit_token(char ***out, size_t *n, size_t *cap, const char *s)
{
    if (*n == *cap) {
        size_t nc = *cap * 2;
        char **no = (char **)realloc(*out, nc * sizeof(char *));
        if (!no) return -1;
        *out = no;
        *cap = nc;
    }
    (*out)[*n] = strdup(s);
    if (!(*out)[*n]) return -1;
    (*n)++;
    return 0;
}

char **vecdb_tokenize(const char *text, size_t *out_count)
{
    *out_count = 0;
    if (!text) return NULL;

    size_t cap = 16, n = 0;
    char **out = (char **)malloc(cap * sizeof(char *));
    if (!out) return NULL;

    const unsigned char *p = (const unsigned char *)text;
    char buf[MAX_TOKEN_LEN + 1];

    while (*p) {
        if (!is_token_char(*p)) { p++; continue; }

        size_t bl = 0;
        while (*p && is_token_char(*p)) {
            uint32_t cp;
            size_t nb = utf8_decode(p, &cp);

            if (is_cjk(cp)) {
                /* flush the word built so far, then index this character */
                if (bl) {
                    buf[bl] = '\0';
                    if (!is_stopword(buf) && emit_token(&out, &n, &cap, buf) != 0) {
                        free_tokens(out, n);
                        return NULL;
                    }
                    bl = 0;
                }
                char cjk[8];
                memcpy(cjk, p, nb);
                cjk[nb] = '\0';
                if (emit_token(&out, &n, &cap, cjk) != 0) {
                    free_tokens(out, n);
                    return NULL;
                }
                p += nb;
                continue;
            }

            for (size_t i = 0; i < nb && bl + nb <= MAX_TOKEN_LEN; i++)
                buf[bl++] = (char)(p[i] < 0x80 ? tolower(p[i]) : p[i]);
            p += nb;
        }

        if (bl) {
            buf[bl] = '\0';
            if (!is_stopword(buf) && emit_token(&out, &n, &cap, buf) != 0) {
                free_tokens(out, n);
                return NULL;
            }
        }
    }

    if (n == 0) { free(out); return NULL; }
    *out_count = n;
    return out;
}

/* ------------------------------------------------------------------ */
/* hash map                                                            */
/* ------------------------------------------------------------------ */
static uint64_t fnv1a(const char *s)
{
    uint64_t h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 1099511628211ULL;
    }
    return h;
}

static term_t *map_find(const text_index_t *idx, const char *term)
{
    if (idx->cap == 0) return NULL;
    size_t mask = idx->cap - 1;
    size_t i = (size_t)fnv1a(term) & mask;
    for (size_t probe = 0; probe < idx->cap; probe++) {
        term_t *e = &idx->entries[i];
        if (!e->term) return NULL;
        if (strcmp(e->term, term) == 0) return e;
        i = (i + 1) & mask;
    }
    return NULL;
}

static int map_grow(text_index_t *idx)
{
    size_t ncap = idx->cap ? idx->cap * 2 : 256;
    term_t *ne = (term_t *)calloc(ncap, sizeof(term_t));
    if (!ne) return -1;

    size_t mask = ncap - 1;
    for (size_t i = 0; i < idx->cap; i++) {
        term_t *old = &idx->entries[i];
        if (!old->term) continue;
        size_t j = (size_t)fnv1a(old->term) & mask;
        while (ne[j].term) j = (j + 1) & mask;
        ne[j] = *old;          /* steal the entry (term string stays owned) */
    }
    free(idx->entries);
    idx->entries = ne;
    idx->cap = ncap;
    return 0;
}

/* Find, or create, the entry for `term`.  When created, ownership of the
 * malloc'd string is transferred to the index and *owned is set to 1. */
static term_t *map_get_or_create(text_index_t *idx, char *term, int *owned)
{
    *owned = 0;
    if (idx->cap == 0 || (idx->count + 1) * 4 >= idx->cap * 3) {
        if (map_grow(idx) != 0) return NULL;
    }
    size_t mask = idx->cap - 1;
    size_t i = (size_t)fnv1a(term) & mask;
    while (idx->entries[i].term) {
        if (strcmp(idx->entries[i].term, term) == 0) return &idx->entries[i];
        i = (i + 1) & mask;
    }
    idx->entries[i].term = term;
    idx->entries[i].post = NULL;
    idx->entries[i].n = idx->entries[i].cap = 0;
    idx->count++;
    *owned = 1;
    return &idx->entries[i];
}

/* ------------------------------------------------------------------ */
/* per-document bookkeeping                                            */
/* ------------------------------------------------------------------ */
static int ensure_docs(text_index_t *idx, uint32_t slot)
{
    if (slot < idx->docs_cap) return 0;
    size_t ncap = idx->docs_cap ? idx->docs_cap : 64;
    while (ncap <= slot) ncap *= 2;

    docterms_t *nd = (docterms_t *)realloc(idx->docs, ncap * sizeof(docterms_t));
    if (!nd) return -1;
    memset(nd + idx->docs_cap, 0, (ncap - idx->docs_cap) * sizeof(docterms_t));
    idx->docs = nd;

    uint32_t *nl = (uint32_t *)realloc(idx->doclen, ncap * sizeof(uint32_t));
    if (!nl) return -1;
    memset(nl + idx->docs_cap, 0, (ncap - idx->docs_cap) * sizeof(uint32_t));
    idx->doclen = nl;

    idx->docs_cap = ncap;
    return 0;
}

static int doc_add_ref(docterms_t *d, uint32_t term, uint32_t at)
{
    if (d->n == d->cap) {
        uint32_t nc = d->cap ? d->cap * 2 : 8;
        docref_t *nr = (docref_t *)realloc(d->refs, (size_t)nc * sizeof(docref_t));
        if (!nr) return -1;
        d->refs = nr;
        d->cap = nc;
    }
    d->refs[d->n].term = term;
    d->refs[d->n].at = at;
    d->n++;
    return 0;
}

/* Insert (slot, tf) into `e->post`, keeping it sorted by slot.
 * Returns the insertion position, or -1 on allocation failure. */
static long posting_insert(term_t *e, uint32_t slot, uint32_t tf)
{
    /* fast path: append (slots usually arrive in increasing order) */
    if (e->n == 0 || e->post[e->n - 1].slot < slot) {
        if (e->n == e->cap) {
            uint32_t nc = e->cap ? e->cap * 2 : 4;
            posting_t *np = (posting_t *)realloc(e->post, (size_t)nc * sizeof(posting_t));
            if (!np) return -1;
            e->post = np;
            e->cap = nc;
        }
        e->post[e->n].slot = slot;
        e->post[e->n].tf = tf;
        return (long)e->n++;
    }

    /* slow path: binary search, then shift the tail */
    uint32_t lo = 0, hi = e->n;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (e->post[mid].slot < slot) lo = mid + 1;
        else hi = mid;
    }
    if (lo < e->n && e->post[lo].slot == slot) {
        e->post[lo].tf = tf;      /* replace */
        return (long)lo;
    }
    if (e->n == e->cap) {
        uint32_t nc = e->cap ? e->cap * 2 : 4;
        posting_t *np = (posting_t *)realloc(e->post, (size_t)nc * sizeof(posting_t));
        if (!np) return -1;
        e->post = np;
        e->cap = nc;
    }
    memmove(&e->post[lo + 1], &e->post[lo], (e->n - lo) * sizeof(posting_t));
    e->post[lo].slot = slot;
    e->post[lo].tf = tf;
    e->n++;
    return (long)lo;
}

/* ------------------------------------------------------------------ */
/* public index API                                                    */
/* ------------------------------------------------------------------ */
text_index_t *text_index_new(void)
{
    return (text_index_t *)calloc(1, sizeof(text_index_t));
}

void text_index_free(text_index_t *idx)
{
    if (!idx) return;
    for (size_t i = 0; i < idx->cap; i++) {
        term_t *e = &idx->entries[i];
        if (!e->term) continue;
        free(e->post);
        free(e->term);
    }
    for (size_t i = 0; i < idx->docs_cap; i++) free(idx->docs[i].refs);
    free(idx->entries);
    free(idx->docs);
    free(idx->doclen);
    free(idx);
}

int text_index_add(text_index_t *idx, uint32_t slot, const char *text)
{
    if (!idx) return VECDB_ERR_INVALID;
    if (ensure_docs(idx, slot) != 0) return VECDB_ERR_NOMEM;

    if (idx->doclen[slot] > 0) text_index_remove(idx, slot);
    if (!text) return VECDB_OK;

    size_t nt = 0;
    char **tokens = vecdb_tokenize(text, &nt);
    if (!tokens) return VECDB_OK;

    uint32_t doclen = 0;
    for (size_t i = 0; i < nt; i++) {
        int owned = 0;
        term_t *e = map_get_or_create(idx, tokens[i], &owned);
        if (!e) {
            for (size_t j = i; j < nt; j++) { free(tokens[j]); tokens[j] = NULL; }
            free(tokens);
            return VECDB_ERR_NOMEM;
        }
        if (owned) {
            tokens[i] = NULL;   /* ownership moved into the index */
        } else {
            free(tokens[i]);
            tokens[i] = NULL;
        }

        /* Insert (or bump) this document's posting in the term's sorted
         * array.  A slot may never appear twice, so an existing posting is
         * found with a binary search rather than appended blindly. */
        {
            size_t ti = (size_t)(e - idx->entries);
            uint32_t lo = 0, hi = e->n;
            while (lo < hi) {
                uint32_t mid = lo + (hi - lo) / 2;
                if (e->post[mid].slot < slot) lo = mid + 1;
                else hi = mid;
            }
            if (lo < e->n && e->post[lo].slot == slot) {
                /* repeated token in the same document */
                e->post[lo].tf++;
                doclen++;
                continue;
            }
            long at = posting_insert(e, slot, 1);
            if (at < 0) { free(tokens); return VECDB_ERR_NOMEM; }
            if (doc_add_ref(&idx->docs[slot], (uint32_t)ti, (uint32_t)at) != 0) {
                free(tokens);
                return VECDB_ERR_NOMEM;
            }
        }
        doclen++;
    }
    free(tokens);

    idx->doclen[slot] = doclen;
    idx->n_docs++;
    idx->total_len += doclen;
    return VECDB_OK;
}

void text_index_remove(text_index_t *idx, uint32_t slot)
{
    if (!idx || slot >= idx->docs_cap || idx->doclen[slot] == 0) return;

    docterms_t *d = &idx->docs[slot];
    /* remove from the back so earlier positions stay valid while shifting */
    for (uint32_t i = d->n; i-- > 0; ) {
        docref_t *ref = &d->refs[i];
        term_t *e = &idx->entries[ref->term];
        uint32_t pos = ref->at;
        if (pos < e->n && e->post[pos].slot == slot) {
            memmove(&e->post[pos], &e->post[pos + 1],
                    (e->n - pos - 1) * sizeof(posting_t));
            e->n--;
        } else {
            /* position drifted (the entry was compacted); binary search */
            uint32_t lo = 0, hi = e->n;
            while (lo < hi) {
                uint32_t mid = lo + (hi - lo) / 2;
                if (e->post[mid].slot < slot) lo = mid + 1;
                else hi = mid;
            }
            if (lo < e->n && e->post[lo].slot == slot) {
                memmove(&e->post[lo], &e->post[lo + 1],
                        (e->n - lo - 1) * sizeof(posting_t));
                e->n--;
            }
        }
    }
    free(d->refs);
    d->refs = NULL;
    d->n = d->cap = 0;

    idx->total_len -= idx->doclen[slot];
    idx->doclen[slot] = 0;
    if (idx->n_docs) idx->n_docs--;
}

/* ------------------------------------------------------------------ */
/* BM25 search                                                         */
/* ------------------------------------------------------------------ */
typedef struct {
    float    score;
    uint32_t slot;
} hit_t;

size_t text_index_search(const text_index_t *idx, const char *query, size_t k,
                         uint32_t *out_slots, float *out_scores,
                         const uint8_t *alive, size_t nslots)
{
    if (!idx || !query || k == 0 || idx->n_docs == 0) return 0;

    size_t nq = 0;
    char **qterms = vecdb_tokenize(query, &nq);
    if (!qterms) return 0;

    float   *scores = (float *)calloc(nslots, sizeof(float));
    uint8_t *seen   = (uint8_t *)calloc(nslots, 1);
    uint32_t *touched = (uint32_t *)malloc(nslots * sizeof(uint32_t));
    if (!scores || !seen || !touched) {
        free(scores); free(seen); free(touched);
        free_tokens(qterms, nq);
        return 0;
    }

    double N = (double)idx->n_docs;
    double avgdl = idx->total_len ? (double)idx->total_len / N : 1.0;
    size_t n_touched = 0;

    for (size_t qi = 0; qi < nq; qi++) {
        /* skip repeated query terms */
        int dup = 0;
        for (size_t qj = 0; qj < qi; qj++) {
            if (strcmp(qterms[qi], qterms[qj]) == 0) { dup = 1; break; }
        }
        if (dup) continue;

        const term_t *e = map_find(idx, qterms[qi]);
        if (!e) continue;

        /* document frequency over living documents only */
        double df = 0;
        for (uint32_t pi = 0; pi < e->n; pi++) {
            uint32_t s = e->post[pi].slot;
            if (s >= nslots) continue;
            if (alive && !alive[s]) continue;
            df += 1.0;
        }
        if (df == 0.0) continue;

        double idf = log(1.0 + (N - df + 0.5) / (df + 0.5));
        double k1p1 = BM25_K1 + 1.0;

        for (uint32_t pi = 0; pi < e->n; pi++) {
            uint32_t s = e->post[pi].slot;
            if (s >= nslots) continue;
            if (alive && !alive[s]) continue;
            double tf = (double)e->post[pi].tf;
            double dl = idx->doclen[s] ? (double)idx->doclen[s] : 1.0;
            double denom = tf + BM25_K1 * (1.0 - BM25_B + BM25_B * dl / avgdl);
            double contrib = idf * (tf * k1p1) / denom;
            if (!seen[s]) { seen[s] = 1; touched[n_touched++] = s; }
            scores[s] += (float)contrib;
        }
    }

    free_tokens(qterms, nq);

    if (n_touched == 0) {
        free(scores); free(seen); free(touched);
        return 0;
    }

    /* Select the k best with a bounded min-heap: O(n_touched * log k)
     * instead of the O(n_touched * k) of repeated linear scans. */
    size_t nout = 0;
    {
        typedef struct { float score; uint32_t slot; } sh_t;
        size_t cap = k < n_touched ? k : n_touched;
        if (cap == 0) { free(scores); free(seen); free(touched); return 0; }
        sh_t *heap = (sh_t *)malloc(cap * sizeof(sh_t));
        if (!heap) { free(scores); free(seen); free(touched); return 0; }
        size_t hn = 0;

        for (size_t j = 0; j < n_touched; j++) {
            uint32_t s = touched[j];
            float sc = scores[s];
            if (hn < cap) {
                size_t i = hn++;
                heap[i].score = sc;
                heap[i].slot = s;
                while (i > 0) {
                    size_t par = (i - 1) / 2;
                    if (heap[i].score < heap[par].score) {
                        sh_t t = heap[i]; heap[i] = heap[par]; heap[par] = t;
                        i = par;
                    } else break;
                }
            } else if (sc > heap[0].score) {
                heap[0].score = sc;
                heap[0].slot = s;
                size_t i = 0;
                for (;;) {
                    size_t l = 2 * i + 1, r = l + 1, b = i;
                    if (l < hn && heap[l].score < heap[b].score) b = l;
                    if (r < hn && heap[r].score < heap[b].score) b = r;
                    if (b == i) break;
                    sh_t t = heap[i]; heap[i] = heap[b]; heap[b] = t;
                    i = b;
                }
            }
        }

        /* The heap is a min-heap: extracting the minimum repeatedly yields
         * ascending scores.  Fill `out` from the back so the caller gets
         * descending (best first) order. */
        size_t left = hn;
        for (size_t i = 0; i < hn; i++) {
            out_slots[hn - 1 - i] = heap[0].slot;
            out_scores[hn - 1 - i] = heap[0].score;
            heap[0] = heap[--left];
            size_t i2 = 0;
            for (;;) {
                size_t l = 2 * i2 + 1, r = l + 1, b = i2;
                if (l < left && heap[l].score < heap[b].score) b = l;
                if (r < left && heap[r].score < heap[b].score) b = r;
                if (b == i2) break;
                sh_t tt = heap[i2]; heap[i2] = heap[b]; heap[b] = tt;
                i2 = b;
            }
        }
        nout = hn;
        free(heap);
    }

    free(scores);
    free(seen);
    free(touched);
    return nout;
}
