; Runtime microbenchmark: tight loop with nested arithmetic.
; Also asserts the accumulated total so it works as a normal test.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(define i 0)
(define total 0)
(while (< i 1000000)
  (set! total (+ total i 1))
  (set! i (+ i 1)))
(check "accumulate 1e6" (= total 500000500000))

(write "benchm done")
(newline)
"PASS"
