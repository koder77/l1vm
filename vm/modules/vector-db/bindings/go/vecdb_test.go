package vecdb

// Smoke test for the Go bindings (cgo over libvecdb.so).
//
// Run with:  go test ./...
//
// It needs the shared library built (`make` in the repository root); the
// cgo directives locate it relative to this package.

import (
	"path/filepath"
	"reflect"
	"testing"
)

func openDB(t *testing.T, name string, opts *Options) *DB {
	t.Helper()
	db, err := Open(filepath.Join(t.TempDir(), name), opts)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	t.Cleanup(func() { _ = db.Close() })
	return db
}

func mustEnc(t *testing.T, v any) []byte {
	t.Helper()
	b, err := EncodePayload(v)
	if err != nil {
		t.Fatalf("EncodePayload(%#v): %v", v, err)
	}
	return b
}

func checkPayload(t *testing.T, got []byte, want any, msg string) {
	t.Helper()
	if dec := DecodePayload(got); !reflect.DeepEqual(dec, want) {
		t.Errorf("%s: payload decoded to %#v, want %#v", msg, dec, want)
	}
}

func TestBasicVector(t *testing.T) {
	db := openDB(t, "v.db", &Options{Dim: 3, Metric: MetricCosine})
	if err := db.Put(1, []float32{1, 0, 0}, mustEnc(t, map[string]any{"kind": "animal"}),
		"a cat sat on the mat"); err != nil {
		t.Fatal(err)
	}
	if err := db.Put(2, []float32{0, 1, 0}, mustEnc(t, map[string]any{"kind": "animal"}),
		"a dog barked at the postman"); err != nil {
		t.Fatal(err)
	}
	if err := db.Put(3, []float32{0, 0, 1}, nil, "a fish swam in the sea"); err != nil {
		t.Fatal(err)
	}
	if got := db.Count(); got != 3 {
		t.Errorf("Count = %d, want 3", got)
	}
	if got := db.Dim(); got != 3 {
		t.Errorf("Dim = %d, want 3", got)
	}
	if got := db.Metric(); got != MetricCosine {
		t.Errorf("Metric = %v, want cosine", got)
	}

	hits, err := db.SearchVector([]float32{0.95, 0.05, 0}, 3)
	if err != nil {
		t.Fatal(err)
	}
	if len(hits) != 3 {
		t.Fatalf("SearchVector returned %d hits, want 3", len(hits))
	}
	if hits[0].ID != 1 {
		t.Errorf("nearest neighbour = id %d, want 1", hits[0].ID)
	}
	if hits[0].Score <= hits[1].Score {
		t.Errorf("scores not descending: %f then %f", hits[0].Score, hits[1].Score)
	}
	checkPayload(t, hits[0].Payload, map[string]any{"kind": "animal"}, "dict payload")
	if hits[0].Text != "a cat sat on the mat" {
		t.Errorf("match text = %q", hits[0].Text)
	}
}

func TestTextSearch(t *testing.T) {
	db := openDB(t, "t.db", nil)
	for id, text := range map[uint64]string{
		1: "the quick brown fox jumps over the lazy dog",
		2: "a packet of postman biscuits",
		3: "foxes and hounds",
	} {
		if err := db.Put(id, nil, nil, text); err != nil {
			t.Fatal(err)
		}
	}
	cases := []struct {
		query string
		want  uint64
	}{
		{"fox", 1},
		{"foxes", 3}, // BM25 has no stemming: 'foxes' is its own token
		{"postman", 2},
	}
	for _, c := range cases {
		hits, err := db.SearchText(c.query, 5)
		if err != nil {
			t.Fatal(err)
		}
		if len(hits) != 1 || hits[0].ID != c.want {
			t.Errorf("SearchText(%q) = %v, want only id %d", c.query, hits, c.want)
		}
	}
	hits, err := db.SearchText("", 5)
	if err != nil {
		t.Fatal(err)
	}
	if len(hits) != 0 {
		t.Errorf("empty query returned %d hits", len(hits))
	}
}

