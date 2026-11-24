; Compiler test: quoted data, cons cells and list primitives.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(define p '(7 8 9))

(check "quote car"    (= (car p) 7))
(check "quote cadr"   (= (car (cdr p)) 8))
(check "quote caddr"  (= (car (cdr (cdr p))) 9))
(check "list car"     (= (car (list 1 2 3)) 1))
(check "cons"         (= (car (cons 42 nil)) 42))
(check "dotted cdr"   (= (cdr (cons 1 2)) 2))
(check "null nil"     (null? nil))
(check "null list"    (not (null? p)))
(check "pair list"    (pair? p))
(check "pair nil"     (not (pair? nil)))
(check "eq symbols"   (eq? 'abc 'abc))
(check "same list"    (equal? p p))
(check "equal fixnum" (equal? (car p) 7))
(check "nested car"   (= (car (car '((4 5) 6))) 4))
(check "cdr tail"     (null? (cdr (list 1))))

(write "lists done")
(newline)
"PASS"
