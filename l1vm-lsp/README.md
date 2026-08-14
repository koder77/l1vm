# l1vm-lsp

A lightweight Language Server for the **Brackets** language (L1VM), written in
plain C with **no external dependencies** beyond the C standard library. It
speaks JSON-RPC over stdio, so it works with any editor that has a generic LSP
client.

This was built with the help of opencode Big Pickle AI.
And is now in beta testing stage. If you find any bugs or other things to improve then let me know!

The LSP marks variables in the main function, if they have no ~ suffix ending.
This is not an error. So you should use ~ as suffix in main:

(main func)
    #var ~  main
    (set const-int64 1 zero 0)
    (set const-int64 1 one~ 1)
    
    ... etc!
    
Usage: after building the LSP wit make, put int into your "~/l1vm/bin" directory. 
So it is in the search path and can be executed by your editor.

## Features

- **Language analysis** — tokenizer and document model for Brackets source
  files (functions, objects, variables, labels, macros, includes).
- **Static diagnostics** — structural and semantic checks, published as you
  type.
- **Real compiler diagnostics** — optionally runs `l1com` on the current
  buffer and merges its errors/warnings into the diagnostics.
- **LSP requests**:
  - completion (builtins, keywords, functions, variables, labels)
  - hover
  - definition / references
  - documentSymbol
  - foldingRange
  - documentHighlight
  - signatureHelp
  - semanticTokens (full)
- **Incremental text sync** (`textDocumentSync.change = 2`).
- Works on unsaved buffers: when `l1com` is enabled, the buffer content is
  compiled from a temp copy next to the real file, so line numbers always
  match.

## Building

Requires a C11 compiler and `make` (tested with GCC).

```sh
make            # builds build/l1vm-lsp
make test       # runs tests/smoke.py (24 checks)
make clean
```

The build is kept clean under `gcc -std=c11 -Wall -Wextra -Werror` and runs
valgrind-clean (0 errors, 0 bytes lost).

## Configuration

The server registers the language id `l1com`. Settings are read from
`initializationOptions` on `initialize` and from `settings` on
`workspace/didChangeConfiguration`.

| Setting           | Type          | Default  | Description                                            |
| ----------------- | ------------- | -------- | ------------------------------------------------------ |
| `l1comEnabled`    | `"on"`/`"off"`/`"auto"` | `"auto"` | Run `l1com` for real diagnostics. `"auto"` uses it if found on `PATH`. |
| `l1comPath`       | string        | `l1com`  | Path to the `l1com` binary.                            |
| `includeDirs`     | string array  | none     | Directories searched for `<...>` library includes.     |
| `staticDiag`      | bool          | `true`   | Enable the built-in static diagnostics.                |
| `missingTildeHint`| bool          | `true`   | Hint when a global variable is used without `~`.       |

Include lookup order: directory of the opened file → `includeDirs` →
the `L1VM_INCLUDE_DIRS` environment variable (`:`-separated).

Example for the bundled L1VM layout:

```json
{
  "l1comEnabled": "auto",
  "l1comPath": "/home/you/l1vm/bin/l1com",
  "includeDirs": ["/home/you/l1vm/include"]
}
```

### Control flow syntax

Completions for the control flow keywords expand to these canonical Brackets
forms (Brackets 3.2.0, `f~` is an int64 flag variable, `cond` any comparison
that yields a flag):

```
// do-while (executed at least once)
(do)
    print_n (body)
(((loop maxloop <) f~ =) f~ while)

// for
(((loop maxloop <) f~ =) f~ for)
    print_n (body)
(next)

// if ... endif
(((x y <) f~ =) f~ if)
    ...
(endif)

// if+ ... else ... endif  (else requires if+)
(((x y <) f~ =) f~ if+)
    ...
(else)
    ...
(endif)
```

`l1com` only runs on the file you are editing. Programs that use
`#include <...>` need the full toolchain (`l1vm-build.sh`, which runs
`l1pre` then `l1com`); such files get static analysis from the server but
no line-accurate compiler diagnostics.

## Editor setup

### Doom Emacs (lsp-mode)

First make sure the `:tools lsp` module is enabled in `~/.doom.d/init.el`
(`(lsp)` under `:tools`), then run `doom sync`.

`lsp-register-client` must only run once `lsp-mode` is loaded. The robust
places are:

1. **Project `.lsp.el`** (preferred) — Doom's lsp module automatically loads a
   `.lsp.el` in the project root with `lsp-mode` available. Put this in
   `.lsp.el` at your project root:

```elisp
;; .lsp.el
(require 'lsp-mode)

(add-to-list 'lsp-language-id-configuration '(l1com-mode . "l1com"))
(add-hook 'l1com-mode-hook #'lsp-deferred)

(lsp-register-client
 (make-lsp-client
  :new-connection (lsp-stdio-connection "/path/to/build/l1vm-lsp")
  :major-modes '(l1com-mode)
  :language-id "l1com"
  :server-id 'l1vm-lsp
  :initialization-options
  '(:l1comEnabled "auto"
    :l1comPath "/home/you/l1vm/bin/l1com"
    :includeDirs ["/home/you/l1vm/include"])))
```

