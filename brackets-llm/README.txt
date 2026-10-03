===============================================================================
 brackets-llm - an opencode-like Brackets (L1VM) code environment
===============================================================================

Generated with opencode Big Pickle AI
License: GPL3

brackets-llm is a small program written in plain C (C11, no mandatory
external dependencies) that lets you generate L1VM "Brackets" programs with a
local LLM. It works like opencode:

  * A system prompt (l1vm-system-prompt.txt) is sent to the LLM at startup.
  * You chat with the LLM.
  * The LLM can autonomously use tools (read_file, write_file, edit_file,
    list_files, web_fetch) to read and modify files on disk and to download
    pages from the internet, without you typing a slash command, just like
    opencode.
  * Brackets code found in the model's answer is saved to a .l1com file.
  * The code is checked with the L1VM language server (l1vm-lsp), which runs
    real compiler diagnostics (l1com), and errors are automatically corrected
    by asking the model to fix them (up to 4 iterations).
  * The program can build (l1vm-build.sh) and run (l1vm) the result.

The program does NOT start an LLM server. It talks to ANY OpenAI-compatible
/v1/chat/completions server that YOU start with the model of your choice
(llama.cpp llama-server, Ollama, vLLM, LM Studio, a local opencode model
server, ...) and you can switch between them with one environment variable.
Hosted APIs work too: https://api.openai.com/v1 (or OpenRouter, Groq, ...) is
just another base URL, together with BRACKETS_LLM_API_KEY.


===============================================================================
 1. REQUIREMENTS
