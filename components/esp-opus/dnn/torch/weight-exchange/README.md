# weight-exchange



## Weight Exchange
Repo wor exchanging weights betweeen torch an tensorflow.keras modules, using an intermediate numpy format.

Routines for loading/dumping torch weights are located in exchange/torch and can be loaded with
```
import exchange.torch
```
and routines for loading/dumping tensorflow weights are located in exchange/tf and can be loaded with
```
import exchange.tf
```

Note that `exchange.torch` requires torch to be installed and `exchange.tf` requires tensorflow. To avoid the necessity of installing both torch and tensorflow in the working environment, none of these submodules is imported when calling `import exchange`. Similarly, the requirements listed in `requirements.txt` do include neither Tensorflow or Pytorch.


## C export
The module `exchange.c_export` contains routines to export weights to C files. On the long run it will be possible to call all `dump_...` functions with either a path string or a `CWriter` instance based on which the export format is chosen. This is currently only implemented for `torch.nn.GRU`, `torch.nn.Linear` and `torch.nn.Conv1d`.