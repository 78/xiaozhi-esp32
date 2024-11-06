#!/bin/bash


case $# in
    3) FEATURES=$1; FOLDER=$2; PYTHON=$3;;
    *) echo "run_inference_test.sh <features file> <output folder> <python path>"; exit;;
esac


SCRIPTFOLDER=$(dirname "$0")

mkdir -p $FOLDER/inference_test

# update checkpoints
for fn in $(find $FOLDER -type f -name "checkpoint*.pth")
do
    tmp=$(basename $fn)
    tmp=${tmp%.pth}
    epoch=${tmp#checkpoint_epoch_}
    echo "running inference with checkpoint $fn..."
    $PYTHON $SCRIPTFOLDER/../test_lpcnet.py $FEATURES $fn $FOLDER/inference_test/output_epoch_${epoch}.wav
done
