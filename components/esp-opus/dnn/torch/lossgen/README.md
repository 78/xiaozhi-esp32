#Packet loss simulator

This code is an attempt at simulating better packet loss scenarios. The most common way of simulating
packet loss is to use a random sequence where each packet loss event is uncorrelated with previous events.
That is a simplistic model since we know that losses often occur in bursts. This model uses real data
to build a generative model for packet loss.

We use the training data provided for the Audio Deep Packet Loss Concealment Challenge, which is available at:

http://plcchallenge2022pub.blob.core.windows.net/plcchallengearchive/test_train.tar.gz

To create the training data, run:

`./process_data.sh /<path>/test_train/train/lossy_signals/`

That will create an ascii loss\_sorted.txt file with all loss data sorted in increasing packet loss
percentage. Then just run:

`python ./train_lossgen.py`

to train a model

To generate a sequence, run

`python3 ./test_lossgen.py <checkpoint> <percentage> output.txt --length 10000`

where <checkpoint> is the .pth model file and <percentage> is the amount of loss (e.g. 0.2 for 20% loss).
