"""
Tensorflow/Keras helper functions to do the following:
    1. \mu law <-> Linear domain conversion
    2. Differentiable prediction from the input signal and LP coefficients
    3. Differentiable transformations Reflection Coefficients (RCs) <-> LP Coefficients
"""
from tensorflow.keras.layers import Lambda, Multiply, Layer, Concatenate
from tensorflow.keras import backend as K
import tensorflow as tf

# \mu law <-> Linear conversion functions
scale = 255.0/32768.0
scale_1 = 32768.0/255.0
def tf_l2u(x):
    s = K.sign(x)
    x = K.abs(x)
    u = (s*(128*K.log(1+scale*x)/K.log(256.0)))
    u = K.clip(128 + u, 0, 255)
    return u

def tf_u2l(u):
    u = tf.cast(u,"float32")
    u = u - 128.0
    s = K.sign(u)
    u = K.abs(u)
    return s*scale_1*(K.exp(u/128.*K.log(256.0))-1)

# Differentiable Prediction Layer
# Computes the LP prediction from the input lag signal and the LP coefficients
# The inputs xt and lpc conform with the shapes in lpcnet.py (the '2400' is coded keeping this in mind)
class diff_pred(Layer):
    def call(self, inputs, lpcoeffs_N = 16, frame_size = 160):
        xt = inputs[0]
        lpc = inputs[1]

        rept = Lambda(lambda x: K.repeat_elements(x , frame_size, 1))
        zpX = Lambda(lambda x: K.concatenate([0*x[:,0:lpcoeffs_N,:], x],axis = 1))
        cX = Lambda(lambda x: K.concatenate([x[:,(lpcoeffs_N - i):(lpcoeffs_N - i + 2400),:] for i in range(lpcoeffs_N)],axis = 2))

        pred = -Multiply()([rept(lpc),cX(zpX(xt))])

        return K.sum(pred,axis = 2,keepdims = True)

# Differentiable Transformations (RC <-> LPC) computed using the Levinson Durbin Recursion
class diff_rc2lpc(Layer):
    def call(self, inputs, lpcoeffs_N = 16):
        def pred_lpc_recursive(input):
            temp = (input[0] + K.repeat_elements(input[1],input[0].shape[2],2)*K.reverse(input[0],axes = 2))
            temp = Concatenate(axis = 2)([temp,input[1]])
            return temp
        Llpc = Lambda(pred_lpc_recursive)
        inputs = inputs[:,:,:lpcoeffs_N]
        lpc_init = inputs
        for i in range(1,lpcoeffs_N):
            lpc_init = Llpc([lpc_init[:,:,:i],K.expand_dims(inputs[:,:,i],axis = -1)])
        return lpc_init

class diff_lpc2rc(Layer):
    def call(self, inputs, lpcoeffs_N = 16):
        def pred_rc_recursive(input):
            ki = K.repeat_elements(K.expand_dims(input[1][:,:,0],axis = -1),input[0].shape[2],2)
            temp = (input[0] - ki*K.reverse(input[0],axes = 2))/(1 - ki*ki)
            temp = Concatenate(axis = 2)([temp,input[1]])
            return temp
        Lrc = Lambda(pred_rc_recursive)
        rc_init = inputs
        for i in range(1,lpcoeffs_N):
            j = (lpcoeffs_N - i + 1)
            rc_init = Lrc([rc_init[:,:,:(j - 1)],rc_init[:,:,(j - 1):]])
        return rc_init
