// Package vecdb provides Go bindings for libvecdb, an embedded vector +
// full-text database written in C.
//
// The database lives in a single file that is written atomically. Records
// carry an opaque payload blob, an optional float vector and optional
// free text, and can be retrieved by id, by nearest-neighbour vector
// search (exact FLAT or approximate HNSW), by BM25 full-text search, or
// by a hybrid blend of both. Scores are higher-is-better everywhere (L2
// reports the negated distance).
//
// The package is a thin cgo layer over the C library: each DB maps onto
// one C handle. Calls are safe to make from multiple goroutines (the C
// library serialises writers with a rwlock), but concurrent writers on
// one handle are serialised — open one DB per goroutine if you need
// parallel writes.
//
// Build requirements: libvecdb.so must be built first (`make` in the
// repository root). The cgo directives locate the header and library
// relative to this package; when running a compiled binary from another
// machine or directory, make sure the loader can find libvecdb.so
// (LD_LIBRARY_PATH or an rpath of your own).
package vecdb

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../.. -lvecdb -Wl,-rpath,${SRCDIR}/../..
#include <stdlib.h>
#include "vecdb.h"
*/
import "C"

import (
	"errors"
	"fmt"
	"runtime"
	"unsafe"
)

// Version of the bindings.
const Version = "1.0.0"

// Status codes, mirroring vecdb_status.
const (
	StatusOK          = int(C.VECDB_OK)
	StatusIO          = int(C.VECDB_ERR_IO)
	StatusNoMem       = int(C.VECDB_ERR_NOMEM)
	StatusInvalid     = int(C.VECDB_ERR_INVALID)
	StatusNotFound    = int(C.VECDB_ERR_NOTFOUND)
	StatusExists      = int(C.VECDB_ERR_EXISTS)
	StatusDimMismatch = int(C.VECDB_ERR_DIM)
	StatusCorrupt     = int(C.VECDB_ERR_CORRUPT)
	StatusNoDimension = int(C.VECDB_ERR_NODIM)
)

// ErrClosed is returned by every operation on a closed database.
var ErrClosed = errors.New("vecdb: database is closed")

// Metric selects the distance used by vector search.
type Metric int

// Distance metrics, mirroring vecdb_metric.
const (
	MetricL2     Metric = C.VECDB_METRIC_L2     // negated euclidean distance
	MetricIP     Metric = C.VECDB_METRIC_IP     // inner product
	MetricCosine Metric = C.VECDB_METRIC_COSINE // cosine similarity
)

func (m Metric) String() string {
	switch m {
	case MetricL2:
		return "l2"
	case MetricIP:
		return "ip"
	case MetricCosine:
		return "cosine"
	default:
		return fmt.Sprintf("metric(%d)", int(m))
	}
}

// Index selects the vector index structure.
type Index int

// Index types, mirroring vecdb_index.
const (
	IndexFlat Index = C.VECDB_INDEX_FLAT // exact brute-force scan
	IndexHNSW Index = C.VECDB_INDEX_HNSW // approximate HNSW graph
)

func (i Index) String() string {
	switch i {
	case IndexFlat:
		return "flat"
	case IndexHNSW:
		return "hnsw"
	default:
		return fmt.Sprintf("index(%d)", int(i))
	}
}

// Put flags accepted by PutEx.
type PutFlag uint

const (
	// InsertOnly fails with StatusExists if the id already exists.
	InsertOnly PutFlag = 1 << 0
	// Sync flushes the database to disk before returning.
	Sync PutFlag = 1 << 1
)

// Options configures a database at open time. The zero value is valid and
// selects the library defaults: dimension learned from the first stored
// vector, L2 metric, FLAT index, HNSW M=16 / efConstruction=100 /
// efSearch=100, seed 42.
//
// The metric and index type are persisted in the file and win over the
// values supplied here when opening an existing database.
type Options struct {
	Dim                uint   // vector length; 0 = learn from the first put
	Metric             Metric // distance metric
	Index              Index  // index structure
	HnswM              uint   // HNSW M; 0 = library default (16)
	HnswEfConstruction uint   // HNSW build breadth; 0 = library default (100)
	HnswEfSearch       uint   // HNSW query breadth; 0 = library default (100)
	Seed               uint64 // RNG seed for HNSW; 0 = library default (42)
}

