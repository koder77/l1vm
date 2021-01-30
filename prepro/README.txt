This is the L1VM preprocessor!

The usage is simple:
./l1pre input.l1com output.l1com ~/l1vm/include/

This creates a new output.l1com file with all includes ready to compile by l1com.

The includes are done like this:
#include <sdl-lib.l1h>

The headers are in the include-lib/ directory and are copied to the ~/l1vm/include at install.

NEW: now "#define" can be set to define strings:

#define PRINT_NEWLINE	(7 0 0 0 intr0)

And now even macros can be set:

#func delay(x) :(8 x 0 0 intr0)
delay(foo)

This will get this output:

(8 foo 0 0 intr0)


NEW now a function variable can be written as "varname~" :

(func foo)
#var ~ @foo
(set int64 1 x_start~ 10)
(set int64 1 y_start~ 10)

The automatic variable end tack on is done by the preprocessor.
The variables are named like "x_start@foo" in the output file.

NEW: SDL GUI: set_gadget_box_grid() to draw a grid of gadgets which can be read out via GUI event function.

NEW: Now the preprocessor can parse the normal comment: "//" too!
