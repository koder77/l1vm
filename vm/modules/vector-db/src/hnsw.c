/*
 * hnsw.c — Hierarchical Navigable Small World graph index.
 *
 * Implements the algorithm from Malkov & Yashunin, "Efficient and robust
 * approximate nearest neighbor search using Hierarchical Navigable Small
 * World graphs" (https://arxiv.org/abs/1603.09320).
 *
 * The graph is stored as a flat array of nodes indexed by the caller's
 * slot number, so a vector lives in `h->vectors[slot * dim]`.  Slots are
 * never recycled while the index is alive (vecdb_compact() rebuilds the
 * whole index instead), which keeps node identity stable.
 *
 * Deleted slots are flagged !in_use; they stay in the graph as routing
 * hops but are never returned and never elected as entry point.
 */
#include "internal.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HNSW_MAX_LEVEL 24
#define HNSW_MIN_M 4

typedef struct {
    uint32_t *items;
    uint16_t  count;
    uint16_t  cap;
} nbrlist_t;

typedef struct {
    uint8_t    in_use;
    uint16_t   level;   /* highest level this node participates in */
    nbrlist_t *nbrs;    /* level + 1 neighbour lists               */
} hnode_t;

typedef struct {
    float    sim;
    uint32_t slot;
} elem_t;

/* Binary heap.  max_heap == 1 keeps the largest sim on top,
 * max_heap == 0 keeps the smallest (a "worst on top" result heap). */
typedef struct {
    elem_t *a;
    size_t  n, cap;
    int     max_heap;
} heap_t;

struct hnsw {
    size_t       dim, m, m0, ef_c, ef;
    vecdb_metric metric;

    float   *vectors;    /* cap * dim */
    hnode_t *nodes;
    size_t   cap;        /* allocated slots */

    uint32_t entry;      /* UINT32_MAX while the graph is empty */
    uint32_t cand_entry; /* best-known replacement for `entry` */
    size_t   live;       /* number of in_use nodes */
    int      max_level;

    uint64_t rng;

    /* scratch reused by every search / insert */
    uint32_t *visited;
    uint32_t  stamp;
    uint32_t *cand_slots;
    float    *cand_sims;
    uint32_t *chosen;   /* insert scratch, m0+1 entries */
    uint32_t *keep;     /* insert scratch, m0+1 entries */
    float    *dist;     /* cached sim(q, slot) for the current search */
};

/* ------------------------------------------------------------------ */
/* tiny RNG (xorshift64*)                                              */
/* ------------------------------------------------------------------ */
static uint64_t rng_next(hnsw_t *h)
{
    uint64_t x = h->rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    h->rng = x;
    return x * 0x2545F4914F6CDD1DULL;
}

/* ------------------------------------------------------------------ */
/* heap helpers                                                        */
/* ------------------------------------------------------------------ */
static int heap_init(heap_t *h, size_t cap, int max_heap)
{
    if (cap < 8) cap = 8;
    h->a = (elem_t *)malloc(cap * sizeof(elem_t));
    if (!h->a) return -1;
    h->n = 0;
    h->cap = cap;
    h->max_heap = max_heap;
    return 0;
}

static void heap_free(heap_t *h)
{
    free(h->a);
    h->a = NULL;
    h->n = h->cap = 0;
}

static int heap_better(const heap_t *h, const elem_t *a, const elem_t *b)
{
    return h->max_heap ? (a->sim > b->sim) : (a->sim < b->sim);
}

static int heap_push(heap_t *h, float sim, uint32_t slot)
{
    if (h->n == h->cap) {
        size_t nc = h->cap * 2;
        elem_t *na = (elem_t *)realloc(h->a, nc * sizeof(elem_t));
        if (!na) return -1;
        h->a = na;
        h->cap = nc;
    }
    size_t i = h->n++;
    h->a[i].sim = sim;
    h->a[i].slot = slot;
    while (i > 0) {
        size_t p = (i - 1) / 2;
        if (heap_better(h, &h->a[i], &h->a[p])) {
            elem_t t = h->a[i];
            h->a[i] = h->a[p];
            h->a[p] = t;
            i = p;
        } else {
            break;
        }
    }
    return 0;
}

static elem_t heap_top(const heap_t *h)
{
    return h->a[0];
}

static elem_t heap_pop(heap_t *h)
{
    elem_t top = h->a[0];
    h->a[0] = h->a[--h->n];
    size_t i = 0;
    for (;;) {
        size_t l = 2 * i + 1, r = l + 1, best = i;
        if (l < h->n && heap_better(h, &h->a[l], &h->a[best])) best = l;
        if (r < h->n && heap_better(h, &h->a[r], &h->a[best])) best = r;
        if (best == i) break;
        elem_t t = h->a[i];
        h->a[i] = h->a[best];
        h->a[best] = t;
        i = best;
    }
    return top;
}

