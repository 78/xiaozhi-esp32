#!/bin/bash

if [ ! -f "$PYTHON" ]
then
    echo "PYTHON variable does not link to a file. Please point it to your python executable."
    exit 1
fi

if [ ! -f "$TESTMODEL" ]
then
    echo "TESTMODEL variable does not link to a file. Please point it to your copy of test_model.py"
    exit 1
fi

if [ ! -f "$OPUSDEMO" ]
then
    echo "OPUSDEMO variable does not link to a file. Please point it to your patched version of opus_demo."
    exit 1
fi

if [ ! -f "$LACE" ]
then
    echo "LACE variable does not link to a file. Please point it to your copy of the LACE checkpoint."
    exit 1
fi

if [ ! -f "$NOLACE" ]
then
    echo "LACE variable does not link to a file. Please point it to your copy of the NOLACE checkpoint."
    exit 1
fi

case $# in
    2) INPUT=$1; OUTPUT=$2;;
    *) echo "process_dataset.sh <input folder> <output folder>"; exit 1;;
esac

if [ -d $OUTPUT ]
then
    echo "output folder $OUTPUT exists, aborting..."
    exit 1
fi

mkdir -p $OUTPUT

if [ "$BITRATES" == "" ]
then
    BITRATES=( 6000 7500 9000 12000 15000 18000 24000 32000 )
    echo "BITRATES variable not defined. Proceeding with default bitrates ${BITRATES[@]}."
fi


echo "LACE=${LACE}" > ${OUTPUT}/info.txt
echo "NOLACE=${NOLACE}" >>  ${OUTPUT}/info.txt

ITEMFILE=${OUTPUT}/items.txt
BITRATEFILE=${OUTPUT}/bitrates.txt

FPROCESSING=${OUTPUT}/processing
FCLEAN=${OUTPUT}/clean
FOPUS=${OUTPUT}/opus
FLACE=${OUTPUT}/lace
FNOLACE=${OUTPUT}/nolace

mkdir -p $FPROCESSING $FCLEAN $FOPUS $FLACE $FNOLACE

echo "${BITRATES[@]}" > $BITRATEFILE

for fn in $(find $INPUT -type f -name "*.wav")
do
    UUID=$(uuid)
    echo "$UUID $fn" >> $ITEMFILE
    PIDS=(  )
    for br in ${BITRATES[@]}
    do
        # run opus
        pfolder=${FPROCESSING}/${UUID}_${br}
        mkdir -p $pfolder
        sox $fn -c 1 -r 16000 -b 16 -e signed-integer $pfolder/clean.s16
        (cd ${pfolder} && $OPUSDEMO voip 16000 1 $br clean.s16 noisy.s16)

        # copy clean and opus
        sox -c 1 -r 16000 -b 16 -e signed-integer $pfolder/clean.s16 $FCLEAN/${UUID}_${br}_clean.wav
        sox -c 1 -r 16000 -b 16 -e signed-integer $pfolder/noisy.s16 $FOPUS/${UUID}_${br}_opus.wav

        # run LACE
        $PYTHON $TESTMODEL $pfolder $LACE $FLACE/${UUID}_${br}_lace.wav &
        PIDS+=( "$!" )

        # run NoLACE
        $PYTHON $TESTMODEL $pfolder $NOLACE $FNOLACE/${UUID}_${br}_nolace.wav &
        PIDS+=( "$!" )
    done
    for pid in ${PIDS[@]}
    do
        wait $pid
    done
done
