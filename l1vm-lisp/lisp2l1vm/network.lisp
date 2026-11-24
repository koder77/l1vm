; TCP/IP socket tests using the compiler's net module builtins.
; The client connects before accept is called; listen() queues the connection.

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

(define host "127.0.0.1")
(define port 30010)

(define server (open-tcp-server host port))
(check "open tcp server" (socket-port? server))
(check "tcp server kind" (tcp-socket? server))
(check "not ssl server" (not (ssl-socket? server)))

(define client (open-tcp-client host port))
(check "open tcp client" (socket-port? client))

(define conn (accept-tcp server))
(check "accept tcp" (socket-port? conn))

(define open-port (lambda (p) (open-tcp-server host p)))
(define close-port (lambda (s) (close-socket s)))
(define extra (open-port 30011))
(check "open in function" (socket-port? extra))
(check "close in function" (= (close-port extra) 0))

(check "write line" (= (socket-write-line client "one") 0))
(check "server read line" (equal? (socket-read-line conn) "one"))
(check "write line two" (= (socket-write-line conn "two") 0))
(check "client read line" (equal? (socket-read-line client) "two"))

(check "write char" (= (socket-write-char conn 65) 0))
(check "read char" (= (socket-read-char client) 65))

(check "host address" (> (string-length (host-address "localhost")) 0))
(check "local host name" (> (string-length (local-host-name)) 0))

(check "close conn" (= (close-socket conn) 0))
(check "close client" (= (close-socket client) 0))
(check "close server" (= (close-socket server) 0))
(check "closed is not socket" (not (socket-port? server)))

(write "network done")
(newline)
"PASS"
