# Deep REDundancy (DRED) with RDO-VAE

This is a rate-distortion-optimized variational autoencoder (RDO-VAE) designed
to coding redundancy information. Pre-trained models are provided as C code
in the dnn/ directory with the corresponding model in dnn/models/ directory
(name starts with rdovae_). If you don't want to train a new DRED model, you can
skip straight to the Inference section.

## Data preparation

For data preparation you need to build Opus as detailed in the top-level README.
You will need to use the --enable-dred configure option.
The build will produce an executable named "dump_data".
To prepare the training data, run:
```
./dump_data -train in_speech.pcm out_features.f32 out_speech.pcm
```
Where the in_speech.pcm speech file is a raw 16-bit PCM file sampled at 16 kHz.
The speech data used for training the model can be found at:
https://media.xiph.org/lpcnet/speech/tts_speech_negative_16k.sw
The out_speech.pcm file isn't needed for DRED, but it is needed to train
the FARGAN vocoder (see dnn/torch/fargan/ for details).

## Training

To perform training, run the following command:
```
python ./train_rdovae.py --cuda-visible-devices 0 --sequence-length 400 --split-mode random_split --state-dim 80 --batch-size 512 --epochs 400 --lambda-max 0.04 --lr 0.003 --lr-decay-factor 0.0001 out_features.f32 output_dir
```
The final model will be in output_dir/checkpoints/chechpoint_400.pth.

The model can be converted to C using:
```
python export_rdovae_weights.py output_dir/checkpoints/chechpoint_400.pth dred_c_dir
```
which will create a number of C source and header files in the fargan_c_dir directory.
Copy these files to the opus/dnn/ directory (replacing the existing ones) and recompile Opus.

## Inference

DRED is integrated within the Opus codec and can be evaluated using the opus_demo
executable. For example:
```
./opus_demo voip 16000 1 64000 -loss 50 -dred 100 -sim_loss 50 input.pcm output.pcm
```
Will tell the encoder to encode a 16 kHz raw audio file at 64 kb/s using up to 1 second
of redundancy (units are based on 10-ms) and then simulate 50% loss. Refer to `opus_demo --help`
for more details.
