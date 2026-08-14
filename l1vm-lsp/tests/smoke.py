#!/usr/bin/env python3
"""Smoke test for l1vm-lsp: drives a JSON-RPC session over stdio."""
import json
import os
import subprocess
import sys

BIN = os.path.join(os.path.dirname(__file__), "..", "build", "l1vm-lsp")
SAMPLE = os.path.join(os.path.dirname(__file__), "sample.l1com")
URI = "file://" + os.path.abspath(SAMPLE)

if not os.path.exists(BIN):
    print("server binary not found - run `make` first")
    sys.exit(1)

proc = subprocess.Popen(
    [BIN],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
)

pending = {}   # id -> method


def send(payload):
    body = json.dumps(payload).encode("utf-8")
    proc.stdin.write(f"Content-Length: {len(body)}\r\n\r\n".encode())
    proc.stdin.write(body)
    proc.stdin.flush()


def recv(timeout=10):
    """Read one framed message from the server (raw framing via os.read)."""
    import os
    import select
    import time
    fd = proc.stdout.fileno()
    buf = b""
    deadline = time.time() + timeout

    # read headers until CRLFCRLF
    while b"\r\n\r\n" not in buf:
        if time.time() > deadline:
            raise TimeoutError("no response within %ss" % timeout)
        r, _, _ = select.select([fd], [], [], 0.5)
        if not r:
            continue
        chunk = os.read(fd, 65536)
        if not chunk:
            raise EOFError("server closed stdout")
        buf += chunk

    head, _, rest = buf.partition(b"\r\n\r\n")
    n = 0
    for line in head.split(b"\r\n"):
        key, _, val = line.partition(b":")
        if key.strip().lower() == b"content-length":
            n = int(val.strip())
    # read body
    while len(rest) < n:
        if time.time() > deadline:
            raise TimeoutError("no response within %ss" % timeout)
        r, _, _ = select.select([fd], [], [], 0.5)
        if not r:
            continue
        chunk = os.read(fd, 65536)
        if not chunk:
            raise EOFError("server closed stdout")
        rest += chunk
    return json.loads(rest[:n].decode("utf-8"))


def request(method, params):
    rid = len(pending) + 1
    pending[rid] = method
    send({"jsonrpc": "2.0", "id": rid, "method": method, "params": params})
    return rid


def notify(method, params):
    send({"jsonrpc": "2.0", "method": method, "params": params})


fails = 0


def check(cond, msg):
    global fails
    print(("OK  " if cond else "FAIL") + " " + msg)
    if not cond:
        fails += 1


with open(SAMPLE, "r", encoding="utf-8") as f:
    sample_text = f.read()

# initialize
rid = request(
    "initialize",
    {
        "processId": None,
        "rootUri": None,
        "capabilities": {},
        "initializationOptions": {"l1comEnabled": "off"},
    },
)
resp = recv()
check(resp.get("id") == rid, "initialize responds with matching id")
check("result" in resp, "initialize returns result")
check("capabilities" in resp.get("result", {}), "initialize returns capabilities")

notify("initialized", {})

# didOpen
notify(
    "textDocument/didOpen",
    {"textDocument": {"uri": URI, "languageId": "l1com", "version": 1,
                      "text": sample_text}},
)

# wait for publishDiagnostics
diags = None
import os
import select
import time
fd = proc.stdout.fileno()
deadline = time.time() + 10
while time.time() < deadline:
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        m = recv(0.5)
        if m.get("method") == "textDocument/publishDiagnostics":
            diags = m
            break
check(diags is not None, "didOpen triggers publishDiagnostics")
if diags is not None:
    d = diags["params"]["diagnostics"]
    check(diags["params"]["uri"] == URI, "diagnostics uri matches")
    print("      diagnostics count: %d" % len(d))
    for x in d:
        print("      [%s] line %d: %s" % (
            x.get("severity"), x["range"]["start"]["line"], x["message"]))