// DefaultOptions returns the library defaults.
func DefaultOptions() Options {
	var c C.vecdb_options
	C.vecdb_options_default(&c)
	return Options{
		Dim:                uint(c.dim),
		Metric:             Metric(c.metric),
		Index:              Index(c.index),
		HnswM:              uint(c.hnsw_m),
		HnswEfConstruction: uint(c.hnsw_ef_construction),
		HnswEfSearch:       uint(c.hnsw_ef_search),
		Seed:               uint64(c.seed),
	}
}

func (o Options) toC() C.vecdb_options {
	var c C.vecdb_options
	C.vecdb_options_default(&c)
	c.dim = C.size_t(o.Dim)
	c.metric = C.vecdb_metric(o.Metric)
	c.index = C.vecdb_index(o.Index)
	c.hnsw_m = C.size_t(o.HnswM)
	c.hnsw_ef_construction = C.size_t(o.HnswEfConstruction)
	c.hnsw_ef_search = C.size_t(o.HnswEfSearch)
	c.seed = C.uint64_t(o.Seed)
	return c
}

// Error is returned for every non-OK status reported by the C library.
type Error struct {
	Status int    // one of the StatusX constants
	Msg    string // human readable, includes the library detail if any
}

func (e *Error) Error() string { return e.Msg }

// StatusOf returns the status code carried by err, if any.
func StatusOf(err error) (int, bool) {
	var e *Error
	if errors.As(err, &e) {
		return e.Status, true
	}
	return 0, false
}

// IsNotFound reports whether err is a StatusNotFound error.
func IsNotFound(err error) bool { return hasStatus(err, StatusNotFound) }

// IsAlreadyExists reports whether err is a StatusExists error
// (an InsertOnly write hit an existing id).
func IsAlreadyExists(err error) bool { return hasStatus(err, StatusExists) }

// IsDimensionMismatch reports whether err is a StatusDimMismatch or
// StatusNoDimension error.
func IsDimensionMismatch(err error) bool {
	return hasStatus(err, StatusDimMismatch) || hasStatus(err, StatusNoDimension)
}

func hasStatus(err error, status int) bool {
	s, ok := StatusOf(err)
	return ok && s == status
}

// Record is one stored row as returned by Get. Vector, Payload and Text
// are copies owned by the caller.
type Record struct {
	ID      uint64
	Vector  []float32
	Payload []byte
	Text    string
}

// Match is one search hit. Payload and Text are copies owned by the
// caller. Score is higher-is-better (see the package comment).
type Match struct {
	ID      uint64
	Score   float32
	Payload []byte
	Text    string
}

// DB is an open database handle.
type DB struct {
	ptr *C.vecdb
}

// Open opens (or creates) a database at path. opts may be nil for the
// library defaults; pass opts.Dim = 0 to learn the dimension from the
// first stored vector.
func Open(path string, opts *Options) (*DB, error) {
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	var copts *C.vecdb_options
	if opts != nil {
		c := opts.toC()
		copts = &c
	}

	handle := C.vecdb_open(cpath, copts)
	if handle == nil {
		return nil, fmt.Errorf("vecdb: could not open %q (missing, corrupt, or dimension mismatch)", path)
	}
	db := &DB{ptr: handle}
	runtime.SetFinalizer(db, func(d *DB) { _ = d.Close() })
	return db, nil
}

// Close flushes and closes the database. It is safe to call Close more
// than once; later calls are no-ops.
func (db *DB) Close() error {
	if db == nil || db.ptr == nil {
		return nil
	}
	handle := db.ptr
	db.ptr = nil
	runtime.SetFinalizer(db, nil)
	return status(int(C.vecdb_close(handle)), db)
}

// Flush persists the database atomically.
func (db *DB) Flush() error {
	if err := db.checkOpen(); err != nil {
		return err
	}
	return db.check(C.vecdb_flush(db.ptr))
}

// Compact drops deleted records and rebuilds the indexes.
func (db *DB) Compact() error {
	if err := db.checkOpen(); err != nil {
		return err
	}
	return db.check(C.vecdb_compact(db.ptr))
}

// Count returns the number of live records, or 0 once closed.
func (db *DB) Count() int {
	if db == nil || db.ptr == nil {
		return 0
	}
	return int(C.vecdb_count(db.ptr))
}

// Capacity returns the number of slots in use, including tombstones left
// behind by deleted records.
func (db *DB) Capacity() int {
	if db == nil || db.ptr == nil {
		return 0
	}
	return int(C.vecdb_capacity(db.ptr))
}

