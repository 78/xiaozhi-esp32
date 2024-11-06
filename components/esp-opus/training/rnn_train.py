#!/usr/bin/python3

from __future__ import print_function

from keras.models import Sequential
from keras.models import Model
from keras.layers import Input
from keras.layers import Dense
from keras.layers import LSTM
from keras.layers import GRU
from keras.layers import CuDNNGRU
from keras.layers import SimpleRNN
from keras.layers import Dropout
from keras import losses
import h5py
from keras.optimizers import Adam

from keras.constraints import Constraint
from keras import backend as K
import numpy as np

import tensorflow as tf
from keras.backend.tensorflow_backend import set_session
config = tf.ConfigProto()
config.gpu_options.per_process_gpu_memory_fraction = 0.44
set_session(tf.Session(config=config))

def binary_crossentrop2(y_true, y_pred):
    return K.mean(2*K.abs(y_true-0.5) * K.binary_crossentropy(y_true, y_pred), axis=-1)

def binary_accuracy2(y_true, y_pred):
    return K.mean(K.cast(K.equal(y_true, K.round(y_pred)), 'float32') + K.cast(K.equal(y_true, 0.5), 'float32'), axis=-1)

def quant_model(model):
    weights = model.get_weights()
    for k in range(len(weights)):
        weights[k] = np.maximum(-128, np.minimum(127, np.round(128*weights[k])*0.0078125))
    model.set_weights(weights)

class WeightClip(Constraint):
    '''Clips the weights incident to each hidden unit to be inside a range
    '''
    def __init__(self, c=2):
        self.c = c

    def __call__(self, p):
        return K.clip(p, -self.c, self.c)

    def get_config(self):
        return {'name': self.__class__.__name__,
            'c': self.c}

reg = 0.000001
constraint = WeightClip(.998)

print('Build model...')

main_input = Input(shape=(None, 25), name='main_input')
x = Dense(32, activation='tanh', kernel_constraint=constraint, bias_constraint=constraint)(main_input)
#x = CuDNNGRU(24, return_sequences=True, kernel_constraint=constraint, recurrent_constraint=constraint, bias_constraint=constraint)(x)
x = GRU(24, recurrent_activation='sigmoid', activation='tanh', return_sequences=True, kernel_constraint=constraint, recurrent_constraint=constraint, bias_constraint=constraint)(x)
x = Dense(2, activation='sigmoid', kernel_constraint=constraint, bias_constraint=constraint)(x)
model = Model(inputs=main_input, outputs=x)

batch_size = 2048

print('Loading data...')
with h5py.File('features10b.h5', 'r') as hf:
    all_data = hf['data'][:]
print('done.')

window_size = 1500

nb_sequences = len(all_data)//window_size
print(nb_sequences, ' sequences')
x_train = all_data[:nb_sequences*window_size, :-2]
x_train = np.reshape(x_train, (nb_sequences, window_size, 25))

y_train = np.copy(all_data[:nb_sequences*window_size, -2:])
y_train = np.reshape(y_train, (nb_sequences, window_size, 2))

print("Marking ignores")
for s in y_train:
    for e in s:
        if (e[1] >= 1):
            break
        e[0] = 0.5

all_data = 0;
x_train = x_train.astype('float32')
y_train = y_train.astype('float32')

print(len(x_train), 'train sequences. x shape =', x_train.shape, 'y shape = ', y_train.shape)

model.load_weights('newweights10a1b_ep206.hdf5')

#weights = model.get_weights()
#for k in range(len(weights)):
#    weights[k] = np.round(128*weights[k])*0.0078125
#model.set_weights(weights)

# try using different optimizers and different optimizer configs
model.compile(loss=binary_crossentrop2,
              optimizer=Adam(0.0001),
              metrics=[binary_accuracy2])

print('Train...')
quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=10, validation_data=(x_train, y_train))
model.save("newweights10a1c_ep10.hdf5")

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=50, initial_epoch=10)
model.save("newweights10a1c_ep50.hdf5")

model.compile(loss=binary_crossentrop2,
              optimizer=Adam(0.0001),
              metrics=[binary_accuracy2])

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=100, initial_epoch=50)
model.save("newweights10a1c_ep100.hdf5")

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=150, initial_epoch=100)
model.save("newweights10a1c_ep150.hdf5")

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=200, initial_epoch=150)
model.save("newweights10a1c_ep200.hdf5")

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=201, initial_epoch=200)
model.save("newweights10a1c_ep201.hdf5")

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=202, initial_epoch=201, validation_data=(x_train, y_train))
model.save("newweights10a1c_ep202.hdf5")

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=203, initial_epoch=202, validation_data=(x_train, y_train))
model.save("newweights10a1c_ep203.hdf5")

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=204, initial_epoch=203, validation_data=(x_train, y_train))
model.save("newweights10a1c_ep204.hdf5")

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=205, initial_epoch=204, validation_data=(x_train, y_train))
model.save("newweights10a1c_ep205.hdf5")

quant_model(model)
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=206, initial_epoch=205, validation_data=(x_train, y_train))
model.save("newweights10a1c_ep206.hdf5")

