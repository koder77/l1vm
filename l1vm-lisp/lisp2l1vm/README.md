# Lisp → L1VM compiler test programs

These `.lisp` files are written for the CLisp-based compiler
`../lisp2l1vm.lisp`.  They use the L1VM Lisp dialect (`define`, `lambda`,
`set!`, `begin`, `write`, `newline`, ...) and print `ok   <label>` or
`FAIL <label>` for every check.  The final Lisp value of every program is
`"PASS"`.

## Building

```sh
./build.sh              # compile every .lisp to .l1com with clisp
./build.sh --vm         # also assemble the .l1com files with l1vm-build.sh
./build.sh --run        # build and run each program through l1vm
```

The compiler itself is the local `../lisp2l1vm.lisp`; nothing is downloaded.
`CLISP`, `L1VM_BUILD`, `L1VM`, `L1VM_MODULES`, `L1VM_SANDBOX` and
`LD_LIBRARY_PATH` can be used to override tool locations and the VM sandbox
root.

Socket builtins are compiled to calls into the local `net-lib.l1h` module,
exactly like string and math builtins use their own modules.  Socket handles
are int64 values (`>= 0` open, `-1` on error).  The compiler emits a small
socket-kind table so `close-socket`, `socket-port?`, `tcp-socket?` and
`ssl-socket?` work without exposing L1VM's six separate close functions.

## Files

| File | Covers |
| --- | --- |
| `arithmetic.lisp` | `+ - * / mod`, `1+ 1-`, `min max abs`, `evenp oddp zerop`, comparisons, chained comparisons |
| `control.lisp` | `if`, `cond`, `when`, `unless`, `and`, `not`, `begin`, `let`, `let*`, `case`, `while`, `dotimes`, `do`, `set!`, `incf`, `decf` |
| `count.lisp` | `while` loop counting from 0 to 100000 |
| `functions.lisp` | `define`/`lambda`, `defun`, literal lambdas, recursion, tail recursion, mutual recursion, `funcall`, `mapcar`, `define` shorthand |
| `lists.lisp` | quoted data, `cons`, `car`, `cdr`, `list`, dotted pairs, `null?`, `pair?`, `eq?`, `equal?` |
| `strings.lisp` | string literals and escapes, `string-length`, `string-append`, `string-copy`, `string-left`, `string-right`, `string-compare`, `int->string`, `double->string`, string predicates |
| `doubles.lisp` | double literals and number syntax, mixed arithmetic, comparisons, type predicates, `int->double`, `double->int` |
| `network.lisp` | TCP loopback sockets: `open-tcp-server`, `open-tcp-client`, `accept-tcp`, line/byte I/O, `close-socket`, socket predicates, `host-address`, `local-host-name` |

All seven programs compile, assemble, and run with every check passing.

The generated compiler only emits the Lisp-data runtime functions a program
actually references, so the list test no longer carries the unused evaluator
and Lisp printer.

`assert`-style checks that compare freshly allocated lists structurally
(`equal?` on two separately quoted lists) are intentionally avoided: the
compiler compares list pointers, not list contents.  The `mapcar` check in
`functions.lisp` therefore compares the list elements instead.

## SSL/TLS test

`ssl/server.lisp` and `ssl/client.lisp` block in `accept`/`connect`, so they
are run as two VM processes and are kept out of the `build.sh` glob.  Run them
with:

```sh
./run-ssl-test.sh
```

The runner stages `../tests/ssl/ssl-cert.pem` as `lisp-ssl-test.pem` in the
VM sandbox root (default `$HOME/l1vm`) and removes it afterwards.  It covers
`open-ssl-server`, `open-ssl-client`, `accept-ssl`, line/byte I/O and
`close-socket`; both logs must contain only `ok` checks and a `done` line.
