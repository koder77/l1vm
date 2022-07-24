L1VM worker client demo.
This demo downloads the "hello-worker" program from my L1VM repository and runs it.

There is the "run-worker" program which runs in a loop and checks the l1vmgodata data base for it's worker id. If it is in the data base it will download and run the program set as a value in the worker id.

The worker program then can connect to the data base and load/store the data it needs.
In this example we set the data base entry by hand. You can modify the "download.sh" script and put your own website or web server ip there!

Here we start: first run the "l1vmgodata" data base:

```
$ ./l1vmgodata 127.0.0.1 2000
```

Then open an new shell:

```
$nc 127.0.0.1 2000
store data :worker01 'hello-worker'
```

Then open a new shell and type in:

```
$ l1vm run-worker -args 127.0.0.1 2000 worker01
```

This should run the program and download and run the "hello-worker" data base client program

Now in the shell with the "nc" connected you can do:

```
get key :01hello
Hello world!
```

So here the client stored "Hello world!" into the data base.
I hope you find this demo useful. You can do much more stuff with this!
