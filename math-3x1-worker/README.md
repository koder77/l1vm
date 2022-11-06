L1VM l1vmgodata 3x1 problem demo
================================
This is a 3x1 problem calculate demo.
It uses a database to share data between worker clients.
Ok: lets take a number, if it's even then divide by 2.
If it is odd, then multiply by 3 and add 1.
Now continue this until 1 is reached.
Every natural number should give 1 as a result.

Here we go:
start l1vmgodata database:

```
$ ./l1vmgodata 127.0.0.1 2000 10001
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

You can check the result for: ```1000```:

```
get key :res1000
1
```