func TestHybrid(t *testing.T) {
	db := openDB(t, "h.db", &Options{Dim: 2, Metric: MetricL2})
	if err := db.Put(1, []float32{0, 0}, nil, "red apple fruit"); err != nil {
		t.Fatal(err)
	}
	if err := db.Put(2, []float32{10, 10}, nil, "blue car vehicle"); err != nil {
		t.Fatal(err)
	}
	hits, err := db.SearchHybrid([]float32{0.1, 0.1}, "apple", 2, 0.5)
	if err != nil {
		t.Fatal(err)
	}
	if len(hits) != 2 {
		t.Fatalf("hybrid returned %d hits, want 2", len(hits))
	}
	if hits[0].ID != 1 {
		t.Errorf("hybrid prefers id %d, want the matching doc 1", hits[0].ID)
	}
	// alpha extremes collapse to one modality
	hits, err = db.SearchHybrid([]float32{9.9, 9.9}, "zzz", 1, 1.0)
	if err != nil {
		t.Fatal(err)
	}
	if len(hits) != 1 || hits[0].ID != 2 {
		t.Errorf("alpha=1 hybrid = %v, want pure vector hit id 2", hits)
	}
	hits, err = db.SearchHybrid(nil, "apple", 1, 0.0)
	if err != nil {
		t.Fatal(err)
	}
	if len(hits) != 1 || hits[0].ID != 1 {
		t.Errorf("alpha=0 text-only hybrid = %v, want id 1", hits)
	}
}

func TestPayloadCodec(t *testing.T) {
	db := openDB(t, "p.db", nil)
	cases := map[uint64]any{
		100: int64(42),
		101: 3.5,
		102: "a string",
		103: []byte{1, 2, 3},
		104: map[string]any{"a": int64(1), "b": []any{int64(2), int64(3)}},
		105: true,
		106: int64(1) << 40,
	}
	for id, value := range cases {
		if err := db.Put(id, nil, mustEnc(t, value), ""); err != nil {
			t.Fatal(err)
		}
	}
	for id, value := range cases {
		rec, err := db.Get(id)
		if err != nil {
			t.Fatalf("Get(%d): %v", id, err)
		}
		checkPayload(t, rec.Payload, value, "round-trip")
	}
}

func TestGetDeleteErrors(t *testing.T) {
	db := openDB(t, "g.db", &Options{Dim: 2})
	if err := db.Put(7, []float32{1, 2}, nil, "hello"); err != nil {
		t.Fatal(err)
	}
	rec, err := db.Get(7)
	if err != nil {
		t.Fatal(err)
	}
	if rec.ID != 7 || rec.Text != "hello" {
		t.Errorf("Get(7) = %+v", rec)
	}
	if !reflect.DeepEqual(rec.Vector, []float32{1, 2}) {
		t.Errorf("Get(7).Vector = %v, want [1 2]", rec.Vector)
	}
	if err := db.Delete(7); err != nil {
		t.Fatal(err)
	}
	if got := db.Count(); got != 0 {
		t.Errorf("Count after delete = %d, want 0", got)
	}
	if _, err := db.Get(7); !IsNotFound(err) {
		t.Errorf("Get(missing) error = %v, want NotFound", err)
	}
	if err := db.Delete(7); !IsNotFound(err) {
		t.Errorf("Delete(missing) error = %v, want NotFound", err)
	}
	if err := db.Put(9, []float32{0, 0}, nil, ""); err != nil {
		t.Fatal(err)
	}
	if err := db.PutEx(9, []float32{0, 0}, nil, "", InsertOnly); !IsAlreadyExists(err) {
		t.Errorf("duplicate InsertOnly error = %v, want AlreadyExists", err)
	}
}

