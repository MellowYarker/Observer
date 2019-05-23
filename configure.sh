#!/bin/bash
# configure and compile libbtc
# compile libbloom
# move dynamic libs to force gcc to use static
# create sqlite3 database with keys table
#   *note* since all these keys are really just integers, see if it's worth it to
#   store them in the DB as numbers rather than varchars. Since we're at risk of
#   performing a lot of DB reads, it might make look up faster.
cd db;
sqlite3 observer.db < configure.sql
