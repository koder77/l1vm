/*
 * vecdb.hpp — a modern C++ wrapper over the vecdb C API.
 *
 * Header-only, C++17, no dependencies beyond the standard library and
 * libvecdb.  It gives you:
 *
 *   * RAII handles (Database) that flush and close on destruction
 *   * std::vector/std::string/std::optional throughout, no raw pointers
 *   * exceptions (vdb::Error and friends) instead of status codes
 *   * a typed payload codec (encode/decode arbitrary C++ values)
 *
 * Example:
 *
 *   #include "vecdb.hpp"
 *
 *   vdb::Database db("/tmp/notes.db", {.dim = 3, .metric = vdb::Cosine});
 *   db.put(1, {1, 0, 0}, vdb::encode("a cat sat on the mat"), "a cat sat");
 *
 *   for (const auto &hit : db.search(std::vector<float>{1, 0, 0}, 5))
 *       std::cout << hit.id << " " << hit.score << "\n";
 *
 * License: MIT.
 */
#ifndef VECDB_HPP
#define VECDB_HPP

#include "vecdb.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace vdb {

// --------------------------------------------------------------------------
// Enumerations, spelled the C++ way.
// --------------------------------------------------------------------------
enum class Metric : int { L2 = VECDB_METRIC_L2, InnerProduct = VECDB_METRIC_IP,
                          Cosine = VECDB_METRIC_COSINE };
enum class Index : int { Flat = VECDB_INDEX_FLAT, Hnsw = VECDB_INDEX_HNSW };

constexpr Metric L2 = Metric::L2;
constexpr Metric InnerProduct = Metric::InnerProduct;
constexpr Metric Cosine = Metric::Cosine;
constexpr Index Flat = Index::Flat;
constexpr Index Hnsw = Index::Hnsw;

// --------------------------------------------------------------------------
// Exceptions.
// --------------------------------------------------------------------------
/// Base class for every error thrown by this wrapper.
class Error : public std::runtime_error {
public:
    Error(int code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}
    /// The underlying vecdb_status value.
    int code() const noexcept { return code_; }
private:
    int code_;
};

class IoError : public Error {
public:
    using Error::Error;
};
class InvalidArgument : public Error {
public:
    using Error::Error;
};
class NotFound : public Error {
public:
    using Error::Error;
};
class AlreadyExists : public Error {
public:
    using Error::Error;
};
class DimensionMismatch : public Error {
public:
    using Error::Error;
};
class CorruptDatabase : public Error {
public:
    using Error::Error;
};

namespace detail {
[[noreturn]] inline void raise(int code, const std::string &detail) {
    std::string base = vecdb_strerror(code);
    std::string msg = detail.empty() ? base : base + ": " + detail;
    switch (code) {
    case VECDB_ERR_IO:       throw IoError(code, msg);
    case VECDB_ERR_INVALID:  throw InvalidArgument(code, msg);
    case VECDB_ERR_NOTFOUND: throw NotFound(code, msg);
    case VECDB_ERR_EXISTS:   throw AlreadyExists(code, msg);
    case VECDB_ERR_DIM:      throw DimensionMismatch(code, msg);
    case VECDB_ERR_NODIM:    throw DimensionMismatch(code, msg);
    case VECDB_ERR_CORRUPT:  throw CorruptDatabase(code, msg);
    default:                 throw Error(code, msg);
    }
}
} // namespace detail

// --------------------------------------------------------------------------
// Options.
// --------------------------------------------------------------------------
struct Options {
    std::size_t dim = 0;                        ///< 0 = learn from first put
    Metric metric = Metric::L2;
    Index index = Index::Flat;
    std::size_t hnsw_m = 0;                     ///< 0 = library default (16)
    std::size_t hnsw_ef_construction = 0;       ///< 0 = library default (100)
    std::size_t hnsw_ef_search = 0;             ///< 0 = library default (100)
    std::uint64_t seed = 0;                     ///< 0 = library default
};

// --------------------------------------------------------------------------
// Results.
// --------------------------------------------------------------------------
/// One search hit.  Owns its payload and text, so it outlives the query.
struct Match {
    std::uint64_t id = 0;
    float score = 0.0f;
    std::vector<std::uint8_t> payload;   ///< raw bytes, empty if none
    std::string text;                    ///< empty if none
    bool has_text = false;

    /// Decode the payload as a value of type T (see vdb::decode).
    template <typename T>
    T as() const;
};

/// A full record as returned by Database::get.
struct Record {
    std::uint64_t id = 0;
    std::vector<float> vector;           ///< empty if the record has no vector
    std::vector<std::uint8_t> payload;
    std::string text;
    bool has_vector = false;
    bool has_text = false;

    template <typename T>
    T as() const;
};

