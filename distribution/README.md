DISTRIBUTION
============
Here is a script for signing .l1obj.bz2 object files by gpg: "sign.sh".
The usage is simple, however you have to customize the script first:

```
gpg --detach-sign --default-key YOURKEYID -o $1.gpg $1.l1obj.bz2
```

You have to edit this to your gpg key ID!

Now you can run it:

```
$ ./sign.sh hello
```

This will create the signature file "hello.gpg" for you.

The distribution directory can be in your local network or somewhere in the internet.

You have to edit the "get-obj.sh" script to your server on your network:

```
curl -O http://localhost:2000/web/$1.l1obj
curl -O http://localhost:2000/web/$1.gpg
```

Now you can use it:

```
$ run.sh hello
```

This will download "hello.l1obj" from the server and run it.

This scripts are useful if you want to make sure your programs are not modified by someone else and maybe do some hosting of programs.
