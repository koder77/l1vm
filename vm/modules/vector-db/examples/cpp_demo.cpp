// cpp_demo.cpp — exercises the C++ wrapper (include/vecdb.hpp).
//
// Build with `make cpp_demo` and run with `./cpp_demo`.

#include "vecdb.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        ++checks;                                                             \
        if (!(cond)) { ++failures; std::printf("FAIL: %s\n", msg); }          \
    } while (0)

int main() {
    char path[256];
    std::snprintf(path, sizeof(path), "/tmp/vecdb_cpp_%ld.db", (long)getpid());
    unlink(path);

    std::printf("=== vecdb C++ wrapper demo ===================================\n\n");

    {
        vdb::Database db(path, {.dim = 3, .metric = vdb::Cosine,
                                  .index = vdb::Hnsw});
        std::printf("opened %s (dim=%zu, index=%s)\n", db.path().c_str(), db.dim(),
                    db.index() == vdb::Hnsw ? "hnsw" : "flat");

        // RAII + typed payloads
        db.put(1, {1, 0, 0}, vdb::encode(std::string("animal")), "a cat sat on the mat");
        db.put(2, {0, 1, 0}, vdb::encode(std::string("animal")), "a dog barked at the postman");
        db.put(3, {0, 0, 1}, vdb::encode(std::string("fish")), "a fish swam in the sea");

        // A trivially-copyable struct payload
        struct Meta { std::int64_t year; float score; };
        db.put(4, {1, 1, 0}, vdb::encode(Meta{2024, 0.5f}), "vector database engine");

        CHECK(db.size() == 4, "four records stored");

        // vector search
        auto hits = db.search({0.9f, 0.1f, 0.0f}, 3);
        CHECK(hits.size() == 3, "vector search returns three");
        CHECK(hits[0].id == 1, "nearest neighbour is id 1");
        CHECK(hits[0].score >= hits[1].score, "scores are descending");
        CHECK(hits[0].as<std::string>() == "animal", "payload decodes to a string");

        // text search
        auto text_hits = db.search_text("postman", 5);
        CHECK(text_hits.size() == 1 && text_hits[0].id == 2, "BM25 finds the postman");

        // hybrid
        auto hybrid = db.search_hybrid({1, 1, 0}, "engine", 2, 0.5f);
        CHECK(!hybrid.empty(), "hybrid returns something");

        // get + struct decode
        auto rec = db.get(4);
        CHECK(rec.has_vector && rec.vector[0] == 1.0f, "record 4 has its vector");
        auto meta = rec.as<Meta>();
        CHECK(meta.year == 2024 && meta.score == 0.5f, "struct payload round-trips");

        // exceptions
        bool threw = false;
        try { db.get(999); } catch (const vdb::NotFound &) { threw = true; }
        CHECK(threw, "get on a missing id throws NotFound");

        threw = false;
        try { db.put(1, {1, 2}, {}, "", /*insert_only=*/true); }
        catch (const vdb::DimensionMismatch &) { threw = true; }
        CHECK(threw, "wrong dimension throws DimensionMismatch");

        threw = false;
        try { db.erase(999); } catch (const vdb::NotFound &) { threw = true; }
        CHECK(threw, "erase on a missing id throws NotFound");

        // copy is deleted; move works
        vdb::Database moved(std::move(db));
        CHECK(moved.size() == 4, "moved-from handle still works");
        moved.erase(3);
        CHECK(moved.size() == 3, "erase works after move");
        moved.flush();
    } // destructors flush + close

    // reopen: persistence survives
    {
        vdb::Database db(path);
        CHECK(db.size() == 3, "reopened count");
        CHECK(db.index() == vdb::Hnsw, "index persisted");
        CHECK(db.metric() == vdb::Cosine, "metric persisted");
        auto rec = db.get(2);
        CHECK(rec.has_text && rec.text == "a dog barked at the postman", "text survived");
        db.compact();
        CHECK(db.size() == 3, "count survives compact");

        auto all_ids = db.ids();
        CHECK(all_ids.size() == 3, "ids() lists three");
    }

    unlink(path);
    std::printf("\nC++ wrapper: %d checks, %d failures\n", checks, failures);
    std::printf("=== done =====================================================\n");
    return failures ? 1 : 0;
}