// --------------------------------------------------------------------------
// Payload codec.
//
// The payload is an opaque byte blob in C.  These helpers give it a shape:
// encode() turns common C++ types into bytes and decode() turns the bytes
// back into a value.  The encoding is little-endian and self-contained.
// --------------------------------------------------------------------------
namespace detail {
inline void append(std::vector<std::uint8_t> &out, const void *p, std::size_t n) {
    const auto *b = static_cast<const std::uint8_t *>(p);
    out.insert(out.end(), b, b + n);
}
template <typename T>
inline void append_pod(std::vector<std::uint8_t> &out, T v) {
    append(out, &v, sizeof(T));
}
} // namespace detail

/// Encode a signed integer (8 bytes, little-endian).
inline std::vector<std::uint8_t> encode(std::int64_t v) {
    std::vector<std::uint8_t> out;
    detail::append_pod(out, v);
    return out;
}
inline std::vector<std::uint8_t> encode(std::uint64_t v) {
    return encode(static_cast<std::int64_t>(v));
}
inline std::vector<std::uint8_t> encode(double v) {
    std::vector<std::uint8_t> out;
    detail::append_pod(out, v);
    return out;
}
inline std::vector<std::uint8_t> encode(float v) {
    std::vector<std::uint8_t> out;
    detail::append_pod(out, v);
    return out;
}
inline std::vector<std::uint8_t> encode(std::string_view s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}
inline std::vector<std::uint8_t> encode(const char *s) {
    return s ? encode(std::string_view(s)) : std::vector<std::uint8_t>{};
}
inline std::vector<std::uint8_t> encode(const std::string &s) {
    return encode(std::string_view(s));
}
/// Encode an arbitrary trivially-copyable value as a raw byte copy.
template <typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>
                                                  && !std::is_same_v<T, std::int64_t>>>
inline std::vector<std::uint8_t> encode(const T &v) {
    std::vector<std::uint8_t> out;
    detail::append(out, &v, sizeof(T));
    return out;
}

/// Decode a fixed-width value from a payload blob.
template <typename T>
inline T decode(const std::vector<std::uint8_t> &blob) {
    if (blob.size() < sizeof(T))
        throw InvalidArgument(VECDB_ERR_INVALID, "payload is too short to decode");
    T v;
    std::memcpy(&v, blob.data(), sizeof(T));
    return v;
}
template <>
inline std::string decode<std::string>(const std::vector<std::uint8_t> &blob) {
    return std::string(blob.begin(), blob.end());
}
template <>
inline std::int64_t decode<std::int64_t>(const std::vector<std::uint8_t> &blob) {
    return vecdb_unpack_i64(blob.data());
}
template <>
inline std::uint64_t decode<std::uint64_t>(const std::vector<std::uint8_t> &blob) {
    return vecdb_unpack_u64(blob.data());
}
template <>
inline double decode<double>(const std::vector<std::uint8_t> &blob) {
    return vecdb_unpack_f64(blob.data());
}
template <>
inline float decode<float>(const std::vector<std::uint8_t> &blob) {
    return vecdb_unpack_f32(blob.data());
}

template <typename T>
T Match::as() const {
    return decode<T>(payload);
}
template <typename T>
T Record::as() const {
    return decode<T>(payload);
}

// --------------------------------------------------------------------------
// Database.
// --------------------------------------------------------------------------
class Database {
public:
    /// Open (or create) a database at `path`.
    explicit Database(const std::string &path, const Options &opts = {}) {
        vecdb_options copts;
        vecdb_options_default(&copts);
        copts.dim = opts.dim;
        copts.metric = static_cast<vecdb_metric>(opts.metric);
        copts.index = static_cast<vecdb_index>(opts.index);
        copts.hnsw_m = opts.hnsw_m;
        copts.hnsw_ef_construction = opts.hnsw_ef_construction;
        copts.hnsw_ef_search = opts.hnsw_ef_search;
        copts.seed = opts.seed;
        db_ = vecdb_open(path.c_str(), &copts);
        if (!db_)
            throw IoError(VECDB_ERR_IO, "could not open " + path);
    }

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    Database(Database &&other) noexcept : db_(other.db_) { other.db_ = nullptr; }
    Database &operator=(Database &&other) noexcept {
        if (this != &other) {
            close();
            db_ = other.db_;
            other.db_ = nullptr;
        }
        return *this;
    }

    ~Database() { close(); }

    /// Flush and close.  Never throws; safe to call repeatedly.
    void close() noexcept {
        if (db_) {
            vecdb_close(db_);
            db_ = nullptr;
        }
    }

    // -- introspection ----------------------------------------------------
    std::size_t size() const noexcept { return vecdb_count(db_); }
    std::size_t capacity() const noexcept { return vecdb_capacity(db_); }
    bool empty() const noexcept { return vecdb_count(db_) == 0; }
    std::size_t dim() const noexcept { return vecdb_dim(db_); }
    Metric metric() const noexcept {
        return static_cast<Metric>(vecdb_metric_of(db_));
    }
    Index index() const noexcept {
        return static_cast<Index>(vecdb_index_of(db_));
    }
    std::string path() const { return vecdb_path(db_); }
    const char *last_error() const noexcept { return vecdb_last_error(db_); }

