NOTE
====
In this examples you may find the older way to use the interrupts in Brackets, like this: (deprecated!!)

```
(4 ind 0 0 intr0)
(7 0 0 0 intr0)
```

This should be replaced by the new preprocessor macros:

```
print_i (ind)
print_n
```

This does the same but is more readable!
You always should use the macros in: ```intr.l1h```!