2. **`~/.doom.d/config.el`** — the complete setup below defines the mode,
   maps `l1com-mode` to the `l1com` language id, auto-starts the server in
   `.l1com` buffers, and adds `SPC c c` / `SPC c l` keybindings for
   `l1vm-build.sh` / `l1vm-build-lint.sh` with working error navigation:

```elisp
;; ~/.doom.d/config.el  (relevant excerpt)
(define-derived-mode l1com-mode prog-mode "l1com"
  "Major mode for L1VM Brackets source files."
  (setq-local comment-start "//"))

(add-to-list 'auto-mode-alist '("\\.l1com\\'" . l1com-mode))

;; start l1vm-lsp automatically in l1com-mode buffers
(add-hook 'l1com-mode-hook #'lsp-deferred)

(with-eval-after-load 'lsp-mode
  ;; without this mapping lsp-mode warns:
  ;; "Unable to calculate the languageId for buffer ..."
  (add-to-list 'lsp-language-id-configuration '(l1com-mode . "l1com"))
  (lsp-register-client
   (make-lsp-client
    :new-connection (lsp-stdio-connection "/path/to/build/l1vm-lsp")
    :major-modes '(l1com-mode)
    :language-id "l1com"
    :server-id 'l1vm-lsp
    :initialization-options
    '(:l1comEnabled "auto"
      :l1comPath "/home/you/l1vm/bin/l1com"
      :includeDirs ["/home/you/l1vm/include"]))))

;; --- L1VM build / lint toolchain (SPC c c / SPC c l) ---

(defvar l1vm-last-compiled-file nil)

(defun l1vm/run-build ()
  "Run the L1VM build script."
  (interactive)
  (let ((file-path (buffer-file-name)))
    (unless file-path
      (user-error "Buffer is not visiting a file"))
    (when (buffer-modified-p) (save-buffer))
    (setq l1vm-last-compiled-file file-path)
    (compile (format "l1vm-build.sh %s"
                     (file-name-sans-extension (file-name-nondirectory file-path))))))

(defun l1vm/run-lint ()
  "Run the L1VM linter script."
  (interactive)
  (let ((file-path (buffer-file-name)))
    (unless file-path
      (user-error "Buffer is not visiting a file"))
    (when (buffer-modified-p) (save-buffer))
    (setq l1vm-last-compiled-file file-path)
    (compile (format "l1vm-build-lint.sh %s"
                     (file-name-sans-extension (file-name-nondirectory file-path))))))

(map! :leader
      (:prefix-map ("c" . "code")
       :desc "Run L1VM Build" "c" #'l1vm/run-build
       :desc "Run L1VM Linter" "l" #'l1vm/run-lint))

;; error matcher for l1com ("error: line N") and l1vm-linter
;; ("linter error: line: N") output
(with-eval-after-load 'compile
  (add-to-list 'compilation-error-regexp-alist 'l1vm-error-matcher)
  (add-to-list 'compilation-error-regexp-alist-alist
               `(l1vm-error-matcher
                 "\\(?:\\(?:[Ee]rror\\|[Ww]arning\\): line:? \\)\\([0-9]+\\)"
                 ,(lambda () l1vm-last-compiled-file)
                 1)))
```

`l1com-mode` is defined at config load time, so it is already available when
`lsp-mode` finishes loading.

The mode derives from `prog-mode`, not from `c-mode`. If you use `yasnippet`,
exclude `l1com-mode` from it so the server's own `do`/`for`/`if+` snippet
completions are used instead of C-style blocks:

```elisp
(add-to-list 'yas-dont-activate 'l1com-mode)
```

If you see the harmless startup warning
`Vertico multiform must not be toggled from recursive minibuffers`, it comes
from Doom enabling the global `vertico-multiform-mode` inside the first
minibuffer. It is cosmetic and can be silenced with:

```elisp
(after! vertico-multiform
  (advice-add #'vertico-multiform-mode :around
              (lambda (orig &rest args)
                (let ((warning-minimum-level :error))
                  (apply orig args)))))