# documentSymbol
rid = request("textDocument/documentSymbol", {"textDocument": {"uri": URI}})
resp = recv()
check(resp.get("id") == rid, "documentSymbol responds")
syms = resp.get("result", [])
names = [s.get("name") for s in syms]
check("main" in names and "hello" in names, "documentSymbol lists main+hello: %s" % names)

# foldingRange
rid = request("textDocument/foldingRange", {"textDocument": {"uri": URI}})
resp = recv()
folds = resp.get("result", [])
check(len(folds) >= 2, "foldingRange finds functions")

# semanticTokens/full
rid = request("textDocument/semanticTokens/full", {"textDocument": {"uri": URI}})
resp = recv()
tok = resp.get("result", {})
data = tok.get("data", [])
check(len(data) > 0 and len(data) % 5 == 0, "semanticTokens returns token data")

# completion on ":h"
rid = request(
    "textDocument/completion",
    {
        "textDocument": {"uri": URI},
        "position": {"line": 15, "character": 6},
    },
)
resp = recv()
items = resp.get("result", {}).get("items", [])
check(len(items) > 0, "completion returns items")

# snippet completion: "do" must expand to real Brackets do-while, not C style
notify(
    "textDocument/didChange",
    {
        "textDocument": {"uri": URI, "version": 2},
        "contentChanges": [{
            "range": {
                "start": {"line": 21, "character": 0},
                "end": {"line": 21, "character": 0},
            },
            "text": "\ndo",
        }],
    },
)
deadline = time.time() + 10
while time.time() < deadline:
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        m = recv(0.5)
        if m.get("method") == "textDocument/publishDiagnostics":
            break
rid = request(
    "textDocument/completion",
    {
        "textDocument": {"uri": URI},
        "position": {"line": 22, "character": 2},
    },
)
resp = recv()
items = resp.get("result", {}).get("items", [])
do_item = next((it for it in items if it.get("label") == "do"), None)
check(do_item is not None, "completion offers 'do' keyword")
snp = (do_item or {}).get("insertText", "")
check((do_item or {}).get("insertTextFormat") == 2 and "(do)" in snp
      and "while" in snp and "{ }" not in snp,
      "do completion is a valid Brackets do-while snippet: %r" % snp)

# hover on "total~" (line 14)
rid = request(
    "textDocument/hover",
    {
        "textDocument": {"uri": URI},
        "position": {"line": 14, "character": 22},
    },
)
resp = recv()
check("result" in resp and resp["result"], "hover returns content")

# definition on ":hello" usage (line 18: "    (:hello !)")
rid = request(
    "textDocument/definition",
    {
        "textDocument": {"uri": URI},
        "position": {"line": 18, "character": 8},
    },
)
resp = recv()
loc = resp.get("result")
check(loc and loc.get("uri") == URI, "definition resolves to function")

# references on "counter~" (line 7: "    (set int64 1 counter~ 0)")
rid = request(
    "textDocument/references",
    {
        "textDocument": {"uri": URI},
        "position": {"line": 7, "character": 19},
        "context": {"includeDeclaration": True},
    },
)
resp = recv()
refs = resp.get("result", [])
check(len(refs) >= 2, "references finds usages")

# didChange (simulate an edit: add a line)
changed = sample_text + "\n(print_n)\n"
notify(
    "textDocument/didChange",
    {
        "textDocument": {"uri": URI, "version": 2},
        "contentChanges": [{"text": changed}],
    },
)
deadline = time.time() + 10
got = False
while time.time() < deadline:
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        m = recv(0.5)
        if m.get("method") == "textDocument/publishDiagnostics":
            got = True
            break
check(got, "didChange triggers new diagnostics")

# incremental didChange: a second document, edited via a range
inc_uri = "file:///tmp/inc.l1com"
inc_text = "(main func)\n    #var ~ main\n(funcend)\n"
notify(
    "textDocument/didOpen",
    {"textDocument": {"uri": inc_uri, "languageId": "l1com",
                      "version": 1, "text": inc_text}},
)
deadline = time.time() + 10
inc_diags = None
while time.time() < deadline:
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        m = recv(0.5)
        if m.get("method") == "textDocument/publishDiagnostics" and \
                m.get("params", {}).get("uri") == inc_uri:
            inc_diags = m["params"]["diagnostics"]
            break