/* ------------------------------------------------------------------ */
/* node / neighbour list helpers                                       */
/* ------------------------------------------------------------------ */
static size_t level_max_links(const hnsw_t *h, int level)
{
    return level == 0 ? h->m0 : h->m;
}

static int nbrlist_push(nbrlist_t *l, uint32_t slot)
{
    if (l->count == l->cap) {
        uint16_t nc = (uint16_t)(l->cap ? l->cap * 2 : 8);
        uint32_t *ni = (uint32_t *)realloc(l->items, (size_t)nc * sizeof(uint32_t));
        if (!ni) return -1;
        l->items = ni;
        l->cap = nc;
    }
    l->items[l->count++] = slot;
    return 0;
}

static void nbrlist_set(nbrlist_t *l, const uint32_t *items, size_t n)
{
    l->count = 0;
    for (size_t i = 0; i < n; i++) {
        if (nbrlist_push(l, items[i]) != 0) return;
    }
}

static int node_ensure_levels(hnsw_t *h, uint32_t slot, int level)
{
    hnode_t *nd = &h->nodes[slot];
    if (nd->nbrs == NULL) {
        nd->nbrs = (nbrlist_t *)calloc((size_t)level + 1, sizeof(nbrlist_t));
        if (!nd->nbrs) return -1;
        nd->level = (uint16_t)level;
        return 0;
    }
    if (level > (int)nd->level) {
        nbrlist_t *nn = (nbrlist_t *)realloc(nd->nbrs, ((size_t)level + 1) * sizeof(nbrlist_t));
        if (!nn) return -1;
        nd->nbrs = nn;
        memset(nd->nbrs + nd->level + 1, 0,
               ((size_t)level - nd->level) * sizeof(nbrlist_t));
        nd->level = (uint16_t)level;
    }
    return 0;
}

static int ensure_slot(hnsw_t *h, uint32_t slot)
{
    if (slot < h->cap) return 0;
    size_t ncap = h->cap ? h->cap : 64;
    while (ncap <= slot) ncap *= 2;

    float *nv = (float *)realloc(h->vectors, ncap * h->dim * sizeof(float));
    if (!nv) return -1;
    memset(nv + h->cap * h->dim, 0, (ncap - h->cap) * h->dim * sizeof(float));
    h->vectors = nv;

    hnode_t *nn = (hnode_t *)realloc(h->nodes, ncap * sizeof(hnode_t));
    if (!nn) return -1;
    memset(nn + h->cap, 0, (ncap - h->cap) * sizeof(hnode_t));
    h->nodes = nn;

    uint32_t *ncs = (uint32_t *)realloc(h->cand_slots, ncap * sizeof(uint32_t));
    if (!ncs) return -1;
    h->cand_slots = ncs;

    float *nsm = (float *)realloc(h->cand_sims, ncap * sizeof(float));
    if (!nsm) return -1;
    h->cand_sims = nsm;

    /* stamps restart from 0 which is safe because the array is zeroed */
    uint32_t *nvis = (uint32_t *)calloc(ncap, sizeof(uint32_t));
    if (!nvis) return -1;
    free(h->visited);
    h->visited = nvis;
    h->stamp = 0;

    float *ndis = (float *)realloc(h->dist, ncap * sizeof(float));
    if (!ndis) return -1;
    h->dist = ndis;

    h->cap = ncap;
    return 0;
}

static inline float sim_slot(const hnsw_t *h, const float *q, uint32_t slot)
{
    return vecdb_sim_rank(h->metric, q, h->vectors + (size_t)slot * h->dim, h->dim);
}

static inline void mark_visited(hnsw_t *h, uint32_t slot)
{
    h->visited[slot] = h->stamp;
}

static inline int is_visited(const hnsw_t *h, uint32_t slot)
{
    return h->visited[slot] == h->stamp;
}

