; File handling tests: write, flush, read, EOF, close and append.
; The data file lives in the VM sandbox root, which is where the runner
; stages this program as well.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(define path "lisp-file-io-test.txt")

; --- output port ---
; write-string emits no newline; write-line appends one. The resulting
; bytes are: alpha!beta\n\ngamma\n
(define p (open-output-file path))
(check "open output" (file-port? p))
(check "write-string" (equal? (write-string "alpha" p) "alpha"))
(check "write-char" (= (write-char 33 p) 33))
(check "write-line" (equal? (write-line "beta" p) "beta"))
(check "write-char newline" (= (write-char 10 p) 10))
(check "write-string tail" (equal? (write-string "gamma" p) "gamma"))
(check "write final newline" (= (write-char 10 p) 10))
(check "flush-output" (eq? (flush-output) T))
(check "close output" (eq? (close-port p) T))

; --- input port, including EOF ---
(define q (open-input-file path))
(check "open input" (file-port? q))
(check "read line 1" (equal? (read-line q) "alpha!beta"))
(check "read line 2" (equal? (read-line q) ""))
(check "read line 3" (equal? (read-line q) "gamma"))
(check "read line eof" (eq? (read-line q) NIL))
(check "read char eof" (eq? (read-char q) NIL))
(check "close input" (eq? (close-port q) T))

; --- append mode ---
(define a (open-append-file path))
(check "open append" (file-port? a))
(check "append line" (equal? (write-line "delta" a) "delta"))
(check "close append" (eq? (close-port a) T))

(define r (open-input-file path))
(check "reread line 1" (equal? (read-line r) "alpha!beta"))
(check "reread line 2" (equal? (read-line r) ""))
(check "reread line 3" (equal? (read-line r) "gamma"))
(check "reread line 4" (equal? (read-line r) "delta"))
(check "reread eof" (eq? (read-line r) NIL))
(check "close reread" (eq? (close-port r) T))

(write "file done")
(newline)
"PASS"
