# Observer

This software is designed to derive bitcoin addresses from *non-random* or *poorly generated* random private keys. These types of private keys are considered to be *weak*; any bitcoin associated with these types of addresses are at risk of being stolen.

### Purpose
Find and move bitcoin secured by weak keys before others with nefarious intentions try to, then offer the owner an easy and reliable way to reclaim the funds.

There are three parts to this process:
* Generate and store a massive amount of weak private keys. (**Almost complete**)
* Watch the bitcoin blockchain to see if any of the addresses in our database have funds. (**Not started**)
* Move any discovered funds and offer the owner a way to reclaim them (*at a new secure address*). (**Not started**)

## WARNING
**Do not send bitcoin to addresses generated by this software** unless the seeds you supply the generator with are sufficiently random *and* you know what you're doing. See [why randomness matters](https://blog.cloudflare.com/why-randomness-matters/).

### How does it work?
The program reads "seeds" from a text file (check out src/100kseeds.txt for some examples), turns them into private keys, then generates the P2PKH, P2SH_P2WPKH, and P2WPKH that can be derived from the private keys. The results are stored in an sqlite3 database, and the private key is added to a bloom filter.

The textfile of seeds should be in the following format

    password
    123456
    hello123
*See the [wiki](https://github.com/MellowYarker/Observer/wiki/Seeds-and-Private-Keys) for examples of turning seeds into private keys.*

This software depends on [libbtc](https://github.com/libbtc/libbtc) (Bitcoin library written in C) and [libbloom](https://github.com/jvirkki/libbloom) (C implementation of a bloom filter), they've both been added to this repo as git subtrees.

## Build
```bash
sh configure.sh
cd src
make
```
The configure script will take some time since it configures and compiles 3 libraries.
Check the Makefile in src for more compile options.

## Usage

```bash
./gen_keys <quantity of seeds> <input file>
```
For example
```
$ ./gen_keys 3000 100kseeds.txt
```

Key sets are stored in an sqlite3 table, here's a quick example of querying the database.
```bash
$ cd db
$ sqlite3 observer.db
SQLite version 3.16.0 2016-11-04 19:09:39
Enter ".help" for usage hints.
sqlite> .mode columns
sqlite> .headers on
sqlite> select * from keys where seed like 'password';
privkey                           seed        P2PKH                               P2SH                                P2WPKH
--------------------------------  ----------  ----------------------------------  ----------------------------------  ------------------------------------------
000000000000000000000000password  password    194Gw5oZnHWNoC1eg2EJSpkYPqT55fmT8L  3DGDdvVL49bZreL8r59ZdBF8nSV1kqT3Nv  bc1qtp0cmn9ug0pyz8ncky8uew2rtvv37a4z2y5nn6
password000000000000000000000000  password    1U44rmtsDPjV1CsrZ9JXh3WFLUTkFD99E   3C5EdoQzkF7N1ESMKpQGZFVirftx9DCKo7  bc1qq5wu5ml0xe7djvha6y00sz8qxunwlxw6glkudg
```
