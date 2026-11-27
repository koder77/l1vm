;;;; lisp2l1vm.lisp -- A Lisp -> L1VM Brackets compiler in Common Lisp
;;;;
;;;; Usage:
;;;;   clisp lisp2l1vm.lisp input.lisp output.l1com
;;;;
;;;; The compiler reads the Lisp dialect used by the L1VM Lisp interpreter
;;;; (DEFINE/LAMBDA, SET!, BEGIN, WRITE, NEWLINE, WHILE, ...), translates it
;;;; to the internal Common Lisp form used by this compiler, and emits L1VM
;;;; Brackets source (.l1com) that can be built with l1vm-build.sh.
;;;;
;;;; The source language is Lisp data, so the compiler can walk, transform
;;;; and generate programs with ordinary list operations.
;;;;
;;;; Syntax and compile errors are reported as FILE:LINE: KIND: MESSAGE.

;;;; Developed with the help of open code Big Pickle AI
;;;; Checked with tests.

(defpackage #:cl2brackets
  (:use #:cl)
  (:export #:compile-source-file #:compile-source-string))
(in-package #:cl2brackets)

;; Source forms are read in this package.  Keeping them separate from the
;; compiler package means user DEFMACRO definitions cannot clobber compiler
;; internals.
(defpackage #:cl2brackets-user
  (:use #:cl))

;;; ---------------------------------------------------------------------------
;;; Global compiler state
;;; ---------------------------------------------------------------------------

(defvar *functions*   (make-hash-table :test 'eq))   ; name -> plist
(defvar *function-order* nil)                        ; names in source order
(defvar *globals*     (make-hash-table :test 'eq))   ; sym -> (bname . type)
(defvar *implicit-globals* nil)                      ; SETQ-created globals, in order
(defvar *literals*    (make-hash-table :test 'equal)); (type value) -> bname
(defvar *out*         nil)   ; emitted lines, reversed
;; Literals are declared by name at assembly time.  Keeping the declaration
;; text in a table lets the emitter drop literals no generated code uses
;; (for example the reserved NIL/T symbol names in non-Lisp programs).
(defvar *literal-decls* (make-hash-table :test 'equal))
(defvar *literal-order* nil) ; names in declaration order
;; A call used only for its side effects still has to pop its return value.
;; Reuse one scratch variable per type instead of allocating a fresh temporary
;; whose value is never read.
(defvar *discard-value* nil)
(defvar *call-scratch* nil)  ; type -> scratch variable, per function
(defvar *cur-fun*     nil)
(defvar *vars*        nil)   ; alist: sym -> (bname . type)
(defvar *used-names*  nil)   ; base names already used in current function
(defvar *temp-count*  0)
(defvar *lit-count*   0)
(defvar *label-count* 0)
(defvar *main-code* nil)
(defvar *func-code* nil)
(defvar *defined-macros* nil)                ; macros from the current source
(defvar *user-package* (find-package "CL2BRACKETS-USER"))
(defvar *lambda-env* nil)                    ; let-bound lambdas, compile-time only
(defvar *local-macros* nil)                  ; MACROLET definitions

;; Source locations for error messages.  *FORM-LINE-TABLE* maps a top-level
;; form to its source line; *CURRENT-SOURCE-LINE* is rebound while that form is
;; being translated or compiled.
(defvar *form-line-table* nil)
(defvar *current-source-line* nil)
(defvar *source-file* "<string>")
(defvar *source-text* nil)

;; Tail-call lowering.  *TAIL-POSITION* is true while compiling an expression
;; whose value is returned directly from the current function.  A self call in
;; that position can reuse the current frame: evaluate the new arguments,
;; overwrite the parameter slots and jump to the function body label.  Unlike
;; the ordinary recursive-call path this needs no VM-stack frame at all.
(defvar *tail-position* nil)

;; Per-call frame liveness.  Each recursive call site records the variables
;; that are still read after the call; only those are saved in its frame.
(defvar *rec-size-cmp-var* nil)              ; scratch for frame-size comparisons

;; Recursion lowering: self-recursive calls are compiled into an explicit
;; VM-stack frame.  The VM stack, rather than Lisp-level recursion, holds the
;; saved locals and a continuation id.  Each recursive function gets private
;; depth/result globals.
(defvar *current-recursive* nil)
(defvar *rec-tracking* nil)
(defvar *rec-vars* nil)
(defvar *rec-cont-count* 0)
(defvar *rec-cont-ids* nil)
(defvar *rec-sp-var* nil)
(defvar *rec-tmp-var* nil)
(defvar *rec-cont-var* nil)
(defvar *rec-decls* nil)

;; Mutual recursion.  Functions in the same strongly connected component share
;; a depth global and a generated return dispatcher.  Continuation ids and
;; their labels are program-global so any function can return to any caller.
(defvar *rec-group-of* nil)          ; function -> group id (or NIL)
(defvar *rec-depth-name* nil)        ; function -> shared depth global name
(defvar *rec-depth-declared* nil)    ; depth global names already declared
(defvar *rec-cont-frames* nil)       ; (id . ((mangled-var . type)...)) globally

;; Runtime Lisp-data support
(defvar *lisp-symbols* (make-hash-table :test 'equal)) ; name -> id
(defvar *lisp-symbol-names* nil)                      ; id -> literal name
(defvar *lisp-strings* (make-hash-table :test 'equal)) ; text -> id
(defvar *string-module-needed* nil)  ; true when generated code calls string.l1h
(defvar *math-module-needed* nil)    ; true when generated code calls math-lib.l1h
(defvar *net-module-needed* nil)     ; true when generated code calls net-lib.l1h
(defvar *net-helpers-emitted* nil)   ; socket-kind/close helpers already appended
(defvar *file-module-needed* nil)    ; true when generated code calls file-lib.l1h
(defvar *file-helpers-emitted* nil)  ; file port/IO helpers already appended
(defvar *sdl-module-needed* nil)     ; true when generated code calls sdl-3-lib.l1h
(defvar *sdl-helpers-emitted* nil)   ; SDL multi-return helpers already appended
(defconstant +string-buffer-size+ 4096) ; room for copied string arguments
;; The string module owns slot 0, the math module owns slot 1, the net module
;; owns slot 2, the file module owns slot 3, and the SDL module owns slot 4.
;; Separate slots let a program load any combination of these modules at the
;; same time.
(defconstant +net-module-slot+ 2)
(defconstant +net-max-sockets+ 8)
(defconstant +file-module-slot+ 3)
(defconstant +file-max-files+ 8)
(defconstant +file-line-pool+ 16)
(defconstant +file-line-size+ 4096)
(defconstant +sdl-module-slot+ 4)
(defvar *lisp-string-texts* nil)                      ; id -> literal name
(defvar *lisp-symbol-count* 0)
(defvar *lisp-string-count* 0)
(defvar *runtime-code* nil)                          ; support functions
(defvar *runtime-needed* nil)
(defvar *runtime-fns* nil)                           ; requested support functions

(defun emit (s) (push s *out*))
(defun emitf (fmt &rest args) (emit (apply #'format nil fmt args)))

(defun reset-state ()
  ;; User macros live in CL2BRACKETS-USER.  Undefine them so one compilation
  ;; cannot leak macros into the next one.
  (dolist (name *defined-macros*)
    (when (and (symbolp name) (macro-function name))
      (fmakunbound name)))
  (setf *defined-macros* nil)
  (clrhash *functions*)
  (clrhash *globals*)
  (setf *implicit-globals* nil)
  (clrhash *literals*)
  (clrhash *lisp-symbols*)
  (clrhash *lisp-strings*)
  (setf *function-order* nil *lambda-env* nil *local-macros* nil
        *literal-order* nil)
  (clrhash *literal-decls*)
  (setf *lit-count* 0 *label-count* 0
        *lisp-symbol-count* 0 *lisp-string-count* 0
        *lisp-symbol-names* nil *lisp-string-texts* nil
        *runtime-code* nil *runtime-needed* nil *runtime-fns* nil
        *string-module-needed* nil
        *math-module-needed* nil
        *net-module-needed* nil *net-helpers-emitted* nil
        *file-module-needed* nil *file-helpers-emitted* nil
        *sdl-module-needed* nil *sdl-helpers-emitted* nil
        *current-recursive* nil *rec-tracking* nil *rec-vars* nil
        *rec-cont-count* 0 *rec-cont-ids* nil
        *rec-sp-var* nil *rec-tmp-var* nil *rec-cont-var* nil
        *rec-decls* nil *tail-position* nil *rec-size-cmp-var* nil
        *rec-group-of* (make-hash-table :test 'eq)
        *rec-depth-name* (make-hash-table :test 'eq)
        *rec-depth-declared* (make-hash-table :test 'equal)
        *rec-cont-frames* nil
        *call-scratch* (make-hash-table)
        *discard-value* nil))

;;; ---------------------------------------------------------------------------
;;; Types
;;; ---------------------------------------------------------------------------
;;; Internal types: :int, :double, :byte, :string, :lisp.
;;; :bool values are represented as int64 in Brackets, as are :string
;;; addresses and :lisp tagged fixnums.  :byte uses the byte stack opcodes.

(defun op= (op name)
  (and (symbolp op) (string-equal (symbol-name op) name)))

(defun safe-name (sym)
  "Turn a Common Lisp symbol name into a safe Brackets identifier."
  (let ((out nil))
    (loop for ch across (string-downcase (string sym))
          do (cond ((alphanumericp ch) (push ch out))
                   ((char= ch #\*) (push #\s out))
                   ((char= ch #\-) (push #\_ out))
                   ((char= ch #\?) (push #\p out))
                   ((char= ch #\!) (push #\b out))
                   (t (push #\_ out))))
    (coerce (nreverse out) 'string)))

(defun normalize-type (type)
  (cond ((keywordp type) type)
        ((symbolp type)
         (let ((name (string-downcase (symbol-name type))))
           (cond ((string= name "double") :double)
                 ((string= name "float") :double)
                 ((string= name "byte") :byte)
                 ((string= name "string") :string)
                 ((string= name "lisp") :lisp)
                 (t :int))))
        (t :int)))

(defun type->bracket-type (type)
  (case (normalize-type type)
    (:double "double")
    (:byte   "byte")
    (:string "string")
    (t       "int64")))

(defun storage-type (type)
  "Brackets declaration type used for values of TYPE.
   A :string value is an int64 address of a string buffer; the buffers
   themselves are only created by ENSURE-LITERAL and string materialization."
  (if (eq (normalize-type type) :string) "int64" (type->bracket-type type)))

;;; The complete Brackets stack opcode family.  Keep the full names here
;;; instead of assembling them from suffixes: generated code must be able to
;;; push/pop int64, double and byte values, plus the generic forms.
(defparameter *stack-opcode-table*
  '((:int    "stpushi" "stpopi")
    (:bool   "stpushi" "stpopi")
    (:string "stpushi" "stpopi")   ; string values are int64 buffer addresses
    (:lisp   "stpushi" "stpopi")   ; tagged fixnums travel as int64
    (:double "stpushd" "stpopd")
    (:byte   "stpushb" "stpopb")
    (:t      "stpush"  "stpop")))  ; fallback for untyped operands

(defun stack-opcode-pair (type)
  (or (assoc (normalize-type type) *stack-opcode-table*)
      (assoc :t *stack-opcode-table*)))

(defun stack-push-opcode (type)
  "Full Brackets push opcode for TYPE (STPUSHI/STPUSHD/STPUSHB/STPUSH)."
  (second (stack-opcode-pair type)))

(defun stack-pop-opcode (type)
  "Full Brackets pop opcode for TYPE (STPOPI/STPOPD/STPOPB/STPOP)."
  (third (stack-opcode-pair type)))

(defun push-suffix (type)
  "Compatible suffix form of STACK-PUSH-OPCODE."
  (let ((op (stack-push-opcode type)))
    (if (string= op "stpush") "" (subseq op 6))))

(defun literal-text (value type)
  (case type
    (:double (format nil "~a" (if (floatp value) value (float value))))
    (:string (format nil "\"~a\"" (substitute #\space #\newline (string value))))
    (:byte   (format nil "~a" value))
    (t       (format nil "~a" value))))

(defun ensure-literal (value type)
  "Register a literal and return its Brackets global name."
  (let* ((key (list type (if (stringp value) value (format nil "~s" value))))
         (found (gethash key *literals*)))
    (if found
        found
        (let ((name (format nil "lit~a" (incf *lit-count*))))
          (setf (gethash key *literals*) name)
          (setf (gethash name *literal-decls*)
                (format nil "    (set ~a ~a ~a ~a)"
                        (type->bracket-type type)
                        (if (eq type :string) "s" "1")
                        name (literal-text value type)))
          (push name *literal-order*)
          name))))

;;; ---------------------------------------------------------------------------
;;; Tagged Lisp values used by the generated runtime
;;; ---------------------------------------------------------------------------
;;; Tags are the low three bits of an int64:
;;;   0 fixnum (payload = value * 8)
;;;   1 cons   (payload = heap cell index)
;;;   2 symbol (payload = symbol id)
;;;   3 string (payload = string id)
;;;   4 closure(payload = closure id)
;;; NIL is symbol 0, so its tagged value is 2.

(defparameter +tag-shift+ 8)
(defparameter +tag-fix+ 0)
(defparameter +tag-cons+ 1)
(defparameter +tag-sym+ 2)
(defparameter +tag-str+ 3)
(defparameter +tag-closure+ 4)
(defparameter +nil-value+ 2)

(defun need-runtime (&optional (what t))
  (setf *runtime-needed* (or *runtime-needed* what)))

;; A generated program rarely needs the whole Lisp-data runtime.  For example,
;; plain list operations never use the S-expression evaluator or the Lisp
;; printer.  Track exactly which support functions are referenced so the
;; output only carries those functions (and their dependencies).
(defparameter *runtime-deps*
  '(("l1_print"      "l1_print_atom" "l1_car" "l1_cdr")
    ("l1_print_atom")
    ("l1_eval"       "l1_consp" "l1_car" "l1_cdr" "l1_null" "l1_cons")
    ("l1_cons")
    ("l1_car")
    ("l1_cdr")
    ("l1_null")
    ("l1_consp")))

(defun need-runtime-fn (name)
  (setf *runtime-needed* t)
  (dolist (fn (cons name (cdr (assoc name *runtime-deps* :test #'string=))))
    (pushnew fn *runtime-fns* :test #'string=)))

(defun runtime-fn-needed-p (name)
  (member name *runtime-fns* :test #'string=))

(declaim (ftype function compile-expr fresh-temp))
(declaim (ftype function compile-tagged-arg))
(declaim (ftype function rec-declaration-line-p))

(defun lisp-symbol-id (sym)
  (let* ((name (string-downcase (string sym)))
         (found (gethash name *lisp-symbols*)))
    (if found
        found
        (let ((id (incf *lisp-symbol-count*)))
          (setf (gethash name *lisp-symbols*) id)
          ;; Reserve a printable literal for the symbol name.
          (push (cons id (ensure-literal name :string)) *lisp-symbol-names*)
          id))))

(defun lisp-string-id (text)
  (let ((found (gethash text *lisp-strings*)))
    (if found
        found
        (let ((id (incf *lisp-string-count*)))
          (setf (gethash text *lisp-strings*) id)
          (push (cons id (ensure-literal text :string)) *lisp-string-texts*)
          id))))

(defun tag-fix (n) (* n +tag-shift+))
(defun tag-cons (n) (+ (* n +tag-shift+) +tag-cons+))
(defun tag-sym (n) (+ (* n +tag-shift+) +tag-sym+))
(defun tag-str (n) (+ (* n +tag-shift+) +tag-str+))

(defun ensure-fixed-symbols ()
  ;; IDs 0 and 1 are fixed so NIL/T have stable values.
  (unless (gethash "nil" *lisp-symbols*)
    (setf (gethash "nil" *lisp-symbols*) 0)
    (push (cons 0 (ensure-literal "NIL" :string)) *lisp-symbol-names*)
    (incf *lisp-symbol-count*))
  (unless (gethash "t" *lisp-symbols*)
    (setf (gethash "t" *lisp-symbols*) 1)
    (push (cons 1 (ensure-literal "T" :string)) *lisp-symbol-names*)
    (incf *lisp-symbol-count*)))

(defun ensure-eval-symbols ()
  ;; The evaluator dispatches on these builtin symbol ids.  Programs that do
  ;; not call EVAL do not need their printable literals in the output.
  (dolist (s '(+ - * / = /= < > <= >= if cond quote cons car cdr list
               null consp atom not progn begin setq let lambda funcall apply
               print print-lisp eval first rest = eq eql equal else))
    (lisp-symbol-id s)))

(defun ensure-special-symbols ()
  (ensure-fixed-symbols)
  (ensure-eval-symbols))

(defun emit-call-result (res rtype)
  "Emit the pop of a call's return value, then reset l1com's register tracking.

   L1COM saves caller registers on the VM stack around a function call, but
   it keeps their names in its register map.  An expression immediately
   following the call can therefore be compiled using a stale register
   before L1COM gets around to restoring the saved values, which corrupts
   the result (this showed up as broken MIN, INCF and DECF after a call to
   the CHECK helper).  RESET-REG clears the register map so the next
   expression reloads its operands from memory."
  (emitf "    (~a ~a)" res (stack-pop-opcode rtype))
  (emit "    (reset-reg)"))

(defun emit-support-call-values (name vals rtype)
  "Call NAME with already-compiled (variable . type) pairs.
   Support functions pop their arguments in reverse CL order, so arguments
   are pushed left-to-right here."
  (when (and (> (length name) 3) (string= name "l1_" :end1 3))
    (need-runtime-fn name))
  (if vals
      (emitf "    (~{~a~^ ~} :~a !)" (mapcar #'car vals) name)
      (emitf "    (:~a !)" name))
  (if rtype
      (let ((res (fresh-temp rtype)))
        (emit-call-result res rtype)
        (values res rtype))
      (values "zero" :int)))

(defun emit-support-call (name args rtype)
  "Call a generated runtime function, compiling ARG expressions first."
  (let ((vals nil))
    (dolist (a args)
      (multiple-value-bind (n ty) (compile-expr a)
        (push (list n ty) vals)))
    (emit-support-call-values name (reverse vals) rtype)))

;;; ---------------------------------------------------------------------------
;;; Variables
;;; ---------------------------------------------------------------------------

(defun lookup-var (sym) (assoc sym *vars*))

(defun var-info (sym)
  (let ((v (lookup-var sym)))
    (if v
        (cdr v)
        (multiple-value-bind (g present) (gethash sym *globals*)
          (if present g (error "Undefined variable: ~a" sym))))))

(defun var-name (sym) (car (var-info sym)))
(defun var-type (sym) (cdr (var-info sym)))

(defun register-global (sym type)
  "Register SYM as a Brackets global unless it is already known."
  (unless (gethash sym *globals*)
    (setf (gethash sym *globals*)
          (cons (safe-name sym) (normalize-type type)))))

(defun register-implicit-global (sym type)
  "Make a SETQ to an otherwise undeclared symbol create a global variable."
  (unless (gethash sym *globals*)
    (register-global sym type)
    (pushnew sym *implicit-globals*)))

(defun unique-base (sym)
  (let ((base (safe-name sym)))
    (cond ((not (member base *used-names* :test #'string=))
           (push base *used-names*) base)
          (t
           (let ((n 2))
             (loop while (member (format nil "~a~a" base n) *used-names*
                                 :test #'string=)
                   do (incf n))
             (let ((name (format nil "~a~a" base n)))
               (push name *used-names*) name))))))

(defun declare-var (name type &optional init)
  (when *rec-tracking*
    (pushnew (cons name (normalize-type type)) *rec-vars*
             :test #'string= :key #'car))
  (emitf "    (set ~a 1 ~a ~a)"
         (storage-type type)
         name (or init (if (eq (normalize-type type) :double) "0.0" "0"))))

(defun bind-variable (sym type &optional init)
  (let ((name (concatenate 'string (unique-base sym) "~")))
    (declare-var name type init)
    (push (cons sym (cons name type)) *vars*)
    name))

(defun fresh-temp (type &optional (prefix "t") init)
  (let ((name (format nil "~a~a~~" prefix (incf *temp-count*))))
    (declare-var name type init)
    name))

(defun fresh-string-buffer ()
  "Allocate a fixed-size Brackets string buffer for materializing a value."
  (let ((name (format nil "sb~a~~" (incf *temp-count*))))
    (push name *used-names*)
    (setf *string-module-needed* t)
    (emitf "    (set string ~a ~a \"\")" +string-buffer-size+ name)
    name))

(defun literal-string-name (name)
  "If NAME is the generated address variable of a string literal, return
   the literal's string-variable name.  Literals are already proper Brackets
   string variables, so they can be printed/compared directly; only values
   held as int64 addresses need a copy into a string buffer."
  (when (and (stringp name) (>= (length name) 8)
             (string= "lit" name :end2 3)
             (string= "addr" name :start2 (- (length name) 4)))
    (let* ((base (subseq name 0 (- (length name) 4)))
           (digits (subseq name 3 (- (length name) 4))))
      (when (and (plusp (length digits))
                 (every #'digit-char-p digits)
                 (gethash base *literal-decls*))
        base))))

(defun printable-string (name type)
  "Return an operand suitable for :print_s/:print_lns.
   A :string value held as an int64 address is copied into a local buffer;
   a literal string variable is used directly."
  (if (eq (normalize-type type) :string)
      (or (literal-string-name name)
          (let ((buf (fresh-string-buffer)))
            (emitf "    (~a ~a :string_copy !)" buf name)
            buf))
      name))

(defun string-buffer-address (buf)
  "Return an int64 variable holding the address of string buffer BUF.
   Brackets requires POINTER destinations to start with an uppercase P."
  (let ((p (fresh-temp :int "Pstr")))
    (emitf "    (~a ~a pointer)" buf p)
    p))

(defparameter *string-builtin-ops*
  '("string-append" "concat" "string-cat" "string-length" "string-len"
    "string-copy" "string-left" "string-right" "string-mid"
    "string-mid->byte" "int->string" "byte->hexstring" "double->string"
    "byte->string" "string-compare" "string-search" "string-regex"
    "string-verify" "string-format-num" "string-padd" "get-env" "set-env"))

(defun string-builtin-op-p (op)
  (some (lambda (name) (op= op name)) *string-builtin-ops*))

(defun discard-scratch (type)
  "Return a reusable scratch variable for popping a discarded call result."
  (let* ((ty (normalize-type type))
         (name (gethash ty *call-scratch*)))
    (or name
        (setf (gethash ty *call-scratch*) (fresh-temp ty "discard")))))

(defun fresh-byte-temp (&optional init)
  (fresh-temp :byte "by" init))

(defun compile-string-builtin (op args)
  "Compile L1VM Lisp string builtins to Brackets string module calls."
  (setf *string-module-needed* t)
  (let ((vals nil))
    (dolist (a args)
      (multiple-value-bind (n ty) (compile-expr a)
        (push (list n (normalize-type ty)) vals)))
    (setf vals (nreverse vals))
    (flet ((require-count (n)
             (unless (= (length vals) n)
               (error "~a expects ~a arguments" op n)))
           (require-str (v)
             (unless (eq (second v) :string)
               (error "~a expects a string argument" op))
             (first v))
           (require-int (v)
             (unless (eq (second v) :int)
               (error "~a expects an integer argument" op))
             (first v))
           (require-double (v)
             (unless (eq (second v) :double)
               (error "~a expects a double argument" op))
             (first v))
           (string-result (buf)
             (values (string-buffer-address buf) :string))
           (int-module-call (module-name operands)
             (let ((res (fresh-temp :int)))
               (emitf "    (~{~a~^ ~} :~a !)" operands module-name)
               (emit-call-result res :int)
               (values res :int))))
      (cond
        ((or (op= op "string-append") (op= op "concat") (op= op "string-cat"))
         (unless (>= (length vals) 2)
           (error "~a needs at least two strings" op))
         (let ((buf (fresh-string-buffer)))
           (emitf "    (~a ~a :string_copy !)" buf (require-str (first vals)))
           (dolist (v (rest vals))
             (emitf "    (~a ~a :string_cat !)" buf (require-str v)))
           (string-result buf)))
        ((or (op= op "string-length") (op= op "string-len"))
         (require-count 1)
         (int-module-call "string_len" (list (require-str (first vals)))))
        ((op= op "string-copy")
         (require-count 1)
         (let ((buf (fresh-string-buffer)))
           (emitf "    (~a ~a :string_copy !)" buf (require-str (first vals)))
           (string-result buf)))
        ((or (op= op "string-left") (op= op "string-right") (op= op "string-mid"))
         (require-count 2)
         (let* ((buf (fresh-string-buffer))
                (addr (string-buffer-address buf))
                (module-name (cond ((op= op "string-left") "string_left")
                                   ((op= op "string-right") "string_right")
                                   (t "string_mid"))))
           (emitf "    (~a ~a ~a :~a !)"
                  (require-str (first vals)) addr
                  (require-int (second vals)) module-name)
           (values addr :string)))
        ((op= op "string-mid->byte")
         (require-count 2)
         (setf *math-module-needed* t)
         (let ((byte (fresh-byte-temp))
               (res (fresh-temp :int)))
           (emitf "    (~a ~a :string_mid_to_byte !)"
                  (require-str (first vals)) (require-int (second vals)))
           (emitf "    (~a ~a)" byte (stack-pop-opcode :byte))
           (emit "    (reset-reg)")
           (emitf "    (~a :math_byte2int !)" byte)
           (emit-call-result res :int)
           (values res :int)))
        ((op= op "int->string")
         (require-count 1)
         (let* ((buf (fresh-string-buffer))
                (addr (string-buffer-address buf))
                (len (ensure-literal 32 :int)))
           (emitf "    (~a ~a ~a :string_int64tostring !)"
                  (require-int (first vals)) addr len)
           (values addr :string)))
        ((op= op "byte->hexstring")
         (require-count 1)
         (let* ((buf (fresh-string-buffer))
                (addr (string-buffer-address buf))
                (len (ensure-literal 3 :int)))
           (emitf "    (~a ~a ~a :string_bytetohexstring !)"
                  (require-int (first vals)) addr len)
           (values addr :string)))
        ((op= op "double->string")
         (require-count 1)
         (let* ((buf (fresh-string-buffer))
                (addr (string-buffer-address buf))
                (len (ensure-literal 32 :int)))
           (emitf "    (~a ~a ~a :string_doubletostring !)"
                  (require-double (first vals)) addr len)
           (values addr :string)))
        ((op= op "byte->string")
         (require-count 1)
         (let* ((buf (fresh-string-buffer))
                (addr (string-buffer-address buf)))
           ;; string_bytenumtostring accepts the byte as an int64; the
           ;; string_bytetostring wrapper expects a typed byte stack value.
           (emitf "    (~a ~a :string_bytenumtostring !)"
                  (require-int (first vals)) addr)
           (values addr :string)))
        ((or (op= op "string-compare") (op= op "string-search")
             (op= op "string-regex") (op= op "string-verify"))
         (require-count 2)
         (let ((module-name (cond ((op= op "string-compare") "string_compare")
                                  ((op= op "string-search") "string_search")
                                  ((op= op "string-regex") "string_regex")
                                  (t "string_verify"))))
           (int-module-call module-name
                            (list (require-str (first vals))
                                  (require-str (second vals))))))
        ((op= op "string-format-num")
         (require-count 1)
         (let* ((buf (fresh-string-buffer))
                (addr (string-buffer-address buf)))
           (emitf "    (~a ~a :string_format_num !)" addr
                  (require-str (first vals)))
           (values addr :string)))
        ((op= op "string-padd")
         (require-count 3)
         (let* ((buf (fresh-string-buffer))
                (addr (string-buffer-address buf)))
           (emitf "    (~a ~a ~a ~a :string_padd !)"
                  addr (require-str (first vals))
                  (require-int (second vals)) (require-str (third vals)))
           (values addr :string)))
        ((op= op "get-env")
         (require-count 1)
         (let* ((buf (fresh-string-buffer))
                (addr (string-buffer-address buf)))
           (emitf "    (~a ~a :get_env !)" (require-str (first vals)) addr)
           (values addr :string)))
        ((op= op "set-env")
         (require-count 2)
         (int-module-call "set_env"
                          (list (require-str (first vals))
                                (require-str (second vals)))))
        (t (error "Unknown string builtin ~a" op))))))

;;; ---------------------------------------------------------------------------
;;; Networking builtins (L1VM net module)
;;; ---------------------------------------------------------------------------
;;; Socket handles are ordinary int64 values: >= 0 is open, -1 is an error.
;;; A small kind array next to the module lets CLOSE-SOCKET and the socket
;;; predicates dispatch without exposing L1VM's six separate close functions.

(defparameter *net-builtin-ops*
  '("open-tcp-server" "open-tcp-client" "accept-tcp"
    "open-ssl-server" "open-ssl-client" "accept-ssl"
    "close-socket" "socket-port?" "tcp-socket?" "ssl-socket?"
    "socket-write-line" "socket-read-line"
    "socket-write-char" "socket-write-byte"
    "socket-read-char" "socket-read-byte"
    "host-address" "local-host-name"))

(defun net-builtin-op-p (op)
  (some (lambda (name) (op= op name)) *net-builtin-ops*))

(defun net-arg-values (args)
  (let ((vals nil))
    (dolist (a args)
      (multiple-value-bind (n ty) (compile-expr a)
        (push (list n (normalize-type ty)) vals)))
    (nreverse vals)))

(defun net-status-call (fn operands)
  "Call a net module wrapper returning one status int."
  (let ((res (fresh-temp :int)))
    (emitf "    (~{~a~^ ~} :~a !)" operands fn)
    (emit-call-result res :int)
    res))

(defun net-open-call (fn operands kind)
  "Call an open/accept wrapper, register its kind, or return -1 on error."
  (let* ((err (fresh-temp :int))
         (handle (fresh-temp :int))
         (ok (fresh-temp :int))
         (bad (fresh-temp :int))
         (res (fresh-temp :int))
         (offset (fresh-temp :int))
         (minus1 (ensure-literal -1 :int))
         (kindlit (ensure-literal kind :int))
         (eight (ensure-literal 8 :int)))
    (emitf "    (~{~a~^ ~} :~a !)" operands fn)
    ;; The module pushes HANDLE then ERR; ERR is on top of the stack.
    (emitf "    (~a stpopi)" err)
    (emitf "    (~a stpopi)" handle)
    (emit "    (reset-reg)")
    (emitf "    ((~a zero ==) ~a =)" err ok)
    (emitf "    ((~a zero <) ~a =)" handle bad)
    ;; Push the result in every branch: a value assigned inside IF is not
    ;; visible after ENDIF in a generated function.
    (emitf "    (~a if+)" ok)
    (emitf "        (~a if+)" bad)
    (emitf "            (~a stpushi)" minus1)
    (emitf "        (else)")
    ;; Brackets array indexes are byte offsets; int64 slots are 8 bytes wide.
    (emitf "            (~a ~a * ~a :=)" handle eight offset)
    (emitf "            (~a l2v_sockkind [ ~a ] =)" kindlit offset)
    (emitf "            (~a stpushi)" handle)
    (emitf "        (endif)")
    (emitf "    (else)")
    (emitf "        (~a stpushi)" minus1)
    (emitf "    (endif)")
    (emit-call-result res :int)
    res))

(defun net-read-byte-call (handle)
  "Read one byte; return -1 when the module reports an error."
  (let* ((err (fresh-temp :int))
         (value (fresh-temp :int))
         (ok (fresh-temp :int))
         (res (fresh-temp :int))
         (minus1 (ensure-literal -1 :int)))
    (emitf "    (~a :socket_read_byte !)" handle)
    ;; The module pushes VALUE then ERR; ERR is on top of the stack.
    (emitf "    (~a stpopi)" err)
    (emitf "    (~a stpopi)" value)
    (emit "    (reset-reg)")
    (emitf "    ((~a zero ==) ~a =)" err ok)
    (emitf "    (~a if+)" ok)
    (emitf "        (~a stpushi)" value)
    (emitf "    (else)")
    (emitf "        (~a stpushi)" minus1)
    (emitf "    (endif)")
    (emit-call-result res :int)
    res))

(defun net-kind-value (handle)
  (emitf "    (~a :l2v_socket_kind !)" handle)
  (let ((kind (fresh-temp :int)))
    (emit-call-result kind :int)
    kind))

(defun net-buffer-result (fn operand-fn)
  "Call a wrapper that fills a string buffer, and return its address."
  (let* ((buf (fresh-string-buffer))
         (addr (string-buffer-address buf))
         (operands (funcall operand-fn addr))
         (status (fresh-temp :int)))
    (emitf "    (~{~a~^ ~} :~a !)" operands fn)
    (emit-call-result status :int)
    (values addr :string)))

(defun compile-net-builtin (op args)
  "Compile Lisp socket builtins into calls to the local net module."
  (setf *net-module-needed* t)
  (let ((vals (net-arg-values args)))
    (labels ((require-count (n)
               (unless (= (length vals) n)
                 (error "~a expects ~a arguments" op n)))
             (require-str (v)
               (unless (eq (second v) :string)
                 (error "~a expects a string argument" op))
               (first v))
             (require-int (v)
               (unless (eq (second v) :int)
                 (error "~a expects an integer argument" op))
               (first v))
             (str (i) (require-str (nth i vals)))
             (int (i) (require-int (nth i vals))))
      (cond
        ;; SOCKET kinds: 1 TCP server, 2 TCP client, 3 TCP accepted,
        ;; 4 SSL server, 5 SSL client, 6 SSL accepted.
        ((op= op "open-tcp-server")
         (require-count 2)
         (values (net-open-call "open_server_socket" (list (str 0) (int 1)) 1) :int))
        ((op= op "open-tcp-client")
         (require-count 2)
         (values (net-open-call "open_client_socket" (list (str 0) (int 1)) 2) :int))
        ((op= op "accept-tcp")
         (require-count 1)
         (values (net-open-call "open_accept_server" (list (int 0)) 3) :int))
        ((op= op "open-ssl-server")
         (require-count 2)
         (values (net-open-call "open_server_socket_ssl" (list (str 0) (int 1)) 4) :int))
        ((op= op "open-ssl-client")
         (require-count 2)
         (values (net-open-call "open_client_socket_ssl" (list (str 0) (int 1)) 5) :int))
        ((op= op "accept-ssl")
         (require-count 1)
         (values (net-open-call "open_accept_server_ssl" (list (int 0)) 6) :int))
        ((op= op "close-socket")
         (require-count 1)
         (values (net-status-call "l2v_close_socket" (list (int 0))) :int))
        ((op= op "socket-port?")
         (require-count 1)
         (let ((kind (net-kind-value (int 0)))
               (res (fresh-temp :int)))
           (emitf "    ((~a zero >) ~a =)" kind res)
           (values res :int)))
        ((op= op "ssl-socket?")
         (require-count 1)
         (let* ((kind (net-kind-value (int 0)))
                (res (fresh-temp :int))
                (four (ensure-literal 4 :int)))
           (emitf "    ((~a ~a >=) ~a =)" kind four res)
           (values res :int)))
        ((op= op "tcp-socket?")
         (require-count 1)
         (let* ((kind (net-kind-value (int 0)))
                (res (fresh-temp :int))
                (four (ensure-literal 4 :int))
                (positive (fresh-temp :int))
                (below-ssl (fresh-temp :int)))
           (emitf "    ((~a zero >) ~a =)" kind positive)
           (emitf "    (~a if+)" positive)
           (emitf "        ((~a ~a <) ~a =)" kind four below-ssl)
           (emitf "        (~a stpushi)" below-ssl)
           (emitf "    (else)")
           (emitf "        (zero stpushi)")
           (emitf "    (endif)")
           (emit-call-result res :int)
           (values res :int)))
        ((or (op= op "socket-write-char") (op= op "socket-write-byte"))
         (require-count 2)
         (values (net-status-call "socket_write_byte" (list (int 0) (int 1))) :int))
        ((or (op= op "socket-read-char") (op= op "socket-read-byte"))
         (require-count 1)
         (values (net-read-byte-call (int 0)) :int))
        ((op= op "socket-write-line")
         (require-count 2)
         ;; socket_write_string writes the string plus an LF terminator.
         (values (net-status-call "socket_write_string" (list (int 0) (str 1))) :int))
        ((op= op "socket-read-line")
         (require-count 1)
         (let ((max (ensure-literal (1- +string-buffer-size+) :int)))
           (net-buffer-result "socket_read_string"
                              (lambda (addr) (list (int 0) addr max)))))
        ((op= op "host-address")
         (require-count 1)
         (net-buffer-result "get_hostbyname"
                            (lambda (addr) (list (str 0) addr))))
        ((op= op "local-host-name")
         (require-count 0)
         (net-buffer-result "get_hostname" (lambda (addr) (list addr))))
        (t (error "Unknown net builtin ~a" op))))))

;;; ---------------------------------------------------------------------------
;;; File builtins (L1VM file module)
;;; ---------------------------------------------------------------------------
;;; Ports are ordinary int64 file handles.  L2V_FILEOPEN tracks which handles
;;; are still live because a port value is just a number and could otherwise be
;;; closed twice.  READ-LINE cycles through a pool of fixed-size buffers so a
;;; returned string stays valid while the program uses it.

(defparameter *file-builtin-ops*
  '("open-input-file" "open-output-file" "open-append-file"
    "file-port?" "read-line" "read-char" "write-string" "write-line"
    "write-char" "flush-output" "close-port"))

(defun file-builtin-op-p (op)
  (some (lambda (name) (op= op name)) *file-builtin-ops*))

(defun file-arg-values (args)
  (let ((vals nil))
    (dolist (a args)
      (multiple-value-bind (n ty) (compile-expr a)
        (push (list n (normalize-type ty)) vals)))
    (nreverse vals)))

(defun file-call-result (fn operands rtype)
  "Call a generated file helper and pop its single return value."
  (let ((res (fresh-temp rtype)))
    (if operands
        (emitf "    (~{~a~^ ~} :~a !)" operands fn)
        (emitf "    (:~a !)" fn))
    (emit-call-result res rtype)
    res))

(defun compile-file-builtin (op args)
  "Compile Lisp file-port operations into L1VM file module calls."
  (setf *file-module-needed* t)
  (let ((vals (file-arg-values args)))
    (labels ((require-count (n)
               (unless (= (length vals) n)
                 (error "~a expects ~a arguments" op n)))
             (require-int (v)
               (unless (eq (second v) :int)
                 (error "~a expects an integer argument" op))
               (first v))
             (require-str (v)
               (unless (eq (second v) :string)
                 (error "~a expects a string argument" op))
               (first v))
             (str (i) (require-str (nth i vals)))
             (int (i) (require-int (nth i vals)))
             (mode (n) (ensure-literal n :int))
             (open-call (n)
               (require-count 1)
               (values (file-call-result "l2v_file_open"
                                         (list (mode n) (str 0)) :int)
                       :int)))
      (cond
        ((op= op "open-input-file")  (open-call 1))
        ((op= op "open-output-file") (open-call 2))
        ((op= op "open-append-file") (open-call 5))
        ((op= op "file-port?")
         (require-count 1)
         (values (file-call-result "l2v_file_valid" (list (int 0)) :int) :int))
        ((op= op "read-line")
         (require-count 1)
         (values (file-call-result "l2v_file_read_line" (list (int 0)) :string)
                 :string))
        ((op= op "read-char")
         (require-count 1)
         (values (file-call-result "l2v_file_read_char" (list (int 0)) :int)
                 :int))
        ((or (op= op "write-string") (op= op "write-line"))
         (require-count 2)
         (let ((newline (ensure-literal (if (op= op "write-line") 1 0) :int)))
           (values (file-call-result "l2v_file_write_string"
                                     (list (str 0) (int 1) newline) :string)
                   :string)))
        ((op= op "write-char")
         (require-count 2)
         (values (file-call-result "l2v_file_write_char"
                                   (list (int 0) (int 1)) :int)
                 :int))
        ((op= op "flush-output")
         (require-count 0)
         (values (file-call-result "l2v_file_flush" nil :int) :int))
        ((op= op "close-port")
         (require-count 1)
         (values (file-call-result "l2v_file_close" (list (int 0)) :int) :int))
        (t (error "Unknown file builtin ~a" op))))))

;;; ---------------------------------------------------------------------------
;;; SDL 3.0 builtins (L1VM sdl-3 module)
;;; ---------------------------------------------------------------------------
;;; Every wrapper in sdl-3-lib.l1h is exposed here.  The table records the
;;; order and stack type of each argument as the C module pops them in reverse.
;;; :byte arguments are cast to L1VM byte stack values before the call.
;;; Multi-return wrappers are wrapped by helpers which keep the extra values
;;; in globals and return the most useful one to Lisp.

(defparameter *sdl-specs*
  '(
    ;; Window / drawing -----------------------------------------------------
    ("sdl-open-screen"       "sdl_open_screen"       (:module :int :int :int :int) :int)
    ("sdl-open-screen-full"  "sdl_open_screen"       (:module :int :int :int (:const 1)) :int)
    ("sdl-quit"              "sdl_quit"              nil :void)
    ("sdl-set-window-title"  "sdl_set_window_title"  (:string) :void)
    ("sdl-toggle-mouse-pointer" "sdl_toggle_mouse_pointer" (:int) :void)
    ("sdl-delay"             "sdl_delay"             (:int) :void)
    ("sdl-update"            "sdl_update"            nil :void)
    ("sdl-font-ttf"          "sdl_font_ttf"          (:string :int) :void)
    ("sdl-text-ttf"          "sdl_text_ttf"          (:int :int :byte :byte :byte :string) :void)
    ("sdl-pixel"             "sdl_pixel"             (:int :int :byte :byte :byte :byte) :void)
    ("sdl-line"              "sdl_line"              (:int :int :int :int :byte :byte :byte :byte) :void)
    ("sdl-rectangle"         "sdl_rectangle"         (:int :int :int :int :byte :byte :byte :byte) :void)
    ("sdl-rectangle-fill"    "sdl_rectangle_fill"    (:int :int :int :int :byte :byte :byte :byte) :void)
    ("sdl-circle"            "sdl_circle"            (:int :int :int :byte :byte :byte :byte) :void)
    ("sdl-circle-fill"       "sdl_circle_fill"       (:int :int :int :byte :byte :byte :byte) :void)
    ("sdl-ellipse"           "sdl_ellipse"           (:int :int :int :int :byte :byte :byte :byte) :void)
    ("sdl-ellipse-fill"      "sdl_ellipse_fill"      (:int :int :int :int :byte :byte :byte :byte) :void)
    ("sdl-pie"               "sdl_pie"               (:int :int :int :int :int :byte :byte :byte :byte) :void)
    ("sdl-pie-fill"          "sdl_pie_fill"          (:int :int :int :int :int :byte :byte :byte :byte) :void)
    ("sdl-trigon"            "sdl_trigon"            (:int :int :int :int :int :int :byte :byte :byte :byte) :void)
    ("sdl-trigon-fill"       "sdl_trigon_fill"       (:int :int :int :int :int :int :byte :byte :byte :byte) :void)
    ("sdl-get-pixelcolor"    "sdl_get_pixelcolor"    (:int :int) :pixelcolor)
    ("sdl-load-picture"      "sdl_load_picture"      (:string :int :int) :int)
    ("sdl-save-picture"      "sdl_save_picture"      (:string) :int)
    ("sdl-play-sound"        "sdl_play_sound"        (:string) :void)

    ;; GUI ------------------------------------------------------------------
    ("init-gadgets"          "init_gui"              (:int) :void)
    ("init-gui"              "init_gui"              (:int) :void)
    ("set-gadget-button"     "set_gadget_button"     (:int :int :int :string :int) :void)
    ("set-gadget-progress"   "set_gadget_progress"   (:int :int :int :string :int :int) :void)
    ("set-gadget-checkbox"   "set_gadget_checkbox"   (:int :int :int :string :int :int) :void)
    ;; The text list is a quoted list of string expressions.
    ("set-gadget-cycle"      "set_gadget_cycle"      (:int :int :string-list :int :int :string :int :int) :void)
    ("set-gadget-string"     "set_gadget_string"     (:int :int :int :string :int :int :int) :void)
    ("set-gadget-box"        "set_gadget_box"        (:int :int :int :int :int :int) :void)
    ("set-gadget-box-grid"   "set_gadget_box_grid"   (:int :int :int :int :int :int :int :int :byte :byte :byte :byte :int) :void)
    ("set-gadget-slider"     "set_gadget_slider"     (:int :int :int :string :int :int :int :int) :void)
    ("set-gadget-slider-vert" "set_gadget_slider_vert" (:int :int :int :int :int :int :int :int :int) :void)
    ("set-gadget-menu"       "set_gadget_menu"       (:int :int :string-list :int :int :string :int :int) :void)
    ("change-gadget-checkbox" "change_gadget_checkbox" (:int :int :int) :void)
    ("change-gadget-cycle"   "change_gadget_cycle"   (:int :int :int) :void)
    ("change-gadget-string"  "change_gadget_string"  (:int :string :int) :void)
    ("change-gadget-box"     "change_gadget_box"     (:int :int) :void)
    ("change-gadget-box-grid" "change_gadget_box_grid" (:int :int :int :byte :byte :byte :byte) :void)
    ("get-gadget-box-grid"   "get_gadget_box_grid"   (:int) :int)
    ("change-gadget-progress" "change_gadget_progress" (:int :int :int) :void)
    ("change-gadget-slider"  "change_gadget_slider"  (:int :int :int) :void)
    ("get-gadget-x2y2"       "get_gadget_x2y2"       (:int) :x2y2)
    ("gadget-event"          "gadget_event"          (:string) :gadget-event)
    ("set-gadget-string-passwd" "set_gadget_string_passwd" (:int :int) :void)
    ("set-gadget-string-multiline" "set_gadget_string_multiline" (:int :int :int :string :int :int :int :int) :void)
    ("change-gadget-string-multiline" "change_gadget_string_multiline" (:int :string :int) :void)
    ("get-gadget-string-multiline" "get_gadget_string_multiline" (:int :string) :void)
    ("free-all-gadgets"      "sdl_free_all_gadgets"  nil :void)
    ("sdl-free-all-gadgets"  "sdl_free_all_gadgets"  nil :void)
    ("unset-gadget"          "sdl_unset_gadget"      (:int) :void)
    ("sdl-unset-gadget"      "sdl_unset_gadget"      (:int) :void)
    ("copy-area"             "copy_area"             (:int :int :int :int) :int)
    ("sdl-copy-area"         "copy_area"             (:int :int :int :int) :int)
    ("restore-area"          "restore_area"          (:int :int :int :int) :int)
    ("sdl-restore-area"      "restore_area"          (:int :int :int :int) :int)

    ;; Mouse, joystick, keyboard -------------------------------------------
    ("sdl-get-mouse-state"   "sdl_get_mouse_state"   nil :mouse-state)
    ("sdl-close-joystick"    "sdl_close_joystick"    nil :void)
    ("sdl-get-joystick-x-axis" "sdl_get_joystick_x_axis" nil :int)
    ("sdl-get-joystick-y-axis" "sdl_get_joystick_y_axis" nil :int)
    ("sdl-get-joystick-x2-axis" "sdl_get_joystick_x2_axis" nil :int)
    ("sdl-get-joystick-y2-axis" "sdl_get_joystick_y2_axis" nil :int)
    ("sdl-get-joystick-buttons" "sdl_get_joystick_buttons" nil :int)
    ("sdl-get-joystick-button" "sdl_get_joystick_button" (:int) :int)
    ("sdl-get-joystick-info" "sdl_get_joystick_info" (:string) :joy-info)
    ("sdl-get-key"           "sdl_get_key"           nil :int)

    ;; Camera ---------------------------------------------------------------
    ("sdl-open-camera"       "sdl_open_camera"       nil :int)
    ("sdl-get-camera-frame"  "sdl_get_camera_frame"  nil :int)
    ("sdl-close-camera"      "sdl_close_camera"      nil :void)))

(defparameter *sdl-builtin-ops*
  (mapcar #'first *sdl-specs*))

(defun sdl-find-spec (op)
  (find op *sdl-specs* :key #'first :test #'op=))

(defun sdl-builtin-op-p (op)
  (not (null (sdl-find-spec op))))

(defun sdl-return-helper (rtype)
  (case rtype
    (:pixelcolor   "l2v_sdl_get_pixelcolor")
    (:mouse-state  "l2v_sdl_get_mouse_state")
    (:joy-info     "l2v_sdl_get_joystick_info")
    (:x2y2         "l2v_sdl_get_gadget_x2y2")
    (:gadget-event "l2v_sdl_gadget_event")
    (t nil)))

(defun sdl-compile-operands (arg-specs args op)
  "Compile SDL arguments into (value . push-opcode) pairs, in push order."
  (let ((operands nil)
        (rest-args args))
    (labels ((need-arg ()
               (unless rest-args
                 (error "~a is missing an argument" op))
               (pop rest-args))
             (add (value push-op)
               (push (list value push-op) operands))
             (string-value (value ty)
               (unless (eq (normalize-type ty) :string)
                 (error "~a expects a string argument" op))
               value)
             (int-value (value ty)
               (unless (eq (normalize-type ty) :int)
                 (error "~a expects an integer argument" op))
               value))
      (dolist (spec arg-specs)
        (cond
          ((eq spec :module)
           (add (ensure-literal +sdl-module-slot+ :int) "stpushi"))
          ((and (consp spec) (eq (first spec) :const))
           (add (ensure-literal (second spec) :int) "stpushi"))
          ((eq spec :string-list)
           (let ((source (need-arg)))
             (unless (and (consp source) (eq (car source) 'quote))
               (error "~a expects a quoted list of strings" op))
             (dolist (item (cadr source))
               (multiple-value-bind (value ty) (compile-expr item)
                 (add (string-value value ty) "stpushi")))))
          (t
           (let ((source (need-arg)))
             (multiple-value-bind (value ty) (compile-expr source)
               (setf ty (normalize-type ty))
               (case spec
                 (:int    (add (int-value value ty) "stpushi"))
                 (:string (add (string-value value ty) "stpushi"))
                 (:byte
                  (cond ((eq ty :byte) (add value "stpushb"))
                        ((eq ty :int)
                         (let* ((literal-byte-p (and (integerp source) (<= 0 source 255)))
                                (by (fresh-byte-temp (and literal-byte-p source))))
                           (unless literal-byte-p
                             (emitf "    (cast ~a ~a =)" value by))
                           (add by "stpushb")))
                        (t (error "~a expects an integer colour value" op))))
                 (t (error "Unsupported SDL argument type ~a" spec))))))))
    (when rest-args
      (error "~a received too many arguments" op))
    (nreverse operands))))

(defun sdl-emit-call (fn operands)
  (dolist (operand operands)
    (emitf "    (~a ~a)" (first operand) (second operand)))
  (if operands
      (emitf "    (~{~a~^ ~} :~a !)" (mapcar #'first operands) fn)
      (emitf "    (:~a !)" fn)))

(defun sdl-call (fn operands rtype)
  "Call an SDL wrapper (or its multi-return helper) and materialize a value."
  (let ((helper (sdl-return-helper rtype)))
    (sdl-emit-call (or helper fn) operands))
  (cond
    ((eq rtype :void)
     (emit "    (reset-reg)")
     "zero")
    (t
     (let ((res (fresh-temp :int)))
       (emit-call-result res :int)
       res))))

(defun compile-sdl-builtin (op args)
  "Compile Lisp SDL operations into L1VM SDL 3.0 module calls."
  (setf *sdl-module-needed* t)
  (let* ((spec (or (sdl-find-spec op) (error "Unknown SDL builtin ~a" op)))
         (wrapper (second spec))
         (operands (sdl-compile-operands (third spec) args op)))
    (values (sdl-call wrapper operands (fourth spec)) :int)))

(defun ensure-sdl-helpers ()
  "Append SDL helper functions when SDL builtins are used."
  (when (and *sdl-module-needed* (not *sdl-helpers-emitted*))
    (setf *sdl-helpers-emitted* t)
    (setf *func-code*
          (append
             *func-code*
             (list
              ;; sdl_get_pixelcolor pushes b,g,r,a (a on top).  Pop all four,
              ;; keep g/b/a in globals, and return r to Lisp.
              (list
               "(l2v_sdl_get_pixelcolor func)"
               "    #var ~ l2v_sdl_get_pixelcolor"
               "    (set int64 1 y~ 0)"
               "    (set int64 1 x~ 0)"
               "    (set int64 1 a~ 0)"
               "    (set int64 1 r~ 0)"
               "    (set int64 1 g~ 0)"
               "    (set int64 1 b~ 0)"
               "    (y~ stpopi)"
               "    (x~ stpopi)"
               "    (x~ y~ :sdl_get_pixelcolor !)"
               "    (a~ stpopi)"
               "    (r~ stpopi)"
               "    (g~ stpopi)"
               "    (b~ stpopi)"
               "    (reset-reg)"
               "    (a~ l2v_sdl_pixel_a :=)"
               "    (r~ l2v_sdl_pixel_r :=)"
               "    (g~ l2v_sdl_pixel_g :=)"
               "    (b~ l2v_sdl_pixel_b :=)"
               "    (r~ stpushi)"
               "(funcend)")

              ;; get_mouse_state pushes x,y,left,middle,right (right on top).
              (list
               "(l2v_sdl_get_mouse_state func)"
               "    #var ~ l2v_sdl_get_mouse_state"
               "    (set int64 1 x~ 0)"
               "    (set int64 1 y~ 0)"
               "    (set int64 1 left~ 0)"
               "    (set int64 1 middle~ 0)"
               "    (set int64 1 right~ 0)"
               "    (:sdl_get_mouse_state !)"
               "    (right~ stpopi)"
               "    (middle~ stpopi)"
               "    (left~ stpopi)"
               "    (y~ stpopi)"
               "    (x~ stpopi)"
               "    (reset-reg)"
               "    (x~ l2v_sdl_mouse_x :=)"
               "    (y~ l2v_sdl_mouse_y :=)"
               "    (left~ l2v_sdl_mouse_left :=)"
               "    (middle~ l2v_sdl_mouse_middle :=)"
               "    (right~ l2v_sdl_mouse_right :=)"
               "    (x~ stpushi)"
               "(funcend)")

              ;; get_joystick_info pushes buttons,axis (axis on top).
              (list
               "(l2v_sdl_get_joystick_info func)"
               "    #var ~ l2v_sdl_get_joystick_info"
               "    (set int64 1 name~ 0)"
               "    (set int64 1 axis~ 0)"
               "    (set int64 1 buttons~ 0)"
               "    (name~ stpopi)"
               "    (name~ :sdl_get_joystick_info !)"
               "    (axis~ stpopi)"
               "    (buttons~ stpopi)"
               "    (reset-reg)"
               "    (axis~ l2v_sdl_joy_axis :=)"
               "    (buttons~ l2v_sdl_joy_buttons :=)"
               "    (axis~ stpushi)"
               "(funcend)")

              ;; get_gadget_x2y2 pushes y2,x2 (x2 on top).
              (list
               "(l2v_sdl_get_gadget_x2y2 func)"
               "    #var ~ l2v_sdl_get_gadget_x2y2"
               "    (set int64 1 gadget~ 0)"
               "    (set int64 1 x2~ 0)"
               "    (set int64 1 y2~ 0)"
               "    (gadget~ stpopi)"
               "    (gadget~ :get_gadget_x2y2 !)"
               "    (x2~ stpopi)"
               "    (y2~ stpopi)"
               "    (reset-reg)"
               "    (x2~ l2v_sdl_gadget_x2 :=)"
               "    (y2~ l2v_sdl_gadget_y2 :=)"
               "    (x2~ stpushi)"
               "(funcend)")

              ;; gadget_event pushes gadget,value,status (status on top).
              (list
               "(l2v_sdl_gadget_event func)"
               "    #var ~ l2v_sdl_gadget_event"
               "    (set int64 1 text~ 0)"
               "    (set int64 1 gadget~ 0)"
               "    (set int64 1 value~ 0)"
               "    (set int64 1 status~ 0)"
               "    (text~ stpopi)"
               "    (text~ :gadget_event !)"
               "    (status~ stpopi)"
               "    (value~ stpopi)"
               "    (gadget~ stpopi)"
               "    (reset-reg)"
               "    (value~ l2v_sdl_gadget_value :=)"
               "    (status~ l2v_sdl_gadget_status :=)"
               "    (gadget~ stpushi)"
               "(funcend)"))))))

(defun ensure-file-helpers ()
  "Append the file module wrappers when file builtins are used."
  (when (and *file-module-needed* (not *file-helpers-emitted*))
    (setf *file-helpers-emitted* t)
    (let* ((one (ensure-literal 1 :int))
           (minus1 (ensure-literal -1 :int))
           (eight (ensure-literal 8 :int))
           (line-size (ensure-literal +file-line-size+ :int))
           (line-max (ensure-literal (1- +file-line-size+) :int))
           (nl (ensure-literal 10 :byte)))
      (setf *func-code*
            (append
             *func-code*
             (list
              ;; h -> 1 when the handle was opened by this program and not closed.
              (list
               "(l2v_file_valid func)"
               "    #var ~ l2v_file_valid"
               "    (set int64 1 h~ 0)"
               "    (set int64 1 o~ 0)"
               "    (set int64 1 v~ 0)"
               "    (set int64 1 f~ 0)"
               "    (set int64 1 f2~ 0)"
               "    (set int64 1 f3~ 0)"
               "    (h~ stpopi)"
               "    ((h~ zero >=) f~ =)"
               "    (f~ if+)"
               "        ((h~ l2v_filemax <) f2~ =)"
               "        (f2~ if+)"
               (format nil "            (h~~ ~a * o~~ :=)" eight)
               "            (l2v_fileopen [ o~ ] v~ =)"
               "            ((v~ zero !=) f3~ =)"
               "            (f3~ if+)"
               (format nil "                (~a stpushi)" one)
               "            (else)"
               "                (zero stpushi)"
               "            (endif)"
               "        (else)"
               "            (zero stpushi)"
               "        (endif)"
               "    (else)"
               "        (zero stpushi)"
               "    (endif)"
               "(funcend)")
              ;; mode path -> file handle, or -1 on failure.
              (list
               "(l2v_file_open func)"
               "    #var ~ l2v_file_open"
               "    (set int64 1 m~ 0)"
               "    (set int64 1 p~ 0)"
               "    (set byte 1 mb~ 0)"
               "    (set int64 1 h~ 0)"
               "    (set int64 1 o~ 0)"
               "    (set int64 1 f~ 0)"
               "    (p~ stpopi)"
               "    (m~ stpopi)"
               "    (m~ stpushb)"
               "    (mb~ stpopb)"
               "    (mb~ p~ :file_open !)"
               "    (h~ stpopi)"
               "    (reset-reg)"
               "    ((h~ zero >=) f~ =)"
               "    (f~ if+)"
               (format nil "        (h~~ ~a * o~~ :=)" eight)
               (format nil "        (~a l2v_fileopen [ o~~ ] =)" one)
               "        (h~ stpushi)"
               "    (else)"
               (format nil "        (~a stpushi)" minus1)
               "    (endif)"
               "(funcend)")
              ;; h -> 1 on success, 0 on failure (file_close has no return code).
              (list
               "(l2v_file_close func)"
               "    #var ~ l2v_file_close"
               "    (set int64 1 h~ 0)"
               "    (set int64 1 v~ 0)"
               "    (set int64 1 o~ 0)"
               "    (h~ stpopi)"
               "    (h~ :l2v_file_valid !)"
               "    (v~ stpopi)"
               "    (reset-reg)"
               "    (v~ if+)"
               "        (h~ :file_close !)"
               (format nil "        (h~~ ~a * o~~ :=)" eight)
               "        (zero l2v_fileopen [ o~ ] =)"
               (format nil "        (~a stpushi)" one)
               "    (else)"
               "        (zero stpushi)"
               "    (endif)"
               "(funcend)")
              ;; h -> address of the current line, or 0 (NIL) at EOF.
              ;; A single fixed buffer slot suffices: callers copy the result
              ;; out before the next read overwrites it.
              (list
               "(l2v_file_read_line func)"
               "    #var ~ l2v_file_read_line"
               "    (set int64 1 h~ 0)"
               "    (set int64 1 v~ 0)"
               "    (set int64 1 a~ 0)"
               "    (set int64 1 e~ 0)"
               "    (set int64 1 f~ 0)"
               "    (h~ stpopi)"
               "    (h~ :l2v_file_valid !)"
               "    (v~ stpopi)"
               "    (reset-reg)"
               "    (v~ if+)"
               "        (Pl2v_flinebase zero + a~ :=)"
               (format nil "        (h~~ a~~ ~a :file_get_string !)" line-max)
               "        (e~ stpopi)"
               "        (reset-reg)"
               "        ((e~ zero ==) f~ =)"
               "        (f~ if+)"
               "            (a~ stpushi)"
               "        (else)"
               "            (zero stpushi)"
               "        (endif)"
               "    (else)"
               "        (zero stpushi)"
               "    (endif)"
               "(funcend)")
              ;; h -> character code, or 0 (NIL) at EOF.
              (list
               "(l2v_file_read_char func)"
               "    #var ~ l2v_file_read_char"
               "    (set int64 1 h~ 0)"
               "    (set int64 1 v~ 0)"
               "    (set int64 1 c~ 0)"
               "    (set int64 1 e~ 0)"
               "    (set int64 1 f~ 0)"
               "    (h~ stpopi)"
               "    (h~ :l2v_file_valid !)"
               "    (v~ stpopi)"
               "    (reset-reg)"
               "    (v~ if+)"
               "        (h~ :file_getc !)"
               "        (e~ stpopi)"
               "        (c~ stpopi)"
               "        (reset-reg)"
               "        ((e~ zero ==) f~ =)"
               "        (f~ if+)"
               "            (c~ stpushi)"
               "        (else)"
               "            (zero stpushi)"
               "        (endif)"
               "    (else)"
               "        (zero stpushi)"
               "    (endif)"
               "(funcend)")
              ;; string h newline-flag -> string on success, 0 (NIL) on error.
              (list
               "(l2v_file_write_string func)"
               "    #var ~ l2v_file_write_string"
               "    (set int64 1 g~ 0)"
               "    (set int64 1 h~ 0)"
               "    (set int64 1 s~ 0)"
               "    (set int64 1 v~ 0)"
               "    (set int64 1 e~ 0)"
               "    (set int64 1 f~ 0)"
               "    (set int64 1 f2~ 0)"
               "    (g~ stpopi)"
               "    (h~ stpopi)"
               "    (s~ stpopi)"
               "    (h~ :l2v_file_valid !)"
               "    (v~ stpopi)"
               "    (reset-reg)"
               "    (v~ if+)"
               "        (h~ s~ :file_put_string !)"
               "        (e~ stpopi)"
               "        (reset-reg)"
               "        ((e~ zero ==) f~ =)"
               "        (f~ if+)"
               "            ((g~ zero !=) f2~ =)"
               "            (f2~ if+)"
               (format nil "                (h~~ ~a :file_putc !)" nl)
               "                (e~ stpopi)"
               "                (reset-reg)"
               "                ((e~ zero ==) f~ =)"
               "                (f~ if+)"
               "                    (s~ stpushi)"
               "                (else)"
               "                    (zero stpushi)"
               "                (endif)"
               "            (else)"
               "                (s~ stpushi)"
               "            (endif)"
               "        (else)"
               "            (zero stpushi)"
               "        (endif)"
               "    (else)"
               "        (zero stpushi)"
               "    (endif)"
               "(funcend)")
              ;; char h -> char on success, 0 (NIL) on error.
              (list
               "(l2v_file_write_char func)"
               "    #var ~ l2v_file_write_char"
               "    (set int64 1 h~ 0)"
               "    (set int64 1 c~ 0)"
               "    (set byte 1 cb~ 0)"
               "    (set int64 1 v~ 0)"
               "    (set int64 1 e~ 0)"
               "    (set int64 1 f~ 0)"
               "    (h~ stpopi)"
               "    (c~ stpopi)"
               "    (h~ :l2v_file_valid !)"
               "    (v~ stpopi)"
               "    (reset-reg)"
               "    (v~ if+)"
               "        (c~ stpushb)"
               "        (cb~ stpopb)"
               "        (h~ cb~ :file_putc !)"
               "        (e~ stpopi)"
               "        (reset-reg)"
               "        ((e~ zero ==) f~ =)"
               "        (f~ if+)"
               "            (c~ stpushi)"
               "        (else)"
               "            (zero stpushi)"
               "        (endif)"
               "    (else)"
               "        (zero stpushi)"
               "    (endif)"
               "(funcend)")
              ;; () -> 1 on success, 0 on failure.
              (list
               "(l2v_file_flush func)"
               "    #var ~ l2v_file_flush"
               "    (set int64 1 e~ 0)"
               "    (set int64 1 f~ 0)"
               "    (:file_flush !)"
               "    (e~ stpopi)"
               "    (reset-reg)"
               "    ((e~ zero ==) f~ =)"
               "    (f~ if+)"
               (format nil "        (~a stpushi)" one)
               "    (else)"
               "        (zero stpushi)"
               "    (endif)"
               "(funcend)")))))))

;;; ---------------------------------------------------------------------------
;;; Crude type inference (used for branch results and return types)
;;; ---------------------------------------------------------------------------

(defun guess-type (expr)
  (cond
    ((integerp expr) :int)
    ((floatp expr) :double)
    ((stringp expr) :string)
    ((characterp expr) :int)
    ((symbolp expr)
     (cond ((member expr '(t nil)) :int)
           (t (or (cdr (assoc expr *vars*))
                  (cdr (gethash expr *globals*))
                  :int))))
    ((consp expr)
     (case (car expr)
       ((+ - * / mod 1+ 1- max min abs)
        (if (some (lambda (x) (eq (guess-type x) :double)) (cdr expr))
            :double :int))
       ((= /= < > <= >= and or not zerop evenp oddp eq eql equal null consp
           integerp numberp doublep stringp symbolp procedurep) :int)
       ((quote cons list car cdr first rest) :lisp)
       ((string-append concat string-cat string-copy string-left string-right
           string-mid int->string byte->hexstring double->string byte->string
           string-format-num string-padd get-env write-string write-line
           read-line) :string)
       ((string-length string-len string-mid->byte string-compare string-search
           string-regex string-verify set-env double->int
           open-input-file open-output-file open-append-file file-port?
           read-char write-char flush-output close-port
           sdl-open-screen sdl-open-screen-full sdl-quit
           sdl-set-window-title sdl-toggle-mouse-pointer sdl-delay
           sdl-update sdl-font-ttf sdl-text-ttf
           sdl-pixel sdl-line sdl-rectangle sdl-rectangle-fill
           sdl-circle sdl-circle-fill sdl-ellipse sdl-ellipse-fill
           sdl-pie sdl-pie-fill sdl-trigon sdl-trigon-fill
           sdl-get-pixelcolor sdl-load-picture sdl-save-picture
           sdl-play-sound
           init-gadgets init-gui set-gadget-button set-gadget-progress
           set-gadget-checkbox set-gadget-cycle set-gadget-string
           set-gadget-box set-gadget-box-grid set-gadget-slider
           set-gadget-slider-vert set-gadget-menu
           change-gadget-checkbox change-gadget-cycle change-gadget-string
           change-gadget-box change-gadget-box-grid change-gadget-progress
           change-gadget-slider gadget-event set-gadget-string-passwd
           set-gadget-string-multiline change-gadget-string-multiline
           free-all-gadgets sdl-free-all-gadgets unset-gadget sdl-unset-gadget
           copy-area sdl-copy-area restore-area sdl-restore-area
           sdl-close-joystick sdl-get-joystick-x-axis sdl-get-joystick-y-axis
           sdl-get-joystick-x2-axis sdl-get-joystick-y2-axis
           sdl-get-joystick-buttons sdl-get-joystick-button
           sdl-get-joystick-info sdl-get-key
           sdl-open-camera sdl-get-camera-frame sdl-close-camera
           sdl-get-mouse-state get-gadget-box-grid get-gadget-x2y2) :int)
       ((get-gadget-string-multiline) :string)
       ((int->double) :double)
       ((if cond progn let let* setq while) (guess-type (car (last expr))))
       (t (or (getf (gethash (car expr) *functions*) :return) :int))))
    (t :int)))

;;; ---------------------------------------------------------------------------
;;; Declaration parsing
;;; ---------------------------------------------------------------------------

(defun parse-declarations (body)
  "Parse (declare (double r) (int64 n) (string s) (return double)) forms."
  (let ((result nil))
    (dolist (form body)
      (when (and (consp form) (eq (car form) 'declare))
        (dolist (d (cdr form))
          (when (consp d)
            (let ((kind (string-downcase (symbol-name (car d)))))
              (cond ((member kind '("int64" "integer" "int" "fixnum") :test #'string=)
                     (dolist (v (cdr d)) (push (cons v :int) result)))
                    ((member kind '("double" "float") :test #'string=)
                     (dolist (v (cdr d)) (push (cons v :double) result)))
                    ((string= kind "string")
                     (dolist (v (cdr d)) (push (cons v :string) result)))
                    ((string= kind "lisp")
                     (dolist (v (cdr d)) (push (cons v :lisp) result)))
                    ((string= kind "return")
                     (push (cons :return (normalize-type (second d))) result))))))))
    result))

;;; ---------------------------------------------------------------------------
;;; First pass: collect functions and globals
;;; ---------------------------------------------------------------------------

(defun designator-symbol (designator)
  "Return the named function in FUNCALL/MAPCAR designator syntax, if any."
  (cond ((symbolp designator) designator)
        ((and (consp designator)
              (member (car designator) '(function quote))
              (symbolp (second designator)))
         (second designator))
        (t nil)))

(defun form-calls-p (form name)
  "True when FORM contains a (NAME ...) call, ignoring quoted data and #'NAME."
  (cond ((atom form) nil)
        ((member (car form) '(quote function)) nil)
        ((eq (car form) name) t)
        ((and (member (car form) '(funcall apply mapcar))
              (eq (designator-symbol (second form)) name))
         t)
        (t (some (lambda (sub) (form-calls-p sub name)) form))))

(defun form-called-functions (form acc)
  "Collect user functions called from FORM."
  (cond ((atom form) acc)
        ((member (car form) '(quote function)) acc)
        (t (let* ((head (car form))
                  (designated (and (member head '(funcall apply mapcar))
                                   (designator-symbol (second form))))
                  (acc (if (and designated (gethash designated *functions*))
                           (adjoin designated acc)
                           acc)))
             (let ((acc (if (gethash head *functions*)
                            (adjoin head acc)
                            acc)))
               (dolist (sub (cdr form) acc)
                 (setf acc (form-called-functions sub acc))))))))

(defun compute-recursion-groups ()
  "Find strongly connected components of the user-function call graph.
   A component of size > 1 is a mutually recursive group: all its members are
   compiled as iterative state machines sharing one depth counter and one
   generated return dispatcher.  Self-recursion remains a one-member case."
  (let ((edges (make-hash-table :test 'eq))
        (indices (make-hash-table :test 'eq))
        (low (make-hash-table :test 'eq))
        (onstack (make-hash-table :test 'eq))
        (stack nil)
        (index 0)
        (components nil))
    (dolist (name *function-order*)
      (let ((acc nil))
        (dolist (form (getf (gethash name *functions*) :body))
          (setf acc (form-called-functions form acc)))
        (setf (gethash name edges) acc)))
    (labels ((connect (v)
               (setf (gethash v indices) index
                     (gethash v low) index)
               (incf index)
               (push v stack)
               (setf (gethash v onstack) t)
               (dolist (w (gethash v edges))
                 (cond ((null (gethash w indices))
                        (connect w)
                        (setf (gethash v low)
                              (min (gethash v low) (gethash w low))))
                       ((gethash w onstack)
                        (setf (gethash v low)
                              (min (gethash v low) (gethash w indices))))))
               (when (= (gethash v low) (gethash v indices))
                 (let ((component nil) (w nil))
                   (loop
                     (setf w (pop stack))
                     (setf (gethash w onstack) nil)
                     (push w component)
                     (when (eq w v) (return)))
                   (push component components)))))
      (dolist (name *function-order*)
        (unless (gethash name indices)
          (connect name))))
    ;; Assign group ids and shared depth counters.
    (let ((gid 0))
      (dolist (component components)
        (let ((self-recursive
                (and (= (length component) 1)
                     (getf (gethash (first component) *functions*) :recursive))))
          (if (> (length component) 1)
              (let ((depth (format nil "l1_recdepth_g~a" (incf gid))))
                (dolist (f component)
                  (setf (gethash f *rec-group-of*) gid
                        (gethash f *rec-depth-name*) depth)
                  (setf (getf (gethash f *functions*) :recursive) t)))
              (when self-recursive
                (setf (gethash (first component) *rec-depth-name*)
                      (format nil "l1_recdepth_~a"
                              (safe-name (first component)))))))))))

(defun rec-same-group-p (a b)
  "True when A and B are the same function or mutually recursive peers."
  (or (eq a b)
      (let ((ga (gethash a *rec-group-of*))
            (gb (gethash b *rec-group-of*)))
        (and ga gb (= ga gb)))))

(defun rec-depth-global ()
  (or (gethash *current-recursive* *rec-depth-name*)
      (format nil "l1_recdepth_~a" (safe-name *current-recursive*))))


(defun declare-form-p (form)
  (and (consp form) (eq (car form) 'declare)))

(defun function-type-env (name)
  "Parameter and DECLARE types visible in NAME's body."
  (let ((fi (gethash name *functions*)))
    (append (copy-list (getf fi :params))
            (remove :return (parse-declarations (getf fi :body))
                    :key #'car))))

(defun infer-function-return-type (name)
  "Use a declared return type, otherwise infer it from the final form.
   This runs after all signatures are collected, so parameters and local
   DECLAREs are visible (the old scan-time guess saw none of them)."
  (let* ((fi (gethash name *functions*))
         (declared (cdr (assoc :return (parse-declarations (getf fi :body)))))
         (body (remove-if #'declare-form-p (getf fi :body))))
    (if declared
        (normalize-type declared)
        (let ((*vars* (function-type-env name)))
          (guess-type (car (last body)))))))

(defun infer-all-return-types ()
  "Iterate until inferred return types stop changing.  Forward references are
   refined on later passes; unknown callees initially contribute :int."
  (dotimes (i 6)
    (let ((changed nil))
      (dolist (name *function-order*)
        (let* ((fi (gethash name *functions*))
               (old (getf fi :return))
               (new (infer-function-return-type name)))
          (unless (eq old new)
            (setf (getf fi :return) new)
            (setf changed t))))
      (unless changed (return)))))

(defun validate-function-types ()
  "All internal value types are carried on the VM stack as int64 (or double).
   Strings are int64 addresses of Brackets string buffers, so both string
   parameters and string returns are valid; there is nothing to reject here."
  nil)

(defun scan-top-level (forms)
  (dolist (form forms)
    (setf *current-source-line* (form-line form))
    (cond
      ((and (consp form) (eq (car form) 'defun))
       (let* ((name (second form))
              (params (third form))
              (body (cdddr form))
              (decls (parse-declarations body)))
         (pushnew name *function-order*)
         (setf (gethash name *functions*)
               (list :params (mapcar (lambda (p)
                                       (cons p (or (cdr (assoc p decls)) :int)))
                                     params)
                     :return nil
                     :recursive (form-calls-p body name)
                     :line *current-source-line*
                     :body body))))
      ((and (consp form) (member (car form) '(defvar defparameter defconstant)))
       (let* ((name (second form)) (init (third form)))
         (setf (gethash name *globals*)
               (cons (safe-name name) (guess-type init))))))))

;;; A SETQ to a symbol that is neither a local binding nor a declared global
;;; creates an implicit global.  This is what lets source such as
;;; (defun show-x () (setq x 42) (write x)) compile without a separate DEFVAR.
;;; The scan is scope-aware so SETQs of parameters and LET bindings remain
;;; local, and it runs before any function is compiled so reads of the new
;;; global are visible too.

(defun binding-symbols (bindings)
  (mapcar (lambda (b) (if (symbolp b) b (first b))) bindings))

(defun scan-implicit-setqs (form bound)
  (cond
    ((atom form) nil)
    ((eq (car form) 'quote) nil)
    (t
     (let ((head (car form)))
       (cond
         ((or (op= head "setq") (op= head "setf"))
          (loop for (target value) on (cdr form) by #'cddr
                do (when (and (symbolp target) (not (member target bound)))
                     (register-implicit-global target (guess-type value)))
                   (scan-implicit-setqs value bound)))
         ((op= head "defmacro") nil)
         ((or (op= head "incf") (op= head "decf"))
          (let ((target (second form)))
            (when (and (symbolp target) (not (member target bound)))
              (register-implicit-global target
                                        (guess-type (or (third form) 1)))))
          (dolist (sub (cddr form))
            (scan-implicit-setqs sub bound)))
         ((and (consp head) (eq (car head) 'lambda))
          ;; Immediate lambda application: ((lambda (args) ...) values...).
          (scan-implicit-setqs head bound)
          (dolist (arg (cdr form))
            (scan-implicit-setqs arg bound)))
         ((op= head "defun")
          (let ((params (third form)))
            (dolist (f (cdddr form))
              (scan-implicit-setqs f (append (binding-symbols params) bound)))))
         ((op= head "lambda")
          (let ((params (second form)))
            (dolist (f (cddr form))
              (scan-implicit-setqs f (append (binding-symbols params) bound)))))
         ((op= head "macrolet")
          ;; Only the body is compiled; definitions run on the host at compile
          ;; time and may use ordinary SETQs on their own parameters.
          (dolist (f (cddr form))
            (scan-implicit-setqs f bound)))
         ((or (op= head "let") (op= head "let*"))
          (let ((bindings (second form)))
            (dolist (b bindings)
              (when (and (consp b) (consp (cdr b)))
                (scan-implicit-setqs (second b) bound)))
            (dolist (f (cddr form))
              (scan-implicit-setqs f (append (binding-symbols bindings) bound)))))
         ((op= head "do")
          (let* ((specs (second form))
                 (vars (binding-symbols specs)))
            (dolist (spec specs)
              (when (consp spec)
                (when (cdr spec) (scan-implicit-setqs (second spec) bound))
                (when (cddr spec)
                  (scan-implicit-setqs (third spec) (append vars bound)))))
            (dolist (f (cddr form))
              (scan-implicit-setqs f (append vars bound)))))
         ((or (op= head "dotimes") (op= head "dolist"))
          (let* ((spec (second form))
                 (var (first spec)))
            (when (and (consp spec) (cdr spec))
              (scan-implicit-setqs (second spec) bound))
            (dolist (f (cddr form))
              (scan-implicit-setqs f (append (list var) bound)))))
         (t
          (dolist (sub (cdr form))
            (scan-implicit-setqs sub bound))))))))

(defun scan-implicit-globals (forms)
  (dolist (form forms)
    (scan-implicit-setqs form nil)))

;;; Infer parameter types from call sites.  The source dialect has no type
;;; declarations; without this a string passed to a helper prints as an address.

(defun collect-call-arg-types (form inferred)
  (when (consp form)
    (unless (member (car form) '(quote function))
      (let* ((name (car form))
             (fi (and (symbolp name) (gethash name *functions*))))
        (when fi
          (let ((params (getf fi :params))
                (args (cdr form))
                (declared (parse-declarations (getf fi :body))))
            (loop for p in params
                  for a in args
                  for i from 0
                  unless (cdr (assoc (car p) declared))
                    do (let* ((bucket (or (gethash name inferred)
                                          (setf (gethash name inferred)
                                                (make-array (length params)
                                                            :initial-element nil))))
                              (ty (guess-type a)))
                         (push ty (aref bucket i)))))))
      (dolist (sub (cdr form))
        (collect-call-arg-types sub inferred)))))

(defun choose-inferred-type (samples)
  (let ((types (remove-duplicates samples)))
    (cond ((null types) :int)
          ((= (length types) 1) (first types))
          ((and (member :int types) (member :double types)) :double)
          (t :int))))

(defun infer-parameter-types (forms)
  (let ((inferred (make-hash-table :test 'eq)))
    (dolist (form forms)
      (setf *current-source-line* (form-line form))
      (collect-call-arg-types form inferred))
    (maphash
     (lambda (name bucket)
       (let ((fi (gethash name *functions*)))
         (when fi
           (loop for p in (getf fi :params)
                 for i from 0
                 for ty = (choose-inferred-type (aref bucket i))
                 do (setf (cdr p) ty)))))
     inferred)))

;;; ---------------------------------------------------------------------------
;;; Expression compiler
;;; ---------------------------------------------------------------------------

;; COMPILE-FORM and EXPAND-LOCAL-MACRO are used by COMPILE-EXPR, which
;; appears earlier in the file.  Declare them to avoid style warnings.
(declaim (ftype function compile-form))
(declaim (ftype function expand-local-macro))

(defun bracket-op (sym)
  (case sym
    ((= eq eql equal) '==)
    ((/=) '!=)
    (t sym)))

;;; Pure numeric expressions whose operands are all compile-time constants can
;;; be replaced by a single literal.  Folding happens before code generation,
;;; so no temporary, comparison or arithmetic instruction is emitted at all.
;;; Only integer/float operators with well-defined host equivalents are folded;
;;; MOD is left alone because L1VM's % has C remainder semantics.

(defun constant-value-and-type (expr)
  "Folded value and type of a constant EXPR, or two NILs when not constant."
  (cond ((integerp expr) (values expr :int))
        ((floatp expr) (values expr :double))
        ((eq expr 't) (values 1 :int))
        ((eq expr 'nil) (values 0 :int))
        ((consp expr) (fold-constant-expr expr))
        (t (values nil nil))))

(defun constant-number-value (expr)
  "Numeric value of a constant EXPR, or NIL when it is not constant."
  (multiple-value-bind (v ty) (constant-value-and-type expr)
    (when (member ty '(:int :double)) v)))

(defun fold-constant-expr (expr)
  "Return two values: folded value and type, or NIL when EXPR is not foldable."
  (unless (consp expr)
    (return-from fold-constant-expr (values nil nil)))
  (let* ((op (car expr))
         (args (cdr expr))
         (vals (mapcar #'constant-number-value args)))
    ;; A constant test selects one branch; fold only when that branch is
    ;; itself a pure constant expression.
    (when (op= op "if")
      (return-from fold-constant-expr
        (let ((test (constant-number-value (car args))))
          (if (null test)
              (values nil nil)
              (constant-value-and-type
               (if (zerop test) (third args) (second args)))))))
    (unless (every (lambda (v) (not (null v))) vals)
      (return-from fold-constant-expr (values nil nil)))
    (let ((double (some #'floatp vals)))
      (flet ((res (v) (values (if double (float v) v)
                              (if double :double :int))))
        (cond
          ((or (op= op "+") (op= op "-") (op= op "*"))
           (let ((f (cond ((op= op "+") #'+) ((op= op "-") #'-) (t #'*))))
             (res (cond ((null vals) (if (op= op "*") 1 0))
                        ((and (op= op "-") (null (cdr vals))) (- (car vals)))
                        (t (apply f vals))))))
          ((and (op= op "/") (cdr vals))
           (handler-case
               (if double
                   (res (apply #'/ (mapcar #'float vals)))
                   (let ((v (car vals)))
                     (dolist (a (cdr vals)) (setf v (truncate v a)))
                     (res v)))
             (error () (values nil nil))))
          ((and (or (op= op "1+") (op= op "1-")) (= (length vals) 1))
           (res (if (op= op "1+") (1+ (car vals)) (1- (car vals)))))
          ((or (op= op "max") (op= op "min"))
           (res (apply (if (op= op "max") #'max #'min) vals)))
          ((and (op= op "abs") (= (length vals) 1))
           (res (abs (car vals))))
          ((op= op "not")
           (values (if (zerop (car vals)) 1 0) :int))
          ;; AND returns the last operand when all are true (matching the
          ;; compiler's short-circuit lowering), not a normalized boolean.
          ((op= op "and")
           (values (if (every (lambda (v) (not (zerop v))) vals)
                       (if vals (car (last vals)) 1) 0)
                   :int))
          ((op= op "or")
           (values (if (some (lambda (v) (not (zerop v))) vals) 1 0) :int))
          ((and (or (op= op "zerop") (op= op "evenp") (op= op "oddp"))
                (= (length vals) 1) (integerp (car vals)))
           (values (if (if (op= op "zerop") (zerop (car vals))
                        (if (op= op "evenp") (evenp (car vals)) (oddp (car vals))))
                       1 0)
                   :int))
          ((and (or (op= op "=") (op= op "/=") (op= op "<") (op= op ">")
                    (op= op "<=") (op= op ">=") (op= op "eq") (op= op "eql")
                    (op= op "equal") (op= op "equal?"))
                (>= (length vals) 2))
           (let* ((f (cond ((or (op= op "=") (op= op "eq") (op= op "eql")
                                (op= op "equal") (op= op "equal?")) #'=)
                           ((op= op "/=") #'/=)
                           ((op= op "<") #'<)
                           ((op= op ">") #'>)
                           ((op= op "<=") #'<=)
                           (t #'>=)))
                  (ok t))
             (loop for (a b) on vals while b do
               (unless (funcall f a b) (setf ok nil)))
             (values (if ok 1 0) :int)))
          (t (values nil nil)))))))

(defun compile-expr (expr)
  "Compile EXPR, returning two values: Brackets variable name and type."
  (cond
    ;; Local MACROLET definitions take precedence over global macros.
    ((and (consp expr) (assoc (car expr) *local-macros*))
     (compile-expr (expand-local-macro expr)))
    ;; Only macros defined by DEFMACRO in the source are expanded here.
    ;; CL macros such as WHEN/COND/DO are handled explicitly by COMPILE-FORM.
    ((and (consp expr) (member (car expr) *defined-macros*))
     (let ((expansion (macroexpand-1 expr)))
       (when (equal expansion expr)
         (error "Macro ~a did not expand" (car expr)))
       (compile-expr expansion)))
    ;; Fold side-effect-free constant arithmetic/comparisons before emitting
    ;; any instructions for them.
    ((multiple-value-bind (folded ftype) (fold-constant-expr expr)
       (when folded
         (return-from compile-expr (values (ensure-literal folded ftype) ftype))))
     nil)
    ((integerp expr) (values (ensure-literal expr :int) :int))
    ((floatp expr)   (values (ensure-literal expr :double) :double))
    ;; A Brackets string literal is a global buffer; its value is the int64
    ;; address of that buffer (the automatically generated NAMEaddr symbol).
    ((stringp expr)  (values (concatenate 'string (ensure-literal expr :string)
                                          "addr")
                             :string))
    ((characterp expr) (values (ensure-literal (char-code expr) :int) :int))
    ((symbolp expr)
     (cond ((eq expr 't)   (values (ensure-literal 1 :int) :int))
           ((eq expr 'nil) (values "zero" :int))
           (t (values (var-name expr) (var-type expr)))))
    ((consp expr) (compile-form expr))
    (t (values (ensure-literal 0 :int) :int))))

(defun compile-expr-nontail (expr)
  "Compile a subexpression whose value is consumed by an enclosing operation,
   never returned directly.  This prevents accidental tail-call lowering."
  (let ((*tail-position* nil))
    (compile-expr expr)))

(defun compile-progn (forms)
  "Only the final form is in tail position."
  (let ((name "zero") (type :int))
    (dolist (f (butlast forms))
      (let ((*tail-position* nil)
            (*discard-value* t))
        (multiple-value-setq (name type) (compile-expr f))))
    (when forms
      (multiple-value-setq (name type) (compile-expr (car (last forms)))))
    (values name type)))

(defun compile-progn-for-return (forms rtype)
  "Compile a function body, boxing a Lisp-valued final form when needed."
  (if (null forms)
      (values "zero" :int)
      (let ((name "zero") (type :int))
        (dolist (f (butlast forms))
          (let ((*tail-position* nil)
                (*discard-value* t))
            (multiple-value-setq (name type) (compile-expr f))))
        (multiple-value-setq (name type)
          (if (eq (normalize-type rtype) :lisp)
              (let ((*tail-position* *tail-position*))
                (compile-tagged-arg (car (last forms))))
              (compile-expr (car (last forms)))))
        (values name type))))

;; Double conversions use the L1VM math module.  The module wrapper for
;; DOUBLE->INT rounds with CEIL, so round the value to an integer first and
;; choose FLOOR for nonnegative values (truncation toward zero).

(defun convert-int-to-double (name)
  (setf *math-module-needed* t)
  (let ((result (fresh-temp :double)))
    (emitf "    (~a :math_int2double !)" name)
    (emit-call-result result :double)
    result))

(defun convert-double-to-int (name)
  (setf *math-module-needed* t)
  (let* ((result (fresh-temp :int))
         (rounded (fresh-temp :double))
         (zero (ensure-literal 0.0 :double))
         (positive (fresh-temp :int)))
    (emitf "    ((~a ~a >=d) ~a =)" name zero positive)
    (emitf "    (~a if+)" positive)
    (emitf "        (~a :math_floordouble !)" name)
    (emitf "        (~a ~a)" rounded (stack-pop-opcode :double))
    (emitf "        (reset-reg)")
    (emit "    (else)")
    (emitf "        (~a :math_ceildouble !)" name)
    (emitf "        (~a ~a)" rounded (stack-pop-opcode :double))
    (emitf "        (reset-reg)")
    (emit "    (endif)")
    (emitf "    (~a :math_double2int !)" rounded)
    (emit-call-result result :int)
    result))

(defun coerce-to-double (name type)
  (if (eq (normalize-type type) :int)
      (convert-int-to-double name)
      name))

(defun flatten-arith-args (op args)
  "Flatten nested applications of the same associative operator.
   (+ a (+ b c)) emits one Brackets expression instead of two temporaries."
  (if (member op '(+ *))
      (loop for a in args
            append (if (and (consp a) (eq (car a) op) (cdr a))
                       (flatten-arith-args op (cdr a))
                       (list a)))
      args))

(defun compile-arith (op args)
  (cond
    ((null args)
     (values (ensure-literal (if (eq op '*) 1 0) :int) :int))
    ((and (eq op '-) (null (cdr args)))
     (multiple-value-bind (name type) (compile-expr (car args))
       (let* ((ty (normalize-type type))
              (res (fresh-temp ty))
              (zero (if (eq ty :double) (ensure-literal 0.0 :double) "zero")))
         (emitf "    (~a ~a ~a ~a :=)" zero op name res)
         (values res ty))))
    (t
     (setf args (flatten-arith-args op args))
     (let ((names nil) (types nil))
       (dolist (a args)
         (multiple-value-bind (n ty) (compile-expr a)
           (push n names) (push ty types)))
       (setf names (reverse names) types (reverse types))
       ;; Values returned by CAR/CDR and friends are tagged fixnums.  Unbox
       ;; them before integer arithmetic so they mix with ordinary literals.
       (when (member :lisp types)
         (let ((shift (ensure-literal +tag-shift+ :int)))
           (setf names
                 (loop for n in names
                       for ty in types
                       collect (if (eq (normalize-type ty) :lisp)
                                   (let ((unboxed (fresh-temp :int)))
                                     (emitf "    (~a ~a / ~a :=)" n shift unboxed)
                                     unboxed)
                                   n)))))
       (let ((rtype (if (member :double types) :double :int))
             (result (car names)))
         (when (eq rtype :double)
           ;; Brackets does not mix int64 and double operands.
           (setf names (loop for n in names
                             for ty in types
                             collect (coerce-to-double n ty)))
           (setf result (car names))
           (when (eq op '%)
             (setf *math-module-needed* t)))
         (if (and (member op '(+ *)) (> (length names) 2))
             ;; Brackets accepts chained infix expressions; emit one line
             ;; instead of a temporary for every intermediate sum/product.
             (let ((tmp (fresh-temp rtype))
                   (text (format nil "~{~a~^ ~}"
                                 (loop for n in names
                                       for first = t then nil
                                       append (if first (list n) (list op n))))))
               (emitf "    (~a ~a :=)" text tmp)
               (setf result tmp))
             (dolist (rest (cdr names))
               (let ((tmp (fresh-temp rtype)))
                 (if (and (eq op '%) (eq rtype :double))
                     (progn
                       (emitf "    (~a ~a :math_fmoddouble !)" result rest)
                       (emit-call-result tmp :double))
                     (emitf "    (~a ~a ~a ~a :=)" result op rest tmp))
                 (setf result tmp))))
         (values result rtype))))))

(defun double-compare-op (op)
  (cond ((eq op '==) "==d")
        ((eq op '!=) "!=d")
        ((eq op '>)  ">d")
        ((eq op '<)  "<d")
        ((eq op '>=) ">=d")
        ((eq op '<=) "<=d")
        (t (symbol-name op))))

;; Compare already-compiled values.  Keeping this separate lets chained
;; comparisons compile each operand exactly once.
(defun compare-values (op a ta b tb)
  (let ((tmp (fresh-temp :int)))
    (cond
      ((and (member (normalize-type ta) '(:string))
            (member (normalize-type tb) '(:string))
            (member op '(== !=)))
       ;; Compare text, not addresses.  :string_compare returns 0 for
       ;; equal strings, so invert that for the boolean result.
       (let ((sa (printable-string a ta))
             (sb (printable-string b tb))
             (cmp (fresh-temp :int))
             (boolop (if (eq op '==) '== '!=)))
         (emitf "    (~a ~a :string_compare !)" sa sb)
         (emit-call-result cmp :int)
         (emitf "    (~a zero ~a ~a :=)" cmp boolop tmp)))
      ((and (eq (normalize-type ta) :double)
            (eq (normalize-type tb) :double))
       ;; The linter rejects (a ==d b int-result :=): it type-checks the
       ;; assignment target against the operands.  The nested assignment
       ;; form is the Brackets idiom for returning a bool from a double
       ;; comparison.
       (emitf "    ((~a ~a ~a) ~a =)" a b (double-compare-op op) tmp))
      ((and (eq (normalize-type ta) :lisp)
            (eq (normalize-type tb) :int))
       (let ((unboxed (fresh-temp :int))
             (shift (ensure-literal +tag-shift+ :int)))
         (emitf "    (~a ~a / ~a :=)" a shift unboxed)
         (emitf "    (~a ~a ~a ~a :=)" unboxed op b tmp)))
      ((and (eq (normalize-type ta) :int)
            (eq (normalize-type tb) :lisp))
       (let ((unboxed (fresh-temp :int))
             (shift (ensure-literal +tag-shift+ :int)))
         (emitf "    (~a ~a / ~a :=)" b shift unboxed)
         (emitf "    (~a ~a ~a ~a :=)" a op unboxed tmp)))
      ((or (and (eq (normalize-type ta) :int)
                (eq (normalize-type tb) :double))
           (and (eq (normalize-type ta) :double)
                (eq (normalize-type tb) :int)))
       (let ((da (coerce-to-double a ta))
             (db (coerce-to-double b tb)))
         (emitf "    ((~a ~a ~a) ~a =)" da db (double-compare-op op) tmp)))
      (t
       (emitf "    (~a ~a ~a ~a :=)" a op b tmp)))
    (values tmp :int)))

(defun compile-compare (op args)
  (let ((values (mapcar (lambda (expr)
                          (multiple-value-list (compile-expr expr)))
                        args)))
    (when (< (length values) 2)
      (error "~a expects at least two arguments" op))
    (let ((result nil))
      (loop for rest on values
            for left = (first rest)
            for right = (second rest)
            while right
            do (multiple-value-bind (v ty)
                   (compare-values op (first left) (second left)
                                   (first right) (second right))
                 (declare (ignore ty))
                 (if result
                     (let ((combined (fresh-temp :int)))
                       (emitf "    (~a ~a && ~a :=)" result v combined)
                       (setf result combined))
                     (setf result v))))
      (values result :int))))

(defun compile-and (args)
  "Short-circuit AND.  Returns 0 at the first false argument, otherwise 1."
  (if (null args)
      (values (ensure-literal 1 :int) :int)
      (let* ((res (fresh-temp :int))
             (end (format nil "ANDEND~a" (incf *label-count*))))
        (loop for rest on args
              for arg = (car rest)
              do (multiple-value-bind (v ty) (compile-expr arg)
                   (declare (ignore ty))
                   (if (null (cdr rest))
                       (emitf "    (~a ~a :=)" v res)
                       (let ((flag (fresh-temp :int)))
                         (emitf "    (~a zero == ~a :=)" v flag)
                         (emitf "    (zero ~a :=)" res)
                         (emitf "    (~a :~a jmpi)" flag end)))))
        (emitf "    (:~a)" end)
        (values res :int))))

(defun compile-or (args)
  "Short-circuit OR.  Returns 1 at the first true argument, otherwise 0."
  (if (null args)
      (values (ensure-literal 0 :int) :int)
      (let* ((res (fresh-temp :int))
             (end (format nil "OREND~a" (incf *label-count*)))
             (true (ensure-literal 1 :int)))
        (dolist (arg args)
          (multiple-value-bind (v ty) (compile-expr arg)
            (declare (ignore ty))
            (let ((false (fresh-temp :int))
                  (truth (fresh-temp :int)))
              (emitf "    (~a zero == ~a :=)" v false)
              (emitf "    (~a zero == ~a :=)" false truth)
              (emitf "    (~a ~a :=)" true res)
              (emitf "    (~a :~a jmpi)" truth end)
              (emitf "    (zero ~a :=)" res))))
        (emitf "    (:~a)" end)
        (values res :int))))

(defun compile-not (arg)
  (let ((name (compile-expr arg))
        (tmp (fresh-temp :int)))
    (emitf "    (~a zero == ~a :=)" name tmp)
    (values tmp :int)))

(defun compile-if (args)
  (let* ((flag (compile-expr-nontail (first args)))
         (then (second args))
         (else (third args))
         (rtype (guess-type (if else else (progn then))))
         (res nil))
    (setf res (fresh-temp rtype))
    (if else
        (progn
          (emitf "    (~a if+)" flag)
          (let ((v (if (eq (normalize-type rtype) :lisp)
                       (compile-tagged-arg then)
                       (compile-expr then))))
            (emitf "    (~a ~a :=)" v res))
          (emit "    (else)")
          (let ((v (if (eq (normalize-type rtype) :lisp)
                       (compile-tagged-arg else)
                       (compile-expr else))))
            (emitf "    (~a ~a :=)" v res))
          (emit "    (endif)"))
        (progn
          (emitf "    (~a if)" flag)
          (let ((v (if (eq (normalize-type rtype) :lisp)
                       (compile-tagged-arg then)
                       (compile-expr then))))
            (emitf "    (~a ~a :=)" v res))
          (emit "    (endif)")))
    (values res rtype)))

(defun cond->if (clauses)
  (cond ((null clauses) nil)
        (t
         (let* ((clause (car clauses))
                (test (car clause))
                (body (cdr clause)))
           (if (eq test 't)
               (cons 'progn body)
               (list 'if test
                     (if (cdr body) (cons 'progn body) (car body))
                     (cond->if (cdr clauses))))))))

(defun lambda-form-p (x)
  (and (consp x) (eq (car x) 'lambda)))

(defun expand-local-macro (form)
  "Expand a MACROLET definition at compile time.  Simple parameter lists
   are supported; the macro body runs in the host Lisp."
  (let* ((entry (assoc (car form) *local-macros*))
         (params (second entry))
         (body (cddr entry))
         (*package* *user-package*))
    (unless entry
      (error "Unknown local macro: ~a" (car form)))
    (unless (= (length params) (length (cdr form)))
      (error "Local macro ~a expects ~a arguments, got ~a"
             (car form) (length params) (length (cdr form))))
    (eval `(let ,(mapcar #'list params (cdr form))
             ,@body))))

(defun compile-macrolet (args)
  (let ((defs (first args))
        (body (cdr args))
        (*local-macros* *local-macros*))
    (dolist (d defs)
      (push (cons (car d) (cons (second d) (cddr d))) *local-macros*))
    (compile-progn body)))

(defun compile-let (args sequential)
  (let ((bindings (first args))
        (body (cdr args)))
    (let ((*vars* *vars*)
          (*lambda-env* *lambda-env*))
      (if sequential
          (dolist (b bindings)
            (cond ((symbolp b) (bind-variable b :int nil))
                  ((lambda-form-p (second b))
                   ;; A lambda is not a runtime object here.  Bind a dummy
                   ;; slot so the name exists, and remember the lambda form
                   ;; for FUNCALL and immediate application.
                   (bind-variable (car b) :int nil)
                   (push (cons (car b) (second b)) *lambda-env*))
                  (t
                   (let* ((sym (car b)) (init (second b)))
                     (multiple-value-bind (name type) (compile-expr-nontail init)
                       (let ((bv (bind-variable sym type nil)))
                         (emitf "    (~a ~a :=)" name bv)))))))
          (let ((inits nil))
            (dolist (b bindings)
              (if (and (consp b) (lambda-form-p (second b)))
                  (push (list "zero" :int (second b)) inits)
                  (multiple-value-bind (n ty)
                      (if (symbolp b) (values "zero" :int)
                          (compile-expr-nontail (second b)))
                    (push (list n ty nil) inits))))
            (setf inits (reverse inits))
            (loop for b in bindings
                  for iv in inits
                  do (let ((sym (if (symbolp b) b (car b))))
                       (let ((bv (bind-variable sym (cadr iv) nil)))
                         (emitf "    (~a ~a :=)" (car iv) bv))
                       (when (third iv)
                         (push (cons sym (third iv)) *lambda-env*))))))
      (compile-progn body))))

(defun compile-setq (args)
  (let ((name "zero") (type :int))
    (loop for (sym val) on args by #'cddr
          do (multiple-value-bind (v vtype) (compile-expr val)
               ;; A SETQ in a macro expansion can appear after the implicit
               ;; global scan, so create the global here as a fallback too.
               (when (and (symbolp sym)
                          (not (lookup-var sym))
                          (not (gethash sym *globals*)))
                 (register-implicit-global sym vtype))
               (let ((target (var-name sym)))
                 (emitf "    (~a ~a :=)" v target)
                 (setf name target type (var-type sym)))))
    (values name type)))

(defun compile-incf (args increment)
  (when (and (symbolp (car args))
             (not (lookup-var (car args)))
             (not (gethash (car args) *globals*)))
    (register-implicit-global (car args) (guess-type (or (second args) 1))))
  (let* ((place (car args))
         (target (var-name place))
         (ty (var-type place))
         (delta (if (cdr args) (second args) (if increment 1 -1))))
    (when (eq (normalize-type ty) :string)
      (error "INCF/DECF does not support string variables"))
    (multiple-value-bind (v vty)
        ;; A typed double variable needs a double delta, even when the
        ;; caller wrote a plain integer.
        (if (and (numberp delta) (eq (normalize-type ty) :double))
            (compile-expr (float delta))
            (compile-expr delta))
      (when (and (eq (normalize-type ty) :int) (eq vty :double))
        (error "INCF/DECF: double delta for an integer variable"))
      (emitf "    (~a ~a ~a ~a :=)" target (if increment '+ '-) v target)
      (values target ty))))

(defun case-body (forms)
  (cond ((null forms) nil)
        ((cdr forms) (cons 'progn forms))
        (t (car forms))))

(defun case->cond (args)
  "Expand CASE into our supported COND form."
  (let ((key (car args))
        (clauses (cdr args)))
    (cons 'cond
          (mapcar
           (lambda (clause)
             (let ((keys (car clause))
                   (body (case-body (cdr clause))))
               (cond
                 ((or (eq keys 't) (eq keys 'otherwise))
                  (list t body))
                 ((listp keys)
                  (cons (cons 'or (mapcar (lambda (k) (list 'eql key k)) keys))
                        (if body (list body) nil)))
                 (t
                  (list (list 'eql key keys)
                        body)))))
           clauses))))

(defun compile-do (args)
  "Compile the useful subset of DO:
     (do ((i 0 (1+ i)) ...) ((>= i n) result...) body...)"
  (let ((specs (first args))
        (finish (second args))
        (body (cddr args)))
    (unless finish (error "DO needs a termination clause"))
    (let ((*vars* *vars*))
      (dolist (spec specs)
        (let* ((sym (first spec))
               (init (second spec)))
          (multiple-value-bind (v ty) (compile-expr (or init 0))
            (let ((bv (bind-variable sym ty nil)))
              (emitf "    (~a ~a :=)" v bv)))))
      (let* ((n (incf *label-count*))
             (start (format nil "DOSTART~a" n))
             (done (format nil "DODONE~a" n)))
        (emitf "    (:~a)" start)
        ;; DO terminates when the test is TRUE (unlike WHILE, which exits
        ;; when its condition is false).
        (let* ((flag (compile-expr (car finish)))
               (test (fresh-temp :int)))
          (emitf "    (~a ~a :=)" flag test)
          (emitf "    (~a :~a jmpi)" test done))
        (compile-progn body)
        ;; Compute all step forms before assigning, as CL DO requires.
        (let ((steps nil))
          (dolist (spec specs)
            (when (cddr spec)
              (multiple-value-bind (v ty) (compile-expr (third spec))
                (declare (ignore ty))
                (push (list (first spec) v) steps))))
          (dolist (s (reverse steps))
            (emitf "    (~a ~a :=)" (second s) (var-name (first s)))))
        (emitf "    (:~a jmp)" start)
        (emitf "    (:~a)" done)
        (let ((res "zero") (rtype :int))
          (dolist (f (cdr finish))
            (multiple-value-setq (res rtype) (compile-expr f)))
          (values res rtype))))))

(defun compile-while (args)
  (let* ((n (incf *label-count*))
         (start (format nil "LOOP~a" n))
         (end (format nil "LOOPE~a" n)))
    (emitf "    (:~a)" start)
    (let* ((flag (compile-expr (first args)))
           (nf (fresh-temp :int)))
      (emitf "    (~a zero == ~a :=)" flag nf)
      (emitf "    (~a :~a jmpi)" nf end))
    (compile-progn (cdr args))
    (emitf "    (:~a jmp)" start)
    (emitf "    (:~a)" end)
    (values "zero" :int)))

(defun compile-dotimes (args)
  (let* ((spec (first args))
         (var (first spec))
         (count-expr (second spec))
         (body (cdr args)))
    (let ((*vars* *vars*))
      (let* ((cnt (compile-expr count-expr))
             (bv (bind-variable var :int nil))
             (one (ensure-literal 1 :int))
             (flag (fresh-temp :int)))
        (emitf "    (zero ~a :=)" bv)
        (emit "    (for-loop)")
        (emitf "    (((~a ~a <) ~a =) ~a for)" bv cnt flag flag)
        (compile-progn body)
        (emitf "    (~a + ~a ~a :=)" bv one bv)
        (emit "    (next)")))
    (values "zero" :int)))

(defun compile-loop (args)
  "A small subset of LOOP: (loop for i from a to b [do ...])."
  (let ((var nil) (from nil) (to nil) (body nil) (i 0))
    (loop while (< i (length args))
          do (let ((a (nth i args)))
               (case a
                 (for (incf i)
                      (setf var (nth i args)) (incf i))
                 (from (incf i) (setf from (nth i args)) (incf i))
                 (to (incf i) (setf to (nth i args)) (incf i))
                 (do (setf body (nthcdr (1+ i) args)) (setf i (length args)))
                 (t
                  ;; treat remaining forms as body
                  (setf body (nthcdr i args)) (setf i (length args))))))
    (let ((*vars* *vars*))
      (let* ((start (compile-expr (or from 0)))
             (end (compile-expr to))
             (bv (bind-variable (or var 'i) :int nil))
             (one (ensure-literal 1 :int))
             (flag (fresh-temp :int)))
        (emitf "    (~a ~a :=)" start bv)
        (emit "    (for-loop)")
        (emitf "    (((~a ~a <=) ~a =) ~a for)" bv end flag flag)
        (compile-progn body)
        (emitf "    (~a + ~a ~a :=)" bv one bv)
        (emit "    (next)")))
    (values "zero" :int)))

(defun compile-print (args newline)
  (if (null args)
      (when newline (emit "    (:print_n !)"))
      (dolist (a args)
        (multiple-value-bind (name type) (compile-expr a)
          (case (normalize-type type)
            (:lisp
             (need-runtime-fn "l1_print")
             (emitf "    (~a :l1_print !)" name))
            (t
             (let ((fn (case (normalize-type type)
                         (:string (if newline ":print_lns" ":print_s"))
                         (:double (if newline ":print_lnd" ":print_d"))
                         (t       (if newline ":print_lni" ":print_i"))))
                   (operand (printable-string name type)))
               (emitf "    (~a ~a !)" operand fn)))))))
  (values "zero" :int))

(defun compile-format (args)
  "Very small FORMAT subset: ~a ~s ~d ~f ~% and literal text."
  (let ((control (second args))
        (vals (cddr args))
        (i 0)
        (pos 0)
        (text nil))
    (labels ((flush-text ()
               (when text
                 (let ((name (ensure-literal (apply #'concatenate 'string
                                                    (reverse text)) :string)))
                   (emitf "    (~a :print_s !)" name))
                 (setf text nil))))
      (loop while (< pos (length control))
            do (let ((ch (char control pos)))
                 (if (char= ch #\~)
                     (let ((dir (char control (1+ pos))))
                       (incf pos 2)
                       (flush-text)
                       (case dir
                         ((#\a #\s #\d #\f)
                          (when (< i (length vals))
                            (multiple-value-bind (name type)
                                (compile-expr (nth i vals))
                              (let* ((fn (case dir
                                           (#\d ":print_i")
                                           (#\f ":print_d")
                                           (t (case type
                                                (:string ":print_s")
                                                (:double ":print_d")
                                                (t ":print_i")))))
                                     (operand (printable-string name type)))
                                (emitf "    (~a ~a !)" operand fn)))
                            (incf i)))
                         (#\% (emit "    (:print_n !)"))
                         (t (emitf "    // unsupported format directive ~~~a" dir))))
                     (progn (push (string ch) text) (incf pos)))))
      (flush-text))
    (values "zero" :int)))

(defun compile-quoted-value (datum)
  "Emit code that builds DATUM as a tagged runtime Lisp value."
  (cond
    ((integerp datum)
     (values (ensure-literal (tag-fix datum) :int) :lisp))
    ((floatp datum)
     ;; Floats are boxed as strings for now; they print correctly.
     (values (ensure-literal (tag-str (lisp-string-id (format nil "~a" datum))) :int) :lisp))
    ((symbolp datum)
     (values (ensure-literal (tag-sym (lisp-symbol-id datum)) :int) :lisp))
    ((stringp datum)
     (values (ensure-literal (tag-str (lisp-string-id datum)) :int) :lisp))
    ((consp datum)
     (need-runtime-fn "l1_cons")
     (multiple-value-bind (a) (compile-quoted-value (car datum))
       (multiple-value-bind (d) (compile-quoted-value (cdr datum))
         (emit-support-call-values "l1_cons"
                                   (list (list a :int) (list d :int))
                                   :int))))
    (t (values (ensure-literal +nil-value+ :int) :lisp))))

(defun compile-quote (expr)
  "Build real runtime cons-cell data for a quoted S-expression."
  (compile-quoted-value expr))

(defun compile-tagged-arg (expr)
  "Compile EXPR as a runtime tagged Lisp value.
   Integers and literals are boxed as fixnums; values already of type :lisp
   are passed through."
  (cond
    ((eq expr 'nil) (values (ensure-literal +nil-value+ :int) :lisp))
    ((eq expr 't) (compile-quoted-value 't))
    ((or (integerp expr) (floatp expr) (stringp expr) (characterp expr))
     (compile-quoted-value expr))
    ((and (consp expr) (eq (car expr) 'quote))
     (compile-quoted-value (second expr)))
    (t
     (multiple-value-bind (n ty) (compile-expr expr)
       (if (eq (normalize-type ty) :lisp)
           (values n :lisp)
           (let ((tmp (fresh-temp :lisp))
                 (eight (ensure-literal 8 :int)))
             (emitf "    (~a ~a * ~a :=)" n eight tmp)
             (values tmp :lisp)))))))

(defun compile-cons (args)
  (need-runtime-fn "l1_cons")
  (let ((vals nil))
    (dolist (a args)
      (multiple-value-bind (n ty) (compile-tagged-arg a)
        (push (list n ty) vals)))
    (emit-support-call-values "l1_cons" (reverse vals) :lisp)))

(defun compile-list (args)
  (need-runtime-fn "l1_cons")
  (let ((result (ensure-literal +nil-value+ :int)))
    (dolist (a (reverse args))
      (multiple-value-bind (v) (compile-tagged-arg a)
        (multiple-value-setq (result)
          (emit-support-call-values "l1_cons"
                                    (list (list v :lisp) (list result :lisp))
                                    :lisp))))
    (values result :lisp)))

(defun compile-unary-runtime (name args &optional (rtype :lisp) tagged-p)
  (need-runtime-fn name)
  (if tagged-p
      (multiple-value-bind (v ty) (compile-tagged-arg (car args))
        (emit-support-call-values name (list (list v ty)) rtype))
      (emit-support-call name (list (car args)) rtype)))

(defun compile-print-lisp (args)
  (need-runtime-fn "l1_print")
  (if (eq (normalize-type (guess-type (car args))) :lisp)
      (emit-support-call "l1_print" (list (car args)) nil)
      (multiple-value-bind (v ty) (compile-tagged-arg (car args))
        (emit-support-call-values "l1_print" (list (list v ty)) nil)))
  (values "zero" :int))

(defun compile-eval (args)
  (need-runtime-fn "l1_eval")
  (multiple-value-bind (v ty) (compile-tagged-arg (car args))
    (emit-support-call-values "l1_eval" (list (list v ty)) :lisp)))

(defun rec-result-global ()
  (format nil "l1_recresult_~a" (safe-name *current-recursive*)))

(defun rec-entry-label (&optional (name *current-recursive*))
  (format nil "~a_ENTRY" (safe-name name)))

(defun rec-body-label ()
  (format nil "~a_BODY" (safe-name *current-recursive*)))

;; Continuation labels are program-global: a mutually recursive callee must be
;; able to jump back into a different function's call site.
(defun rec-cont-label (id)
  (format nil "RET~a" id))

(defun mangle-rec-var (name fun)
  "Brackets #var ~ FUN renames NAME~ to NAMEFUN.  The global return dispatcher
   needs the mangled name to restore a different function's local."
  (if (and (plusp (length name)) (char= (char name (1- (length name))) #\~))
      (concatenate 'string (subseq name 0 (1- (length name)))
                   (safe-name fun))
      name))

(defun rec-push-lines (vars)
  (mapcar (lambda (v)
            (format nil "    (~a ~a)" (car v) (stack-push-opcode (cdr v))))
          vars))

(defun rec-pop-lines (vars)
  (mapcar (lambda (v)
            (format nil "    (~a ~a)" (car v) (stack-pop-opcode (cdr v))))
          (reverse vars)))

(defun rec-placeholder-p (line)
  (and (>= (length line) 16)
       (string= line "    @@REC_PUSH@@" :end1 16 :end2 16)))

(defun rec-placeholder-id (line)
  (let ((s (subseq line 16)))
    (read-from-string (string-trim " " s))))

(defun expand-rec-push-lines (lines frames)
  "Replace each call-site placeholder with pushes for that site's live vars."
  (mapcan (lambda (line)
            (if (rec-placeholder-p line)
                (let* ((id (rec-placeholder-id line))
                       (vars (or (cdr (assoc id frames)) nil)))
                  (rec-push-lines vars))
                (list line)))
          lines))

(defun line-identifiers (line)
  "Brackets identifiers (variable names, opcodes, labels) in LINE."
  (let ((ids nil)
        (buf (make-array 0 :element-type 'character
                           :adjustable t :fill-pointer 0)))
    (labels ((flush ()
               (when (plusp (fill-pointer buf))
                 (push (subseq buf 0 (fill-pointer buf)) ids)
                 (setf (fill-pointer buf) 0))))
      (loop for ch across line
            do (if (or (alphanumericp ch) (find ch "_~"))
                   (vector-push-extend ch buf)
                   (flush)))
      (flush))
    (nreverse ids)))

(defun line-var-uses (line)
  "Approximate the variables LINE reads.  Definitions (assignment targets and
   STPOP destinations) are removed.  We deliberately do not kill values: a
   superset of live variables is safe, a subset could corrupt a caller."
  (if (rec-declaration-line-p line)
      nil
      (let* ((ids (line-identifiers line))
             (uses (copy-list ids)))
        (cond ((some (lambda (x) (and (>= (length x) 5)
                                      (string= x "stpop" :end1 5)))
                     ids)
               (setf uses (remove (first ids) uses :count 1 :test #'string=)))
              (t
               (when ids
                 (setf uses (remove (car (last ids)) uses
                                    :count 1 :test #'string=)))))
        uses)))

(defun line-defined-label (line)
  "Return the label defined by a line like `(:LABEL)`, if any."
  (let ((s (string-trim " " line)))
    (when (and (>= (length s) 3) (char= (char s 0) #\() (char= (char s 1) #\:))
      (let ((end (position #\) s :start 2)))
        (when end (subseq s 2 end))))))

(defun line-jump-target (line)
  "Return the target of a line like `(:LABEL jmp)`, if any."
  (let ((s (string-trim " " line)))
    (when (and (>= (length s) 7) (char= (char s 0) #\() (char= (char s 1) #\:))
      (let ((end (position #\Space s :start 2)))
        (when (and end (string= (subseq s end) " jmp)"))
          (subseq s 2 end))))))

(defun compute-rec-frames (lines vars extra-use)
  "For every recursive call placeholder, collect the declared variables read
   after it.  EXTRA-USE is the function's final result variable, which is read
   by the shared return path rather than by the body itself.

   A call inside a loop can jump back to a label *before* the call, so values
   used earlier in the loop body are live across the call even though a purely
   linear scan would miss them.  For those sites we conservatively save every
   variable."
  (let ((frames nil))
    (loop for tail on lines
          for line = (car tail)
          when (rec-placeholder-p line)
            do (let* ((id (rec-placeholder-id line))
                      (before (ldiff lines tail))
                      (defined (remove nil (mapcar #'line-defined-label before)))
                      (jumps (remove nil (mapcar #'line-jump-target (cdr tail))))
                      (back-edge (intersection defined jumps :test #'string=))
                      (uses (if back-edge
                                (mapcar #'car vars)
                                (cons extra-use
                                      (loop for l in (cdr tail)
                                            append (line-var-uses l)))))
                      (live (remove-if-not
                             (lambda (v) (member (car v) uses :test #'string=))
                             vars)))
                 (push (cons id live) frames)))
    (nreverse frames)))

(defun expand-rec-size-lines (lines frames)
  "Replace frame-size placeholders with the literal size for that call site."
  (mapcan (lambda (line)
            (if (and (>= (length line) 16)
                     (string= line "    @@REC_SIZE@@" :end1 16 :end2 16))
                (let* ((id (read-from-string (string-trim " " (subseq line 16))))
                       (vars (or (cdr (assoc id frames)) nil)))
                  (list (format nil "    (~a stpushi)"
                                (ensure-literal (length vars) :int))))
                (list line)))
          lines))

(defun rec-dispatcher-code ()
  "One global function restores a saved frame and jumps to its continuation.
   Because continuation labels are program-global, this works for both self-
   recursion and mutually recursive groups."
  (if (null *rec-cont-frames*)
      nil
      (let* ((cont "rdcont~")
             (tmp "rdtmp~")
             (frames (reverse *rec-cont-frames*))
             (chain nil))
        (labels ((build-chain (fs)
                   (when fs
                     (append
                      (list (format nil "    (~a ~a == ~a :=)"
                                    cont (ensure-literal (car (first fs)) :int) tmp)
                            (format nil "    (~a if+)" tmp)
                            (format nil "    (:~a jmp)"
                                    (format nil "RDRET~a" (car (first fs))))
                            "    (else)")
                      (build-chain (rest fs))
                      (list "    (endif)")))))
          (setf chain (build-chain frames)))
        (append
         (list "(l1_rec_dispatch func)"
               "    #var ~ l1_rec_dispatch"
               "    (set int64 1 rdcont~ 0)"
               "    (set int64 1 rdtmp~ 0)"
               "    (set int64 1 rdresult~ 0)"
               "    (rdresult~ stpopi)"
               "    (rdcont~ stpopi)")
         chain
         (mapcan (lambda (frame)
                   (append
                    (list (format nil "    (:~a)" (format nil "RDRET~a" (car frame))))
                    (rec-pop-lines (cdr frame))
                    (list "    (rdresult~ stpushi)"
                          (format nil "    (:~a jmp)"
                                  (rec-cont-label (car frame))))))
                 frames)
         (list "(funcend)")))))

(defun rec-return-lines (last rtype frames)
  "Return path shared by all recursive functions.  A pending frame is unwound
   by the global dispatcher, which knows which caller's locals to restore."
  (declare (ignore frames))
  (let* ((depth (rec-depth-global))
         (result (rec-result-global))
         (one (ensure-literal 1 :int))
         (n (incf *label-count*))
         (dispatch (format nil "RECDISPATCH~a" n))
         (done (format nil "RECRETURN~a" n)))
    ;; The combined call must be outside the IF block: Brackets saves live
    ;; registers around calls inside a conditional, and the dispatcher never
    ;; returns, so those saved registers would stay on the VM stack and be
    ;; mistaken for frame variables during restore.
    (list (format nil "    (~a if+)" depth)
          (format nil "    (~a ~a :=)" last result)
          (format nil "    (~a stpopi)" *rec-tmp-var*)  ; saved size
          (format nil "    (~a stpopi)" *rec-cont-var*) ; continuation id
          (format nil "    (~a ~a - ~a :=)" depth one depth)
          (format nil "    (:~a jmp)" dispatch)
          "    (else)"
          (format nil "    (~a ~a)" last (stack-push-opcode rtype))
          (format nil "    (:~a jmp)" done)
          "    (endif)"
          (format nil "    (:~a)" dispatch)
          (format nil "    (~a ~a :l1_rec_dispatch !)" *rec-cont-var* result)
          (format nil "    (:~a)" done))))


(defun emit-recursive-call (fi arg-vals callee)
  "Emit a frame-saving call to CALLEE with already compiled ARG-VALS."
  (let* ((rtype (getf fi :return))
         (params (getf fi :params))
         (res (fresh-temp rtype))
         (cont (incf *rec-cont-count*)))
    (unless (member (normalize-type rtype) '(:int :lisp :string))
      (error "Recursive function ~a must return int64, string or lisp, not ~a"
             callee rtype))
    (unless (= (length arg-vals) (length params))
      (error "Function ~a expects ~a arguments, got ~a"
             callee (length params) (length arg-vals)))
    (push cont *rec-cont-ids*)
    (emit (format nil "    @@REC_PUSH@@~a" cont))
    (emitf "    (~a stpushi)" (ensure-literal cont :int))
    (emit (format nil "    @@REC_SIZE@@~a" cont))
    ;; Match the normal call convention: last argument pushed first, so the
    ;; first Common Lisp argument is on top for the callee's prologue.
    (loop for pair in (reverse arg-vals)
          for pp in (reverse params)
          for n = (first pair)
          for ptype = (cdr pp)
          do (emitf "    (~a ~a)" n (stack-push-opcode ptype)))
    (let ((one (ensure-literal 1 :int)))
      (emitf "    (~a ~a :=)" (rec-depth-global) *rec-sp-var*)
      (emitf "    (~a ~a + ~a :=)" *rec-sp-var* one *rec-sp-var*)
      (emitf "    (~a ~a :=)" *rec-sp-var* (rec-depth-global)))
    (emitf "    (:~a jmp)" (rec-entry-label callee))
    (emitf "(:~a)" (rec-cont-label cont))
    (emit-call-result res rtype)
    (values res rtype)))

(defun compile-recursive-call (fi args callee)
  (let ((arg-vals nil))
    (dolist (a args)
      (multiple-value-bind (n ty) (compile-expr-nontail a)
        (push (list n ty) arg-vals)))
    (emit-recursive-call fi (nreverse arg-vals) callee)))


(defun compile-tail-recursive-call (fi args)
  "Lower a self call in tail position by reusing the current activation.
   All new arguments are evaluated before any parameter slot is overwritten,
   then control jumps past the parameter prologue to the body label.  No
   continuation id, saved locals or extra VM stack space are needed."
  (let* ((params (getf fi :params))
         (rtype (normalize-type (getf fi :return))))
    (unless (= (length args) (length params))
      (error "Function ~a expects ~a arguments, got ~a"
             *current-recursive* (length params) (length args)))
    (unless (member rtype '(:int :lisp :string))
      (error "Tail-recursive call in ~a needs an int64, string or lisp return type, not ~a"
             *current-recursive* rtype))
    (let ((vals nil))
      (loop for a in args
            for p in params
            for ptype = (normalize-type (cdr p))
            do (unless (member ptype '(:int :lisp :string))
                 (error "Tail-recursive call in ~a needs int64, string or lisp parameters, not ~a"
                        *current-recursive* ptype))
               (multiple-value-bind (n ty)
                   (if (eq ptype :lisp)
                       (let ((*tail-position* nil)) (compile-tagged-arg a))
                       (compile-expr-nontail a))
                 (declare (ignore ty))
                 (push n vals)))
      (setf vals (reverse vals))
      ;; Assign only after every argument has been evaluated.  Brackets `:=`
      ;; cannot fail, so the assignments cannot corrupt the other temporaries.
      (loop for v in vals
            for p in params
            do (emitf "    (~a ~a :=)" v (var-name (car p))))
      (emitf "    (:~a jmp)" (rec-body-label))
      ;; The value is produced by the jump target; this name is never read
      ;; (any assignment emitted after the jump is unreachable).
      (values "zero" rtype))))

(defun compile-call (op args &optional discard)
  (let ((fi (gethash op *functions*)))
    (unless fi
      (error 'lisp-compile-error
             :file *source-file*
             :line (or (source-line-of-symbol op) *current-source-line* 1)
             :detail (format nil "Unknown function: ~a" op)))
    (if (and *current-recursive* (rec-same-group-p op *current-recursive*))
        ;; Tail-call reuse is only safe for a direct self call: a peer in the
        ;; group may have different parameters and its own local variables.
        (if (and *tail-position* (eq op *current-recursive*))
            (compile-tail-recursive-call fi args)
            (compile-recursive-call fi args op))
        (let* ((rtype (getf fi :return))
               (vals nil))
          (dolist (a args)
            (multiple-value-bind (n ty) (compile-expr-nontail a)
              (push (list n ty) vals)))
          (setf vals (reverse vals))
          ;; Combined call form pushes arguments left-to-right.  Emit them in
          ;; reverse CL order so the first argument ends up on top and the
          ;; function prologue can pop parameters in CL order.
          (let ((emit-vals (reverse vals)))
            (if emit-vals
                (emitf "    (~{~a~^ ~} :~a !)"
                       (mapcar #'car emit-vals) (safe-name op))
                (emitf "    (:~a !)" (safe-name op))))
          (let ((res (if discard (discard-scratch rtype) (fresh-temp rtype))))
            (emit-call-result res rtype)
            (values res rtype))))))

(defun compile-lambda-call (lambda-form args)
  "Inline-apply a compile-time LAMBDA form.  This is closure-like in the
   sense that the body can refer to lexically visible variables; we do not
   need a runtime function object."
  (let ((params (second lambda-form))
        (body (cddr lambda-form)))
    (unless (= (length params) (length args))
      (error "LAMBDA expects ~a arguments, got ~a" (length params) (length args)))
    (compile-let (cons (mapcar #'list params args) body) nil)))

(defun function-designator-name (designator)
  (designator-symbol designator))

(defun compile-funcall (args)
  (let ((designator (first args))
        (call-args (cdr args)))
    (cond
      ((lambda-form-p designator)
       (compile-lambda-call designator call-args))
      ((and (symbolp designator) (assoc designator *lambda-env*))
       (compile-lambda-call (cdr (assoc designator *lambda-env*)) call-args))
      (t
       (let ((name (function-designator-name designator)))
         (unless name
           (error "FUNCALL needs #'name, 'name, or a let-bound lambda"))
         (compile-call name call-args))))))

(defun compile-mapcar (args)
  "Compile (MAPCAR #'F LIST) as a flat loop.  Only a unary named function
   and one list are supported.  If F belongs to the current recursive group,
   the element call uses the frame-saving lowering rather than a real call."
  (unless (= (length args) 2)
    (error "MAPCAR supports exactly one function and one list"))
  (let* ((name (function-designator-name (first args)))
         (fi (and name (gethash name *functions*))))
    (unless name (error "MAPCAR needs a named function designator"))
    (unless fi (error "MAPCAR: unknown function ~a" name))
    (let ((recursive-mapcall
            (and *current-recursive* (rec-same-group-p name *current-recursive*))))
      (let* ((params (getf fi :params))
             (rtype (normalize-type (getf fi :return))))
        (unless (= (length params) 1)
          (error "MAPCAR currently supports unary functions only"))
        (let ((ptype (normalize-type (cdr (first params)))))
          (unless (member ptype '(:int :lisp))
            (error "MAPCAR supports only int64 or lisp function arguments"))
          (unless (member rtype '(:int :lisp))
            (error "MAPCAR supports only int64 or lisp function results"))
          (need-runtime-fn "l1_consp")
          (need-runtime-fn "l1_car")
          (need-runtime-fn "l1_cdr")
          (need-runtime-fn "l1_cons")
          (multiple-value-bind (src stype) (compile-tagged-arg (second args))
            (declare (ignore stype))
            (let* ((cursor (fresh-temp :lisp))
                   (res (fresh-temp :lisp))
                   (out (fresh-temp :lisp))
                   (arg (fresh-temp ptype))
                   (eight (ensure-literal 8 :int))
                   (nilval (ensure-literal +nil-value+ :int))
                   (n (incf *label-count*))
                   (start (format nil "MAPSTART~a" n))
                   (rev (format nil "MAPREV~a" n)))
              (emitf "    (~a ~a :=)" src cursor)
              (emitf "    (~a ~a :=)" nilval res)
              (emitf "    (:~a)" start)
              (let ((flag (emit-support-call-values
                           "l1_consp" (list (list cursor :lisp)) :int)))
                (emitf "    (~a if+)" flag))
              (let ((cell (emit-support-call-values
                           "l1_car" (list (list cursor :lisp)) :lisp)))
                (if (eq ptype :int)
                    (emitf "    (~a ~a / ~a :=)" cell eight arg)
                    (emitf "    (~a ~a :=)" cell arg))
                (let ((result
                        (if recursive-mapcall
                            (multiple-value-bind (v ty)
                                (emit-recursive-call fi (list (list arg ptype)) name)
                              (declare (ignore ty))
                              v)
                            (progn
                              (emitf "    (~a :~a !)" arg (safe-name name))
                              (let ((v (fresh-temp rtype)))
                                (emit-call-result v rtype)
                                v)))))
                  (let ((tagged
                          (if (eq rtype :int)
                              (let ((tv (fresh-temp :lisp)))
                                (emitf "    (~a ~a * ~a :=)" result eight tv)
                                tv)
                              result)))
                    (let ((newres (emit-support-call-values
                                   "l1_cons"
                                   (list (list tagged :lisp) (list res :lisp))
                                   :lisp)))
                      (emitf "    (~a ~a :=)" newres res)))))
              (let ((newcur (emit-support-call-values
                             "l1_cdr" (list (list cursor :lisp)) :lisp)))
                (emitf "    (~a ~a :=)" newcur cursor))
              (emitf "    (:~a jmp)" start)
              (emit "    (else)")
              (emit "    (endif)")
              ;; The cons loop built the result backwards; reverse it.
              (emitf "    (~a ~a :=)" nilval out)
              (emitf "    (:~a)" rev)
              (let ((flag (emit-support-call-values
                           "l1_consp" (list (list res :lisp)) :int)))
                (emitf "    (~a if+)" flag))
              (let ((cell (emit-support-call-values
                           "l1_car" (list (list res :lisp)) :lisp)))
                (let ((newout (emit-support-call-values
                               "l1_cons"
                               (list (list cell :lisp) (list out :lisp))
                               :lisp)))
                  (emitf "    (~a ~a :=)" newout out)))
              (let ((newres (emit-support-call-values
                             "l1_cdr" (list (list res :lisp)) :lisp)))
                (emitf "    (~a ~a :=)" newres res))
              (emitf "    (:~a jmp)" rev)
              (emit "    (else)")
              (emit "    (endif)")
              (values out :lisp))))))))

(defun compile-type-predicate (args types &optional lisp-value)
  "Compile a compile-time-resolved type predicate used by L1VM Lisp.
   The generated VM has no heap-wide generic type tags, so predicates on
   expressions with a known static type become constants."
  (multiple-value-bind (name ty) (compile-expr-nontail (car args))
    (declare (ignore name))
    (let ((ty (normalize-type ty)))
      (values (ensure-literal
               (if (or (member ty types)
                       (and lisp-value (eq ty :lisp)))
                   1 0)
               :int)
              :int))))

(defun compile-form (form)
  (let ((op (car form))
        (args (cdr form))
        ;; Only this form's own result may be discarded: subexpressions still
        ;; feed an enclosing operation and need their own result variables.
        (discard *discard-value*))
    ;; Tail position only flows through forms that return the value of a
    ;; subform directly.  Everything else consumes its operands, so a self
    ;; call there must use the ordinary frame-saving path.
    (let ((*discard-value* nil)
          (*tail-position*
            (and *tail-position*
                 (or (and (consp op) (lambda-form-p op))
                     (gethash op *functions*)
                     (member op '(if cond when unless progn let let* macrolet
                                     case funcall quote))))))
      (cond
      ((and (consp op) (lambda-form-p op))
       (compile-lambda-call op args))
      ((or (op= op "+") (op= op "-") (op= op "*")
           (op= op "/") (op= op "mod"))
       (compile-arith (if (op= op "mod") '% (intern (symbol-name op) :cl2brackets))
                      args))
      ((op= op "1+") (compile-arith '+ (list (car args) 1)))
      ((op= op "1-") (compile-arith '- (list (car args) 1)))
      ((or (op= op "max") (op= op "min"))
       (let ((best (compile-expr (car args))))
         (dolist (a (cdr args))
           (let* ((v (compile-expr a))
                  (flag (fresh-temp :int))
                  (bop (if (op= op "max") '< '>))
                  (res (fresh-temp (guess-type a))))
             (emitf "    (~a ~a ~a ~a :=)" best bop v flag)
             (emitf "    (~a if+)" flag)
             (emitf "    (~a ~a :=)" v res)
             (emit "    (else)")
             (emitf "    (~a ~a :=)" best res)
             (emit "    (endif)")
             (setf best res)))
         (values best :int)))
      ((op= op "abs")
       (multiple-value-bind (v ty) (compile-expr (car args))
         (let ((res (fresh-temp ty)))
           (emitf "    (zero ~a > ~a :=)" v res)
           (emitf "    (~a if+)" res)
           (emitf "    (zero ~a - ~a :=)" v res)
           (emit "    (else)")
           (emitf "    (~a ~a :=)" v res)
           (emit "    (endif)")
           (values res ty))))
      ((or (op= op "=") (op= op "/=") (op= op "<") (op= op ">")
           (op= op "<=") (op= op ">=") (op= op "eq") (op= op "eql")
           (op= op "equal"))
       (let ((name (symbol-name op)))
         (compile-compare (intern (cond ((or (string= name "EQ")
                                             (string= name "EQL")
                                             (string= name "EQUAL")
                                             (string= name "=")) "==")
                                        ((string= name "/=") "!=")
                                        (t name))
                                   :cl2brackets)
                          args)))
      ((op= op "and") (compile-and args))
      ((op= op "or") (compile-or args))
      ((op= op "not") (compile-not (car args)))
      ((op= op "zerop") (compile-compare '== (list (car args) 0)))
      ((or (op= op "integerp") (op= op "numberp"))
       (compile-type-predicate args '(:int :double)))
      ((op= op "doublep") (compile-type-predicate args '(:double)))
      ((op= op "stringp") (compile-type-predicate args '(:string)))
      ((op= op "symbolp") (compile-type-predicate args nil t))
      ((op= op "procedurep") (compile-type-predicate args nil nil))
      ((op= op "int->double")
       (multiple-value-bind (v ty) (compile-expr (car args))
         (values (coerce-to-double v ty) :double)))
      ((op= op "double->int")
       (multiple-value-bind (v ty) (compile-expr (car args))
         (if (eq (normalize-type ty) :double)
             (values (convert-double-to-int v) :int)
             (values v :int))))
      ((or (op= op "quit") (op= op "exit"))
       (emit "    (zero :exit !)")
       (values "zero" :int))
      ((or (op= op "evenp") (op= op "oddp"))
       (let* ((v (compile-expr (car args)))
              (two (ensure-literal 2 :int))
              (r (fresh-temp :int))
              (flag (fresh-temp :int)))
         (emitf "    (~a ~a % ~a :=)" v two r)
         (emitf "    (~a zero == ~a :=)" r flag)
         (when (op= op "oddp")
           (let ((nf (fresh-temp :int)))
             (emitf "    (~a zero == ~a :=)" flag nf)
             (setf flag nf)))
         (values flag :int)))
      ((op= op "if") (compile-if args))
      ((op= op "cond") (compile-expr (cond->if args)))
      ((op= op "when")
       (compile-expr (list 'if (car args) (cons 'progn (cdr args)))))
      ((op= op "unless")
       (compile-expr (list 'if (list 'not (car args))
                           (cons 'progn (cdr args)))))
      ((op= op "progn") (compile-progn args))
      ((op= op "let") (compile-let args nil))
      ((op= op "let*") (compile-let args t))
      ((op= op "macrolet") (compile-macrolet args))
      ((or (op= op "setq") (op= op "setf")) (compile-setq args))
      ((op= op "incf") (compile-incf args t))
      ((op= op "decf") (compile-incf args nil))
      ((or (op= op "while") (op= op "loop-while")) (compile-while args))
      ((op= op "dotimes") (compile-dotimes args))
      ((op= op "do") (compile-do args))
      ((op= op "loop") (compile-loop args))
      ((op= op "case") (compile-expr (case->cond args)))
      ((op= op "quote") (compile-quote (car args)))
      ((op= op "cons") (compile-cons args))
      ((op= op "list") (compile-list args))
      ((or (op= op "car") (op= op "first")) (compile-unary-runtime "l1_car" args))
      ((or (op= op "cdr") (op= op "rest")) (compile-unary-runtime "l1_cdr" args))
      ((op= op "null") (compile-unary-runtime "l1_null" args :int t))
      ((or (op= op "consp") (op= op "pairp")) (compile-unary-runtime "l1_consp" args :int t))
      ((or (op= op "print-lisp") (op= op "printl")) (compile-print-lisp args))
      ((op= op "eval") (compile-eval args))
      ((op= op "funcall") (compile-funcall args))
      ((op= op "mapcar") (compile-mapcar args))
      ((net-builtin-op-p op) (compile-net-builtin op args))
      ((string-builtin-op-p op) (compile-string-builtin op args))
      ((file-builtin-op-p op) (compile-file-builtin op args))
      ((sdl-builtin-op-p op) (compile-sdl-builtin op args))
      ((op= op "print") (compile-print args t))
      ((op= op "princ") (compile-print args nil))
      ((or (op= op "terpri") (op= op "print-line") (op= op "newline"))
       (compile-print nil t))
      ((op= op "format") (compile-format args))
      (t (compile-call op args discard))))))

;;; ---------------------------------------------------------------------------
;;; Generated Lisp-data runtime
;;; ---------------------------------------------------------------------------
;;; Everything here uses flat arrays and loops only: no Brackets recursion.

(defun runtime-function-name (lines)
  "Extract the function name from the first line of a runtime block."
  (let* ((first (first lines))
         (end (and first (position #\space first))))
    (when end (subseq first 1 end))))

(defun emit-runtime (lines)
  (when (runtime-fn-needed-p (runtime-function-name lines))
    (dolist (l (reverse lines))
      (push l *runtime-code*))))

(defun runtime-declarations ()
  ;; The heap arrays are only touched by cons/car/cdr.  Programs that use just
  ;; NIL tests or the evaluator dispatch do not need them.
  (append
   (when (or (runtime-fn-needed-p "l1_cons")
             (runtime-fn-needed-p "l1_car")
             (runtime-fn-needed-p "l1_cdr"))
     (list "(set int64 20000 l1_cararr)"
           "(set int64 20000 l1_cdrarr)"
           "(set int64 2 l1_hparr 1 0)"))
   (when (runtime-fn-needed-p "l1_eval")
     (list "(set int64 32 l1_evalargs)"))))

(defun generate-runtime-code ()
  (unless *runtime-needed*
    (return-from generate-runtime-code nil))
  ;; Only the evaluator needs the builtin symbol ids; avoid registering their
  ;; printable literals for programs that never call EVAL.
  (when (runtime-fn-needed-p "l1_eval")
    (ensure-eval-symbols))
  (let* ((zero (ensure-literal 0 :int))
         (one (ensure-literal 1 :int))
         (eight (ensure-literal 8 :int))
         (tag-fix (ensure-literal +tag-fix+ :int))
         (tag-cons (ensure-literal +tag-cons+ :int))
         (nilval (ensure-literal +nil-value+ :int))
         ;; Lisp printer literals.
         (need-print (runtime-fn-needed-p "l1_print"))
         (lparen (and need-print (ensure-literal 40 :int)))
         (rparen (and need-print (ensure-literal 41 :int)))
         (fmt (and need-print (ensure-literal "%c" :string)))
         (space (and need-print (ensure-literal " " :string)))
         (dot (and need-print (ensure-literal " . " :string)))
         (quest (and need-print (ensure-literal "?" :string)))
         ;; Evaluator dispatch literals.
         (need-eval (runtime-fn-needed-p "l1_eval"))
         (sym-plus (and need-eval (ensure-literal (tag-sym (lisp-symbol-id '+)) :int)))
         (sym-minus (and need-eval (ensure-literal (tag-sym (lisp-symbol-id '-)) :int)))
         (sym-times (and need-eval (ensure-literal (tag-sym (lisp-symbol-id '*)) :int)))
         (sym-div (and need-eval (ensure-literal (tag-sym (lisp-symbol-id '/)) :int)))
         (sym-eq (and need-eval (ensure-literal (tag-sym (lisp-symbol-id '=)) :int)))
         (sym-lt (and need-eval (ensure-literal (tag-sym (lisp-symbol-id '<)) :int)))
         (sym-cons (and need-eval (ensure-literal (tag-sym (lisp-symbol-id 'cons)) :int)))
         (sym-list (and need-eval (ensure-literal (tag-sym (lisp-symbol-id 'list)) :int)))
         (sym-car (and need-eval (ensure-literal (tag-sym (lisp-symbol-id 'car)) :int)))
         (sym-cdr (and need-eval (ensure-literal (tag-sym (lisp-symbol-id 'cdr)) :int)))
         (sym-null (and need-eval (ensure-literal (tag-sym (lisp-symbol-id 'null)) :int)))
         (sym-consp (and need-eval (ensure-literal (tag-sym (lisp-symbol-id 'consp)) :int)))
         (sym-quote (and need-eval (ensure-literal (tag-sym (lisp-symbol-id 'quote)) :int)))
         (sym-t (and need-eval (ensure-literal (tag-sym (lisp-symbol-id 't)) :int))))

    ;; cons
    (emit-runtime
     (list "(l1_cons func)" "    #var ~ l1_cons"
           "    (set int64 1 a~ 0)" "    (set int64 1 b~ 0)"
           "    (set int64 1 idx~ 0)" "    (set int64 1 off~ 0)"
           "    (set int64 1 res~ 0)" "    (set int64 1 next~ 0)"
           "    (b~ stpopi)" "    (a~ stpopi)"
           (format nil "    (l1_hparr [ ~a ] idx~~ =)" zero)
           (format nil "    (idx~~ ~a + next~~ :=)" one)
           (format nil "    (idx~~ ~a * off~~ :=)" eight)
           (format nil "    (idx~~ ~a * res~~ :=)" eight)
           (format nil "    (~a res~~ + res~~ :=)" tag-cons)
           ;; Push the result before the array writes: the array-write code
           ;; uses temporary registers and must not be trusted to preserve
           ;; our locals.
           "    (res~ stpushi)"
           "    (a~ l1_cararr [ off~ ] =)"
           "    (b~ l1_cdrarr [ off~ ] =)"
           (format nil "    (next~~ l1_hparr [ ~a ] =)" zero)
           "(funcend)" ""))

    ;; car
    (let ((end (format nil "L1CAREND")))
      (emit-runtime
       (list "(l1_car func)" "    #var ~ l1_car"
             "    (set int64 1 x~ 0)" "    (set int64 1 tag~ 0)"
             "    (set int64 1 idx~ 0)" "    (set int64 1 off~ 0)"
             "    (set int64 1 flag~ 0)" "    (set int64 1 res~ 0)"
             "    (x~ stpopi)"
             (format nil "    (x~~ ~a % tag~~ :=)" eight)
             (format nil "    (tag~~ ~a == flag~~ :=)" tag-cons)
             "    (flag~ if+)"
             (format nil "        (x~~ ~a / idx~~ :=)" eight)
             (format nil "        (idx~~ ~a * off~~ :=)" eight)
             "        (l1_cararr [ off~ ] res~ =)"
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             (format nil "        (~a stpushi)" nilval)
             "    (endif)"
             (format nil "    (:~a)" end)
             "(funcend)" "")))

    ;; cdr
    (let ((end (format nil "L1CDREND")))
      (emit-runtime
       (list "(l1_cdr func)" "    #var ~ l1_cdr"
             "    (set int64 1 x~ 0)" "    (set int64 1 tag~ 0)"
             "    (set int64 1 idx~ 0)" "    (set int64 1 off~ 0)"
             "    (set int64 1 flag~ 0)" "    (set int64 1 res~ 0)"
             "    (x~ stpopi)"
             (format nil "    (x~~ ~a % tag~~ :=)" eight)
             (format nil "    (tag~~ ~a == flag~~ :=)" tag-cons)
             "    (flag~ if+)"
             (format nil "        (x~~ ~a / idx~~ :=)" eight)
             (format nil "        (idx~~ ~a * off~~ :=)" eight)
             "        (l1_cdrarr [ off~ ] res~ =)"
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             (format nil "        (~a stpushi)" nilval)
             "    (endif)"
             (format nil "    (:~a)" end)
             "(funcend)" "")))

    ;; eval: flat S-expression evaluator.  Arguments may be atoms or
    ;; (quote X) forms; nested calls are intentionally not evaluated.
    ;; Evaluated arguments are kept on the VM stack, so array-write
    ;; register clobbering cannot damage the loop counters.
    (let ((argloop "L1EVALARGS")
          (listloop "L1EVALLIST")
          (cleanloop "L1EVALCLEAN")
          (end "L1EVALEND"))
      (emit-runtime
       (list "(l1_eval func)" "    #var ~ l1_eval"
             "    (set int64 1 x~ 0)" "    (set int64 1 rest~ 0)"
             "    (set int64 1 op~ 0)" "    (set int64 1 arg~ 0)"
             "    (set int64 1 val~ 0)" "    (set int64 1 cnt~ 0)"
             "    (set int64 1 i~ 0)" "    (set int64 1 a~ 0)"
             "    (set int64 1 b~ 0)" "    (set int64 1 res~ 0)"
             "    (set int64 1 flag~ 0)" "    (set int64 1 tmp~ 0)"
             "    (x~ stpopi)"
             ;; NIL is self-evaluating.
             (format nil "    (x~~ ~a == flag~~ :=)" nilval)
             "    (flag~ if+)"
             (format nil "        (~a stpushi)" nilval)
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"
             ;; Atoms other than NIL evaluate to themselves.
             "    (x~ :l1_consp !)"
             "    (tmp~ stpopi)"
             (format nil "    (~a tmp~~ == flag~~ :=)" zero)
             "    (flag~ if+)"
             "        (x~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"
             ;; Split the form into operator and argument list.
             "    (x~ :l1_car !)"
             "    (op~ stpopi)"
             "    (x~ :l1_cdr !)"
             "    (rest~ stpopi)"
             (format nil "    (~a cnt~~ :=)" zero)
             (format nil "    (:~a)" argloop)
             "    (rest~ :l1_consp !)"
             "    (tmp~ stpopi)"
             "    (tmp~ if+)"
             "        (rest~ :l1_car !)"
             "        (arg~ stpopi)"
             ;; Unwrap (quote X) arguments.
             "        (arg~ :l1_consp !)"
             "        (tmp~ stpopi)"
             "        (tmp~ if+)"
             "            (arg~ :l1_car !)"
             "            (a~ stpopi)"
             (format nil "            (a~~ ~a == flag~~ :=)" sym-quote)
             "            (flag~ if+)"
             "                (arg~ :l1_cdr !)"
             "                (val~ stpopi)"
             "                (val~ :l1_car !)"
             "                (val~ stpopi)"
             "            (else)"
             "                (arg~ val~ :=)"
             "            (endif)"
             "        (else)"
             "            (arg~ val~ :=)"
             "        (endif)"
             "        (val~ stpushi)"
             (format nil "        (cnt~~ ~a + cnt~~ :=)" one)
             "        (rest~ :l1_cdr !)"
             "        (rest~ stpopi)"
             (format nil "        (:~a jmp)" argloop)
             "    (else)"
             "    (endif)"

             ;; (+ a b)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-plus)
             "    (flag~ if+)"
             "        (b~ stpopi)"
             "        (a~ stpopi)"
             (format nil "        (a~~ ~a / a~~ :=)" eight)
             (format nil "        (b~~ ~a / b~~ :=)" eight)
             "        (a~ b~ + res~ :=)"
             (format nil "        (res~~ ~a * res~~ :=)" eight)
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"

             ;; (- a b)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-minus)
             "    (flag~ if+)"
             "        (b~ stpopi)"
             "        (a~ stpopi)"
             (format nil "        (a~~ ~a / a~~ :=)" eight)
             (format nil "        (b~~ ~a / b~~ :=)" eight)
             "        (a~ b~ - res~ :=)"
             (format nil "        (res~~ ~a * res~~ :=)" eight)
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"

             ;; (* a b)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-times)
             "    (flag~ if+)"
             "        (b~ stpopi)"
             "        (a~ stpopi)"
             (format nil "        (a~~ ~a / a~~ :=)" eight)
             (format nil "        (b~~ ~a / b~~ :=)" eight)
             "        (a~ b~ * res~ :=)"
             (format nil "        (res~~ ~a * res~~ :=)" eight)
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"

             ;; (/ a b)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-div)
             "    (flag~ if+)"
             "        (b~ stpopi)"
             "        (a~ stpopi)"
             (format nil "        (a~~ ~a / a~~ :=)" eight)
             (format nil "        (b~~ ~a / b~~ :=)" eight)
             "        (a~ b~ / res~ :=)"
             (format nil "        (res~~ ~a * res~~ :=)" eight)
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"

             ;; (= a b)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-eq)
             "    (flag~ if+)"
             "        (b~ stpopi)"
             "        (a~ stpopi)"
             "        (a~ b~ == flag~ :=)"
             "        (flag~ if+)"
             (format nil "            (~a stpushi)" sym-t)
             (format nil "            (:~a jmp)" end)
             "        (else)"
             (format nil "            (~a stpushi)" nilval)
             (format nil "            (:~a jmp)" end)
             "        (endif)"
             "    (else)"
             "    (endif)"

             ;; (< a b)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-lt)
             "    (flag~ if+)"
             "        (b~ stpopi)"
             "        (a~ stpopi)"
             (format nil "        (a~~ ~a / a~~ :=)" eight)
             (format nil "        (b~~ ~a / b~~ :=)" eight)
             "        (a~ b~ < flag~ :=)"
             "        (flag~ if+)"
             (format nil "            (~a stpushi)" sym-t)
             (format nil "            (:~a jmp)" end)
             "        (else)"
             (format nil "            (~a stpushi)" nilval)
             (format nil "            (:~a jmp)" end)
             "        (endif)"
             "    (else)"
             "    (endif)"

             ;; (cons a b)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-cons)
             "    (flag~ if+)"
             "        (b~ stpopi)"
             "        (a~ stpopi)"
             "        (a~ b~ :l1_cons !)"
             "        (res~ stpopi)"
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"

             ;; (list ...)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-list)
             "    (flag~ if+)"
             (format nil "        (~a res~~ :=)" nilval)
             "        (cnt~ i~ :=)"
             (format nil "        (:~a)" listloop)
             (format nil "        (i~~ ~a == flag~~ :=)" zero)
             "        (flag~ if+)"
             "            (res~ stpushi)"
             (format nil "            (:~a jmp)" end)
             "        (else)"
             "        (endif)"
             "        (a~ stpopi)"
             "        (a~ res~ :l1_cons !)"
             "        (res~ stpopi)"
             (format nil "        (i~~ ~a - i~~ :=)" one)
             (format nil "        (:~a jmp)" listloop)
             "    (else)"
             "    (endif)"

             ;; (car x)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-car)
             "    (flag~ if+)"
             "        (a~ stpopi)"
             "        (a~ :l1_car !)"
             "        (res~ stpopi)"
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"

             ;; (cdr x)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-cdr)
             "    (flag~ if+)"
             "        (a~ stpopi)"
             "        (a~ :l1_cdr !)"
             "        (res~ stpopi)"
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"

             ;; (null x)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-null)
             "    (flag~ if+)"
             "        (a~ stpopi)"
             "        (a~ :l1_null !)"
             "        (res~ stpopi)"
             "        (res~ if+)"
             (format nil "            (~a stpushi)" sym-t)
             (format nil "            (:~a jmp)" end)
             "        (else)"
             (format nil "            (~a stpushi)" nilval)
             (format nil "            (:~a jmp)" end)
             "        (endif)"
             "    (else)"
             "    (endif)"

             ;; (consp x)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-consp)
             "    (flag~ if+)"
             "        (a~ stpopi)"
             "        (a~ :l1_consp !)"
             "        (res~ stpopi)"
             "        (res~ if+)"
             (format nil "            (~a stpushi)" sym-t)
             (format nil "            (:~a jmp)" end)
             "        (else)"
             (format nil "            (~a stpushi)" nilval)
             (format nil "            (:~a jmp)" end)
             "        (endif)"
             "    (else)"
             "    (endif)"

             ;; (quote x)
             (format nil "    (op~~ ~a == flag~~ :=)" sym-quote)
             "    (flag~ if+)"
             "        (res~ stpopi)"
             "        (res~ stpushi)"
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"

             ;; Unknown operator: discard arguments, return NIL.
             (format nil "    (:~a)" cleanloop)
             (format nil "    (cnt~~ ~a == flag~~ :=)" zero)
             "    (flag~ if+)"
             (format nil "        (~a stpushi)" nilval)
             (format nil "        (:~a jmp)" end)
             "    (else)"
             "    (endif)"
             "    (a~ stpopi)"
             (format nil "    (cnt~~ ~a - cnt~~ :=)" one)
             (format nil "    (:~a jmp)" cleanloop)
             (format nil "    (:~a)" end)
             "(funcend)" "")))

    ;; null / consp
    (emit-runtime
     (list "(l1_null func)" "    #var ~ l1_null"
           "    (set int64 1 x~ 0)" "    (set int64 1 res~ 0)"
           "    (x~ stpopi)"
           (format nil "    (x~~ ~a == res~~ :=)" nilval)
           "    (res~ stpushi)" "(funcend)" ""))
    (emit-runtime
     (list "(l1_consp func)" "    #var ~ l1_consp"
           "    (set int64 1 x~ 0)" "    (set int64 1 tag~ 0)"
           "    (set int64 1 res~ 0)"
           "    (x~ stpopi)"
           (format nil "    (x~~ ~a % tag~~ :=)" eight)
           (format nil "    (tag~~ ~a == res~~ :=)" tag-cons)
           "    (res~ stpushi)" "(funcend)" ""))

    ;; print_atom: no recursion, just a flat dispatch chain.
    (let ((end "L1ATOMEND"))
      (let ((lines
              (list "(l1_print_atom func)" "    #var ~ l1_print_atom"
                    "    (set int64 1 x~ 0)" "    (set int64 1 tag~ 0)"
                    "    (set int64 1 flag~ 0)" "    (set int64 1 n~ 0)"
                    "    (x~ stpopi)"
                    (format nil "    (x~~ ~a % tag~~ :=)" eight)
                    (format nil "    (tag~~ ~a == flag~~ :=)" tag-fix)
                    "    (flag~ if+)"
                    (format nil "        (x~~ ~a / n~~ :=)" eight)
                    "        (n~ :print_i !)"
                    (format nil "        (:~a jmp)" end)
                    "    (else)"
                    "    (endif)")))
        (dolist (pair (sort (copy-list *lisp-symbol-names*) #'< :key #'car))
          (let ((val (ensure-literal (tag-sym (car pair)) :int)))
            (setf lines
                  (append lines
                          (list (format nil "    (x~~ ~a == flag~~ :=)" val)
                                "    (flag~ if+)"
                                (format nil "        (~a :print_s !)" (cdr pair))
                                (format nil "        (:~a jmp)" end)
                                "    (else)"
                                "    (endif)")))))
        (dolist (pair (sort (copy-list *lisp-string-texts*) #'< :key #'car))
          (let ((val (ensure-literal (tag-str (car pair)) :int)))
            (setf lines
                  (append lines
                          (list (format nil "    (x~~ ~a == flag~~ :=)" val)
                                "    (flag~ if+)"
                                (format nil "        (~a :print_s !)" (cdr pair))
                                (format nil "        (:~a jmp)" end)
                                "    (else)"
                                "    (endif)")))))
        (setf lines
              (append lines
                      (list (format nil "    (~a :print_s !)" quest)
                            (format nil "    (:~a)" end)
                            "(funcend)" "")))
        (emit-runtime lines)))

    ;; print: flat list printer, iterative.
    (let ((loop "L1PRINTLOOP") (end "L1PRINTEND"))
      (emit-runtime
       (list "(l1_print func)" "    #var ~ l1_print"
             "    (set int64 1 x~ 0)" "    (set int64 1 carv~ 0)"
             "    (set int64 1 first~ 0)"
             "    (set int64 1 tag~ 0)" "    (set int64 1 flag~ 0)"
             "    (x~ stpopi)"
             (format nil "    (~a first~~ :=)" one)
             (format nil "    (:~a)" loop)
             (format nil "    (x~~ ~a % tag~~ :=)" eight)
             (format nil "    (tag~~ ~a == flag~~ :=)" tag-cons)
             "    (flag~ if+)"
             "        (first~ if+)"
             (format nil "            (~a ~a :printf_i !)" fmt lparen)
             "        (else)"
             (format nil "            (~a :print_s !)" space)
             "        (endif)"
             (format nil "        (~a first~~ :=)" zero)
             "        (x~ :l1_car !)"
             "        (carv~ stpopi)"
             "        (carv~ :l1_print_atom !)"
             "        (x~ :l1_cdr !)"
             "        (x~ stpopi)"
             (format nil "        (x~~ ~a == flag~~ :=)" nilval)
             "        (flag~ if+)"
             (format nil "            (~a ~a :printf_i !)" fmt rparen)
             (format nil "            (:~a jmp)" end)
             "        (else)"
             (format nil "            (x~~ ~a % tag~~ :=)" eight)
             (format nil "            (tag~~ ~a == flag~~ :=)" tag-cons)
             (format nil "            (flag~~ ~a == flag~~ :=)" zero)
             "            (flag~ if+)"
             (format nil "                (~a :print_s !)" dot)
             "                (x~ :l1_print_atom !)"
             (format nil "                (~a ~a :printf_i !)" fmt rparen)
             (format nil "                (:~a jmp)" end)
             "            (else)"
             "            (endif)"
             "        (endif)"
             (format nil "        (:~a jmp)" loop)
             "    (else)"
             "        (x~ :l1_print_atom !)"
             "    (endif)"
             (format nil "    (:~a)" end)
             "    (:print_n !)"
             "(funcend)" "")))))

;;; ---------------------------------------------------------------------------
;;; Function and main compilation
;;; ---------------------------------------------------------------------------

(defun rec-declaration-line-p (line)
  (and (>= (length line) 10)
       (string= line "    (set " :end1 9 :end2 9)))

(defun hoist-rec-declarations (lines)
  "Move local (SET ...) declarations to the top of a recursive function.
   Frame pushes can refer to variables declared later in the body."
  (let ((header (subseq lines 0 2))
        (rest (nthcdr 2 lines)))
    (append header
            (remove-if-not #'rec-declaration-line-p rest)
            (remove-if #'rec-declaration-line-p rest))))

(defun compile-function (name)
  (let* ((fi (gethash name *functions*))
         (params (getf fi :params))
         (rtype (getf fi :return))
         (recursive (getf fi :recursive))
         (*vars* nil)
         (*lambda-env* nil)
         (*local-macros* nil)
         (*used-names* nil)
         (*temp-count* 0)
         (*call-scratch* (make-hash-table))
         (*discard-value* nil)
         (*current-recursive* (and recursive name))
         (*rec-tracking* recursive)
         (*rec-vars* nil)
         (*rec-cont-ids* nil)
         (*rec-sp-var* nil)
         (*rec-tmp-var* nil)
         (*rec-cont-var* nil)
         (*rec-size-cmp-var* nil)
         ;; The final form of a function body returns its value directly.
         (*tail-position* t)
         (*out* nil))
    (when recursive
      (unless (member (normalize-type rtype) '(:int :lisp :string))
        (error "Recursive function ~a must return int64, string or lisp, not ~a"
               name rtype))
      (let ((depth (rec-depth-global)))
        (unless (gethash depth *rec-depth-declared*)
          (setf (gethash depth *rec-depth-declared*) t)
          (push (format nil "    (set int64 1 ~a 0)" depth) *rec-decls*)))
      (push (format nil "    (set int64 1 ~a 0)" (rec-result-global))
            *rec-decls*))
    (emitf "(~a func)" (safe-name name))
    (emitf "    #var ~~ ~a" (safe-name name))
    (when recursive
      ;; These temporaries are deliberately not part of *REC-VARS*: they are
      ;; live only while a call is being set up or a frame is being unwound.
      (let ((*rec-tracking* nil))
        (setf *rec-sp-var* (fresh-temp :int "recsp")
              *rec-tmp-var* (fresh-temp :int "rectmp")
              *rec-cont-var* (fresh-temp :int "reccont")
              *rec-size-cmp-var* (fresh-temp :int "recsz")))
      (emitf "(:~a)" (rec-entry-label)))
    ;; Pop arguments in CL order: first parameter was pushed last (top).
    (dolist (p params)
      (let ((bv (bind-variable (car p) (cdr p) nil)))
        (emitf "    (~a ~a)" bv (stack-pop-opcode (cdr p)))))
    ;; Non-tail recursive calls jump to ENTRY (which pops new arguments).
    ;; Tail calls jump here instead, after overwriting the parameter slots.
    (when recursive
      (emitf "(:~a)" (rec-body-label)))
    ;; Body
    (let* ((body-forms (remove-if (lambda (f)
                                    (and (consp f) (eq (car f) 'declare)))
                                  (getf fi :body)))
           (last (compile-progn-for-return body-forms rtype)))
      (if recursive
          (let* ((vars (reverse *rec-vars*))
                 (lines (reverse *out*))
                 (frames nil))
            ;; Frames carry int64 slots.  :string values are int64 addresses,
            ;; so they are safe; only :double needs a separate stack instruction.
            (dolist (v vars)
              (when (eq (normalize-type (cdr v)) :double)
                (error "Recursive function ~a has a double variable ~a; recursive frames cannot carry doubles."
                       name (car v))))
            (setf lines (hoist-rec-declarations lines))
            ;; Only variables still read after a call need saving.  The final
            ;; result variable is read by the shared return path, so include
            ;; it explicitly.
            (setf frames (compute-rec-frames lines vars last))
            ;; The global dispatcher restores these variables by their
            ;; #var-mangled names, because it may run in another function.
            (dolist (frame frames)
              (let ((mangled (mapcar (lambda (v)
                                       (cons (mangle-rec-var (car v) name)
                                             (cdr v)))
                                     (cdr frame))))
                (pushnew (cons (car frame) mangled) *rec-cont-frames*
                         :key #'car :test #'=)))
            (setf lines (expand-rec-push-lines lines frames))
            (setf lines (expand-rec-size-lines lines frames))
            (append lines
                    (rec-return-lines last rtype frames)
                    (list "(funcend)")))
          (progn
            ;; Push return value
            (emitf "    (~a ~a)" last (stack-push-opcode rtype))
            (emit "(funcend)")
            (reverse *out*))))))

(defun compile-top (form)
  (cond
    ((and (consp form) (eq (car form) 'defun)) nil)
    ((and (consp form) (op= (car form) "defmacro")) nil)
    ;; Source is compiled in one fixed package.  Accept these forms so that
    ;; ordinary CL source files do not fail, but they do not create a
    ;; runtime package system.
    ((and (consp form) (op= (car form) "defpackage")) nil)
    ((and (consp form) (op= (car form) "in-package")) nil)
    ((and (consp form) (member (car form) '(defvar defparameter defconstant)))
     (let* ((name (second form))
            (g (gethash name *globals*))
            (v (compile-expr (third form))))
       ;; Declare with a constant zero, then assign at run time: Brackets
       ;; initializers cannot reliably reference another variable (e.g. an
       ;; address such as litNaddr) at declaration time.
       (emitf "    (set ~a 1 ~a 0)"
              (storage-type (cdr g))
              (car g))
       (emitf "    (~a ~a :=)" v (car g))))
    (t (compile-expr form))))

;;; ---------------------------------------------------------------------------
;;; L1VM Lisp source dialect
;;; ---------------------------------------------------------------------------
;;; The interpreter spells a few ordinary Lisp operations differently.  Translate
;;; those spellings before the normal compiler sees them, so the rest of the
;;; compiler only deals with one internal form.

(defun dialect-symbol (name)
  ;; Canonical forms live in the compiler package so that the existing EQ-based
  ;; type inference recognizes them (CL inherited symbols are shared anyway).
  (intern (string-upcase name) (find-package "CL2BRACKETS")))

(defparameter *dialect-heads*
  `(("SET!"           ,(dialect-symbol "setq"))
    ("BEGIN"          ,(dialect-symbol "progn"))
    ("WRITE"          ,(dialect-symbol "princ"))
    ("NEWLINE"        ,(dialect-symbol "terpri"))
    ("EXIT"           ,(dialect-symbol "quit"))
    ("QUIT"           ,(dialect-symbol "quit"))
    ("MOD"            ,(dialect-symbol "mod"))
    ("EQUAL?"         ,(dialect-symbol "equal"))
    ("EQ?"            ,(dialect-symbol "eql"))
    ("NULL?"          ,(dialect-symbol "null"))
    ("PAIR?"          ,(dialect-symbol "consp"))
    ("NUMBER?"        ,(dialect-symbol "integerp"))
    ("INTEGER?"       ,(dialect-symbol "integerp"))
    ("DOUBLE?"        ,(dialect-symbol "doublep"))
    ("STRING?"        ,(dialect-symbol "stringp"))
    ("SYMBOL?"        ,(dialect-symbol "symbolp"))
    ("PROCEDURE?"     ,(dialect-symbol "procedurep"))
    ("STRING-LEN"     ,(dialect-symbol "string-length"))
    ("STRING-CAT"     ,(dialect-symbol "string-append"))
    ("STRING-COPY"    ,(dialect-symbol "string-copy"))
    ("STRING-LEFT"    ,(dialect-symbol "string-left"))
    ("STRING-RIGHT"   ,(dialect-symbol "string-right"))
    ("STRING-MID"     ,(dialect-symbol "string-mid"))
    ("STRING-MID->BYTE" ,(dialect-symbol "string-mid->byte"))
    ("INT->STRING"    ,(dialect-symbol "int->string"))
    ("BYTE->HEXSTRING" ,(dialect-symbol "byte->hexstring"))
    ("DOUBLE->STRING" ,(dialect-symbol "double->string"))
    ("INT->DOUBLE"    ,(dialect-symbol "int->double"))
    ("DOUBLE->INT"    ,(dialect-symbol "double->int"))
    ("BYTE->STRING"   ,(dialect-symbol "byte->string"))
    ("STRING-COMPARE" ,(dialect-symbol "string-compare"))
    ("STRING-SEARCH"  ,(dialect-symbol "string-search"))
    ("STRING-REGEX"   ,(dialect-symbol "string-regex"))
    ("STRING-VERIFY"  ,(dialect-symbol "string-verify"))
    ("STRING-FORMAT-NUM" ,(dialect-symbol "string-format-num"))
    ("STRING-PADD"    ,(dialect-symbol "string-padd"))
    ("GET-ENV"        ,(dialect-symbol "get-env"))
    ("SET-ENV"        ,(dialect-symbol "set-env"))
    ("OPEN-INPUT-FILE"  ,(dialect-symbol "open-input-file"))
    ("OPEN-OUTPUT-FILE" ,(dialect-symbol "open-output-file"))
    ("OPEN-APPEND-FILE" ,(dialect-symbol "open-append-file"))
    ("FILE-PORT?"       ,(dialect-symbol "file-port?"))
    ("READ-LINE"        ,(dialect-symbol "read-line"))
    ("READ-CHAR"        ,(dialect-symbol "read-char"))
    ("WRITE-STRING"     ,(dialect-symbol "write-string"))
    ("WRITE-LINE"       ,(dialect-symbol "write-line"))
    ("WRITE-CHAR"       ,(dialect-symbol "write-char"))
    ("FLUSH-OUTPUT"     ,(dialect-symbol "flush-output"))
    ("CLOSE-PORT"       ,(dialect-symbol "close-port"))
    ;; SDL 3.0
    ("SDL-OPEN-SCREEN"      ,(dialect-symbol "sdl-open-screen"))
    ("SDL-OPEN-SCREEN-FULL" ,(dialect-symbol "sdl-open-screen-full"))
    ("SDL-QUIT"             ,(dialect-symbol "sdl-quit"))
    ("SDL-SET-WINDOW-TITLE" ,(dialect-symbol "sdl-set-window-title"))
    ("SDL-TOGGLE-MOUSE-POINTER",(dialect-symbol "sdl-toggle-mouse-pointer"))
    ("SDL-DELAY"            ,(dialect-symbol "sdl-delay"))
    ("SDL-UPDATE"           ,(dialect-symbol "sdl-update"))
    ("SDL-FONT-TTF"         ,(dialect-symbol "sdl-font-ttf"))
    ("SDL-TEXT-TTF"         ,(dialect-symbol "sdl-text-ttf"))
    ("SDL-PIXEL"            ,(dialect-symbol "sdl-pixel"))
    ("SDL-LINE"             ,(dialect-symbol "sdl-line"))
    ("SDL-RECTANGLE"        ,(dialect-symbol "sdl-rectangle"))
    ("SDL-RECTANGLE-FILL"  ,(dialect-symbol "sdl-rectangle-fill"))
    ("SDL-CIRCLE"           ,(dialect-symbol "sdl-circle"))
    ("SDL-CIRCLE-FILL"      ,(dialect-symbol "sdl-circle-fill"))
    ("SDL-ELLIPSE"          ,(dialect-symbol "sdl-ellipse"))
    ("SDL-ELLIPSE-FILL"     ,(dialect-symbol "sdl-ellipse-fill"))
    ("SDL-PIE"              ,(dialect-symbol "sdl-pie"))
    ("SDL-PIE-FILL"         ,(dialect-symbol "sdl-pie-fill"))
    ("SDL-TRIGON"           ,(dialect-symbol "sdl-trigon"))
    ("SDL-TRIGON-FILL"      ,(dialect-symbol "sdl-trigon-fill"))
    ("SDL-GET-PIXELCOLOR"   ,(dialect-symbol "sdl-get-pixelcolor"))
    ("SDL-LOAD-PICTURE"     ,(dialect-symbol "sdl-load-picture"))
    ("SDL-SAVE-PICTURE"     ,(dialect-symbol "sdl-save-picture"))
    ("SDL-PLAY-SOUND"       ,(dialect-symbol "sdl-play-sound"))
    ("INIT-GADGETS"         ,(dialect-symbol "init-gadgets"))
    ("INIT-GUI"             ,(dialect-symbol "init-gui"))
    ("SET-GADGET-BUTTON"    ,(dialect-symbol "set-gadget-button"))
    ("SET-GADGET-PROGRESS"  ,(dialect-symbol "set-gadget-progress"))
    ("SET-GADGET-CHECKBOX"  ,(dialect-symbol "set-gadget-checkbox"))
    ("SET-GADGET-CYCLE"     ,(dialect-symbol "set-gadget-cycle"))
    ("SET-GADGET-STRING"    ,(dialect-symbol "set-gadget-string"))
    ("SET-GADGET-BOX"       ,(dialect-symbol "set-gadget-box"))
    ("SET-GADGET-BOX-GRID"  ,(dialect-symbol "set-gadget-box-grid"))
    ("SET-GADGET-SLIDER"    ,(dialect-symbol "set-gadget-slider"))
    ("SET-GADGET-SLIDER-VERT",(dialect-symbol "set-gadget-slider-vert"))
    ("SET-GADGET-MENU"      ,(dialect-symbol "set-gadget-menu"))
    ("CHANGE-GADGET-CHECKBOX",(dialect-symbol "change-gadget-checkbox"))
    ("CHANGE-GADGET-CYCLE"  ,(dialect-symbol "change-gadget-cycle"))
    ("CHANGE-GADGET-STRING" ,(dialect-symbol "change-gadget-string"))
    ("CHANGE-GADGET-BOX"    ,(dialect-symbol "change-gadget-box"))
    ("CHANGE-GADGET-BOX-GRID",(dialect-symbol "change-gadget-box-grid"))
    ("GET-GADGET-BOX-GRID"  ,(dialect-symbol "get-gadget-box-grid"))
    ("CHANGE-GADGET-PROGRESS",(dialect-symbol "change-gadget-progress"))
    ("CHANGE-GADGET-SLIDER" ,(dialect-symbol "change-gadget-slider"))
    ("GET-GADGET-X2Y2"      ,(dialect-symbol "get-gadget-x2y2"))
    ("GADGET-EVENT"         ,(dialect-symbol "gadget-event"))
    ("SET-GADGET-STRING-PASSWD",(dialect-symbol "set-gadget-string-passwd"))
    ("SET-GADGET-STRING-MULTILINE",(dialect-symbol "set-gadget-string-multiline"))
    ("CHANGE-GADGET-STRING-MULTILINE",(dialect-symbol "change-gadget-string-multiline"))
    ("GET-GADGET-STRING-MULTILINE",(dialect-symbol "get-gadget-string-multiline"))
    ("FREE-ALL-GADGETS"     ,(dialect-symbol "free-all-gadgets"))
    ("SDL-FREE-ALL-GADGETS" ,(dialect-symbol "sdl-free-all-gadgets"))
    ("UNSET-GADGET"         ,(dialect-symbol "unset-gadget"))
    ("SDL-UNSET-GADGET"     ,(dialect-symbol "sdl-unset-gadget"))
    ("COPY-AREA"            ,(dialect-symbol "copy-area"))
    ("SDL-COPY-AREA"        ,(dialect-symbol "sdl-copy-area"))
    ("RESTORE-AREA"         ,(dialect-symbol "restore-area"))
    ("SDL-RESTORE-AREA"     ,(dialect-symbol "sdl-restore-area"))
    ("SDL-CLOSE-JOYSTICK"   ,(dialect-symbol "sdl-close-joystick"))
    ("SDL-GET-JOYSTICK-X-AXIS",(dialect-symbol "sdl-get-joystick-x-axis"))
    ("SDL-GET-JOYSTICK-Y-AXIS",(dialect-symbol "sdl-get-joystick-y-axis"))
    ("SDL-GET-JOYSTICK-X2-AXIS",(dialect-symbol "sdl-get-joystick-x2-axis"))
    ("SDL-GET-JOYSTICK-Y2-AXIS",(dialect-symbol "sdl-get-joystick-y2-axis"))
    ("SDL-GET-JOYSTICK-BUTTONS",(dialect-symbol "sdl-get-joystick-buttons"))
    ("SDL-GET-JOYSTICK-BUTTON",(dialect-symbol "sdl-get-joystick-button"))
    ("SDL-GET-JOYSTICK-INFO" ,(dialect-symbol "sdl-get-joystick-info"))
    ("SDL-GET-KEY"           ,(dialect-symbol "sdl-get-key"))
    ("SDL-OPEN-CAMERA"       ,(dialect-symbol "sdl-open-camera"))
    ("SDL-GET-CAMERA-FRAME"  ,(dialect-symbol "sdl-get-camera-frame"))
    ("SDL-CLOSE-CAMERA"      ,(dialect-symbol "sdl-close-camera"))
    ("SDL-GET-MOUSE-STATE"   ,(dialect-symbol "sdl-get-mouse-state"))))

(defun dialect-head (head)
  (dolist (entry *dialect-heads* head)
    (when (op= head (first entry))
      (return (second entry)))))

(defun translate-dialect-body (forms)
  (mapcar (lambda (form) (translate-dialect-form form nil)) forms))

(defun translate-dialect-bindings (bindings)
  (mapcar
   (lambda (binding)
     (if (and (consp binding) (consp (cdr binding)))
         (list (first binding)
               (translate-dialect-form (second binding) nil))
         binding))
   bindings))

(defun translate-dialect-clauses (clauses)
  (mapcar
   (lambda (clause)
     (if (consp clause)
         (cons (if (op= (first clause) "ELSE")
                   t
                   (translate-dialect-form (first clause) nil))
               (translate-dialect-body (cdr clause)))
         clause))
   clauses))

(defun translate-dialect-form (form &optional (top-level t))
  "Translate one L1VM Lisp form into the compiler's internal Common Lisp form."
  (if (not (consp form))
      form
      (let ((head (car form)))
        (cond
          ((op= head "QUOTE")
           (cons 'quote (cdr form)))
          ((op= head "DEFINE")
           (let ((target (second form))
                 (rest-forms (cddr form)))
             (cond
               ;; (define f (lambda (x) ...)) -> (defun f (x) ...)
               ((and (symbolp target)
                     (consp (car rest-forms))
                     (op= (caar rest-forms) "LAMBDA"))
                (cons 'defun
                      (cons target
                            (cons (second (car rest-forms))
                                  (translate-dialect-body
                                   (cddr (car rest-forms)))))))
               ;; (define (f x) ...)
               ((consp target)
                (cons 'defun
                      (cons (car target)
                            (cons (cdr target)
                                  (translate-dialect-body rest-forms)))))
               ;; (define x value)
               (top-level
                (list (dialect-symbol (if (cdr rest-forms)
                                          "defparameter"
                                          "defvar"))
                      target
                      (translate-dialect-form (car rest-forms) nil)))
               (t
                (list 'setq target
                      (translate-dialect-form (car rest-forms) nil))))))
          ((op= head "LAMBDA")
           (cons 'lambda
                 (cons (second form)
                       (translate-dialect-body (cddr form)))))
          ((or (op= head "LET") (op= head "LET*"))
           (cons (dialect-head head)
                 (cons (translate-dialect-bindings (second form))
                       (translate-dialect-body (cddr form)))))
          ((op= head "COND")
           (cons 'cond (translate-dialect-clauses (cdr form))))
          ((and (consp head) (op= (car head) "LAMBDA"))
           (cons (translate-dialect-form head nil)
                 (translate-dialect-body (cdr form))))
          (t
           (cons (dialect-head head)
                 (translate-dialect-body (cdr form))))))))

(defun translate-dialect-forms (forms)
  (mapcar (lambda (form) (translate-dialect-form form t)) forms))

(defun net-close-branch-lines (clauses indent minus1 eight)
  "Build the nested IF chain used by the CLOSE-SOCKET helper.
   Each branch pushes its own result: locals assigned inside an IF block are
   not visible after ENDIF in a generated function."
  (if (null clauses)
      (list (format nil "~a(~a stpushi)" indent minus1))
      (destructuring-bind (kind-literal fn) (first clauses)
        (append
         (list (format nil "~a((k~~ ~a ==) f~~ =)" indent kind-literal)
               (format nil "~a(f~~ if+)" indent)
               (format nil "~a    (h~~ :~a !)" indent fn)
               (format nil "~a    (s~~ stpopi)" indent)
               (format nil "~a    (reset-reg)" indent)
               (format nil "~a    (h~~ ~a * o~~ :=)" indent eight)
               (format nil "~a    (zero l2v_sockkind [ o~~ ] =)" indent)
               (format nil "~a    (s~~ stpushi)" indent)
               (format nil "~a(else)" indent))
         (net-close-branch-lines (rest clauses)
                                 (concatenate 'string indent "    ")
                                 minus1 eight)
         (list (format nil "~a(endif)" indent))))))

(defun ensure-net-helpers ()
  "Append the socket-kind lookup and generic close helpers when needed."
  (when (and *net-module-needed* (not *net-helpers-emitted*))
    (setf *net-helpers-emitted* t)
    (let* ((one (ensure-literal 1 :int))
           (two (ensure-literal 2 :int))
           (three (ensure-literal 3 :int))
           (four (ensure-literal 4 :int))
           (five (ensure-literal 5 :int))
           (six (ensure-literal 6 :int))
           (eight (ensure-literal 8 :int))
           (minus1 (ensure-literal -1 :int))
           (close-clauses
             (list (list one "close_server_socket")
                   (list two "close_client_socket")
                   (list three "close_accept_server")
                   (list four "close_server_socket_ssl")
                   (list five "close_client_socket_ssl")
                   (list six "close_accept_server_ssl"))))
      (setf *func-code*
            (append
             *func-code*
             (list
              (list
               "(l2v_socket_kind func)"
               "    #var ~ l2v_socket_kind"
               "    (set int64 1 h~ 0)"
               "    (set int64 1 k~ 0)"
               "    (set int64 1 f~ 0)"
               "    (set int64 1 off~ 0)"
               "    (h~ stpopi)"
               "    ((h~ zero <) f~ =)"
               "    (f~ if+)"
               "        (zero stpushi)"
               "    (else)"
               (format nil "        ((h~~ ~a >=) f~~ =)" eight)
               "        (f~ if+)"
               "            (zero stpushi)"
               "        (else)"
               (format nil "            (h~~ ~a * off~~ :=)" eight)
               "            (l2v_sockkind [ off~ ] k~ =)"
               "            (k~ stpushi)"
               "        (endif)"
               "    (endif)"
               "(funcend)")
              (append
               (list
                "(l2v_close_socket func)"
                "    #var ~ l2v_close_socket"
                "    (set int64 1 h~ 0)"
                "    (set int64 1 k~ 0)"
                "    (set int64 1 s~ 0)"
                "    (set int64 1 f~ 0)"
                "    (set int64 1 o~ 0)"
                "    (h~ stpopi)"
                "    (h~ :l2v_socket_kind !)"
                "    (k~ stpopi)"
                "    (reset-reg)"
                "    ((k~ zero ==) f~ =)"
                "    (f~ if+)"
                (format nil "        (~a stpushi)" minus1)
                "    (else)")
               (net-close-branch-lines close-clauses "        " minus1 eight)
               (list "    (endif)"
                     "(funcend)"))))))))

;;; ---------------------------------------------------------------------------
;;; Peephole optimization of the emitted Brackets code
;;;
;;; The expression compiler deliberately emits a fresh temporary for every
;;; intermediate value.  That is simple and correct, but it leaves two
;;; recurring patterns in the output:
;;;
;;;   (a OP b t~ :=)        (i < limit t1~ :=)
;;;   (t~ target :=)   ->   (t1~ zero == t2~ :=)
;;;                         (t2~ :LOOPE1 jmpi)
;;;
;;; The first pair stores a result in a temporary only to copy it into the
;;; real destination.  The second computes a condition, inverts it and then
;;; branches.  Both can be folded: write the expression straight into its
;;; final destination, and branch on the inverse comparison instead.  The
;;; pass below also drops declarations of temporaries that become unused.
;;;
;;; Everything here works on the final text, after all compiler phases have
;;; run, so the existing constant folding, tail-call lowering, recursion
;;; frames and runtime pruning remain untouched.
;;; ---------------------------------------------------------------------------

(defvar *inverse-comparisons*
  (let ((table (make-hash-table :test 'equal)))
    (dolist (pair '(("==" . "!=") ("!=" . "==")
                    ("<" . ">=") (">=" . "<")
                    (">" . "<=") ("<=" . ">")
                    ("==d" . "!=d") ("!=d" . "==d")
                    ("<d" . ">=d") (">=d" . "<d")
                    (">d" . "<=d") ("<=d" . ">d")))
      (setf (gethash (car pair) table) (cdr pair))
      (setf (gethash (cdr pair) table) (car pair)))
    table))

(defun br-tokenize (text)
  "Split the inside of a Brackets expression into tokens.
   Quoted strings (which may contain spaces) stay in one token."
  (let ((i 0)
        (n (length text))
        (start nil)
        (in-string nil)
        (escaped nil)
        (tokens nil))
    (loop while (< i n) do
      (let ((ch (char text i)))
        (cond
          (in-string
           (when (and (not escaped) (char= ch #\"))
             (setf in-string nil))
           (setf escaped (and (not escaped) (char= ch #\\)))
           (incf i))
          ((char= ch #\")
           (setf start i in-string t escaped nil)
           (incf i))
          ((or (char= ch #\Space) (char= ch #\Tab) (char= ch #\Newline))
           (when start
             (push (subseq text start i) tokens)
             (setf start nil))
           (incf i))
          (t
           (unless start (setf start i))
           (incf i)))))
    (when start (push (subseq text start n) tokens))
    (nreverse tokens)))

(defun br-line-parts (line)
  "Return (indent tokens) for a parenthesized Brackets line, NIL otherwise."
  (let ((lp (position #\( line)))
    (when lp
      (let ((rp (position #\) line :from-end t)))
        (when (and rp (> rp lp))
          (list (subseq line 0 lp)
                (br-tokenize (subseq line (1+ lp) rp))))))))

(defun br-render (parts)
  (destructuring-bind (indent tokens) parts
    (concatenate 'string indent "(" (format nil "~{~a~^ ~}" tokens) ")")))

(defun br-replace-nth (tokens index value)
  (let ((copy (copy-list tokens)))
    (setf (nth index copy) value)
    copy))

(defun br-generated-temp-p (name)
  "True for compiler-created scalar temporaries: t1~, t23~, ..."
  (and (stringp name)
       (> (length name) 2)
       (char= (char name 0) #\t)
       (char= (char name (1- (length name))) #\~)
       (loop for i from 1 below (1- (length name))
             always (digit-char-p (char name i)))))

(defun br-declaration-name (tokens)
  (when (and (>= (length tokens) 4)
             (string= (first tokens) "set"))
    (fourth tokens)))

(defun br-assignment-dest (tokens)
  (when (and (>= (length tokens) 3)
             (member (car (last tokens)) '("=" ":=") :test #'string=))
    (nth (- (length tokens) 2) tokens)))

(defun br-copy-operands (tokens)
  (when (and (= (length tokens) 3)
             (member (third tokens) '("=" ":=") :test #'string=))
    (values (first tokens) (second tokens))))

(defun br-pop-variable (tokens)
  (when (and (= (length tokens) 2)
             (member (second tokens)
                     '("stpop" "stpopi" "stpopd" "stpopb") :test #'string=))
    (first tokens)))

(defun br-comparison-assignment (tokens)
  "Match (a OP b dst :=) and return the destination and inverse operator."
  (when (= (length tokens) 5)
    (let ((op (second tokens))
          (dst (fourth tokens)))
      (when (and (member (fifth tokens) '("=" ":=") :test #'string=)
                 (gethash op *inverse-comparisons*))
        (values dst op (gethash op *inverse-comparisons*))))))

(defun br-zero-equality (tokens)
  "Match (flag zero == neg :=) or (zero flag == neg :=)."
  (when (= (length tokens) 5)
    (destructuring-bind (a b op dst assign) tokens
      (when (and (string= op "==")
                 (member assign '("=" ":=") :test #'string=))
        (cond ((and (string= b "zero") (br-generated-temp-p a))
               (values a dst))
              ((and (string= a "zero") (br-generated-temp-p b))
               (values b dst)))))))

(defun br-conditional-jump (tokens)
  (when (= (length tokens) 3)
    (destructuring-bind (var label insn) tokens
      (when (and (string= insn "jmpi")
                 (> (length label) 0)
                 (char= (char label 0) #\:))
        (values var label)))))

(defun br-collect-declarations (blocks)
  "All variable names declared anywhere in the generated program.
   Declarations written by the main prologue are included too."
  (let ((names (make-hash-table :test 'equal)))
    (dolist (block blocks)
      (dolist (line block)
        (let ((parts (br-line-parts line)))
          (when parts
            (let ((name (br-declaration-name (second parts))))
              (when name (setf (gethash name names) t)))))))
    names))

(defun optimize-brackets-block (lines external-decls)
  "Apply local peephole rewrites to one function body."
  (let* ((vector (coerce lines 'vector))
         (count (length vector))
         (dead (make-array count :initial-element nil))
         (parts (map 'vector #'br-line-parts vector))
         (decl-positions (make-hash-table :test 'equal)))
    (labels ((tokens (i) (second (aref parts i)))
             (set-tokens (i ts)
               (setf (aref parts i)
                     (list (first (aref parts i)) ts)))
             (declared-before-p (name i)
               (let ((pos (gethash name decl-positions)))
                 (cond (pos (< pos i))
                       ((gethash name external-decls) t)
                       (t nil)))))

      ;; Record declaration positions for destination-before-use checks.
      (dotimes (i count)
        (let ((ts (tokens i)))
          (when (and ts (br-declaration-name ts))
            (let ((name (br-declaration-name ts)))
              (unless (gethash name decl-positions)
                (setf (gethash name decl-positions) i))))))

      ;; Usage counts before any rewriting: loop inversion may only reuse the
      ;; flag temporary when the negation is its sole consumer.
      (let ((pre-uses (make-hash-table :test 'equal)))
        (dotimes (i count)
          (let ((ts (tokens i)))
            (when (and ts (not (br-declaration-name ts)))
              (dolist (token ts)
                (when (br-generated-temp-p token)
                  (incf (gethash token pre-uses 0)))))))
        ;; Pass 1: branch on the inverse comparison instead of materializing a
        ;; boolean, inverting it, and then branching on the inversion.
        (dotimes (i count)
          (unless (aref dead i)
            (multiple-value-bind (flag negated) (br-zero-equality (tokens i))
              (when (and flag negated
                         (= (gethash flag pre-uses 0) 2)
                         (< (1+ i) count))
                (multiple-value-bind (jump-var label)
                    (br-conditional-jump (tokens (1+ i)))
                  (declare (ignore label))
                  (when (and jump-var (string= jump-var negated))
                    ;; Find the comparison that produced FLAG.  Comparisons are
                    ;; emitted immediately before the inversion in all current
                    ;; control forms, but allow a few intervening lines.
                    (let ((producer nil))
                      (loop for j from (1- i) downto (max 0 (- i 8))
                            while (not producer)
                            do (let ((ts (tokens j)))
                                 (when (and ts (br-assignment-dest ts)
                                            (string= (br-assignment-dest ts) flag))
                                   (multiple-value-bind (dst op inverse)
                                       (br-comparison-assignment ts)
                                     (declare (ignore dst))
                                     (when op
                                       (setf producer (list j inverse)))))))
                      (when producer
                        (let* ((j (first producer))
                               (ts (tokens j))
                               (jump-ts (tokens (1+ i))))
                          (setf ts (br-replace-nth ts 1 (second producer)))
                          (set-tokens j ts)
                          (set-tokens (1+ i) (br-replace-nth jump-ts 0 flag))
                          (setf (aref dead i) t)))))))))))

      ;; Count how often each generated temporary is read.  Copy propagation
      ;; is only safe when the producer is consumed exactly once.
      (let ((uses (make-hash-table :test 'equal)))
        (dotimes (i count)
          (unless (aref dead i)
            (let ((ts (tokens i)))
              (when (and ts (not (br-declaration-name ts)))
                (dolist (token ts)
                  (when (br-generated-temp-p token)
                    (incf (gethash token uses 0))))))))
        ;; Pass 2: write an expression result straight into its final
        ;; destination instead of through a single-use temporary.
        (let ((producers (make-hash-table :test 'equal)))
          (dotimes (i count)
            (unless (aref dead i)
              (let ((ts (tokens i)))
                (cond
                  ((null ts) (clrhash producers))
                  ((br-declaration-name ts) nil)
                  ((br-pop-variable ts)
                   (let ((var (br-pop-variable ts)))
                     (when (br-generated-temp-p var)
                       (setf (gethash var producers) (list i :pop ts)))))
                  ((br-assignment-dest ts)
                   (let ((dst (br-assignment-dest ts)))
                     (when (br-generated-temp-p dst)
                       (setf (gethash dst producers) (list i :math ts)))
                     (multiple-value-bind (src target) (br-copy-operands ts)
                       (when (and src target)
                         (cond
                           ((string= src target)
                            (setf (aref dead i) t)
                            (clrhash producers))
                           ((and (br-generated-temp-p src)
                                 (= (gethash src uses 0) 2)
                                 (gethash src producers)
                                 (= (first (gethash src producers)) (1- i))
                                 (declared-before-p target i))
                            (destructuring-bind (pos kind pts)
                                (gethash src producers)
                              (declare (ignore pos))
                              (let ((new-ts
                                      (if (eq kind :pop)
                                          (br-replace-nth pts 0 target)
                                          (br-replace-nth pts
                                                          (- (length pts) 2)
                                                          target))))
                                (set-tokens (1- i) new-ts)
                                (setf (aref dead i) t)
                                 (remhash src producers)))))))))
                  (t (clrhash producers))))))

        ;; Pass 3: declarations whose temporary disappeared in the passes
        ;; above are dead.  Zero-initialized declarations emit no instructions
        ;; but still enlarge the generated source and data segment.  Recount
        ;; references after the rewrites: propagation can remove the last use
        ;; of a temporary.
        (let ((remaining (make-hash-table :test 'equal)))
          (dotimes (i count)
            (unless (aref dead i)
              (let ((ts (tokens i)))
                (when (and ts (not (br-declaration-name ts)))
                  (dolist (token ts)
                    (when (br-generated-temp-p token)
                      (incf (gethash token remaining 0))))))))
          (dotimes (i count)
            (unless (aref dead i)
              (let* ((ts (tokens i))
                     (name (and ts (br-declaration-name ts))))
                (when (and name (br-generated-temp-p name)
                           (zerop (gethash name remaining 0)))
                  (setf (aref dead i) t))))))

      ;; Render the surviving lines.
      (let ((result nil))
        (dotimes (i count)
          (unless (aref dead i)
            (let ((p (aref parts i)))
              (push (if p (br-render p) (aref vector i)) result))))
        (nreverse result)))))))

(defun name-referenced-p (name lines)
  "True when NAME appears as a complete token in any of LINES.
   Used to drop literal declarations whose name is never emitted."
  (let ((len (length name)))
    (dolist (line lines nil)
      (let ((n (length line))
            (start 0))
        (loop
          (let ((p (search name line :start2 start)))
            (unless p (return))
            (when (and (or (= p 0)
                           (not (alphanumericp (char line (1- p)))))
                       (or (>= (+ p len) n)
                           (not (alphanumericp (char line (+ p len))))))
              (return-from name-referenced-p t))
            (setf start (1+ p))))))))

(defun emitted-code-lines ()
  "All generated Brackets lines that can reference a literal."
  (append *main-code*
          (apply #'append (mapcar #'copy-list *func-code*))
          *runtime-code*))

(defun emit-literal-declarations (stream)
  "Emit declarations only for literals used by the generated code."
  (let ((lines (emitted-code-lines)))
    (dolist (name (reverse *literal-order*))
      (let ((decl-line (gethash name *literal-decls*)))
        (when (and decl-line
                   (or (name-referenced-p name lines)
                       (name-referenced-p
                        (concatenate 'string name "addr") lines)))
          (format stream "~a~%" decl-line))))))

(defun compile-source (forms stream)
  ;; Bind *PACKAGE* for the whole compilation: source symbols, macro
  ;; definitions, and printed quoted data all use the same package.
  (let ((*package* *user-package*))
    (reset-state)
    ;; Translate form by form so each translated form keeps its source line.
    (let ((translated nil)
          (translated-lines nil))
      (dolist (f forms)
        (setf *current-source-line* (form-line f))
        (push (translate-dialect-form f t) translated)
        (push *current-source-line* translated-lines))
      (setf forms (nreverse translated))
      (loop for f in forms for l in (nreverse translated-lines)
            do (remember-form-line f l)))
    (ensure-fixed-symbols)
    ;; DEFMACRO is evaluated by the host Lisp before any expansion.  This is
    ;; the clearest example of source code being ordinary Lisp data: the
    ;; compiler EVALs a list in order to teach itself a new syntactic form.
    (dolist (f forms)
      (setf *current-source-line* (form-line f))
      (when (and (consp f) (op= (car f) "defmacro"))
        (eval f)
        (pushnew (second f) *defined-macros*)))
    ;; Store function bodies for pass two.
    (dolist (f forms)
      (setf *current-source-line* (form-line f))
      (when (and (consp f) (eq (car f) 'defun))
        (setf (gethash (second f) *functions*)
              (append (gethash (second f) *functions*) (list :body (cdddr f))))))
    (scan-top-level forms)
    (scan-implicit-globals forms)
    (infer-parameter-types forms)
    (infer-all-return-types)
    (validate-function-types)
    (compute-recursion-groups)
    ;; Compile main.
    (let ((*vars* nil) (*lambda-env* nil) (*local-macros* nil)
          (*used-names* nil) (*temp-count* 0) (*out* nil)
          (*call-scratch* (make-hash-table))
          ;; MAIN never returns a value: even its last form's result is
          ;; discarded before :exit.
          (*discard-value* t))
      (dolist (f forms)
        (setf *current-source-line* (form-line f))
        (compile-top f))
      (setf *main-code* (reverse *out*)))
    ;; Compile functions.
    (setf *func-code* nil)
    (dolist (name (reverse *function-order*))
      (setf *current-source-line*
            (or (getf (gethash name *functions*) :line) *current-source-line*))
      (push (compile-function name) *func-code*))
    ;; The global return dispatcher is generated after all call sites are
    ;; known, so it can restore any caller's saved locals.
    (let ((any-recursive
            (some (lambda (n) (getf (gethash n *functions*) :recursive))
                  *function-order*)))
      (when any-recursive
        ;; Even a program with only tail calls emits a return-path dispatcher
        ;; call, so define a stub when there are no saved frames.
        (let ((dispatcher
                (or (rec-dispatcher-code)
                    (list "(l1_rec_dispatch func)"
                          "    #var ~ l1_rec_dispatch"
                          "    (zero :exit !)"
                          "(funcend)"))))
          ;; Append after the functions: the restored variables are declared
          ;; inside them, and Brackets checks declarations in source order.
          (setf *func-code* (append *func-code* (list dispatcher))))))
    ;; Compile the Lisp-data runtime last: it may add literals that must be
    ;; declared in main before the assembled output is written.
    (generate-runtime-code)
    (ensure-net-helpers)
    (ensure-file-helpers)
    (ensure-sdl-helpers)
    ;; Optimize the emitted Brackets before literal declarations are selected:
    ;; rewrites here can remove the last reference to a literal.
    (let ((decls (br-collect-declarations
                  (list *main-code* (apply #'append *func-code*) *runtime-code*))))
      (setf *main-code* (optimize-brackets-block *main-code* decls))
      (setf *func-code* (mapcar (lambda (block)
                                  (optimize-brackets-block block decls))
                                *func-code*))
      (setf *runtime-code* (optimize-brackets-block *runtime-code* decls)))
    ;; Assemble output.
    (format stream "(:main jmp)~%")
    (format stream "#include <intr-func.l1h>~%~%")
    (format stream "(main func)~%    #var ~~ main~%    (set int64 1 zero 0)~%")
    (when *math-module-needed*
      (format stream "    (set int64 1 mathmod 1)~%"))
    (when *net-module-needed*
      (format stream "    (set int64 1 l2v_netmod ~a)~%" +net-module-slot+)
      (format stream "    (set int64 1 netmax~~ ~a)~%" +net-max-sockets+)
      (format stream "    (set int64 ~a l2v_sockkind 0)~%" +net-max-sockets+))
    (when *file-module-needed*
      (format stream "    (set int64 1 l2v_filemod ~a)~%" +file-module-slot+)
      (format stream "    (set int64 1 l2v_filemax ~a)~%" +file-max-files+)
      (format stream "    (set int64 ~a l2v_fileopen 0)~%" +file-max-files+)
      (format stream "    (set string ~a l2v_flinebuf \"\")~%"
              (* +file-line-pool+ +file-line-size+))
      (format stream "    (set int64 1 Pl2v_flinebase 0)~%")
      (format stream "    (l2v_flinebuf Pl2v_flinebase pointer)~%"))
    (when *sdl-module-needed*
      (format stream "    (set int64 1 l2v_sdlmod ~a)~%" +sdl-module-slot+)
      ;; SDL pixel-colour globals (filled by l2v_sdl_get_pixelcolor helper)
      (format stream "    (set int64 1 l2v_sdl_pixel_r 0)~%")
      (format stream "    (set int64 1 l2v_sdl_pixel_g 0)~%")
      (format stream "    (set int64 1 l2v_sdl_pixel_b 0)~%")
      (format stream "    (set int64 1 l2v_sdl_pixel_a 0)~%")
      ;; SDL mouse-state globals (filled by l2v_sdl_get_mouse_state helper)
      (format stream "    (set int64 1 l2v_sdl_mouse_x 0)~%")
      (format stream "    (set int64 1 l2v_sdl_mouse_y 0)~%")
      (format stream "    (set int64 1 l2v_sdl_mouse_left 0)~%")
      (format stream "    (set int64 1 l2v_sdl_mouse_middle 0)~%")
      (format stream "    (set int64 1 l2v_sdl_mouse_right 0)~%")
      ;; SDL joystick-info globals (filled by l2v_sdl_get_joystick_info helper)
      (format stream "    (set int64 1 l2v_sdl_joy_axis 0)~%")
      (format stream "    (set int64 1 l2v_sdl_joy_buttons 0)~%")
      ;; SDL gadget helper globals
      (format stream "    (set int64 1 l2v_sdl_gadget_x2 0)~%")
      (format stream "    (set int64 1 l2v_sdl_gadget_y2 0)~%")
      (format stream "    (set int64 1 l2v_sdl_gadget_value 0)~%")
      (format stream "    (set int64 1 l2v_sdl_gadget_status 0)~%"))
    (when *string-module-needed*
      (format stream "    (zero :string_init !)~%"))
    (when *math-module-needed*
      (format stream "    (mathmod :math_init !)~%"))
    (when *net-module-needed*
      ;; net_init returns 0 on success; pop that status before user code runs.
      (format stream "    (set int64 1 netinit~~ 0)~%")
      (format stream "    (l2v_netmod netmax~~ :net_init !)~%")
      (format stream "    (netinit~~ stpopi)~%")
      (format stream "    (reset-reg)~%"))
    (when *file-module-needed*
      ;; file_init returns 0 on success; pop that status before user code runs.
      (format stream "    (set int64 1 fileinit~~ 0)~%")
      (format stream "    (l2v_filemod l2v_filemax :file_init !)~%")
      (format stream "    (fileinit~~ stpopi)~%")
      (format stream "    (reset-reg)~%"))

    (when *runtime-needed*
      (dolist (d (runtime-declarations)) (format stream "    ~a~%" d)))
    (dolist (d (reverse *rec-decls*)) (format stream "~a~%" d))
    (emit-literal-declarations stream)
    ;; SETQ-created globals need a storage declaration in MAIN before any
    ;; generated code (including function bodies) reads or writes them.
    (dolist (sym (reverse *implicit-globals*))
      (let* ((g (gethash sym *globals*))
             (type (normalize-type (cdr g))))
        (format stream "    (set ~a 1 ~a ~a)~%"
                (storage-type type) (car g)
                (if (eq type :double) "0.0" "0"))))
    (dolist (l *main-code*) (format stream "~a~%" l))
    (when *sdl-module-needed*
      (format stream "    (l2v_sdlmod :free_mod !)~%"))
    (when *file-module-needed*
      (format stream "    (:file_free_mem !)~%")
      (format stream "    (l2v_filemod :free_mod !)~%"))
    (when *string-module-needed*
      (format stream "    (zero :free_mod !)~%"))
    (when *net-module-needed*
      (format stream "    (l2v_netmod :free_mod !)~%"))
    (format stream "    (zero :exit !)~%")
    (format stream "(funcend)~%~%")
    (dolist (lines *func-code*)
      (dolist (l lines) (format stream "~a~%" l))
      (format stream "~%"))
    (dolist (l *runtime-code*)
      (format stream "~a~%" l))
    ;; Module libraries must be included after all functions (see string.l1com).
    (when *string-module-needed*
      (format stream "~%#include <string.l1h>~%"))
    (when *math-module-needed*
      (format stream "~%#include <math-const.l1h>~%")
      (format stream "#include <math-lib.l1h>~%"))
    (when *net-module-needed*
      (format stream "~%#include <net-lib.l1h>~%"))
    (when *file-module-needed*
      (format stream "~%#include <file-lib.l1h>~%"))
    (when *sdl-module-needed*
      (format stream "~%#include <sdl-3-lib.l1h>~%"))))

;; Common Lisp reads "5." as the integer 5, while L1VM Lisp means the double
;; 5.0.  Rewrite that one token form before the reader sees the source.

(defun trailing-dot-integer-token-p (token)
  (let* ((core (if (and (> (length token) 0)
                        (member (char token 0) '(#\+ #\-) :test #'char=))
                   (subseq token 1)
                   token))
         (len (length core)))
    (and (> len 1)
         (char= (char core (1- len)) #\.)
         (every #'digit-char-p (subseq core 0 (1- len))))))

(defun normalize-lisp-number-syntax (source)
  (with-output-to-string (out)
    (let ((i 0)
          (n (length source))
          (in-string nil)
          (in-comment nil)
          (escaped nil))
      (loop while (< i n) do
        (let ((ch (char source i)))
          (cond
            (in-comment
             (write-char ch out)
             (when (char= ch #\Newline) (setf in-comment nil))
             (incf i))
            (in-string
             (write-char ch out)
             (cond (escaped (setf escaped nil))
                   ((char= ch #\\) (setf escaped t))
                   ((char= ch #\") (setf in-string nil)))
             (incf i))
            ((char= ch #\;)
             (setf in-comment t)
             (write-char ch out)
             (incf i))
            ((char= ch #\")
             (setf in-string t)
             (write-char ch out)
             (incf i))
            ((or (digit-char-p ch)
                 (and (member ch '(#\+ #\-) :test #'char=)
                      (< (1+ i) n)
                      (digit-char-p (char source (1+ i)))))
             (let ((start i)
                   (j i))
               (when (member ch '(#\+ #\-) :test #'char=) (incf j))
               (loop while (< j n) do
                 (let ((c (char source j)))
                   (if (or (digit-char-p c)
                           (char= c #\.)
                           (char= c #\e)
                           (char= c #\E)
                           (and (member c '(#\+ #\-) :test #'char=)
                                (> j start)
                                (member (char source (1- j)) '(#\e #\E)
                                        :test #'char=)))
                       (incf j)
                       (return))))
               (let ((token (subseq source start j)))
                 (write-string token out)
                 (when (trailing-dot-integer-token-p token)
                   (write-char #\0 out))
                 (setf i j))))
            (t
             (write-char ch out)
             (incf i))))))))

;;; ---------------------------------------------------------------------------
;;; Source reading and error reporting
;;; ---------------------------------------------------------------------------

;; Reader and compiler errors are re-signalled with the source line, so an
;; error prints its location and the original message instead of only stopping
;; the translation.

(define-condition lisp-source-error (error)
  ((file   :initarg :file   :initform "<string>" :reader source-error-file)
   (line   :initarg :line   :initform 1          :reader source-error-line)
   (kind   :initarg :kind   :initform "error"    :reader source-error-kind)
   (detail :initarg :detail :initform ""         :reader source-error-detail))
  (:report (lambda (condition stream)
             (format stream "~a:~a: ~a: ~a"
                     (source-error-file condition)
                     (source-error-line condition)
                     (source-error-kind condition)
                     (source-error-detail condition)))))

(define-condition lisp-syntax-error (lisp-source-error)
  ((kind :initform "syntax error")))

(define-condition lisp-compile-error (lisp-source-error)
  ((kind :initform "error")))

(defun condition-text (condition)
  "The condition's message, without CLisp's leading newline."
  (string-trim '(#\Newline #\Return #\Space #\Tab)
               (princ-to-string condition)))

(defun form-line (form &optional (default 1))
  "Source line of FORM, or DEFAULT when it has no recorded location."
  (or (and *form-line-table* (gethash form *form-line-table*))
      default))

(defun remember-form-line (form line)
  "Record LINE as the source line of FORM, when line tracking is active."
  (when *form-line-table*
    (setf (gethash form *form-line-table*) line)))

(defun line-number-at (source position)
  "Return the 1-based source line containing character offset POSITION."
  (let ((line 1)
        (end (min (max position 0) (length source))))
    (loop for i from 0 below end do
      (when (char= (char source i) #\Newline)
        (incf line)))
    line))

(defun skip-source-whitespace (source position)
  "Move POSITION past whitespace and line comments to the next form's first
   character.  READ skips this text itself, but we need the form's real line."
  (let ((i (min (max position 0) (length source)))
        (n (length source)))
    (loop
      (cond
        ((>= i n) (return i))
        ((member (char source i) '(#\Space #\Tab #\Newline #\Return #\Page)
                 :test #'char=)
         (incf i))
        ((char= (char source i) #\;)
         (incf i)
         (loop while (and (< i n)
                          (not (char= (char source i) #\Newline)))
               do (incf i)))
        (t (return i))))))

(defun line-start-position (source line)
  "Character offset of the first character of LINE (1-based)."
  (let ((pos 0)
        (n (length source)))
    (loop repeat (1- (max 1 line)) while (< pos n) do
      (let ((newline (position #\Newline source :start pos)))
        (setf pos (if newline (1+ newline) n))))
    pos))

(defun search-text-case-insensitive (needle text start)
  "First case-insensitive occurrence of NEEDLE at or after START, or NIL."
  (let ((len (length needle))
        (limit (- (length text) (length needle))))
    (loop for i from (max 0 start) to limit do
      (when (string-equal needle text :start2 i :end2 (+ i len))
        (return i))
      finally (return nil))))

(defun source-line-of-symbol (name)
  "Best-effort line of NAME in the source, starting at the current form's line."
  (when (and *source-text* name)
    (let ((pos (search-text-case-insensitive
                (string name) *source-text*
                (line-start-position *source-text*
                                     (or *current-source-line* 1)))))
      (when pos (line-number-at *source-text* pos)))))

(defun unmatched-paren-position (source)
  "Return (position . :open|:close) for the first unmatched parenthesis, or NIL.
   Strings, comments, escaped symbols and character literals are ignored."
  (let ((stack nil)
        (i 0)
        (n (length source)))
    (loop while (< i n) do
      (let ((ch (char source i)))
        (cond
          ;; Line comment.
          ((char= ch #\;)
           (incf i)
           (loop while (and (< i n)
                            (not (char= (char source i) #\Newline)))
                 do (incf i)))
          ;; Block comment #| ... |#.
          ((and (char= ch #\#) (< (1+ i) n) (char= (char source (1+ i)) #\|))
           (incf i 2)
           (loop while (< i n) do
             (if (and (char= (char source i) #\|)
                      (< (1+ i) n)
                      (char= (char source (1+ i)) #\#))
                 (progn (incf i 2) (return))
                 (incf i))))
          ;; Escaped symbol |a ( b|.
          ((char= ch #\|)
           (incf i)
           (loop while (< i n) do
             (let ((c (char source i)))
               (cond ((char= c #\\) (incf i 2))
                     ((char= c #\|) (incf i) (return))
                     (t (incf i))))))
          ;; String literal.
          ((char= ch #\")
           (incf i)
           (loop while (< i n) do
             (let ((c (char source i)))
               (cond ((char= c #\\) (incf i 2))
                     ((char= c #\") (incf i) (return))
                     (t (incf i))))))
          ;; Character literal, e.g. #\(, is not an open parenthesis.
          ((and (char= ch #\#) (< (1+ i) n) (char= (char source (1+ i)) #\\))
           (incf i 2)
           (when (< i n) (incf i)))
          ((char= ch #\()
           (push i stack)
           (incf i))
          ((char= ch #\))
           (if stack
               (progn (pop stack) (incf i))
               (return (cons i :close))))
          (t (incf i)))))
    (when stack
      (cons (car (last stack)) :open))))

(defun read-source-forms (source file &optional (package *user-package*))
  "Read all top-level forms from SOURCE.  Return the forms, their source lines
   and SOURCE itself as three values.  Reader errors become LISP-SYNTAX-ERROR
   carrying FILE and the exact source line."
  (let ((forms nil)
        (lines nil)
        (*package* package))
    (with-input-from-string (in source)
      (loop
        (let ((start (file-position in)))
          (handler-case
              (let ((form (read in nil :eof)))
                (if (eq form :eof)
                    (return (values (nreverse forms) (nreverse lines) source))
                    (progn
                      (push form forms)
                      (push (line-number-at
                             source (skip-source-whitespace source start))
                            lines))))
            (error (condition)
              ;; The reader consumes the offending token, so use the scanner
              ;; for unmatched parentheses and the stream position otherwise.
              (let* ((mismatch (unmatched-paren-position source))
                     (position
                       (cond
                         ((typep condition 'end-of-file)
                          (if (and mismatch (eq (cdr mismatch) :open))
                              (car mismatch)
                              start))
                         ((typep condition 'reader-error)
                          (if (and mismatch (eq (cdr mismatch) :close))
                              (car mismatch)
                              (file-position in)))
                         (t (file-position in)))))
                (error 'lisp-syntax-error
                       :file file
                       :line (line-number-at source position)
                       :detail (condition-text condition))))))))))

(defun compile-with-source (forms stream lines file source)
  "Run COMPILE-SOURCE with source line tracking and location-aware errors."
  (let* ((table (make-hash-table :test 'eq))
         (first-line (or (first lines) 1)))
    (loop for form in forms for line in lines
          do (setf (gethash form table) line))
    (let ((*form-line-table* table)
          (*source-file* file)
          (*source-text* source)
          (*current-source-line* first-line))
      (handler-case
          (compile-source forms stream)
        (lisp-source-error (condition)
          (error condition))
        (error (condition)
          (error 'lisp-compile-error
                 :file file
                 :line (or *current-source-line* first-line)
                 :detail (condition-text condition)))))))

(defun compile-source-string (source &optional (stream *standard-output*))
  (let ((*readtable* *readtable*)
        (*package* *user-package*))
    (multiple-value-bind (forms lines text)
        (read-source-forms (normalize-lisp-number-syntax source) "<string>")
      (compile-with-source forms stream lines "<string>" text))))

(defun compile-source-file (input output)
  (let ((text (with-open-file (f input :direction :input)
                (let ((content (make-string (file-length f))))
                  (read-sequence content f)
                  content))))
    (multiple-value-bind (forms lines source)
        (read-source-forms (normalize-lisp-number-syntax text) input)
      ;; Compile into memory first so a failed compilation never leaves a
      ;; partial output file behind.
      (let ((result (with-output-to-string (out)
                      (compile-with-source forms out lines input source))))
        (with-open-file (out output :direction :output :if-exists :supersede)
          (write-string result out))))))

;;; ---------------------------------------------------------------------------
;;; Command line entry point
;;; ---------------------------------------------------------------------------

(defun command-line-arguments ()
  #+clisp ext:*args*
  #-clisp (error "This compiler needs CLisp's EXT:*ARGV* command-line support."))

(let* ((raw (command-line-arguments))
       (args (if (and raw (string= (first raw) "--")) (rest raw) raw)))
  (if (or (/= (length args) 2)
          (member (first args) '("-h" "--help") :test #'string=))
      (progn
        (format *error-output*
                "usage: clisp lisp2l1vm.lisp input.lisp output.l1com~%")
        #+clisp (ext:exit 2))
      (handler-case
          (progn
            (compile-source-file (first args) (second args))
            (format *error-output* "Compiled ~a -> ~a~%"
                    (first args) (second args)))
        (lisp-source-error (condition)
          (format *error-output* "~a~%" condition)
          #+clisp (ext:exit 1)))))
