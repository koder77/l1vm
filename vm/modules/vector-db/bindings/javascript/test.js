#!/usr/bin/env node
'use strict';
/**
 * Smoke test for the vecdb JavaScript bindings (koffi over libvecdb.so).
 *
 * Run with:  node bindings/javascript/test.js
 *
 * It needs the shared library built (`make`) either next to this file, in
 * the repository root (its ancestor), on LD_LIBRARY_PATH, or pointed at by
 * the VECDB_LIBRARY environment variable.
 */

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');

const {
    VecDB,
    NotFoundError,
    AlreadyExistsError,
    DimensionMismatchError,
} = require('./index.js');

let checks = 0;
let failures = 0;

function check(cond, msg) {
    checks += 1;
    if (!cond) {
        failures += 1;
        console.log(`FAIL: ${msg}`);
    }
}

function checkEq(actual, expected, msg) {
    checks += 1;
    try {
        assert.deepStrictEqual(actual, expected);
    } catch {
        failures += 1;
        console.log(`FAIL: ${msg} (got ${JSON.stringify(actual)}, want ${JSON.stringify(expected)})`);
    }
}

function expectThrows(fn, ErrClass, msg) {
    checks += 1;
    try {
        fn();
    } catch (err) {
        if (err instanceof ErrClass) return;
        failures += 1;
        console.log(`FAIL: ${msg} (got ${err.name}: ${err.message})`);
        return;
    }
    failures += 1;
    console.log(`FAIL: ${msg} (nothing thrown)`);
}

function tmpdb(name) {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'vecdb-js-'));
    return path.join(dir, name);
}

function testBasicVector() {
    const p = tmpdb('v.db');
    const db = new VecDB(p, { dim: 3, metric: 'cosine' });
    db.add(1, { vector: [1, 0, 0], text: 'a cat sat on the mat',
        payload: { kind: 'animal' } });
    db.add(2, { vector: [0, 1, 0], text: 'a dog barked at the postman',
        payload: { kind: 'animal' } });
    db.add(3, { vector: [0, 0, 1], text: 'a fish swam in the sea' });
    check(db.count() === 3, 'count after 3 adds');
    check(db.dim === 3, 'dim == 3');
    check(db.metric === 'cosine', 'metric round-trips');
    check(db.path === p, 'path round-trips');

    const hits = db.search([0.95, 0.05, 0], 3);
    check(hits.length === 3, 'vector search returns 3');
    check(hits[0].id === 1, 'nearest neighbour is id 1');
    check(hits[0].score > hits[1].score, 'scores descending');
    checkEq(hits[0].payload, { kind: 'animal' }, 'object payload decodes');
    check(hits[0].text === 'a cat sat on the mat', 'match carries the text');
    db.close();
}

function testTextSearch() {
    const db = new VecDB(tmpdb('t.db'));
    db.add(1, { text: 'the quick brown fox jumps over the lazy dog' });
    db.add(2, { text: 'a packet of postman biscuits' });
    db.add(3, { text: 'foxes and hounds' });
    let hits = db.searchText('fox', 5);
    check(hits.length === 1 && hits[0].id === 1, "BM25 matches the exact token 'fox'");
    hits = db.searchText('foxes', 5);
    check(hits.length === 1 && hits[0].id === 3, "BM25 has no stemming: 'foxes' is its own token");
    hits = db.searchText('postman', 5);
    check(hits.length === 1 && hits[0].id === 2, 'postman matched id 2');
    check(db.searchText('', 5).length === 0, 'empty query yields no hits');
    db.close();
}

function testHybrid() {
    const db = new VecDB(tmpdb('h.db'), { dim: 2, metric: 'l2' });
    db.add(1, { vector: [0, 0], text: 'red apple fruit' });
    db.add(2, { vector: [10, 10], text: 'blue car vehicle' });
    let hits = db.searchHybrid([0.1, 0.1], 'apple', 2, 0.5);
    check(hits.length === 2, 'hybrid returns both');
    check(hits[0].id === 1, 'hybrid prefers the matching doc');
    // alpha extremes collapse to one modality
    hits = db.searchHybrid([9.9, 9.9], 'zzz', 1, 1.0);
    check(hits.length === 1 && hits[0].id === 2, 'alpha=1 is pure vector');
    hits = db.searchHybrid(null, 'apple', 1, 0.0);
    check(hits.length === 1 && hits[0].id === 1, 'alpha=0 with text only');
    db.close();
}