// Dim returns the vector length, or 0 if no vector has been stored yet
// (and once closed).
func (db *DB) Dim() int {
	if db == nil || db.ptr == nil {
		return 0
	}
	return int(C.vecdb_dim(db.ptr))
}

// Metric returns the distance metric (persisted with the database).
func (db *DB) Metric() Metric {
	if db == nil || db.ptr == nil {
		return MetricL2
	}
	return Metric(C.vecdb_metric_of(db.ptr))
}

// Index returns the index type (persisted with the database).
func (db *DB) Index() Index {
	if db == nil || db.ptr == nil {
		return IndexFlat
	}
	return Index(C.vecdb_index_of(db.ptr))
}

// Path returns the database file path (empty once closed).
func (db *DB) Path() string {
	if db == nil || db.ptr == nil {
		return ""
	}
	return C.GoString(C.vecdb_path(db.ptr))
}

// SetEfSearch tunes the HNSW search breadth at runtime: bigger means
// better recall and slower queries. It fails when the database is not
// HNSW-indexed.
func (db *DB) SetEfSearch(ef uint) error {
	if err := db.checkOpen(); err != nil {
		return err
	}
	return db.check(C.vecdb_set_ef_search(db.ptr, C.size_t(ef)))
}

// ListIDs returns every live id, in unspecified order.
func (db *DB) ListIDs() ([]uint64, error) {
	if err := db.checkOpen(); err != nil {
		return nil, err
	}
	h := db.ptr
	n := C.vecdb_list_ids(h, nil, 0)
	if n == 0 {
		return []uint64{}, nil
	}
	buf := make([]C.uint64_t, n)
	got := C.vecdb_list_ids(h, &buf[0], n)
	ids := make([]uint64, got)
	for i := range ids {
		ids[i] = uint64(buf[i])
	}
	return ids, nil
}

// Put stores a record (upsert). Any of vec, payload and text may be
// empty. The first vector stored fixes the database dimension unless
// Options.Dim was set at open time.
func (db *DB) Put(id uint64, vec []float32, payload []byte, text string) error {
	return db.PutEx(id, vec, payload, text, 0)
}

// PutEx stores a record with flags (InsertOnly, Sync).
func (db *DB) PutEx(id uint64, vec []float32, payload []byte, text string, flags PutFlag) error {
	if err := db.checkOpen(); err != nil {
		return err
	}
	if len(vec) == 0 {
		vec = nil
	}
	if len(payload) == 0 {
		payload = nil
	}
	ctext := cstr(text)
	return db.check(C.vecdb_put_ex(db.ptr, C.uint64_t(id),
		vecPtr(vec), C.size_t(len(vec)),
		bytePtr(payload), C.size_t(len(payload)),
		ctext, C.uint(flags)))
}

// Get fetches one record. It returns an error with StatusNotFound when
// the id does not exist.
func (db *DB) Get(id uint64) (*Record, error) {
	if err := db.checkOpen(); err != nil {
		return nil, err
	}
	var rec C.vecdb_record
	if err := db.check(C.vecdb_get(db.ptr, C.uint64_t(id), &rec)); err != nil {
		return nil, err
	}
	out := &Record{ID: uint64(rec.id)}
	if rec.vec != nil {
		dim := int(C.vecdb_dim(db.ptr))
		src := unsafe.Slice(rec.vec, dim)
		out.Vector = make([]float32, dim)
		for i, v := range src {
			out.Vector[i] = float32(v)
		}
	}
	if rec.payload != nil && rec.payload_len > 0 {
		out.Payload = append([]byte(nil),
			unsafe.Slice((*byte)(rec.payload), int(rec.payload_len))...)
	}
	if rec.text != nil {
		out.Text = C.GoString(rec.text)
	}
	return out, nil
}

// Delete removes a record (a tombstone remains until Compact). It
// returns an error with StatusNotFound when the id does not exist.
func (db *DB) Delete(id uint64) error {
	if err := db.checkOpen(); err != nil {
		return err
	}
	return db.check(C.vecdb_delete(db.ptr, C.uint64_t(id)))
}

// DeleteMany removes several records, skipping ids that do not exist. It
// returns the number of records actually removed.
func (db *DB) DeleteMany(ids []uint64) int {
	if db == nil || db.ptr == nil {
		return 0
	}
	removed := 0
	for _, id := range ids {
		if C.vecdb_delete(db.ptr, C.uint64_t(id)) == C.VECDB_OK {
			removed++
		}
	}
	return removed
}