/* ------------------------------------------------------------------ */
/* construction / teardown                                             */
/* ------------------------------------------------------------------ */
hnsw_t *hnsw_new(size_t dim, size_t m, size_t ef_construction, size_t ef_search,
                 vecdb_metric metric, uint64_t seed)
{
    if (dim == 0 || dim > ((size_t)1 << 20)) return NULL;
    if (m < HNSW_MIN_M) m = HNSW_MIN_M;
    if (m > 4096) m = 4096;
    if (ef_construction < 10) ef_construction = 10;
    if (ef_search < 10) ef_search = 10;

    hnsw_t *h = (hnsw_t *)calloc(1, sizeof(hnsw_t));
    if (!h) return NULL;

    h->dim = dim;
    h->m = m;
    h->m0 = 2 * m;
    h->ef_c = ef_construction;
    h->ef = ef_search;
    h->metric = metric;
    h->entry = UINT32_MAX;
    h->cand_entry = UINT32_MAX;
    h->live = 0;
    h->max_level = -1;
    /* reusable per-insertion scratch (never touched concurrently: the
     * caller serialises writes) */
    h->chosen = (uint32_t *)malloc((h->m0 + 1) * sizeof(uint32_t));
    h->keep   = (uint32_t *)malloc((h->m0 + 1) * sizeof(uint32_t));
    if (!h->chosen || !h->keep) {
        free(h->chosen);
        free(h->keep);
        free(h);
        return NULL;
    }
    h->rng = seed ? seed : 0x9E3779B97F4A7C15ULL;
    return h;
}

void hnsw_free(hnsw_t *h)
{
    if (!h) return;
    for (size_t i = 0; i < h->cap; i++) {
        hnode_t *nd = &h->nodes[i];
        if (nd->nbrs) {
            for (int l = 0; l <= nd->level; l++) free(nd->nbrs[l].items);
            free(nd->nbrs);
        }
    }
    free(h->nodes);
    free(h->vectors);
    free(h->cand_slots);
    free(h->cand_sims);
    free(h->chosen);
    free(h->keep);
    free(h->dist);
    free(h->visited);
    free(h);
}

void hnsw_set_ef(hnsw_t *h, size_t ef)
{
    if (h && ef >= 10) h->ef = ef;
}

double hnsw_avg_level(const hnsw_t *h)
{
    size_t n = 0;
    double sum = 0;
    for (size_t i = 0; i < h->cap; i++) {
        if (h->nodes[i].in_use) { sum += h->nodes[i].level; n++; }
    }
    return n ? sum / (double)n : 0.0;
}

/* ------------------------------------------------------------------ */
/* graph search primitives                                             */
/* ------------------------------------------------------------------ */

/* Greedy descent within one level: move to a strictly closer neighbour
 * until no further improvement is possible. */
static uint32_t greedy_search(hnsw_t *h, const float *q, uint32_t ep, int level)
{
    float cur = sim_slot(h, q, ep);
    for (;;) {
        int improved = 0;
        const nbrlist_t *nl = &h->nodes[ep].nbrs[level];
        for (uint16_t i = 0; i < nl->count; i++) {
            uint32_t nb = nl->items[i];
            if (!h->nodes[nb].in_use) continue;
            float s = sim_slot(h, q, nb);
            if (s > cur) { cur = s; ep = nb; improved = 1; }
        }
        if (!improved) return ep;
    }
}

/* Best-first search of `level`, retaining the `ef` best in min-heap `res`
 * (res->a[0] is the current worst of the result set).
 *
 * Distances are memoised in h->dist[] for the duration of one search so
 * that a node reached through several paths is scored exactly once.  This
 * is the single biggest cost in HNSW construction: without it the same
 * neighbour is re-scored once per incoming edge. */
static void search_layer(hnsw_t *h, const float *q, uint32_t ep, size_t ef,
                         int level, heap_t *res)
{
    heap_t cand;
    if (heap_init(&cand, ef + 1, 1) != 0) return;
    if (heap_init(res, ef + 1, 0) != 0) { heap_free(&cand); return; }

    h->stamp++;
    if (h->stamp == 0) {
        memset(h->visited, 0, h->cap * sizeof(uint32_t));
        h->stamp = 1;
    }

    float s0 = sim_slot(h, q, ep);
    mark_visited(h, ep);
    h->dist[ep] = s0;
    heap_push(&cand, s0, ep);
    heap_push(res, s0, ep);

    while (cand.n) {
        elem_t c = heap_pop(&cand);
        if (res->n >= ef && c.sim < heap_top(res).sim) break;

        const nbrlist_t *nl = &h->nodes[c.slot].nbrs[level];
        for (uint16_t i = 0; i < nl->count; i++) {
            uint32_t nb = nl->items[i];
            if (!h->nodes[nb].in_use) continue;
            if (is_visited(h, nb)) continue;
            mark_visited(h, nb);
            float sn = sim_slot(h, q, nb);
            h->dist[nb] = sn;
            if (res->n < ef || sn > heap_top(res).sim) {
                if (heap_push(&cand, sn, nb) != 0) break;
                if (heap_push(res, sn, nb) != 0) break;
                if (res->n > ef) heap_pop(res);
            }
        }
    }
    heap_free(&cand);
}

