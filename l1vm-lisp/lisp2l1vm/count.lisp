; Count a loop from 0 up to 100000.
(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(define i 0)
(while (< i 100000)
  (incf i 1))
(check "count to 100000" (= i 100000))

(write "count done")
(newline)
"PASS"
