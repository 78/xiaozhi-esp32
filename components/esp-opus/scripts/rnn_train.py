#!/usr/bin/python

from __future__ import print_function

from keras.models import Sequential
from keras.models import Model
from keras.layers import Input
from keras.layers import Dense
from keras.layers import LSTM
from keras.layers import GRU
from keras.layers import SimpleRNN
from keras.layers import Dropout
from keras import losses
import h5py

from keras import backend as K
import numpy as np

def binary_crossentrop2(y_true, y_pred):
    return K.mean(2*K.abs(y_true-0.5) * K.binary_crossentropy(y_pred, y_true), axis=-1)

print('Build model...')
#model = Sequential()
#model.add(Dense(16, activation='tanh', input_shape=(None, 25)))
#model.add(GRU(12, dropout=0.0, recurrent_dropout=0.0, activation='tanh', recurrent_activation='sigmoid', return_sequences=True))
#model.add(Dense(2, activation='sigmoid'))

main_input = Input(shape=(None, 25), name='main_input')
x = Dense(16, activation='tanh')(main_input)
x = GRU(12, dropout=0.1, recurrent_dropout=0.1, activation='tanh', recurrent_activation='sigmoid', return_sequences=True)(x)
x = Dense(2, activation='sigmoid')(x)
model = Model(inputs=main_input, outputs=x)

batch_size = 64

print('Loading data...')
with h5py.File('features.h5', 'r') as hf:
    all_data = hf['features'][:]
print('done.')

window_size = 1500

nb_sequences = len(all_data)/window_size
print(nb_sequences, ' sequences')
x_train = all_data[:nb_sequences*window_size, :-2]
x_train = np.reshape(x_train, (nb_sequences, window_size, 25))

y_train = np.copy(all_data[:nb_sequences*window_size, -2:])
y_train = np.reshape(y_train, (nb_sequences, window_size, 2))

all_data = 0;
x_train = x_train.astype('float32')
y_train = y_train.astype('float32')

print(len(x_train), 'train sequences. x shape =', x_train.shape, 'y shape = ', y_train.shape)

# try using different optimizers and different optimizer configs
model.compile(loss=binary_crossentrop2,
              optimizer='adam',
              metrics=['binary_accuracy'])

print('Train...')
model.fit(x_train, y_train,
          batch_size=batch_size,
          epochs=200,
          validation_data=(x_train, y_train))
model.save("newweights.hdf5")
