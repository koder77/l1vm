 SSL/TLS socket tests, client side.
; Started by ../run-ssl-test.sh together with server.lisp.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(define port 30012)
(define host "127.0.0.1")

(define client (open-ssl-client host port))
(check "open ssl client" (ssl-socket? client))
(check "ssl client is socket" (socket-port? client))

(check "ssl read-line" (equal? (socket-read-line client) "alpha"))
(check "ssl write-line" (= (socket-write-line client "beta") 0))
(check "ssl read-char" (= (socket-read-char client) 67))
(check "ssl write-char" (= (socket-write-char client 66) 0))

(check "close ssl client" (= (close-socket client) 0))
(check "closed is not ssl socket" (not (ssl-socket? client)))

(write "ssl client done")
(newline)
"PASS"
