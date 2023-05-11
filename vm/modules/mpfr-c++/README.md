AUTOMATICALLY GENERATE MPFR libraries
=====================================
Open up this directory in a shell.

NOTE:
====
This file includes a patched version of "mpreal.h"
I did replace all places with "srcptr()"" to "xsrcptr()".
This makes it possible to use "mpreal.h" with recent versions of mpfr library, which
defined "xsrcptr" to something else!

Copy "mpreal.h" to "/usr/local/include".

1. $ export CC=clang && export CCPP=clang++ && zerobuild zerobuild-generate.txt
   $ ./generate-mpfr-lib
2. Load "mpfr-lib-head.l1com" into an text editor.
3. Create an lib file with the name: "mpfr-lib-auto.l1com".
4. Paste the beginning of the head file into the new text file.
5. Paste generated file "mpfr-lib.l1com" into text.
6. Paste the end of "mpfr-lib-head.l1com" into text.
7. Save file "mpfr-lib-auto.l1com".
8. Create new file: "mpfr-combined.cpp"
9. Paste the file: "mpfr.cpp" into new file.
10. Build C library:
11. $ export CC=clang && export CCPP=clang++ && zerobuild zerobuild.txt
12. Copy "libl1vmmpfr.so" into "~/bin" or "/usr/local/lib".
13. Copy file: "mpfr-lib-auto.l1com" into "l1vm/lib" directory.
13. Build VM library goto "l1vm" root directory!
14. $ comp/l1com lib/mpfr-lib-auto
15. $ assemb/l1asm lib/mpfr-lib-auto
16. FINISHED!!
