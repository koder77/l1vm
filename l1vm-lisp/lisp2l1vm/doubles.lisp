; Compiler test: double literals, mixed arithmetic and conversions.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(check "literal"      (= 1.5 (+ 1.0 0.5)))
(check "add"          (= (+ 1.5 2.25) 3.75))
(check "mixed add"    (= (+ 1 2.5) 3.5))
(check "mul"          (= (* 2.5 4.0) 10.0))
(check "mixed div"    (= (/ 5 2.0) 2.5))
(check "mod"          (= (mod 7.5 2.0) 1.5))
(check "chain"        (= (+ 0.5 1.0 1.5) 3.0))
(check "eq"           (= 2.5 2.5))
(check "mixed eq"     (= 2 2.0))
(check "not equal"    (/= 1.5 2.5))
(check "chained cmp"  (not (< 1.0 3.0 2.0)))
(check "double?"      (double? 1.5))
(check "integer?"     (integer? 2))
(check "number?"      (number? 3.5))
(check "int->double"  (= (int->double 7) 7.0))
(check "double->int"  (= (double->int 7.9) 7))
(check "neg conv"     (= (double->int -2.7) -2))
(check "trailing dot" (= 5. 5.0))
(check "leading dot"  (= .5 0.5))
(check "exponent"     (= 1.5e2 150.0))

(write "doubles done")
(newline)
"PASS"
