; Compiler test: conditionals, bindings and iteration forms.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(check "if true"    (= (if T 42 0) 42))
(check "if false"   (= (if NIL 0 42) 42))
(check "if no else" (not (if NIL 42)))
(check "cond"       (= (cond ((= 1 2) 11) ((= 2 2) 42) (else 0)) 42))
(check "cond none"  (not (cond ((= 1 2) 11))))
(check "when"       (= (when T 42) 42))
(check "unless"     (= (unless NIL 42) 42))
(check "and"        (= (and 1 2 3) 3))
(check "not"        (not NIL))
(check "begin"      (= (begin 1 2 42) 42))
(check "let"        (= (let ((a 2) (b 3)) (+ a b)) 5))
(check "let*"       (= (let* ((a 2) (b (+ a 1))) (* a b)) 6))
(check "case"       (= (case 2 ((1) 10) ((2) 42) (otherwise 0)) 42))

(define counter 0)
(while (< counter 5)
  (set! counter (+ counter 1)))
(check "while"      (= counter 5))
(incf counter 2)
(check "incf"       (= counter 7))
(decf counter 1)
(check "decf"       (= counter 6))

(define total 0)
(dotimes (i 4)
  (set! total (+ total i)))
(check "dotimes"    (= total 6))

(define acc 0)
(do ((i 0 (1+ i))) ((>= i 4) acc)
  (set! acc (+ acc i)))
(check "do"         (= acc 6))

(write "control done")
(newline)
"PASS"
