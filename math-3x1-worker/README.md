L1VM l1vmgodata 3x1 problem demo
================================
This is a 3x1 problem calculate demo.
It uses a database to share data between worker clients.
Ok: lets take a number, if it's even then divide by 2.
If it is odd, then multiply by 3 and add 1.
Now continue this until 1 is reached.
Every natural number should give 1 as a result.
NEW: now all steps to 1 are stored in the database!
Here we go:
start l1vmgodata database:

```
$ ./l1vmgodata 127.0.0.1 2000 10001 60000
```

now open a second shell:

```
$ l1vm init-database 
```

If this is finished, you can run the worker:

```
$ l1vm 3x1-worker
```

Now the calculations are done. You can run multiple workers too!

Now check the results. Start ```nc```:

```
$ nc 127.0.0.1 2000
```

You can check the result for: ```1000``` step 1:

```
get key :res1000-1
500
```

To get the next steps just increase the step number:

```
get key :res1000-2
250
```
Here is the worker output for ```999```:

```
res999-1 2998
res999-2 1499
res999-3 4498
res999-4 2249
res999-5 6748
res999-6 3374
res999-7 1687
res999-8 5062
res999-9 2531
res999-10 7594
res999-11 3797
res999-12 11392
res999-13 5696
res999-14 2848
res999-15 1424
res999-16 712
res999-17 356
res999-18 178
res999-19 89
res999-20 268
res999-21 134
res999-22 67
res999-23 202
res999-24 101
res999-25 304
res999-26 152
res999-27 76
res999-28 38
res999-29 19
res999-30 58
res999-31 29
res999-32 88
res999-33 44
res999-34 22
res999-35 11
res999-36 34
res999-37 17
res999-38 52
res999-39 26
res999-40 13
res999-41 40
res999-42 20
res999-43 10
res999-44 5
res999-45 16
res999-46 8
res999-47 4
res999-48 2
res999-49 1
```
