# Optimizing a rational function to optimize a tanh() approximation

import numpy as np
import tensorflow as tf
from tensorflow.keras.models import Model
from tensorflow.keras.layers import Input, GRU, Dense, Embedding, Reshape, Concatenate, Lambda, Conv1D, Multiply, Add, Bidirectional, MaxPooling1D, Activation
import tensorflow.keras.backend as K
from tensorflow.keras.optimizers import Adam, SGD

def my_loss1(y_true, y_pred):
    return 1*K.mean(K.square(y_true-y_pred)) + 1*K.max(K.square(y_true-y_pred), axis=1)

def my_loss2(y_true, y_pred):
    return .1*K.mean(K.square(y_true-y_pred)) + 1*K.max(K.square(y_true-y_pred), axis=1)

def my_loss3(y_true, y_pred):
    return .01*K.mean(K.square(y_true-y_pred)) + 1*K.max(K.square(y_true-y_pred), axis=1)

# Using these initializers to seed the approximation
# with a reasonable starting point
def num_init(shape, dtype=None):
    rr = tf.constant([[945], [105], [1]], dtype=dtype)
    #rr = tf.constant([[946.56757], [98.01368], [0.66841]], dtype=dtype)
    print(rr)
    return rr

def den_init(shape, dtype=None):
    rr = tf.constant([[945], [420], [15]], dtype=dtype)
    #rr = tf.constant([[946.604], [413.342], [12.465]], dtype=dtype)
    print(rr)
    return rr


x = np.arange(-10, 10, .01)
N = len(x)
x = np.reshape(x, (1, -1, 1))
x2 = x*x

x2in = np.concatenate([x2*0 + 1, x2, x2*x2], axis=2)
yout = np.tanh(x)


model_x = Input(shape=(None, 1,))
model_x2 = Input(shape=(None, 3,))

num = Dense(1, name='num', use_bias=False, kernel_initializer=num_init)
den = Dense(1, name='den', use_bias=False, kernel_initializer=den_init)

def ratio(x):
    return tf.minimum(1., tf.maximum(-1., x[0]*x[1]/x[2]))

out_layer = Lambda(ratio)
output = out_layer([model_x, num(model_x2), den(model_x2)])

model = Model([model_x, model_x2], output)
model.summary()

model.compile(Adam(0.05, beta_1=0.9, beta_2=0.9, decay=2e-5), loss='mean_squared_error')
model.fit([x, x2in], yout, batch_size=1, epochs=500000, validation_split=0.0)

model.compile(Adam(0.001, beta_2=0.9, decay=1e-4), loss=my_loss1)
model.fit([x, x2in], yout, batch_size=1, epochs=50000, validation_split=0.0)

model.compile(Adam(0.0001, beta_2=0.9, decay=1e-4), loss=my_loss2)
model.fit([x, x2in], yout, batch_size=1, epochs=50000, validation_split=0.0)

model.compile(Adam(0.00001, beta_2=0.9, decay=1e-4), loss=my_loss3)
model.fit([x, x2in], yout, batch_size=1, epochs=50000, validation_split=0.0)

model.save_weights('tanh.h5')
