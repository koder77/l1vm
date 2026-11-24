; Compiler test: integer arithmetic and comparisons.
; Each check prints "ok   " or "FAIL"; the final value is PASS.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(check "add"       (= (+ 1 2 3) 6))
(check "sub"       (= (- 10 3 2) 5))
(check "mul"       (= (* 2 3 4) 24))
(check "div"       (= (/ 20 2 2) 5))
(check "mod"       (= (mod 17 5) 2))
(check "neg"       (= (+ -3 5) 2))
(check "nested"    (= (* (+ 1 2) (- 9 4)) 15))
(check "empty +"   (= (+) 0))
(check "empty -"   (= (-) 0))
(check "1+"        (= (1+ 41) 42))
(check "1-"        (= (1- 43) 42))
(check "max"       (= (max 3 9 4) 9))
(check "min"       (= (min 3 9 4) 3))
(check "abs"       (= (abs -42) 42))
(check "evenp"     (evenp 42))
(check "oddp"      (oddp 41))
(check "zerop"     (zerop 0))
(check "compare"   (and (< 1 2) (<= 2 2) (> 3 2) (>= 3 3) (/= 1 2)))
(check "chain <"   (not (< 1 3 2)))

(write "arithmetic done")
(newline)
"PASS"
