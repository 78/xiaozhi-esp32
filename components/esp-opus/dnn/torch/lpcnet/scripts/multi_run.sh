#!/bin/bash

case $# in
    9) SETUP=$1; OUTDIR=$2; NAME=$3; NUMDEVICES=$4; ROUNDS=$5; LPCNEXT=$6; LPCNET=$7; TESTSUITE=$8; TESTITEMS=$9;;
    *) echo "multi_run.sh setup outdir name num_devices rounds_per_device lpcnext_repo lpcnet_repo testsuite_repo testitems"; exit;;
esac


LOOPRUN=${LPCNEXT}/loop_run.sh

mkdir -p $OUTDIR

for ((i = 0; i < $NUMDEVICES; i++))
do
    echo "launching job queue for device $i"
    nohup bash $LOOPRUN $SETUP $OUTDIR "$NAME" "cuda:$i" $ROUNDS $LPCNEXT $LPCNET $TESTSUITE $TESTITEMS > $OUTDIR/job_${i}_out.txt &
done
