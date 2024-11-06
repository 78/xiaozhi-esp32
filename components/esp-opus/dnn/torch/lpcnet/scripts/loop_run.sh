#!/bin/bash


case $# in
    9) SETUP=$1; OUTDIR=$2; NAME=$3; DEVICE=$4; ROUNDS=$5; LPCNEXT=$6; LPCNET=$7; TESTSUITE=$8; TESTITEMS=$9;;
    *) echo "loop_run.sh setup outdir name device rounds lpcnext_repo lpcnet_repo testsuite_repo testitems"; exit;;
esac


PYTHON="/home/ubuntu/opt/miniconda3/envs/torch/bin/python"
TESTFEATURES=${LPCNEXT}/testitems/features/all_0_orig_features.f32
WARPQREFERENCE=${LPCNEXT}/testitems/wav/all_0_orig.wav
METRICS="warpq,pesq,pitch_error,voicing_error"
LPCNETDEMO=${LPCNET}/lpcnet_demo

for ((round = 1; round <= $ROUNDS; round++))
do
    echo
    echo round $round

    UUID=$(uuidgen)
    TRAINOUT=${OUTDIR}/${UUID}/training
    TESTOUT=${OUTDIR}/${UUID}/testing
    CHECKPOINT=${TRAINOUT}/checkpoints/checkpoint_last.pth
    FINALCHECKPOINT=${TRAINOUT}/checkpoints/checkpoint_finalize_last.pth

    # run training
    echo "starting training..."
    $PYTHON $LPCNEXT/train_lpcnet.py $SETUP $TRAINOUT --device $DEVICE --test-features $TESTFEATURES --warpq-reference $WARPQREFERENCE

    # run finalization
    echo "starting finalization..."
    $PYTHON $LPCNEXT/train_lpcnet.py $SETUP $TRAINOUT \
            --device $DEVICE --test-features $TESTFEATURES \
            --warpq-reference $WARPQREFERENCE \
            --finalize --initial-checkpoint $CHECKPOINT

    # create test configs
    $PYTHON $LPCNEXT/make_test_config.py ${OUTDIR}/${UUID}/testconfig.yml "$NAME $UUID" $CHECKPOINT --lpcnet-demo $LPCNETDEMO
    $PYTHON $LPCNEXT/make_test_config.py ${OUTDIR}/${UUID}/testconfig_finalize.yml "$NAME $UUID finalized" $FINALCHECKPOINT --lpcnet-demo $LPCNETDEMO

    # run tests
    echo "starting test 1 (no finalization)..."
    $PYTHON $TESTSUITE/run_test.py ${OUTDIR}/${UUID}/testconfig.yml  \
            $TESTITEMS ${TESTOUT}/prefinal --num-workers 8 \
            --num-testitems 400 --metrics $METRICS

    echo "starting test 2 (after finalization)..."
    $PYTHON $TESTSUITE/run_test.py ${OUTDIR}/${UUID}/testconfig_finalize.yml  \
            $TESTITEMS ${TESTOUT}/final --num-workers 8 \
            --num-testitems 400 --metrics $METRICS
done