func TestDimensionMismatch(t *testing.T) {
	db := openDB(t, "d.db", &Options{Dim: 3})
	if err := db.Put(1, []float32{1, 2}, nil, ""); !IsDimensionMismatch(err) {
		t.Errorf("wrong-dim Put error = %v, want DimensionMismatch", err)
	}
	if err := db.Put(1, []float32{1, 2, 3}, nil, ""); err != nil {
		t.Fatalf("correct-dim Put: %v", err)
	}
	if _, err := db.SearchVector([]float32{1}, 1); !IsDimensionMismatch(err) {
		t.Errorf("wrong-dim search error = %v, want DimensionMismatch", err)
	}
}

func TestPutManyListIDs(t *testing.T) {
	db := openDB(t, "m.db", &Options{Dim: 2})
	for i := 0; i < 50; i++ {
		if err := db.Put(uint64(i), []float32{float32(i), 0},
			mustEnc(t, i), "doc number"); err != nil {
			t.Fatal(err)
		}
	}
	if got := db.Count(); got != 50 {
		t.Fatalf("Count = %d, want 50", got)
	}
	ids, err := db.ListIDs()
	if err != nil {
		t.Fatal(err)
	}
	seen := make(map[uint64]bool, len(ids))
	for _, id := range ids {
		seen[id] = true
	}
	if len(seen) != 50 {
		t.Errorf("ListIDs covered %d distinct ids, want 50", len(seen))
	}
	var evens []uint64
	for i := 0; i < 50; i += 2 {
		evens = append(evens, uint64(i))
	}
	if removed := db.DeleteMany(evens); removed != 25 {
		t.Errorf("DeleteMany removed %d, want 25", removed)
	}
	if got := db.Count(); got != 25 {
		t.Errorf("Count after DeleteMany = %d, want 25", got)
	}
}

func TestPersistence(t *testing.T) {
	path := filepath.Join(t.TempDir(), "persist.db")
	db, err := Open(path, &Options{Dim: 2, Index: IndexHNSW, Metric: MetricIP})
	if err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 20; i++ {
		if err := db.Put(uint64(i), []float32{float32(i), 1}, mustEnc(t, i), "text"); err != nil {
			t.Fatal(err)
		}
	}
	if err := db.Flush(); err != nil {
		t.Fatal(err)
	}
	if err := db.Close(); err != nil {
		t.Fatal(err)
	}

	db, err = Open(path, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	if got := db.Count(); got != 20 {
		t.Errorf("reopened Count = %d, want 20", got)
	}
	if db.Index() != IndexHNSW {
		t.Errorf("Index = %v, want hnsw", db.Index())
	}
	if db.Metric() != MetricIP {
		t.Errorf("Metric = %v, want ip", db.Metric())
	}
	rec, err := db.Get(13)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(rec.Vector, []float32{13, 1}) {
		t.Errorf("reloaded vector = %v, want [13 1]", rec.Vector)
	}
	checkPayload(t, rec.Payload, int64(13), "reloaded payload")
	if err := db.Compact(); err != nil {
		t.Fatal(err)
	}
	if got := db.Count(); got != 20 {
		t.Errorf("Count after Compact = %d, want 20", got)
	}
}

func TestClosedAndDefaults(t *testing.T) {
	if got := DefaultOptions(); got.Dim != 0 || got.Metric != MetricL2 ||
		got.Index != IndexFlat || got.HnswM != 16 || got.HnswEfConstruction != 100 ||
		got.HnswEfSearch != 100 || got.Seed != 42 {
		t.Errorf("DefaultOptions = %+v", got)
	}
	db := openDB(t, "c.db", &Options{Dim: 1})
	if err := db.Close(); err != nil {
		t.Fatal(err)
	}
	if err := db.Close(); err != nil {
		t.Errorf("second Close = %v, want nil", err)
	}
	if err := db.Put(1, []float32{1}, nil, ""); err != ErrClosed {
		t.Errorf("Put after Close = %v, want ErrClosed", err)
	}
	if got := db.Count(); got != 0 {
		t.Errorf("Count after Close = %d, want 0", got)
	}
}
