# LPCNet

Low complexity implementation of the WaveRNN-based LPCNet algorithm, as described in:

- J.-M. Valin, J. Skoglund, [LPCNet: Improving Neural Speech Synthesis Through Linear Prediction](https://jmvalin.ca/papers/lpcnet_icassp2019.pdf), *Proc. International Conference on Acoustics, Speech and Signal Processing (ICASSP)*, arXiv:1810.11846, 2019.
- J.-M. Valin, U. Isik, P. Smaragdis, A. Krishnaswamy, [Neural Speech Synthesis on a Shoestring: Improving the Efficiency of LPCNet](https://jmvalin.ca/papers/improved_lpcnet.pdf), *Proc. ICASSP*, arxiv:2106.04129, 2022.
- K. Subramani, J.-M. Valin, U. Isik, P. Smaragdis, A. Krishnaswamy, [End-to-end LPCNet: A Neural Vocoder With Fully-Differentiable LPC Estimation](https://jmvalin.ca/papers/lpcnet_end2end.pdf), *Proc. INTERSPEECH*, arxiv:2106.04129, 2022.

For coding/PLC applications of LPCNet, see:

- J.-M. Valin, J. Skoglund, [A Real-Time Wideband Neural Vocoder at 1.6 kb/s Using LPCNet](https://jmvalin.ca/papers/lpcnet_codec.pdf), *Proc. INTERSPEECH*, arxiv:1903.12087, 2019.
- J. Skoglund, J.-M. Valin, [Improving Opus Low Bit Rate Quality with Neural Speech Synthesis](https://jmvalin.ca/papers/opusnet.pdf), *Proc. INTERSPEECH*, arxiv:1905.04628, 2020.
- J.-M. Valin, A. Mustafa, C. Montgomery, T.B. Terriberry, M. Klingbeil, P. Smaragdis, A. Krishnaswamy, [Real-Time Packet Loss Concealment With Mixed Generative and Predictive Model](https://jmvalin.ca/papers/lpcnet_plc.pdf), *Proc. INTERSPEECH*, arxiv:2205.05785, 2022.
- J.-M. Valin, J. Büthe, A. Mustafa, [Low-Bitrate Redundancy Coding of Speech Using a Rate-Distortion-Optimized Variational Autoencoder](https://jmvalin.ca/papers/valin_dred.pdf), *Proc. ICASSP*, arXiv:2212.04453, 2023. ([blog post](https://www.amazon.science/blog/neural-encoding-enables-more-efficient-recovery-of-lost-audio-packets))

# Introduction

Work in progress software for researching low CPU complexity algorithms for speech synthesis and compression by applying Linear Prediction techniques to WaveRNN. High quality speech can be synthesised on regular CPUs (around 3 GFLOP) with SIMD support (SSE2, SSSE3, AVX, AVX2/FMA, NEON currently supported). The code also supports very low bitrate compression at 1.6 kb/s.

The BSD licensed software is written in C and Python/Keras. For training, a GTX 1080 Ti or better is recommended.

This software is an open source starting point for LPCNet/WaveRNN-based speech synthesis and coding.

# Using the existing software

You can build the code using:

```
./autogen.sh
./configure
make
```
Note that the autogen.sh script is used when building from Git and will automatically download the latest model
(models are too large to put in Git). By default, LPCNet will attempt to use 8-bit dot product instructions on AVX\*/Neon to
speed up inference. To disable that (e.g. to avoid quantization effects when retraining), add --disable-dot-product to the
configure script. LPCNet does not yet have a complete implementation for some of the integer operations on the ARMv7
architecture so for now you will also need --disable-dot-product to successfully compile on 32-bit ARM.

It is highly recommended to set the CFLAGS environment variable to enable AVX or NEON *prior* to running configure, otherwise
no vectorization will take place and the code will be very slow. On a recent x86 CPU, something like
```
export CFLAGS='-Ofast -g -march=native'
```
should work. On ARM, you can enable Neon with:
```
export CFLAGS='-Ofast -g -mfpu=neon'
```
While not strictly required, the -Ofast flag will help with auto-vectorization, especially for dot products that
cannot be optimized without -ffast-math (which -Ofast enables). Additionally, -falign-loops=32 has been shown to
help on x86.

You can test the capabilities of LPCNet using the lpcnet\_demo application. To encode a file:
```
./lpcnet_demo -encode input.pcm compressed.bin
```
where input.pcm is a 16-bit (machine endian) PCM file sampled at 16 kHz. The raw compressed data (no header)
is written to compressed.bin and consists of 8 bytes per 40-ms packet.

To decode:
```
./lpcnet_demo -decode compressed.bin output.pcm
```
where output.pcm is also 16-bit, 16 kHz PCM.

Alternatively, you can run the uncompressed analysis/synthesis using -features
instead of -encode and -synthesis instead of -decode.
The same functionality is available in the form of a library. See include/lpcnet.h for the API.

To try packet loss concealment (PLC), you first need a PLC model, which you can get with:
```
./download_model.sh plc-3b1eab4
```
or (for the PLC challenge submission):
```
./download_model.sh plc_challenge
```
PLC can be tested with:
```
./lpcnet_demo -plc_file noncausal_dc error_pattern.txt input.pcm output.pcm
```
where error_pattern.txt is a text file with one entry per 20-ms packet, with 1 meaning "packet lost" and 0 meaning "packet not lost".
noncausal_dc is the non-causal (5-ms look-ahead) with special handling for DC offsets. It's also possible to use "noncausal", "causal",
or "causal_dc".

# Training a new model

This codebase is also meant for research and it is possible to train new models. These are the steps to do that:

1. Set up a Keras system with GPU.

1. Generate training data:
   ```
   ./dump_data -train input.s16 features.f32 data.s16
   ```
   where the first file contains 16 kHz 16-bit raw PCM audio (no header) and the other files are output files. This program makes several passes over the data with different filters to generate a large amount of training data.

1. Now that you have your files, train with:
   ```
   python3 training_tf2/train_lpcnet.py features.f32 data.s16 model_name
   ```
   and it will generate an h5 file for each iteration, with model\_name as prefix. If it stops with a
   "Failed to allocate RNN reserve space" message try specifying a smaller --batch-size for  train\_lpcnet.py.

1. You can synthesise speech with Python and your GPU card (very slow):
   ```
   ./dump_data -test test_input.s16 test_features.f32
   ./training_tf2/test_lpcnet.py lpcnet_model_name.h5 test_features.f32 test.s16
   ```

1. Or with C on a CPU (C inference is much faster):
   First extract the model files nnet\_data.h and nnet\_data.c
   ```
   ./training_tf2/dump_lpcnet.py lpcnet_model_name.h5
   ```
   and move the generated nnet\_data.\* files to the src/ directory.
   Then you just need to rebuild the software and use lpcnet\_demo as explained above.

# Speech Material for Training

Suitable training material can be obtained from [Open Speech and Language Resources](https://www.openslr.org/).  See the datasets.txt file for details on suitable training data.

# Reading Further

1. [LPCNet: DSP-Boosted Neural Speech Synthesis](https://people.xiph.org/~jm/demo/lpcnet/)
1. [A Real-Time Wideband Neural Vocoder at 1.6 kb/s Using LPCNet](https://people.xiph.org/~jm/demo/lpcnet_codec/)
1. Sample model files (check compatibility): https://media.xiph.org/lpcnet/data/
