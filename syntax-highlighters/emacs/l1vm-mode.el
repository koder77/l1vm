;;; l1vm-mode.el --- sample major mode for editing l1vm source code. -*- coding: utf-8; lexical-binding: t; -*-

;; Copyright © 2022, by koder77

;; Author: koder77 ( spietzonke@gmail.com )
;; Version: 1.0
;; Created: 10 Jul 2022
;; Keywords: languages
;; Homepage: midnight-koder.net

;; This file is not part of GNU Emacs.

;;; License:

;; You can redistribute this program and/or modify it under the terms of the GNU General Public License version 2.

;;; Commentary:

;; short description here

;; full doc on how to use here

;;; Code:

;; create the list for font-lock.
;; each category of keyword is given a particular face
(setq l1vm-font-lock-keywords
      (let* (
            ;; define several category of keywords
            (x-keywords '("if+" "if" "else" "endif" "for-loop" "for" "next" "do" "while" "set" "pushb" "pushw" "pushdw" "pushd" "pullb" "pullw" "pulldw" "pullqw" "pulld" "addi" "subi" "muli" "divi" "addd" "subd" "muld" "divd" "smuli" "sdivi" "andi" "ori" "bandi" "bori" "bxori" "modi" "eqi" "neqi" "gri" "lsi" "greqi" "lseqi" "eqd" "neqd" "grd" "lsd" "greqd" "lseqd" "jmp" "jmpi" "stpushb" "stpopb" "stpushi" "stpopi" "stpushd" "stpopd" "stpush" "stpop" "loada" "loadd" "intr0" "intr1" "inclsijmpi" "decgrijmpi" "movi" "movd" "loadl" "jmpa" "jsr" "rts" "load" "noti" "call" "!" "exit" "loadreg" "reset-reg" "pointer" "cast"))
            (x-types '("byte" "string" "int16" "int32" "int64" "double"))
            (x-constants '("const-byte" "const-string" "const-int16" "const-int32" "const-int64" "const-double"))
            (x-functions '("#func" "#define" "#var" "#include" "func" "funcend" "ASM" "ASM_END" "optimize-if-off" "optimize-if" "no-var-pull-off" "no-var-pull-on" "reset-reg"))
            ;; generate regex string for each category of keywords
            (x-keywords-regexp (regexp-opt x-keywords 'words))
            (x-types-regexp (regexp-opt x-types 'words))
            (x-constants-regexp (regexp-opt x-constants 'words))
            (x-functions-regexp (regexp-opt x-functions 'words)))
          `(
            (,x-functions-regexp . 'font-lock-function-name-face)
            (,x-constants-regexp . 'font-lock-constant-face)
            (,x-types-regexp . 'font-lock-type-face)
          (,x-keywords-regexp . 'font-lock-keyword-face)
          ;; note: order above matters, because once colored, that part won't change.
          ;; in general, put longer words first
          )))

;;;###autoload
(define-derived-mode l1vm-mode c-mode "l1vm mode"
  "Major mode for editing l1com files."

  ;; code for syntax highlighting
  (setq font-lock-defaults '((l1vm-font-lock-keywords))))

;; add the mode to the `features' list
(provide 'l1vm-mode)

;;; l1vm-mode.el ends here
