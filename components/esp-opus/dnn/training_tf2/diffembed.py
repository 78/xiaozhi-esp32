"""
Modification of Tensorflow's Embedding Layer:
    1. Not restricted to be the first layer of a model
    2. Differentiable (allows non-integer lookups)
        - For non integer lookup, this layer linearly interpolates between the adjacent embeddings in the following way to preserver gradient flow
            - E = (1 - frac(x))*embed(floor(x)) + frac(x)*embed(ceil(x))
"""

import tensorflow as tf
from tensorflow.keras.layers import Layer

class diff_Embed(Layer):
    """
    Parameters:
        - units: int
            Dimension of the Embedding
        - dict_size: int
            Number of Embeddings to lookup
        - pcm_init: boolean
            Initialized for the embedding matrix
    """
    def __init__(self, units=128, dict_size = 256, pcm_init = True, initializer = None, **kwargs):
        super(diff_Embed, self).__init__(**kwargs)
        self.units = units
        self.dict_size = dict_size
        self.pcm_init = pcm_init
        self.initializer = initializer

    def build(self, input_shape):
        w_init = tf.random_normal_initializer()
        if self.pcm_init:
            w_init = self.initializer
        self.w = tf.Variable(initial_value=w_init(shape=(self.dict_size, self.units),dtype='float32'),trainable=True)

    def call(self, inputs):
        alpha = inputs - tf.math.floor(inputs)
        alpha = tf.expand_dims(alpha,axis = -1)
        alpha = tf.tile(alpha,[1,1,1,self.units])
        inputs = tf.cast(inputs,'int32')
        M = (1 - alpha)*tf.gather(self.w,inputs) + alpha*tf.gather(self.w,tf.clip_by_value(inputs + 1, 0, 255))
        return M

    def get_config(self):
        config = super(diff_Embed, self).get_config()
        config.update({"units": self.units})
        config.update({"dict_size" : self.dict_size})
        config.update({"pcm_init" : self.pcm_init})
        config.update({"initializer" : self.initializer})
        return config