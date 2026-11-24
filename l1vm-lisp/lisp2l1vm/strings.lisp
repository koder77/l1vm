; Compiler test: string module builtins and string predicates.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(check "literal"      (= (string-length "hello") 5))
(check "empty"        (= (string-length "") 0))
(check "escaped tab"  (= (string-length "a\tb") 3))
(check "append two"   (= (string-length (string-append "ab" "cd")) 4))
(check "append three" (= (string-length (string-append "a" "b" "c")) 3))
(check "copy"         (= (string-length (string-copy "abcdef")) 6))
(check "left"         (equal? (string-left "abcdef" 3) "abc"))
(check "right"        (equal? (string-right "abcdef" 3) "def"))
(check "compare eq"   (= (string-compare "abc" "abc") 0))
(check "compare ne"   (not (= (string-compare "abc" "abd") 0)))
(check "int->string"  (equal? (int->string 42) "42"))
(check "double->string" (= (string-length (double->string 1.5)) 12))
(check "string?"      (string? "abc"))
(check "not string?"  (not (string? 42)))
(check "equal? str"   (equal? "abc" "abc"))
(check "not equal?"   (not (equal? "abc" "abd")))

(write "strings done")
(newline)
"PASS"
