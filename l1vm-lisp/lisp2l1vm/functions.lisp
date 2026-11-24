; Compiler test: user-defined functions, recursion and higher-order calls.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(define double (lambda (x) (* x 2)))
(check "define lambda"  (= (double 21) 42))

(check "literal lambda" (= ((lambda (x y) (+ x y)) 3 4) 7))

(defun triple (x) (* x 3))
(check "defun"          (= (triple 14) 42))

(defun sum3 (a b c) (+ a b c))
(check "defun 3 args"   (= (sum3 1 2 3) 6))

(defun fact (n) (if (= n 0) 1 (* n (fact (- n 1)))))
(check "recursion"      (= (fact 5) 120))

(defun tail-sum (n acc) (if (= n 0) acc (tail-sum (- n 1) (+ acc n))))
(check "tail recursion" (= (tail-sum 10 0) 55))

(define (shorthand x) (+ x 1))
(check "define sugar"   (= (shorthand 41) 42))

(check "funcall"        (= (funcall #'double 21) 42))
(define mapped (mapcar #'double '(1 2 3)))
(check "mapcar"         (and (= (car mapped) 2)
                             (= (car (cdr mapped)) 4)
                             (= (car (cdr (cdr mapped))) 6)))

(defun even-n (n) (if (= n 0) T (odd-n (- n 1))))
(defun odd-n (n)  (if (= n 0) NIL (even-n (- n 1))))
(check "mutual rec"     (even-n 10))

(write "functions done")
(newline)
"PASS"
