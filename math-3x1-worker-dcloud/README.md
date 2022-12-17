L1VM l1vmgodata 3x1 problem demo using dcloud cluster l1vmgodata databases
==========================================================================
This example shows how to use a database cluster running more than one database on different machines.
The l1vmgodata databases are used as if there is one big database on one machine.
The store data function stores the data cycled on all databases.
The read functions gets the data from the cluster node where it is stored.
So you can use a cluster of multiple machines to store and get data! <br>

This is a 3x1 problem calculate demo.
It uses a database to share data between worker clients.
Ok: lets take a number, if it's even then divide by 2.
If it is odd, then multiply by 3 and add 1.
Now continue this until 1 is reached.
Every natural number should give 1 as a result.
NEW: now all steps to 1 are stored in the database!
Here we go:
Start two l1vmgodata databases:

```
$ ./l1vmgodata 127.0.0.1 2000 10001 35000
```

```
$ ./l1vmgodata 127.0.0.1 2001 10002 35000
```

now open a second shell:

```
$ l1vm init-database 
```

If this is finished, you can run the worker:

```
$ l1vm 3x1-worker
```

Now the calculations are done.

Now check the results. Start:

```
$ l1vm 3x1-reader -q
```

```
dcloud_init
ip: 127.0.0.1 port: 2000
USAGE 85.00% : 29772 of 35000
dcloud_init: got socket handle: 0
dcloud_init
ip: 127.0.0.1 port: 2001
USAGE 85.00% : 29773 of 35000
dcloud_init: got socket handle: 1
number? (empty input to end) 42
21
64
32
16
8
4
2
1
```

You can also open the database webpage form at: ```http://127.0.0.1:10001/``` or ```http://127.0.0.1:10002/```
