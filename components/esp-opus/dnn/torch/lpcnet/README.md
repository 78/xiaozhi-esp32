# LPCNet

Incomplete pytorch implementation of LPCNet

## Data preparation
For data preparation use dump_data in github.com/xiph/LPCNet. To turn this into
a training dataset, copy data and feature file to a folder and run

python add_dataset_config.py my_dataset_folder


## Training
To train a model, create and adjust a setup file, e.g. with

python make_default_setup.py my_setup.yml --path2dataset my_dataset_folder

Then simply run

python train_lpcnet.py my_setup.yml my_output

## Inference
Create feature file with dump_data from github.com/xiph/LPCNet. Then run e.g.

python test_lpcnet.py features.f32 my_output/checkpoints/checkpoint_ep_10.pth out.wav

Inference runs on CPU and takes usually between 3 and 20 seconds per generated second of audio,
depending on the CPU.