/* Algorithm 4 of the paper: pick M neighbours that are close to `q` and
 * mutually diverse.  Keep e only when it is closer to q than to every
 * neighbour already chosen.  `cand`/`cand_sim` are sorted in place. */
static size_t select_neighbors(const hnsw_t *h, const float *q,
                               uint32_t *cand, float *cand_sim,
                               size_t ncand, size_t M, uint32_t *out)
{
    /* sort by descending similarity (insertion sort; ncand <= ef_c) */
    for (size_t i = 1; i < ncand; i++) {
        uint32_t s = cand[i];
        float    v = cand_sim[i];
        size_t   j = i;
        while (j > 0 && cand_sim[j - 1] < v) {
            cand[j] = cand[j - 1];
            cand_sim[j] = cand_sim[j - 1];
            j--;
        }
        cand[j] = s;
        cand_sim[j] = v;
    }

    size_t nout = 0;
    (void)q;   /* cand_sim[i] already holds sim(q, cand[i]) */
    for (size_t i = 0; i < ncand && nout < M; i++) {
        uint32_t e = cand[i];
        int keep = 1;
        for (size_t r = 0; r < nout; r++) {
            float d_er = vecdb_sim_rank(h->metric, h->vectors + (size_t)e * h->dim,
                                        h->vectors + (size_t)out[r] * h->dim, h->dim);
            if (d_er > cand_sim[i]) { keep = 0; break; }
        }
        if (keep) out[nout++] = e;
    }
    return nout;
}

/* ------------------------------------------------------------------ */
/* insertion                                                           */
/* ------------------------------------------------------------------ */
static int rand_level(hnsw_t *h)
{
    double r = (double)(rng_next(h) >> 11) / 9007199254740992.0;
    if (r < 1e-12) r = 1e-12;
    double ml = 1.0 / log((double)h->m);
    int lvl = (int)floor(-log(r) * ml);
    if (lvl < 0) lvl = 0;
    if (lvl > HNSW_MAX_LEVEL) lvl = HNSW_MAX_LEVEL;
    return lvl;
}

int hnsw_add(hnsw_t *h, uint32_t slot, const float *vec)
{
    if (ensure_slot(h, slot) != 0) return VECDB_ERR_NOMEM;

    memcpy(h->vectors + (size_t)slot * h->dim, vec, h->dim * sizeof(float));

    int level = rand_level(h);
    if (node_ensure_levels(h, slot, level) != 0) return VECDB_ERR_NOMEM;
    h->nodes[slot].in_use = 1;
    h->live++;

    if (h->entry == UINT32_MAX) {
        h->entry = slot;
        h->max_level = level;
        return VECDB_OK;
    }

    /* scratch lives in the graph struct: allocated once, m0+1 entries */
    uint32_t *chosen = h->chosen;
    uint32_t *keep   = h->keep;
    if (!chosen || !keep) return VECDB_ERR_NOMEM;

    uint32_t ep = h->entry;
    for (int l = h->max_level; l > level; l--)
        ep = greedy_search(h, vec, ep, l);

    int top = level < h->max_level ? level : h->max_level;
    for (int l = top; l >= 0; l--) {
        heap_t res;
        search_layer(h, vec, ep, h->ef_c, l, &res);
        if (res.n == 0) { heap_free(&res); continue; }

        /* the closest candidate seeds the next (lower) layer */
        uint32_t best = res.a[0].slot;
        float best_sim = res.a[0].sim;
        for (size_t i = 1; i < res.n; i++) {
            if (res.a[i].sim > best_sim) { best_sim = res.a[i].sim; best = res.a[i].slot; }
        }
        ep = best;

        size_t nc = res.n;
        if (nc > h->cap) nc = h->cap;   /* scratch bound */
        for (size_t i = 0; i < nc; i++) {
            h->cand_slots[i] = res.a[i].slot;
            h->cand_sims[i] = res.a[i].sim;
        }
        heap_free(&res);

        size_t M = level_max_links(h, l);
        size_t nch = select_neighbors(h, vec, h->cand_slots, h->cand_sims, nc, M, chosen);

        /* the new node keeps the chosen neighbours ... */
        nbrlist_set(&h->nodes[slot].nbrs[l], chosen, nch);

        /* ... and every chosen neighbour gains an edge back (with pruning) */
        for (size_t i = 0; i < nch; i++) {
            uint32_t b = chosen[i];
            if (b == slot) continue;
            nbrlist_t *bl = &h->nodes[b].nbrs[l];
            if (nbrlist_push(bl, slot) != 0) continue;
            if (bl->count > M) {
                size_t nb = bl->count;
                if (nb > h->cap) nb = h->cap;
                const float *bv = h->vectors + (size_t)b * h->dim;
                for (size_t j = 0; j < nb; j++) {
                    h->cand_slots[j] = bl->items[j];
                    h->cand_sims[j] = vecdb_sim_rank(h->metric, bv,
                                                     h->vectors + (size_t)bl->items[j] * h->dim,
                                                     h->dim);
                }
                size_t nk = select_neighbors(h, bv, h->cand_slots, h->cand_sims, nb, M, keep);
                nbrlist_set(bl, keep, nk);
            }
        }
    }

    if (level > h->max_level) {
        h->max_level = level;
        h->entry = slot;
    }
    /* keep `cand_entry` pointing at a living node on the top level */
    if (level == h->max_level &&
        (h->cand_entry == UINT32_MAX || !h->nodes[h->cand_entry].in_use))
        h->cand_entry = slot;
    return VECDB_OK;
}

