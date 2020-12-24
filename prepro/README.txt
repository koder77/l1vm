This is the L1VM preprocessor!

The usage is simple:
./l1pre input.l1com output.l1com ~/l1vm/include/

This creates a new output.l1com file with all includes ready to compile by l1com.

The includes are done like this:
#include <sdl-lib.l1h>

The headers are in the include-lib/ directory and are copied to the ~/l1vm/include at install.
