CROSSWORDS puzzle grid designer
===============================
With this program you can generate a word grid puzzle.
You have an empty grid with one or more seed words and a word list to fill in.

1. Installation
---------------
<pre>
Unzip the "dictionary.tar.bz2" archive and copy it to "~/l1vm/" the "l1vm" directory in your "/home" directory.
Create a new directory "crosswords" inside the "~/l1vm/" directory.

Now you need to edit "crosswords.l1com": set your user name in the wordfile path as shown in the source code.

You can edit the program to use another "seedstr" and set the position in the grid.

Now compile "crosswords":

$ l1vm-build.sh crosswords
</pre>

2. Usage
-------
<pre>
$ ./l1vm crosswords -q >crossword-01.txt 2>&1

The program generates the "wordlist.txt" file in "~/l1vm/crosswords/".
You can sort the word list:

$ cat wordlist.txt > wordlist-sorted.txt

Now put the empty grid and the sorted word list into a file with the grid at the top and the words below.

Have fun!
</pre>