void hnsw_remove(hnsw_t *h, uint32_t slot)
{
    if (!h || slot >= h->cap || !h->nodes[slot].in_use) return;
    h->nodes[slot].in_use = 0;
    h->live--;

    if (slot != h->entry) return;

    /* Elect a new entry point without rescanning every slot on each
     * delete (that would be O(capacity) and dominate bulk deletes).
     * `cand_entry` is maintained by hnsw_add: it points at a living node
     * whose level equals the graph's current top level. */
    if (h->cand_entry != UINT32_MAX && h->cand_entry != slot &&
        h->nodes[h->cand_entry].in_use &&
        (int)h->nodes[h->cand_entry].level == h->max_level) {
        h->entry = h->cand_entry;
        h->cand_entry = UINT32_MAX;
        return;
    }

    /* Slow path: the cached candidate is gone (or wrong level).  Scan once
     * for the highest-level living node and remember it. */
    uint32_t new_ep = UINT32_MAX;
    int best = -1;
    for (size_t i = 0; i < h->cap; i++) {
        if (!h->nodes[i].in_use) continue;
        if ((int)h->nodes[i].level > best) {
            best = (int)h->nodes[i].level;
            new_ep = (uint32_t)i;
            if (best == h->max_level) break;
        }
    }
    h->cand_entry = UINT32_MAX;

    if (new_ep == UINT32_MAX) {
        h->entry = UINT32_MAX;
        h->max_level = -1;
    } else {
        h->entry = new_ep;
        h->max_level = best;
    }
}

/* ------------------------------------------------------------------ */
/* query                                                               */
/* ------------------------------------------------------------------ */
size_t hnsw_search(hnsw_t *h, const float *q, size_t k,
                   uint32_t *out_slots, float *out_scores, const uint8_t *alive)
{
    if (!h || h->entry == UINT32_MAX || k == 0) return 0;

    uint32_t ep = h->entry;
    for (int l = h->max_level; l >= 1; l--)
        ep = greedy_search(h, q, ep, l);

    size_t ef = h->ef > k ? h->ef : k;
    heap_t res;
    search_layer(h, q, ep, ef, 0, &res);
    if (res.n == 0) { heap_free(&res); return 0; }

    /* keep the k best living hits in a small "worst on top" heap */
    heap_t top;
    size_t n = 0;
    if (heap_init(&top, k, 0) == 0) {
        for (size_t i = 0; i < res.n; i++) {
            uint32_t slot = res.a[i].slot;
            if (!h->nodes[slot].in_use) continue;
            if (alive && !alive[slot]) continue;
            heap_push(&top, res.a[i].sim, slot);
            if (top.n > k) heap_pop(&top);
        }
        n = top.n;
        for (size_t i = 0; i < n; i++) {
            out_slots[i] = top.a[i].slot;
            out_scores[i] = top.a[i].sim;
        }
        heap_free(&top);
    }
    heap_free(&res);

    /* descending sort (k is small) */
    for (size_t i = 1; i < n; i++) {
        uint32_t s = out_slots[i];
        float    v = out_scores[i];
        size_t   j = i;
        while (j > 0 && out_scores[j - 1] < v) {
            out_slots[j] = out_slots[j - 1];
            out_scores[j] = out_scores[j - 1];
            j--;
        }
        out_slots[j] = s;
        out_scores[j] = v;
    }
    return n;
}
