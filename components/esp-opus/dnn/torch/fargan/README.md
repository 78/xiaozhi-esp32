# Framewise Auto-Regressive GAN (FARGAN)

Implementation of FARGAN, a low-complexity neural vocoder. Pre-trained models
are provided as C code in the dnn/ directory with the corresponding model in
dnn/models/ directory (name starts with fargan_). If you don't want to train
a new FARGAN model, you can skip straight to the Inference section.

## Data preparation

For data preparation you need to build Opus as detailed in the top-level README.
You will need to use the --enable-deep-plc configure option.
The build will produce an executable named "dump_data".
To prepare the training data, run:
```
./dump_data -train in_speech.pcm out_features.f32 out_speech.pcm
```
Where the in_speech.pcm speech file is a raw 16-bit PCM file sampled at 16 kHz.
The speech data used for training the model can be found at:
https://media.xiph.org/lpcnet/speech/tts_speech_negative_16k.sw

## Training

To perform pre-training, run the following command:
```
python ./train_fargan.py out_features.f32 out_speech.pcm output_dir --epochs 400 --batch-size 4096 --lr 0.002 --cuda-visible-devices 0
```
Once pre-training is complete, run adversarial training using:
```
python adv_train_fargan.py out_features.f32 out_speech.pcm output_dir --lr 0.000002 --reg-weight 5 --batch-size 160 --cuda-visible-devices 0 --initial-checkpoint output_dir/checkpoints/fargan_400.pth
```
The final model will be in output_dir/checkpoints/fargan_adv_50.pth.

The model can optionally be converted to C using:
```
python dump_fargan_weights.py output_dir/checkpoints/fargan_adv_50.pth fargan_c_dir
```
which will create a fargan_data.c and a fargan_data.h file in the fargan_c_dir directory.
Copy these files to the opus/dnn/ directory (replacing the existing ones) and recompile Opus.

## Inference

To run the inference, start by generating the features from the audio using:
```
./fargan_demo -features test_speech.pcm test_features.f32
```
Synthesis can be achieved either using the PyTorch code or the C code.
To synthesize from PyTorch, run:
```
python test_fargan.py output_dir/checkpoints/fargan_adv_50.pth test_features.f32 output_speech.pcm
```
To synthesize from the C code, run:
```
./fargan_demo -fargan-synthesis test_features.f32 output_speech.pcm
```
