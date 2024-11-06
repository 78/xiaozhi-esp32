# Opus Speech Coding Enhancement

This folder hosts models for enhancing Opus SILK.

## Environment setup
The code is tested with python 3.11. Conda setup is done via


`conda create -n osce python=3.11`

`conda activate osce`

`python -m pip install -r requirements.txt`


## Generating training data
First step is to convert all training items to 16 kHz and 16 bit pcm and then concatenate them. A convenient way to do this is to create a file list and then run

`python scripts/concatenator.py filelist 16000 dataset/clean.s16 --db_min -40 --db_max 0`

which on top provides some random scaling. Data is taken from the datasets listed in dnn/datasets.txt and the exact list of items used for training and validation is
located in dnn/torch/osce/resources.

Second step is to run a patched version of opus_demo in the dataset folder, which will produce the coded output and add feature files. To build the patched opus_demo binary, check out the exp-neural-silk-enhancement branch and build opus_demo the usual way. Then run

`cd dataset && <path_to_patched_opus_demo>/opus_demo voip 16000 1 9000 -silk_random_switching 249 clean.s16 coded.s16 `

The argument to -silk_random_switching specifies the number of frames after which parameters are switched randomly.

## Regression loss based training
Create a default setup for LACE or NoLACE via

`python make_default_setup.py model.yml --model lace/nolace --path2dataset <path2dataset>`

Then run

`python train_model.py model.yml <output folder> --no-redirect`

for running the training script in foreground or

`nohup python train_model.py model.yml <output folder> &`

to run it in background. In the latter case the output is written to `<output folder>/out.txt`.

## Adversarial training (NoLACE only)
Create a default setup for NoLACE via

`python make_default_setup.py nolace_adv.yml --model nolace --adversarial --path2dataset <path2dataset>`

Then run

`python adv_train_model.py nolace_adv.yml <output folder> --no-redirect`

for running the training script in foreground or

`nohup python adv_train_model.py nolace_adv.yml <output folder> &`

to run it in background. In the latter case the output is written to `<output folder>/out.txt`.

## Inference
Generating inference data is analogous to generating training data. Given an item 'item1.wav' run
`mkdir item1.se && sox item1.wav -r 16000 -e signed-integer -b 16 item1.raw && cd item1.se && <path_to_patched_opus_demo>/opus_demo voip 16000 1 <bitrate> ../item1.raw noisy.s16`

The folder item1.se then serves as input for the test_model.py script or for the --testdata argument of train_model.py resp. adv_train_model.py

autogen.sh downloads pre-trained model weights to the subfolder dnn/models of the main repo.