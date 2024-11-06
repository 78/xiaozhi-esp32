# lpcnet-testsuite

## setup
The test script is written for Linux only. It requires sox to be installed and available.

Setup is done as usual via

```
pip install -r requirements.txt
```

The test scrip run_warpq_test.py requires a setup file in yaml format, which specifies how
to generate a wave file OUTPUT from a wave file INPUT sampled resampled to the specified
sampling rate as a list of shell commands. This makes it easy to test other neural vocoders
with it as well. Two examples are given in examples. INPUT and OUTPUT will be replaced by using
the string.format(INPUT=input,OUTPUT=output) method.

Here is one example:

```
test: "LPCNet reference test"
processing:
  - "sox {INPUT} {INPUT}.raw"
  - "/local/code/LPCNet/lpcnet_demo -features {INPUT}.raw {INPUT}.features.f32"
  - "/local/code/LPCNet/lpcnet_demo -synthesis {INPUT}.features.f32 {INPUT}.decoded.raw"
  - "sox -r 16000 -L -e signed-integer -b 16 -c 1 {INPUT}.decoded.raw {OUTPUT}"
```

The structure of the output folder is as follows:

```
output_folder
+-- html
    +-- index.html
    +-- items
+-- processing
+-- setup.yml
+-- stats.txt
+-- scores.txt
```

scores.txt contains the WARP-Q scores in descending order (best to worse)
stats.txt contains mean values over all, the 10 best and the 10 worst items
setup.yml contains all information to repeat the run
htms contains a self-contained website displaying the 10 best and 10 worst items
processing contains processing output