I used this Common Lisp Highlighter as a template for my Brackets language of L1VM!
Stefan Pietzonke (jay-t@gmx.net)

Just copy this folder to:
~/.atom/packages


# Common Lisp Highlighting & Support in Atom

An awesome package lovingly created to support proper syntax highlighting of
Common Lisp files.

(If you want Emacs Lisp highlighting use Alhadis' [Emacs Highlighting](https://github.com/Alhadis/language-emacs-lisp) package)

This is *not* converted from the Textmate Bundle, and therefore does not have
the shortcomings of said bundle!

## Warning:
There are some flaws in parsing lambda-lists, especially ones with `&optional (foo "bar")`
type things in it.  Since parsing that is *not* regular (I don't think) I am probably
going to have to re-write it in an *actual* language.  So just be careful and let
me know if you know how to fix it! Thanks! :-D

## TODO:
* Get Complex numbers `#c(5.0 3/2)` working properly.
* Remove a lot of the scheme/Racket-isms that are in the code, as I have use a *LOT*
  of their regexes to jump-start this work.
* Get `loop` and all of it's keywords working just fine.
* Set the read-macro regex, especially for pathnames
* Fix typing the single quote character `'` so it doesn't auto-fill a second one
* Other stuff I haven't thought of yet...


![Lisp](https://cdn.rawgit.com/serialhex/language-common-lisp/eaae981b68cff11951f296174f1248f03c7e1083/lisplogo_alien.svg)