// SearchVector returns the k nearest neighbours of vec. For MetricL2 an
// exact match scores 0.0 (the score is the negated distance), for
// MetricIP it is the dot product and for MetricCosine the cosine
// similarity in [-1, 1].
func (db *DB) SearchVector(vec []float32, k int) ([]Match, error) {
	if err := db.checkOpen(); err != nil {
		return nil, err
	}
	if err := db.checkVector(vec); err != nil {
		return nil, err
	}
	if k <= 0 {
		return nil, nil
	}
	out := make([]C.vecdb_match, k)
	n := C.vecdb_search_vector(db.ptr, vecPtr(vec), C.size_t(len(vec)),
		C.size_t(k), &out[0], C.size_t(k))
	return collectMatches(out[:n]), nil
}

// SearchText runs a BM25 full-text search for query.
func (db *DB) SearchText(query string, k int) ([]Match, error) {
	if err := db.checkOpen(); err != nil {
		return nil, err
	}
	if k <= 0 || query == "" {
		return nil, nil
	}
	out := make([]C.vecdb_match, k)
	cq := C.CString(query)
	defer C.free(unsafe.Pointer(cq))
	n := C.vecdb_search_text(db.ptr, cq, C.size_t(k), &out[0], C.size_t(k))
	return collectMatches(out[:n]), nil
}

// SearchHybrid blends vector and text results: alpha weights the vector
// signal, 1 is pure vector search and 0 pure text search. Either query
// may be empty to fall back to the other modality.
func (db *DB) SearchHybrid(vec []float32, text string, k int, alpha float32) ([]Match, error) {
	if err := db.checkOpen(); err != nil {
		return nil, err
	}
	if len(vec) > 0 {
		if err := db.checkVector(vec); err != nil {
			return nil, err
		}
	}
	if k <= 0 || (len(vec) == 0 && text == "") {
		return nil, nil
	}
	out := make([]C.vecdb_match, k)
	ctext := cstr(text)
	n := C.vecdb_search_hybrid(db.ptr, vecPtr(vec), C.size_t(len(vec)),
		ctext, C.size_t(k), C.float(alpha), &out[0], C.size_t(k))
	return collectMatches(out[:n]), nil
}

// -- internals -------------------------------------------------------------

// checkOpen returns ErrClosed when the handle has been closed (or was
// never valid). Every mutating operation calls it first.
func (db *DB) checkOpen() error {
	if db == nil || db.ptr == nil {
		return ErrClosed
	}
	return nil
}

// check converts a status into nil or an *Error, with the handle's last
// error attached as detail.
func (db *DB) check(rc C.int) error {
	return status(int(rc), db)
}

func status(rc int, db *DB) error {
	if rc == StatusOK {
		return nil
	}
	msg := C.GoString(C.vecdb_strerror(C.int(rc)))
	if db != nil && db.ptr != nil {
		if detail := C.GoString(C.vecdb_last_error(db.ptr)); detail != "" {
			msg += ": " + detail
		}
	}
	return &Error{Status: rc, Msg: "vecdb: " + msg}
}

// checkVector validates a query vector up-front instead of relying on
// the C search functions, which only report errors as an empty result.
func (db *DB) checkVector(vec []float32) error {
	dim := db.Dim()
	if dim == 0 {
		return &Error{Status: StatusNoDimension, Msg: "vecdb: database has no vector dimension"}
	}
	if len(vec) != dim {
		return &Error{Status: StatusDimMismatch,
			Msg: fmt.Sprintf("vecdb: query dimension %d != %d", len(vec), dim)}
	}
	return nil
}

func collectMatches(src []C.vecdb_match) []Match {
	if len(src) == 0 {
		return nil
	}
	out := make([]Match, len(src))
	for i, m := range src {
		out[i] = Match{ID: uint64(m.id), Score: float32(m.score)}
		if m.payload != nil && m.payload_len > 0 {
			out[i].Payload = append([]byte(nil),
				unsafe.Slice((*byte)(m.payload), int(m.payload_len))...)
		}
		if m.text != nil {
			out[i].Text = C.GoString(m.text)
		}
	}
	return out
}

func cstr(s string) *C.char {
	if s == "" {
		return nil
	}
	return C.CString(s)
}

func bytePtr(b []byte) unsafe.Pointer {
	if len(b) == 0 {
		return nil
	}
	return unsafe.Pointer(&b[0])
}

func vecPtr(v []float32) *C.float {
	if len(v) == 0 {
		return nil
	}
	return (*C.float)(unsafe.Pointer(&v[0]))
}
