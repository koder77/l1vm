; SSL/TLS socket tests, server side.
; Started by ../run-ssl-test.sh together with client.lisp.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(define port 30012)
(define cert "lisp-ssl-test.pem")

(define server (open-ssl-server cert port))
(check "open ssl server" (ssl-socket? server))
(check "ssl is socket" (socket-port? server))
(check "integer is not ssl socket" (not (ssl-socket? 42)))

(define conn (accept-ssl server))
(check "accept ssl" (ssl-socket? conn))

(check "ssl write-line" (= (socket-write-line conn "alpha") 0))
(check "ssl read-line" (equal? (socket-read-line conn) "beta"))
(check "ssl write-char" (= (socket-write-char conn 67) 0))
(check "ssl read-char" (= (socket-read-char conn) 66))

(check "close ssl connection" (= (close-socket conn) 0))
(check "close ssl server" (= (close-socket server) 0))
(check "closed is not ssl socket" (not (ssl-socket? server)))

(write "ssl server done")
(newline)
"PASS"
