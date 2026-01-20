Doom Emacs INSTALL README
=========================
Emacs 27.x
==========
Copy the "l1vm-mode.el" into:

```
~/.config/emacs/modules/tools/lsp/autoload/
```

Emacs 28.x
==========
Copy the "l1vm-mode.el" into:

```
~/.emacs/modules/lang/l1vm/autoload/
```



Both (27.x and 28.x)
====================
Put the following line into your "~/.config/doom/init.el" file after the line: "(doom! :input)" (where the other syntax highlighter are set):

```
(l1vm +lsp)         ; L1VM Brackets and assembly
```


Now run in: "~/.emacs.d/bin":

```
$ doom sync
```

The doom files now should be up to date.
Now use in Emacs:

```
Alt + x
```

To select the "l1vm-mode".


Snippets
========
copy the "snippets" directory into your "doom" directory.
It contains snippets to create a normal function and contract functions.