===============================================================================

  * A Linux/Unix system with gcc/cc and make.
  * A running OpenAI-compatible server. Any of these works:
        - llama.cpp llama-server  (https://github.com/ggml-org/llama.cpp)
        - Ollama                   (https://ollama.com)
        - vLLM, LM Studio, SGLang, a local opencode model server, ...
    Recommended context size: 65536.
  * The L1VM toolchain in ~/l1vm/bin/:
        - l1vm-lsp          (the L1VM language server)
        - l1com             (the Brackets compiler)
        - l1pre             (the preprocessor)
        - l1vm-build.sh     (build script)
        - l1vm              (the virtual machine to run programs)
    and the L1VM include directory ~/l1vm/include/.

Optional, only for https (the web_fetch tool and hosted LLM APIs such as
OpenAI):

  * libcurl development files (libcurl4-openssl-dev / libcurl-devel) for
    https:// requests. The Makefile probes for libcurl at build time and
    enables it automatically. Without libcurl the program still talks to
    plain http:// model servers by itself, and it uses the "curl" program as a
    helper for https:// downloads (so plain http:// needs no extra package at
    all). Build without libcurl on purpose with:   make clean && make CURL_OK=1


===============================================================================
 2. BUILDING
===============================================================================

    cd brackets-llm
    make

This creates the binary  build/brackets-llm .

Optionally install it:

    make install          # copies it to ~/l1vm/bin/brackets-llm


===============================================================================
 3. STARTING YOUR LLM SERVER
===============================================================================

brackets-llm only speaks the OpenAI REST protocol, so any server that offers
/v1/chat/completions works. Pick one:

  llama.cpp
    ~/llama.cpp/build/bin/llama-server \
        -m /path/to/your-model.gguf \
        --port 8080 \
        -c 65536 \
        --host 127.0.0.1

    -> export BRACKETS_LLM_BASE_URL=http://127.0.0.1:8080/v1

  Ollama
    ollama serve
    ollama pull qwen2.5-coder:14b

    -> export BRACKETS_LLM_BASE_URL=http://localhost:11434/v1
       export BRACKETS_LLM_MODEL=qwen2.5-coder:14b

  vLLM
    vllm serve Qwen/Qwen2.5-Coder-14B-Instruct --port 8000

    -> export BRACKETS_LLM_BASE_URL=http://localhost:8000/v1

  LM Studio (local server tab) or a local opencode model server: same
  protocol, just point BRACKETS_LLM_BASE_URL at it.

  OpenAI (hosted, https)
    No local server needed, but you need an API key:

    -> export BRACKETS_LLM_BASE_URL=https://api.openai.com/v1
       export BRACKETS_LLM_API_KEY=sk-...
       export BRACKETS_LLM_MODEL=gpt-4.1-mini

    Any other OpenAI-compatible https endpoint works the same way
    (OpenRouter, Groq, Together, Azure OpenAI, a corporate gateway).

Wait until the server log says it is listening.

Find the exact model id your server reports (brackets-llm must send it):

    curl http://127.0.0.1:8080/v1/models          # llama.cpp / vLLM / LM Studio
    curl http://localhost:11434/v1/models         # Ollama
    # or, inside brackets-llm:  /status

With llama.cpp the model id is usually the full path to the .gguf file, e.g.:

    /home/you/llama-models/model.gguf

NOTE for Qwen3.x "thinking" models:
The "llamacpp" backend sends "chat_template_kwargs":
{"enable_thinking": false} so that the model produces code instead of
spending tokens on reasoning. The generic "openai" backend does not send that
field (not every server understands it), so use
BRACKETS_LLM_BACKEND=llamacpp for llama-server and the default
BRACKETS_LLM_BACKEND=openai for everything else.


===============================================================================
 4. CONFIGURATION (ENVIRONMENT VARIABLES)
===============================================================================

  BRACKETS_LLM_BACKEND      Which backend to use:
                              openai   (default) POST
                                       {base_url}/chat/completions with
                                       "Authorization: Bearer <api_key>"
                              llamacpp same protocol, plus
                                       chat_template_kwargs.enable_thinking
                                       =false, no auth header, never streams
                              auto     probe GET {base_url}/models and fall
                                       back to llamacpp with a warning
  BRACKETS_LLM_BASE_URL     Base URL of the OpenAI-compatible API. A missing
                            path is completed with "/v1", and a trailing
                            "/chat/completions" is accepted too, so all of
                            these work:
                              http://localhost:11434      (Ollama)
                              http://localhost:8000       (vLLM)
                              http://127.0.0.1:1234/v1    (LM Studio)
                              http://127.0.0.1:8080/v1    (llama-server)
                              https://api.openai.com/v1   (hosted API)
                            Default: http://127.0.0.1:8080/v1
                            (BRACKETS_LLM_URL is still read as a fallback)
  BRACKETS_LLM_MODEL        Model id as reported by the server.
                            Default: Qwen3.6-35B-A3B
                            (in practice you must set this to the id from
                            "curl <base_url>/models" or from /status)
  BRACKETS_LLM_API_KEY      Value for "Authorization: Bearer ...". Most local
                            servers ignore it; OpenAI and other hosted APIs
                            require it. OPENAI_API_KEY is used as a fallback.
                            Default: (no header)
  BRACKETS_LLM_CA_FILE      Extra CA bundle for https, if the server does not
                            use a publicly trusted certificate (corporate TLS
                            interception, a self-signed gateway):
                              export BRACKETS_LLM_CA_FILE=/path/to/ca.pem
                            Unset = the trust store libcurl was built with.
                            Local http:// servers ignore this.
  BRACKETS_LLM_TEMPERATURE  Sampling temperature. Unset = do not send the
                            field, the server default is used.
                            Default: 0.7 was hardcoded before, now unset
  BRACKETS_LLM_TOP_P        Nucleus sampling, same rule as temperature.
  BRACKETS_LLM_MAX_TOKENS   Limit for the answer; 0/unset = server default.
  BRACKETS_LLM_STREAM       1 = request Server-Sent Events and print the
                            answer while it is generated. 0 (default) waits
                            for the complete answer.
  BRACKETS_LLM_TIMEOUT      Socket read/send timeout in seconds.
                            0 (default) = wait as long as the model needs.
  BRACKETS_LLM_DEBUG        1 = print one line per request to stderr.
  BRACKETS_LLM_HISTORY_BUDGET
                            Character budget for the conversation sent with
                            each request (the system prompt is always kept).
                            Default: 150000
  BRACKETS_LLM_SYSTEM_PROMPT  Path to the system prompt file.
                            Default: l1vm-system-prompt.txt
  L1VM_LSP                  Path to the l1vm-lsp binary.
                            Default: $HOME/l1vm/bin/l1vm-lsp
  L1VM_L1COM                Path to the l1com binary.
                            Default: $HOME/l1vm/bin/l1com
  L1VM_INCLUDE_DIR          L1VM include directory for <...> includes.
                            Default: $HOME/l1vm/include
  BRACKETS_LLM_CONFIRM      If set to 1/yes/y/true, the program asks you
                            (interactive mode) before the model writes any
                            file. By default writes are allowed automatically.
  BRACKETS_LLM_NET_CONFIRM  Set to 0/no/n/false/off to turn the question off.
                             By default the program asks you (interactive
                             mode) before the model fetches each URL, and a
                             refused request is never sent. Every request is
                             printed as "[fetch] <status line>" in the
                             terminal.

Example (llama.cpp, the old way still works):

    export BRACKETS_LLM_BACKEND=llamacpp
    export BRACKETS_LLM_MODEL="/home/you/llama-models/qwen-llm.gguf"
    export BRACKETS_LLM_URL="http://127.0.0.1:8080"

Example (Ollama, streaming):

    export BRACKETS_LLM_BASE_URL=http://localhost:11434/v1
    export BRACKETS_LLM_MODEL=qwen2.5-coder:14b
    export BRACKETS_LLM_STREAM=1
    export BRACKETS_LLM_TEMPERATURE=0.2

Example (OpenAI over https):

    export BRACKETS_LLM_BASE_URL=https://api.openai.com/v1
    export BRACKETS_LLM_API_KEY=sk-...
    export BRACKETS_LLM_MODEL=gpt-4.1-mini

https requests go through libcurl (the same library that fetches URLs), so
there is no separate TLS code path. http:// still uses the plain socket path
and needs no libraries at all. If the program was built without libcurl, only
http:// works.

Inside a session, /status prints the effective configuration plus the model
ids the server reports, and /backend openai|llamacpp|auto switches the backend
without restarting.


===============================================================================
 5. RUNNING
===============================================================================

    cd brackets-llm
    ./build/brackets-llm

The program prints its configuration and a help text. Then you can simply
type a request, for example:

    You> Write a Hello World program in Brackets.

The assistant reply is printed. If it contains Brackets code, the program
  * extracts the code,
  * saves it to a .l1com file,
  * checks it with l1vm-lsp (static analysis + real l1com compiler
    diagnostics),
  * if there are errors, asks the model again (up to 4 times) until the code
    compiles cleanly or gives up,
  * writes the final code to the .l1com file.

The conversation is kept in memory; on each request the system prompt plus
the most recent messages are sent to the server.

With BRACKETS_LLM_STREAM=1 the answer is printed piece by piece while the model
generates it (Server-Sent Events). The tool call handling and the token count
work exactly as without streaming: the deltas are collected into the same
answer object the non-streaming request would have returned.


===============================================================================
 6. AUTONOMOUS TOOL USE (opencode-style)
===============================================================================

The model can use tools on its own, exactly like opencode. You do NOT have to
type a slash command. When you ask, for example:

    You> Look at hi.l1com and change the greeting to "Good morning".

the model may:
  1. call read_file to inspect the file,
  2. call write_file (or edit_file) to modify it,
  3. and only then reply with a final text answer.

The program advertises these tools to the server in the chat request:

  read_file(path, limit?, offset?)   -> returns the file contents
                                       (truncated at ~128 KB).
  write_file(path, content)          -> creates or overwrites a file.
  edit_file(path, oldString, newString)
                                    -> replaces one exact substring, so the
                                       model does not have to resend a whole
                                       large file.
  list_files(dir?)                   -> lists the files/directories of a
                                       directory.
  web_fetch(url, max_bytes?)         -> downloads a document from the
                                       internet (see below).

While a tool call is running, every [tool] round is shown in the terminal and
the tool results are fed back into the conversation so the model can continue
until it is finished. To prevent endless loops the model may run at most
MAX_TOOL_ITERS (8) consecutive tool rounds before the program stops it.

Every tool write of a .l1com source file is verified with l1vm-lsp right
after the write (like the code blocks in section 5): the program prints an
"LSP check" summary listing any errors (up to the first 6) and feeds that
summary back into the conversation, so the model sees the diagnostics and can
fix the file in a follow-up tool round.

File writes happen automatically by default (opencode-like). To be asked for
confirmation on every write, start with:

    export BRACKETS_LLM_CONFIRM=1

Writes are still subject to the OS file permissions. Paths may be relative to
the working directory or start with ~ (expanded to your HOME).

After the final answer, if it contains Brackets (.l1com) code, it is saved,
checked and auto-corrected exactly as described in section 8.


------------------------------------------------------------------------------
  6.1 web_fetch (the internet)
------------------------------------------------------------------------------

The model can download data from the internet on its own, so it does not have
to invent facts or rely on your training data. For example:

    You> What does the L1VM README say about the "push" instruction?

the model may call web_fetch, read the page, and then answer.

    web_fetch(url, max_bytes?)

  * Only absolute http:// and https:// URLs are accepted; file://, ftp://
    and similar schemes are rejected.
  * The user is asked before every request and can refuse it. A refused URL
    is not requested at all; the model gets "denied: ..." instead. See below.
  * Redirects are followed (up to 5 hops), chunked responses are decoded, and
    the final URL after the redirects is reported back, so the model knows
    where the text actually came from.
  * HTML (and XHTML) is converted to readable plain text: tags, scripts and
    styles are dropped, block elements become line breaks, list items get
    markers, and common HTML entities (as well as Latin-1 input) are turned
    into UTF-8. That keeps pages usable for a model instead of returning a
    wall of markup.
  * The body is capped: 64000 bytes by default, 200000 at most. Use the
    optional max_bytes argument for a long page or a large file. A body that
    was cut is marked "truncated" in the result header.
  * Binary content (image/*, video/*, audio/*, application/pdf,
    application/octet-stream, ...) and bodies that contain NUL bytes are not
    returned as raw bytes; the model instead gets a short note with the status
    line, the content type and the size.
  * HTTP errors are NOT tool failures. A 404 or a 500 is returned to the
    model as a normal result (starting with "HTTP <status> ...") so it can
    react to it, e.g. try another URL. Only transport and URL problems
    (DNS, connection refused, timeout, unsupported scheme) are reported as
    errors starting with "error:". Refused requests start with "denied:".

Result format:

    HTTP 200 https://example.com/page.html  [content-type: text/html, 18234 bytes]

    <the page as plain text>

Only https:// needs libcurl (or the curl program) - plain http:// is
downloaded by the built-in HTTP client, so the tool works out of the box.
Every outgoing request is printed in the terminal as:

    [fetch] HTTP 200 https://example.com/page.html  [content-type: ...]

Internet access is asked for before each URL, so nothing is downloaded
behind your back. In interactive mode the model has to get your consent
first:

    The model wants to download this file from the internet:
        https://example.com/page.html
    Allow this request? [y/N]

Only "y" (or "Y") allows it. An empty answer, "n", or Ctrl+D refuses the
request, and a refused URL is never requested from the network - the model
just gets "denied: user did not allow fetching ...". To let the model fetch
without asking (scripts, trusted models):

    export BRACKETS_LLM_NET_CONFIRM=0

When stdin is not a terminal there is nobody to ask, so the request runs
unattended, exactly like before.


===============================================================================
 7. COMMANDS (slash commands)
===============================================================================

  /read <file>     Print the contents of a file.
  /write <file>    Write the last generated code to <file> (or, if the file
                   name ends in .l1com, verify it with l1vm-lsp afterwards).
  /save <name>     Save the last generated code to <name>.l1com and verify
                   it with l1vm-lsp afterwards.
  /list            List the files in the working directory.
  /build <name>    Build <name.l1com> with l1vm-build.sh
                   (output is written to build.txt and shown).
  /run <name>      Run <name> with l1vm.
  /code            Print the last generated code block.
  /compact         Summarize + collapse old messages in the context window
                   (keeps the system prompt and the last 8 messages).
  /status          Show the backend configuration and the model ids the
                   server reports.
  /backend [name]  Show the current backend, or switch at runtime to
                   openai, llamacpp or auto.
  /new             Reset the conversation (new chat session).
  /help            Show the help text.
  /quit            Exit.

When the server reports it (OpenAI-compatible "usage" field), every LLM reply
is followed by a token count line: "[tokens in: N, out: M]".


===============================================================================
 8. HOW THE AUTO-CORRECTION WORKS
===============================================================================

1. The model's reply is scanned for a fenced code block (```l1com ... ```)
   or a block containing "(main func)".
2. The code is written to a unique temporary .brackets-check-<pid>-<n>.l1com
   file and opened in l1vm-lsp via JSON-RPC over stdio (Content-Length
   framing). Every check opens a fresh document (didOpen) with its own
   URL. This is important: l1vm-lsp ignores didOpen for an already-open
   URI (and would report nothing), and it runs the real l1com compiler
   only on didOpen, not on didChange.
3. l1vm-lsp publishes diagnostics (severity 1 = error). These include the
   static analysis and the real l1com/l1pre compiler output.
4. If any error is present, the diagnostics are appended to the conversation
   and the model is asked to output the complete corrected program.
5. The new code is checked again. This repeats until there are no errors or
   MAX_FIX_ITERS (4) is reached. When you run brackets-llm in a terminal,
   it asks "Continue: y/n" after every failed fix attempt; answer "n" to
   abort the auto-fix loop immediately (the file is then NOT saved).
6. The final code is saved under the program name (e.g. hello.l1com if the
   model put "// hello.l1com" at the top, otherwise program-<n>.l1com).
   A clean result prints "saved <name> (LSP check OK)". If MAX_FIX_ITERS is
   reached and the code still has errors, the file is saved anyway to keep
   the work, but a loud WARNING with the remaining errors is printed (and
   the file must not be treated as verified). If the LSP itself fails, an
   unverified-save warning is printed instead.

If the LSP cannot be started, the program prints a warning and continues
without code checking.


===============================================================================
 9. TROUBLESHOOTING
===============================================================================

PROBLEM: "cannot connect to 127.0.0.1:8080" / "no usable model backend"
  -> The server is not running or BRACKETS_LLM_BASE_URL is wrong. Start it
     (section 3) and check:  /status  (it prints the URL, endpoint and the
     model ids the server reports).

PROBLEM: "HTTP 401 ... invalid_api_key" against a hosted API
  -> BRACKETS_LLM_API_KEY is missing, wrong or expired. Check with:
     curl https://api.openai.com/v1/models -H "Authorization: Bearer $BRACKETS_LLM_API_KEY"
     The key must be sent with BRACKETS_LLM_BACKEND=openai (llamacpp sends
     no Authorization header at all).

PROBLEM: "SSL peer certificate or SSH remote key was not OK"
  -> The https server uses a certificate that the system trust store does not
     know. For a corporate gateway: export BRACKETS_LLM_CA_FILE=/path/to/ca.pem
     (ask whoever runs the gateway). Do not work around it by disabling the
     certificate check - the api_key would go to an unverified peer.

PROBLEM: "https is not available for POST requests in this build"
  -> The binary was compiled without libcurl. Rebuild (the Makefile detects
     libcurl automatically), or use an http:// endpoint.

PROBLEM: "HTTP 404 ... model 'x' not found" / "error: model not found"
  -> BRACKETS_LLM_MODEL does not match the id reported by the server.
     Run:  curl <base_url>/models   or use /status, then export exactly
     that id.

PROBLEM: "no usable event in the stream"
  -> You set BRACKETS_LLM_STREAM=1 but the server does not stream that way.
     Use BRACKETS_LLM_STREAM=0 (or the llamacpp backend, which never
     streams).

PROBLEM: server returns "request (N tokens) exceeds the available context
         size"
  -> The system prompt is large (~18k tokens). Start llama-server with a big
     context, e.g. -c 65536.

PROBLEM: "LSP check failed" or "could not start l1vm-lsp"
  -> l1vm-lsp is not in $HOME/l1vm/bin or was not built. Set L1VM_LSP to your
     l1vm-lsp binary. Build it first if needed (see ~/develop/l1vm/l1vm-lsp).

PROBLEM: the LSP check previously hung forever when a second program was
         created in the same session
  -> Fixed: every check now opens a unique document (didOpen) instead of
     re-opening the same URI, and the client no longer waits on a silent
     server indefinitely (60 s read timeout).

PROBLEM: a program is reported as "(no errors)" but the saved file still
         has errors
  -> Older l1vm-lsp builds ran the real l1com compiler only on didOpen.
     Use the current l1vm-lsp (rebuilt from ~/develop/l1vm/l1vm-lsp), and
     make sure brackets-llm uses unique per-check URIs so every check runs
     the compiler. Also check that the l1com/l1pre binaries on PATH do not
     hang reading /dev/stdin.

PROBLEM: the model ignores Brackets and answers about the Brackets editor
  -> Make sure you are using a model that can follow the L1VM system prompt,
     and check that the server actually received the system prompt
     (the program reports the prompt file size at startup).
     For Qwen3.x thinking models brackets-llm already disables thinking.

PROBLEM: /build fails
  -> Look at build.txt in the working directory. Common causes: a missing
     l1vm-build.sh in $HOME/l1vm/bin, or the code uses an include that is not
     in $HOME/l1vm/include.

PROBLEM: the model never uses the file tools
  -> Make sure your model/server supports OpenAI function/tool calling
     (the program sends a "tools" array in the request). Qwen3 models
     support it. If your model only supports plain text, it will simply
     answer with code/text and file changes must be done with /write or /save.

PROBLEM: the model never uses web_fetch
  -> Same cause as above (tool calling support), or the model does not know
     the URL. Give it the URL, or ask a question that clearly needs the
     current documentation, e.g. "look up ... on the web first".

PROBLEM: web_fetch returns "error: ... https is not supported" / no TLS
  -> The build has no libcurl and no usable "curl" program. Install
     libcurl4-openssl-dev (Debian/Ubuntu), libcurl-devel (Fedora) or
     install the curl command line tool, then "make clean && make".
     To check what the build found, run:  make -n | tr ' ' '\n' | grep -i curl
     Plain http:// URLs work without any of this.

PROBLEM: web_fetch returns "denied: ..."
  -> The request was refused. Answer "y" at the "Allow this request?"
     prompt to allow it, or set BRACKETS_LLM_NET_CONFIRM=0 to stop asking
     (also needed when stdin is not a terminal).

PROBLEM: web_fetch returns "error: connection refused" / "unknown host"
  -> The URL host/port does not exist or is blocked (firewall, no route,
     proxy required). Check it yourself:  curl -v <url>

PROBLEM: web_fetch returns only a "note:" with a content type
  -> The document is binary (image, PDF, zip, ...). The program does not
     hand raw binary data to the model on purpose. Download it yourself
     and inspect the parts you need.


===============================================================================
 10. PROJECT LAYOUT
===============================================================================

    src/main.c        chat loop, agent tool loop, code extraction,
                      auto-correction, commands
    src/provider.h    the model backend interface: LlmConfig, LlmChat,
                      LlmReply, LlmProvider (the abstraction main.c uses)
    src/provider.c    backend factory, config defaults, base_url
                      normalization, history budget, /models listing
    src/provider_openai.c
                      the OpenAI-compatible REST core: request payload,
                      auth headers, JSON answer parsing, SSE streaming
                      (accumulates deltas back into an OpenAI-shaped answer)
    src/provider_llamacpp.c
                      the llama.cpp flavour of that core (thinking off,
                      no auth, no streaming)
    src/tools.c/h     autonomous tools the LLM can call: read_file,
                      write_file, edit_file, list_files, web_fetch
                      (incl. the HTML -> plain text conversion)
    src/config.h      configuration via environment variables
    src/http.c/h      HTTP client: raw POSIX sockets for http://, libcurl
                      for https:// (GET and POST, with or without streaming);
                      the curl program as a fallback for https GET; redirects,
                      chunked decoding, buffered POST and line-by-line POST
                      for streams; used for the LLM and for web_fetch
    src/lspclient.c/h L1VM LSP client (spawns l1vm-lsp, JSON-RPC over stdio)
    src/json.c/h      JSON value model / parser / emitter  (from the L1VM project)
    src/sb.c/h        string builder (from the L1VM project)
    src/inputline.c/h  input helper (readline when available, else fgets)
    src/intr.c/h      terminal control helpers
    l1vm-system-prompt.txt  the Brackets/L1VM system prompt sent to the LLM
    Makefile          build, clean, install; auto-detects readline and libcurl

License: GPL v3 (see the header comments in the source files, the JSON/SB
parts are from the L1VM project by Stefan Pietzonke).
