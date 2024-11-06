#!/bin/sh

#directory containing the loss files
datadir=$1

for i in $datadir/*_is_lost.txt
do
	perc=`cat $i | awk '{a+=$1}END{print a/NR}'`
	echo $perc $i
done > percentage_list.txt

sort -n percentage_list.txt | awk '{print $2}' > percentage_sorted.txt

for i in `cat percentage_sorted.txt`
do
	cat $i
done > loss_sorted.txt
