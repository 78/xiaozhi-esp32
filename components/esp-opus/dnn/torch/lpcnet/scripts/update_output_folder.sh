#!/bin/bash


case $# in
    3) FOLDER=$1; MODEL=$2; PYTHON=$3;;
    *) echo "update_output_folder.sh folder model python"; exit;;
esac


SCRIPTFOLDER=$(dirname "$0")


# update setup
echo "updating $FOLDER/setup.py..."
$PYTHON $SCRIPTFOLDER/update_setups.py $FOLDER/setup.yml --model $MODEL

# update checkpoints
for fn in $(find $FOLDER -type f -name "checkpoint*.pth")
do
    echo "updating $fn..."
    $PYTHON $SCRIPTFOLDER/update_checkpoints.py $fn --model $MODEL
done