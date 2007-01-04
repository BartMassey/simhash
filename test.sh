#!/bin/sh

# create filese named a and b that you want to compare, then
# use this script to try various hash sizes

SIZES="64 256 1024 4096 8192"
for j in $SIZES; do
  for i in a b ; do
    ./simhash -f $j -s 4 $i > $i-$j.sim
  done
done
for i in $SIZES; do
  for j in $SIZES; do
    echo -n `./simhash -c a-$i.sim b-$j.sim 2>/dev/null` ' '
  done
  echo ''
done
