package vecdb

// Payload codec. The payload column is an opaque blob, so both the
// Python and JavaScript bindings serialise payloads JSON-first, with a
// tiny "\x00VDB" tag for the two cases JSON cannot express losslessly:
// raw bytes and exact 64-bit numbers. Go does the same, so payloads
// written by one language decode in the others.
//
//	EncodePayload([]byte)          -> b"\x00VDBb" + raw bytes
//	EncodePayload(int, all widths) -> b"\x00VDBi" + 8 byte little-endian signed
//	EncodePayload(float32/64)      -> b"\x00VDBf" + 8 byte little-endian IEEE-754
//	string, bool, anything else    -> UTF-8 JSON
//
// DecodePayload reverses this on a best-effort basis: tagged values are
// recognised exactly, foreign payloads (from the C API, the CLI or other
// bindings) fall back to JSON, then to fixed-width numbers, then to
// text, then to raw bytes.

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"math"
	"strings"
	"unicode/utf8"
)

var (
	tag     = []byte{0x00, 'V', 'D', 'B'}
	blobTag = []byte{0x00, 'V', 'D', 'B', 'b'}
	intTag  = []byte{0x00, 'V', 'D', 'B', 'i'}
	fltTag  = []byte{0x00, 'V', 'D', 'B', 'f'}
)

// EncodePayload serialises a Go value into payload bytes. It returns a
// nil slice for a nil value.
func EncodePayload(v any) ([]byte, error) {
	switch x := v.(type) {
	case nil:
		return nil, nil
	case []byte:
		return concat(blobTag, x), nil
	case string:
		return json.Marshal(x)
	case bool:
		return json.Marshal(x)
	case int:
		return tagInt(int64(x)), nil
	case int8:
		return tagInt(int64(x)), nil
	case int16:
		return tagInt(int64(x)), nil
	case int32:
		return tagInt(int64(x)), nil
	case int64:
		return tagInt(x), nil
	case uint:
		return tagUint(uint64(x))
	case uint8:
		return tagInt(int64(x)), nil
	case uint16:
		return tagInt(int64(x)), nil
	case uint32:
		return tagInt(int64(x)), nil
	case uint64:
		return tagUint(x)
	case float32:
		return tagFloat(float64(x)), nil
	case float64:
		return tagFloat(x), nil
	default:
		b, err := json.Marshal(v)
		if err != nil {
			return nil, fmt.Errorf("vecdb: cannot encode payload: %w", err)
		}
		return b, nil
	}
}

// MustEncodePayload is EncodePayload that panics on error. Handy for
// tests and static payloads.
func MustEncodePayload(v any) []byte {
	b, err := EncodePayload(v)
	if err != nil {
		panic(err)
	}
	return b
}

// DecodePayload is the best-effort inverse of EncodePayload. It returns
// nil for an absent payload and []byte for opaque binary data.
func DecodePayload(raw []byte) any {
	if len(raw) == 0 {
		return []byte{}
	}

	if bytes.HasPrefix(raw, tag) && len(raw) > len(tag) {
		switch {
		case raw[len(tag)] == 'b':
			return append([]byte(nil), raw[len(blobTag):]...)
		case raw[len(tag)] == 'i' && len(raw) == len(intTag)+8:
			return int64(binary.LittleEndian.Uint64(raw[len(intTag):]))
		case raw[len(tag)] == 'f' && len(raw) == len(fltTag)+8:
			return math.Float64frombits(binary.LittleEndian.Uint64(raw[len(fltTag):]))
		}
	}

	// JSON, when the payload looks like it
	trimmed := strings.TrimLeft(string(raw), " \t\r\n\v\f")
	if trimmed != "" && (strings.ContainsRune("{[\"", rune(trimmed[0])) ||
		trimmed == "true" || trimmed == "false" || trimmed == "null") {
		dec := json.NewDecoder(bytes.NewReader(raw))
		dec.UseNumber()
		var v any
		if err := dec.Decode(&v); err == nil {
			return normaliseNumbers(v)
		}
	}

	switch len(raw) {
	case 8:
		return int64(binary.LittleEndian.Uint64(raw))
	case 4:
		return math.Float32frombits(binary.LittleEndian.Uint32(raw))
	}
	if utf8.Valid(raw) {
		return string(raw)
	}
	return append([]byte(nil), raw...)
}

func tagInt(v int64) []byte {
	b := make([]byte, 8)
	binary.LittleEndian.PutUint64(b, uint64(v))
	return concat(intTag, b)
}

func tagUint(v uint64) ([]byte, error) {
	if v > math.MaxInt64 {
		// Too big for the signed int tag: JSON keeps it exact.
		return json.Marshal(v)
	}
	return tagInt(int64(v)), nil
}

func tagFloat(v float64) []byte {
	b := make([]byte, 8)
	binary.LittleEndian.PutUint64(b, math.Float64bits(v))
	return concat(fltTag, b)
}

// normaliseNumbers converts the json.Number values produced by the
// decoder back into int64 / float64, walking into arrays and maps.
func normaliseNumbers(v any) any {
	switch x := v.(type) {
	case json.Number:
		if i, err := x.Int64(); err == nil {
			return i
		}
		if f, err := x.Float64(); err == nil {
			return f
		}
		return x.String()
	case map[string]any:
		for k, e := range x {
			x[k] = normaliseNumbers(e)
		}
		return x
	case []any:
		for i, e := range x {
			x[i] = normaliseNumbers(e)
		}
		return x
	default:
		return v
	}
}

func concat(a, b []byte) []byte {
	out := make([]byte, 0, len(a)+len(b))
	out = append(out, a...)
	return append(out, b...)
}