function testPayloadTypes() {
    const db = new VecDB(tmpdb('p.db'));
    const cases = new Map([
        [100, 42],
        [101, 3.5],
        [102, 'a string'],
        [103, new Uint8Array([1, 2, 3])],
        [104, { a: 1, b: [2, 3] }],
        [105, true],
        [106, 2 ** 40], // large integer: survives exactly through the int tag
    ]);
    for (const [id, value] of cases) db.add(id, { payload: value });
    for (const [id, value] of cases) {
        const rec = db.get(id);
        if (value instanceof Uint8Array) {
            checkEq(rec.payload, value, `payload ${id} round-trips (bytes)`);
            checkEq(rec.rawPayload.subarray(5), value,
                `rawPayload ${id} keeps the tag-prefixed bytes`);
        } else {
            checkEq(rec.payload, value, `payload ${id} round-trips (${String(value)})`);
        }
    }
    db.close();
}

function testGetDeleteErrors() {
    const db = new VecDB(tmpdb('g.db'), { dim: 2 });
    db.add(7, { vector: [1, 2], text: 'hello' });
    const rec = db.get(7);
    check(rec.id === 7, 'get id');
    checkEq(rec.vector, [1, 2], 'get vector');
    check(rec.text === 'hello', 'get text');
    check(rec.payload === null, 'no payload decodes to null');
    db.delete(7);
    check(db.count() === 0, 'count after delete');
    expectThrows(() => db.get(7), NotFoundError, 'get missing throws NotFoundError');
    expectThrows(() => db.delete(7), NotFoundError, 'delete missing throws NotFoundError');
    db.add(9, { vector: [0, 0] });
    expectThrows(() => db.add(9, { vector: [0, 0], upsert: false }),
        AlreadyExistsError, 'duplicate insert-only throws AlreadyExistsError');
    db.close();
}

function testDimensionMismatch() {
    const db = new VecDB(tmpdb('d.db'), { dim: 3 });
    expectThrows(() => db.add(1, { vector: [1, 2] }),
        DimensionMismatchError, 'wrong-dim put throws DimensionMismatchError');
    db.add(1, { vector: [1, 2, 3] });
    check(db.count() === 1, 'correct-dim put works');
    db.close();
}

function testAddManyAndIds() {
    const db = new VecDB(tmpdb('m.db'), { dim: 2 });
    const records = [];
    for (let i = 0; i < 50; i++) {
        records.push({ id: i, vector: [i, 0], text: `doc number ${i}`, payload: i });
    }
    const n = db.addMany(records);
    check(n === 50, 'addMany wrote 50');
    check(db.count() === 50, 'count is 50');
    checkEq([...db.ids()].sort((a, b) => a - b), [...Array(50).keys()],
        'list_ids covers all');
    const evens = [];
    for (let i = 0; i < 50; i += 2) evens.push(i);
    const removed = db.deleteMany(evens);
    check(removed === 25, 'delete_many removed 25');
    check(db.count() === 25, 'count after deleteMany');
    db.close();
}

function testPersistence() {
    const p = tmpdb('persist.db');
    let db = new VecDB(p, { dim: 2, index: 'hnsw', metric: 'ip' });
    const records = [];
    for (let i = 0; i < 20; i++) {
        records.push({ id: i, vector: [i, 1], text: `text ${i}`, payload: i });
    }
    db.addMany(records);
    db.flush();
    db.close();

    db = new VecDB(p);
    check(db.count() === 20, 'reopened count');
    check(db.index === 'hnsw', 'index persisted');
    check(db.metric === 'ip', 'metric persisted');
    const rec = db.get(13);
    checkEq(rec.vector, [13, 1], 'vector survived reload');
    check(rec.payload === 13, 'payload survived reload');
    db.compact();
    check(db.count() === 20, 'count survives compact');
    db.close();
}

function testQueryDispatch() {
    const db = new VecDB(tmpdb('q.db'), { dim: 2 });
    db.add(1, { vector: [1, 0], text: 'alpha beta' });
    db.add(2, { vector: [0, 1], text: 'gamma delta' });
    check(db.query({ text: 'alpha', k: 1 }).length === 1, 'query text only');
    check(db.query({ vector: [1, 0], k: 2 }).length === 2, 'query vector only');
    check(db.query({ text: 'alpha', vector: [1, 0], k: 2 }).length === 2, 'query hybrid');
    check(db.query({ k: 2 }).length === 0, 'query with nothing returns []');
    db.close();
}

function main() {
    const tests = [
        testBasicVector,
        testTextSearch,
        testHybrid,
        testPayloadTypes,
        testGetDeleteErrors,
        testDimensionMismatch,
        testAddManyAndIds,
        testPersistence,
        testQueryDispatch,
    ];
    for (const t of tests) {
        try {
            t();
        } catch (err) {
            failures += 1;
            console.log(`FAIL: ${t.name} threw ${err.name}: ${err.message}`);
        }
    }
    console.log(`\njavascript bindings: ${checks} checks, ${failures} failures`);
    process.exit(failures ? 1 : 0);
}

main();
