## Neural Pitch Estimation

- Dataset Installation
    1. Download and unzip PTDB Dataset:
        wget https://www2.spsc.tugraz.at/databases/PTDB-TUG/SPEECH_DATA_ZIPPED.zip
        unzip SPEECH_DATA_ZIPPED.zip

    2. Inside "SPEECH DATA" above, run ptdb_process.sh to combine male/female

    3. To Download and combine demand, simply run download_demand.sh

- LPCNet preparation
    1. To extract xcorr, add lpcnet_extractor.c and add relevant functions to lpcnet_enc.c, add source for headers/c files and Makefile.am, and compile to generate ./lpcnet_xcorr_extractor object

- Dataset Augmentation and training (check out arguments to each of the following)
    1. Run data_augmentation.py
    2. Run training.py using augmented data
    3. Run experiments.py