    /// Every live id, in unspecified order.
    std::vector<std::uint64_t> ids() const {
        std::size_t n = vecdb_list_ids(db_, nullptr, 0);
        std::vector<std::uint64_t> out(n);
        if (n)
            vecdb_list_ids(db_, out.data(), n);
        return out;
    }

    /// HNSW search breadth (larger = better recall, slower queries).
    void set_ef_search(std::size_t ef) { check(vecdb_set_ef_search(db_, ef)); }

    // -- persistence ------------------------------------------------------
    void flush() { check(vecdb_flush(db_)); }
    void compact() { check(vecdb_compact(db_)); }

    // -- writes -----------------------------------------------------------
    /// Store a record (upsert).  Any argument may be empty/"" to omit it.
    void put(std::uint64_t id,
             const std::vector<float> &vector = {},
             const std::vector<std::uint8_t> &payload = {},
             std::string_view text = {},
             bool insert_only = false,
             bool sync = false) {
        unsigned flags = 0;
        if (insert_only) flags |= VECDB_INSERT_ONLY;
        if (sync) flags |= VECDB_SYNC;
        const float *vp = vector.empty() ? nullptr : vector.data();
        const void *pp = payload.empty() ? nullptr : payload.data();
        // Keep the NUL-terminated copy alive for the duration of the call.
        const std::string t = make_cstr(text);
        const char *tp = text.empty() ? nullptr : t.c_str();
        check(vecdb_put_ex(db_, id, vp, vector.size(), pp, payload.size(), tp, flags));
    }

    /// Remove a record.  Throws NotFound if the id is absent.
    void erase(std::uint64_t id) { check(vecdb_delete(db_, id)); }

    /// Fetch one record.  Throws NotFound if the id is absent.
    Record get(std::uint64_t id) const {
        vecdb_record rec;
        check(vecdb_get(db_, id, &rec));
        Record out;
        out.id = rec.id;
        if (rec.vec) {
            out.has_vector = true;
            out.vector.assign(rec.vec, rec.vec + vecdb_dim(db_));
        }
        if (rec.payload && rec.payload_len) {
            const auto *p = static_cast<const std::uint8_t *>(rec.payload);
            out.payload.assign(p, p + rec.payload_len);
        }
        if (rec.text) {
            out.has_text = true;
            out.text = rec.text;
        }
        return out;
    }

    // -- searches ---------------------------------------------------------
    /// k nearest neighbours by vector similarity (higher score is better).
    std::vector<Match> search(const std::vector<float> &query,
                              std::size_t k = 10) const {
        if (k == 0) return {};
        std::vector<vecdb_match> raw(k);
        std::size_t n = vecdb_search_vector(db_, query.data(), query.size(), k,
                                            raw.data(), raw.size());
        return convert(raw, n);
    }

    /// BM25 full-text search.
    std::vector<Match> search_text(std::string_view query,
                                   std::size_t k = 10) const {
        if (k == 0 || query.empty()) return {};
        std::string q = make_cstr(query);
        std::vector<vecdb_match> raw(k);
        std::size_t n = vecdb_search_text(db_, q.c_str(), k, raw.data(), raw.size());
        return convert(raw, n);
    }

    /// Hybrid search: alpha weights the vector signal (1 = vector, 0 = text).
    std::vector<Match> search_hybrid(const std::vector<float> &vector,
                                     std::string_view text, std::size_t k = 10,
                                     float alpha = 0.5f) const {
        if (k == 0) return {};
        std::string t = make_cstr(text);
        const float *vp = vector.empty() ? nullptr : vector.data();
        const char *tp = text.empty() ? nullptr : t.c_str();
        std::vector<vecdb_match> raw(k);
        std::size_t n = vecdb_search_hybrid(db_, vp, vector.size(), tp, k, alpha,
                                            raw.data(), raw.size());
        return convert(raw, n);
    }

private:
    vecdb *db_ = nullptr;

    // The C API has no (pointer, length) text argument, so we must build a
    // NUL-terminated copy of any string_view.
    static std::string make_cstr(std::string_view s) {
        return std::string(s);
    }

    void check(int rc) const {
        if (rc == VECDB_OK) return;
        detail::raise(rc, db_ && vecdb_last_error(db_) ? vecdb_last_error(db_) : "");
    }

    static std::vector<Match> convert(const std::vector<vecdb_match> &raw,
                                      std::size_t n) {
        std::vector<Match> out;
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            Match m;
            m.id = raw[i].id;
            m.score = raw[i].score;
            if (raw[i].payload && raw[i].payload_len) {
                const auto *p = static_cast<const std::uint8_t *>(raw[i].payload);
                m.payload.assign(p, p + raw[i].payload_len);
            }
            if (raw[i].text) {
                m.has_text = true;
                m.text = raw[i].text;
            }
            out.push_back(std::move(m));
        }
        return out;
    }
};

} // namespace vdb

#endif /* VECDB_HPP */
