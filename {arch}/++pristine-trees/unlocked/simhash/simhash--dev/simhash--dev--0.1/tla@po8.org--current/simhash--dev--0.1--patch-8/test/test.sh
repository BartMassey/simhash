#!/bin/sh
SIZES="64 256 1024 4096 8192"
for j in $SIZES; do
  for i in a.c b.c ; do
    ../simhash -f $j -s 4 $i > $i-$j.sim
  done
done
for i in $SIZES; do
  for j in $SIZES; do
    echo -n `../simhash -c a.c-$i.sim b.c-$j.sim 2>/dev/null` ' '
  done
  echo ''
done
