#!/bin/bash


INPUT="dataset/LibriSpeech"
OUTPUT="testdata"
OPUSDEMO="/local/experiments/ietf_enhancement_studies/bin/opus_demo_patched"
BITRATES=( 6000 7500 ) # 9000 12000 15000 18000 24000 32000 )


mkdir -p $OUTPUT

for fn in $(find $INPUT -name "*.wav")
do
    name=$(basename ${fn%*.wav})
    sox $fn -r 16000 -b 16 -e signed-integer ${OUTPUT}/tmp.raw
    for br in ${BITRATES[@]}
    do
        folder=${OUTPUT}/"${name}_${br}.se"
        echo "creating ${folder}..."
        mkdir -p $folder
        cp ${OUTPUT}/tmp.raw ${folder}/clean.s16
        (cd ${folder} && $OPUSDEMO voip 16000 1 $br clean.s16 noisy.s16)
    done
    rm -f ${OUTPUT}/tmp.raw
done
