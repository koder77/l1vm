Testing the new build script
============================
Copy the build scripts with "wspace" in their name to "~/l1vm/bin".

Now run the build script in this directory: (in your shell)

```
$ l1vm-build-wspace.sh hello-test
```

This is the output:

```
building: hello-test
error: found double spaces!
line: 9
>  (hello  :print_s !)


error: found double spaces!
line: 11
>  (zero :exit  !)


error: found double spaces!
line: 97
>  (hello  :print_s !)


ERROR: found errors!
preprocessor build failed:
```

To fix this you have to remove the second spaces in this lines.
Line 9 looks now like this:

```
    (hello :print_s !)

```

And the line 11:

```
    (zero :exit !)

```

Now the test program can be build!
