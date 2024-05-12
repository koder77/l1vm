# nanoid.h  
A 2.3 Kb single header [NanoID](https://github.com/ai/nanoid) implementation in C

## What is NanoID?
NanoID is a tiny, secure URL-friendly unique string ID generator.  
It's **safe**. It uses cryptographically strong random APIs that guarantees proper distribution of symbols.  
And it's **compact**. It uses a larger alphabet than the standard UUID (`A-Za-z0-9_-`), and has a similar number of unique IDs in just 21 symbols instead of 36.

## Usage

```c
#include "nanoid.h"
```

This header file exposes 3 functions.  

`simple()` - generate an id of default length (21) using the default character set  
```c
char *id = simple(); // _n7XMO8sd8lCDnDLmyezf
```


`generate(int size)` - generate an id of custom length using the default character list  

```c
char *id = generate(5); // xgTpQ
```

`custom(char custom_alphs[], int size)` - generate an id of custom length using a custom character set  

```c
char custom_alphs[] = "abcde";
char *id = custom(custom_alphs, 7); // ecbebec
```

## New Features
Added **safer** functions, to use them you can prefix the default functions with safe_ for example you can use the **safe** version of by executing: `safe_simple()`.
The new **safe** functions use /dev/urandom as a seed to generate the random block that is returned to the user. Due to this the header file is 1.1 Kb larger 
than the original which was 1.2 Kb