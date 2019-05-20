# Observer - a Bitcoin address generator

WIP, currently able to generate private and public keys from a textfile.

The textfile of seeds should be in the following format

    password
    123456
    hello123

When given a string, for example, "*password*", two 32 char private keys will be generated: 

    000...00password
    password00...000

These are added to a bloom filter (and in the future, a database as well).

This software depends on [libbtc](https://github.com/libbtc/libbtc)(Bitcoin library written in C) and [libbloom](https://github.com/jvirkki/libbloom) (C implementation of a bloom filter), I'll be adding these as git subtrees soon.

I will also add a configuration file to build libbtc and libbloom so that they can be linked by the compiler. Below is the project file structure.

    Observer
    ├── src
    ├── libbtc
    │   └── libbtc.pc
    ├── libbloom
        └── build
            └── libbloom.a