```

### Eglot (plain Emacs)

```elisp
(require 'eglot)

(define-derived-mode l1com-mode prog-mode "l1com"
  "Major mode for L1VM Brackets source files."
  (setq-local comment-start "//"))
(add-to-list 'auto-mode-alist '("\\.l1com\\'" . l1com-mode))
(add-to-list 'eglot-server-programs '(l1com-mode . "/path/to/build/l1vm-lsp"))
(add-hook 'l1com-mode-hook #'eglot-ensure)
```

### VS Code

VS Code cannot start an arbitrary server by itself; a minimal client extension
does the job. Create an extension directory with this `package.json`:

```json
{
  "name": "l1vm-lsp-client",
  "version": "0.1.0",
  "engines": { "vscode": "^1.85.0" },
  "activationEvents": ["onLanguage:l1com"],
  "main": "./extension.js",
  "contributes": {
    "languages": [{
      "id": "l1com",
      "extensions": [".l1com"],
      "configuration": "./language-configuration.json"
    }]
  }
}
```

`extension.js`:

```js
const vscode = require("vscode");
const cp = require("child_process");

exports.activate = function (context) {
  const server = cp.spawn("/path/to/build/l1vm-lsp", [], {
    stdio: ["pipe", "pipe", "pipe"],
  });
  const client = new (require("vscode-languageclient/node").LanguageClient)(
    "l1vm-lsp",
    "l1vm-lsp",
    { run: { server: { module: undefined, transport: 1 } } }, // stdio
    { documentSelector: [{ language: "l1com" }],
      initializationOptions: {
        l1comEnabled: "auto",
        l1comPath: "/home/you/l1vm/bin/l1com",
        includeDirs: ["/home/you/l1vm/include"],
      } }
  );
  // wire the spawned process to the LanguageClient:
  // client uses a stream; easiest is to provide the child process via
  // module.run kind 'fork' with a launcher script, or set the
  // LanguageClient's process options (see vscode-languageclient docs).
  context.subscriptions.push(client.start());
};
```

If you prefer, skip `cp.spawn` and point the client at a launcher script
(`exec /path/to/build/l1vm-lsp`) instead — that is the most reliable setup.

### Neovim (native LSP, 0.10+)

```lua
vim.filetype.add({ extension = { l1com = "l1com" } })

vim.lsp.config["l1vm"] = {
  cmd = { "/path/to/build/l1vm-lsp" },
  filetypes = { "l1com" },
  settings = {
    l1comEnabled = "auto",
    l1comPath = "/home/you/l1vm/bin/l1com",
    includeDirs = { "/home/you/l1vm/include" },
  },
}
vim.lsp.enable("l1vm")
```

### Kate

Enable the **LSP Client** plugin under `Settings → Configure Kate → Plugins`
(restart Kate if needed). Then add the server under
`Settings → Configure Kate → LSP Client → User Server Settings` — the same
file lives at `~/.config/kate/lspclient/settings.json` and can be edited
directly. Kate uses strict JSON (no comments, no trailing commas):

```json
{
  "servers": {
    "l1com": {
      "command": ["/path/to/build/l1vm-lsp"],
      "highlightingModeRegex": "^l1com$",
      "url": "https://github.com/koder77/l1vm/l1vm-lsp",
      "initializationOptions": {
        "l1comEnabled": "auto",
        "l1comPath": "/home/you/l1vm/bin/l1com",
        "includeDirs": ["/home/you/l1vm/include"]
      }
    }
  }
}
```

`highlightingModeRegex` must match Kate's highlighting mode for `.l1com`
files, so the bundled highlighter from the L1VM repo should be installed
first. Copy `syntax-highlighters/kate/l1com.xml` into
`~/.local/share/katepart5/syntax/` (newer Kate/KF6 builds also read
`~/.local/share/org.kde.syntax-highlighting/syntax/`):

```sh
mkdir -p ~/.local/share/katepart5/syntax
cp syntax-highlighters/kate/l1com.xml ~/.local/share/katepart5/syntax/
```

The mode is named `l1com`, so both the LSP language id and the regex match
out of the box. After adding the config, use
`Tools → LSP Client → Restart All LSP Servers`. Startup problems are logged
in the **Output** tab under "LSP Client Log".

Settings can also be overridden per project via a `.kateproject` file:

```json
{
  "lspclient": {
    "servers": {
      "l1com": {
        "settings": {
          "l1comEnabled": "auto",
          "l1comPath": "/home/you/l1vm/bin/l1com"
        }
      }
    }
  }
}
```

### Other editors

Any editor with a generic LSP client (Helix, Sublime Text LSP package, kak-lsp,
etc.) can connect: the server uses the standard stdio transport and the
language id `l1com`.

## Testing

```sh
make test
```

`tests/smoke.py` drives two full JSON-RPC sessions against the built binary:

1. `initialize` (full sync) → open `tests/sample.l1com` → checks diagnostics,
   documentSymbol, foldingRange, semanticTokens, completion, hover, definition,
   references, a `didChange`, an incremental range edit, and unknown-method
   error handling.
2. An l1com-enabled session: `tests/broken.l1com` must produce compiler
   diagnostics (parsed from `checkdef:`-prefixed output), `tests/valid.l1com`
   must produce none.

A manual session looks like this:

```sh
printf 'Content-Length: 59\r\n\r\n{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
  | ./build/l1vm-lsp
```

## Layout

```
src/l1lang.c   language analysis + LSP feature implementations
src/l1lang.h   document model, settings, API
src/lsp.c      JSON-RPC framing, LSP dispatch, diagnostics publishing
src/main.c     entry point
src/json.c     JSON value model, parser, emitter
src/sb.c       string builder
tests/         smoke test and sample files
```