check(inc_diags == [], "incremental doc opens with 0 diagnostics")

# replace "funcend" -> "funcen" inside line 2 via a range edit
notify(
    "textDocument/didChange",
    {
        "textDocument": {"uri": inc_uri, "version": 2},
        "contentChanges": [{
            "range": {
                "start": {"line": 2, "character": 1},
                "end": {"line": 2, "character": 9},
            },
            "text": "funcen",
        }],
    },
)
deadline = time.time() + 10
inc_diags = None
while time.time() < deadline:
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        m = recv(0.5)
        if m.get("method") == "textDocument/publishDiagnostics" and \
                m.get("params", {}).get("uri") == inc_uri:
            inc_diags = m["params"]["diagnostics"]
            break
check(inc_diags is not None and len(inc_diags) >= 1,
      "incremental edit produces diagnostics: %s" %
      ([x["message"] for x in (inc_diags or [])]))

# unknown method -> error
rid = request("bogus/method", {})
resp = recv()
check(resp.get("error", {}).get("code") == -32601, "unknown method returns -32601")

# shutdown + exit
rid = request("shutdown", None)
resp = recv()
check(resp.get("result") is None, "shutdown returns null")
notify("exit", None)

proc.wait(timeout=5)

# ---- l1com integration session (separate server, compiler enabled) ----
L1COM = None
for cand in (
    os.environ.get("L1COM", ""),
    "/home/stefan/l1vm/bin/l1com",
    "/usr/local/l1vm/bin/l1com",
):
    if cand and os.path.isfile(cand):
        L1COM = cand
        break
if not L1COM:
    import shutil

    L1COM = shutil.which("l1com")

if not L1COM:
    print("SKIP l1com integration checks (no l1com binary found)")
else:
    proc = subprocess.Popen(
        [BIN],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    fd = proc.stdout.fileno()
    inc = "/home/stefan/l1vm/include"
    init = {"l1comEnabled": "on", "l1comPath": L1COM}
    if os.path.isdir(inc):
        init["includeDirs"] = [inc]
    rid = request(
        "initialize",
        {"processId": None, "rootUri": None, "capabilities": {},
         "initializationOptions": init},
    )
    resp = recv()
    check(resp.get("id") == rid, "l1com session initializes")
    notify("initialized", {})

    def open_doc(name, fname):
        with open(fname, "r", encoding="utf-8") as f:
            text = f.read()
        uri = "file://" + os.path.abspath(fname)
        notify(
            "textDocument/didOpen",
            {"textDocument": {"uri": uri, "languageId": "l1com",
                              "version": 1, "text": text}},
        )
        deadline = time.time() + 20
        while time.time() < deadline:
            r, _, _ = select.select([fd], [], [], 0.5)
            if r:
                m = recv(0.5)
                if m.get("method") == "textDocument/publishDiagnostics":
                    return m["params"]["diagnostics"]
        return None

    broken = os.path.join(os.path.dirname(__file__), "broken.l1com")
    valid = os.path.join(os.path.dirname(__file__), "valid.l1com")

    diags = open_doc("broken.l1com", broken)
    comp = [d for d in (diags or []) if d.get("source") == "l1com"]
    lines = [d["range"]["start"]["line"] + 1 for d in comp]
    msgs = [d["message"] for d in comp]
    check(len(comp) >= 2, "l1com diagnostics reported for broken file: %s" % lines)
    check(6 in lines and 7 in lines, "l1com line numbers correct: %s" % lines)
    check(any("b" in m and "not defined" in m for m in msgs),
          "l1com message content parsed")

    diags = open_doc("valid.l1com", valid)
    comp = [d for d in (diags or []) if d.get("source") == "l1com"]
    check(len(comp) == 0, "no l1com diagnostics for valid file")

    rid = request("shutdown", None)
    resp = recv()
    notify("exit", None)
    proc.wait(timeout=5)

print()
if fails:
    print("%d check(s) FAILED" % fails)
    sys.exit(1)
print("all checks passed")
