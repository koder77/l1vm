========================================================================
L1VM Linter Autogen Tools - Documentation and Usage - Antigravity
========================================================================

This directory contains two complementary tools for the automated
generation of linter information for the L1VM project:
1. l1vm-cfunc  (Analyzes C functions for stack operations)
2. l1vm-func   (Maps these stack operations to wrapper functions in L1VM
header files (.l1h) and generates corresponding annotations)

------------------------------------------------------------------------
1. l1vm-cfunc (C Function Parser)
------------------------------------------------------------------------

This tool scans a C source code file for functions with the signature:
(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)

It analyzes their function bodies for occurrences of pop operations (stpopi,
stpopd, stpopb) and push operations (stpushi, stpushd, stpushb) in order to
determine the respective data types (int64, double, byte). Multiple arguments
are captured in the order of their appearance and separated by commas. Compilation:
gcc -Wall -Wextra -O2 l1vm-cfunc.c -o l1vm-cfunc

Usage:
./l1vm-cfunc <c_source_file> <output_file.txt>

Example:
./l1vm-cfunc nanoid.c nanoid-lint.txt

Contents of the generated output file (Example):
nanoid_create (int64) (none)
nanoid_create_custom (int64, int64, int64) (none)

------------------------------------------------------------------------
2. l1vm-func (L1vm Header Annotator)
------------------------------------------------------------------------

This tool accepts an L1vm header file (.l1h) and the output from
"l1vm-cfunc". It searches the header file for L1vm wrapper
functions (e.g., "(nanoid_create func)") and matches them against the C functions
found in the linter file.

For each match, it generates a new file containing exclusively fully
parenthesized linter annotations (without commas and without leading spaces). Compilation:
gcc -Wall -Wextra -O2 l1vm-func.c -o l1vm-func

Usage:
./l1vm-func <header_file.l1h> <cfunc_output.txt> <new_output_file.l1h>

Example:
./l1vm-func nanoid.l1h nanoid-lint.txt nanoid_annotated.l1h

Contents of the generated output file (Example):
// (func args nanoid_create int64)
// (return args nanoid_create none)
// (func args nanoid_create_custom int64 int64 int64)
// (return args nanoid_create_custom none)

------------------------------------------------------------------------
3. Complete Workflow Walkthrough (Example)
------------------------------------------------------------------------

1. Generation of raw linter data from the C code:
$ ./l1vm-cfunc nanoid.c nanoid-lint.txt

2. Generation of the final L1vm annotations for the header file:
$ ./l1vm-func nanoid.l1h nanoid-lint.txt nanoid_annotated.l1h

========================================================================
There are flags to set the begin and end of a "stpop" args block:

// args-start

// args-end

And if the number of "stpop" is variable:

// args-variable

For the return "stpush":

// return-start

// return-end

// return-variable

This is needed in C functions which have more than one exit path "stpush".
See "message-demo.c".
