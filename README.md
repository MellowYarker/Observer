WIP, currently able to generate private and public keys from a textfile.

For example, the file consists of:
    password
    123456
    hello123
When given a string, ex, **password**, two 32 char private keys will be generated: 
    000...00**password** 
    **password**00...000 
These are added to a bloom filter (and in the future, a database as well).

This software depends on [libbtc](https://github.com/libbtc/libbtc)(Bitcoin library written in C) and [libbloom](https://github.com/jvirkki/libbloom) (C implementation of a bloom filter), I'll be adding these as git subtrees soon.

I will add a configuration file to build libbtc and libbloom, below is the project file structure.

Observer
├── src
│
├── libbtc
│   └── libbtc.pc
│
├── libbloom
    └── build
        └── libbloom.a
  
