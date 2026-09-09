===============================================================================
 brackets-llm - an opencode-like Brackets (L1VM) code environment
===============================================================================

Generated with opencode Big Pickle AI
License: GPL3

brackets-llm is a small program written in plain C (C11, no external
dependencies) that lets you generate L1VM "Brackets" programs with a local
LLM. It works like opencode:

  * A system prompt (l1vm-system-prompt.txt) is sent to the LLM at startup.
  * You chat with the LLM.
  * The LLM can autonomously use tools (read_file, write_file, list_files)
    to read and modify files on disk without you typing a slash command,
    just like opencode.
  * Brackets code found in the model's answer is saved to a .l1com file.
  * The code is checked with the L1VM language server (l1vm-lsp), which runs
    real compiler diagnostics (l1com), and errors are automatically corrected
    by asking the model to fix them (up to 4 iterations).
  * The program can build (l1vm-build.sh) and run (l1vm) the result.

The program does NOT start an LLM server. It connects to a llama.cpp
llama-server (or any OpenAI-compatible server) that YOU start with the model
of your choice.


===============================================================================
 1. REQUIREMENTS
===============================================================================

  * A Linux/Unix system with gcc/cc and make.
  * A running llama.cpp llama-server (https://github.com/ggml-org/llama.cpp)
    serving an OpenAI-compatible /v1/chat/completions endpoint, OR any other
    OpenAI-compatible server. Recommended context size: 65536.
  * The L1VM toolchain in ~/l1vm/bin/:
        - l1vm-lsp          (the L1VM language server)
        - l1com             (the Brackets compiler)
        - l1pre             (the preprocessor)
        - l1vm-build.sh     (build script)
        - l1vm              (the virtual machine to run programs)
    and the L1VM include directory ~/l1vm/include/.


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

Start llama-server yourself. Example with a 65k context:

    ~/llama.cpp/build/bin/llama-server \
        -m /path/to/your-model.gguf \
        --port 8080 \
        -c 65536 \
        --host 127.0.0.1

Wait until the log shows "server is listening on http://127.0.0.1:8080".

Find the exact model id your server reports (brackets-llm must send it):

    curl http://127.0.0.1:8080/v1/models

The model id is usually the full path to the .gguf file, e.g.:

    /home/you/llama-models/model.gguf

NOTE for Qwen3.x "thinking" models:
brackets-llm automatically disables thinking (sends
"chat_template_kwargs": {"enable_thinking": false}) so that the model
produces code instead of spending tokens on reasoning. If you use a
different model family, remove/adapt this option in src/main.c
(function llm_call).


===============================================================================
 4. CONFIGURATION (ENVIRONMENT VARIABLES)
===============================================================================

  BRACKETS_LLM_URL          Address of the LLM server.
                            Default: http://127.0.0.1:8080
  BRACKETS_LLM_MODEL        Model id as reported by the server.
                            Default: Qwen3.6-35B-A3B
                            (in practice you must set this to the id from
                            "curl http://127.0.0.1:8080/v1/models")
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

Example:

    export BRACKETS_LLM_MODEL="/home/you/llama-models/qwen-llm.gguf"
    export BRACKETS_LLM_URL="http://127.0.0.1:8080"


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


===============================================================================
 6. AUTONOMOUS TOOL USE (opencode-style)
===============================================================================

The model can use tools on its own, exactly like opencode. You do NOT have to
type a slash command. When you ask, for example:

    You> Look at hi.l1com and change the greeting to "Good morning".

the model may:
  1. call read_file to inspect the file,
  2. call write_file to modify it,
  3. and only then reply with a final text answer.

The program advertises these tools to the server in the chat request:

  read_file(path)    -> returns the file contents (truncated at ~128 KB).
  write_file(path, content)  -> creates or overwrites a file.
  list_files(dir)    -> lists the files/directories of a directory.

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

PROBLEM: "could not reach llama-server at http://127.0.0.1:8080"
  -> The server is not running or the URL/port is wrong. Start llama-server
     (section 3) or set BRACKETS_LLM_URL.

PROBLEM: "error: model not found" / unsupported model response
  -> BRACKETS_LLM_MODEL does not match the id reported by the server.
     Run:  curl http://127.0.0.1:8080/v1/models
     and export exactly that id.

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


===============================================================================
 10. PROJECT LAYOUT
===============================================================================

    src/main.c        chat loop, agent tool loop, code extraction,
                      auto-correction, commands
    src/tools.c/h     autonomous tools the LLM can call: read_file,
                      write_file, list_files
    src/config.h      configuration via environment variables
    src/http.c/h      minimal HTTP client (raw POSIX sockets) for the LLM
    src/lspclient.c/h L1VM LSP client (spawns l1vm-lsp, JSON-RPC over stdio)
    src/json.c/h      JSON value model / parser / emitter  (from the L1VM project)
    src/sb.c/h        string builder (from the L1VM project)
    l1vm-system-prompt.txt  the Brackets/L1VM system prompt sent to the LLM
    Makefile          build, clean, install

License: GPL v3 (see the header comments in the source files, the JSON/SB
parts are from the L1VM project by Stefan Pietzonke).
