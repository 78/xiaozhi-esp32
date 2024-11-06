#!/bin/sh

for i in fargan_data.c pitchdnn_data.c dred_rdovae_dec_data.c dred_rdovae_enc_data.c plc_data.c lace_data.c nolace_data.c
do
    cat dnn/$i | perl -ne 'if (/DEBUG/ || /#else/) {$skip=1} if (!$skip && !/ifdef DOT_PROD/) {s/^ *//; s/, /,/g; print $_} elsif (/endif/) {$skip=0}' > tmp_data.c
    mv tmp_data.c dnn/$i
done
