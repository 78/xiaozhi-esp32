"""
Custom Loss functions and metrics for training/analysis
"""

from tf_funcs import *
import tensorflow as tf

# The following loss functions all expect the lpcnet model to output the lpc prediction

# Computing the excitation by subtracting the lpc prediction from the target, followed by minimizing the cross entropy
def res_from_sigloss():
    def loss(y_true,y_pred):
        p = y_pred[:,:,0:1]
        model_out = y_pred[:,:,2:]
        e_gt = tf_l2u(y_true - p)
        e_gt = tf.round(e_gt)
        e_gt = tf.cast(e_gt,'int32')
        sparse_cel = tf.keras.losses.SparseCategoricalCrossentropy(reduction=tf.keras.losses.Reduction.NONE)(e_gt,model_out)
        return sparse_cel
    return loss

# Interpolated and Compensated Loss (In case of end to end lpcnet)
# Interpolates between adjacent embeddings based on the fractional value of the excitation computed (similar to the embedding interpolation)
# Also adds a probability compensation (to account for matching cross entropy in the linear domain), weighted by gamma
def interp_mulaw(gamma = 1):
    def loss(y_true,y_pred):
        y_true = tf.cast(y_true, 'float32')
        p = y_pred[:,:,0:1]
        real_p = y_pred[:,:,1:2]
        model_out = y_pred[:,:,2:]
        e_gt = tf_l2u(y_true - p)
        exc_gt = tf_l2u(y_true - real_p)
        prob_compensation = tf.squeeze((K.abs(e_gt - 128)/128.0)*K.log(256.0))
        regularization = tf.squeeze((K.abs(exc_gt - 128)/128.0)*K.log(256.0))
        alpha = e_gt - tf.math.floor(e_gt)
        alpha = tf.tile(alpha,[1,1,256])
        e_gt = tf.cast(e_gt,'int32')
        e_gt = tf.clip_by_value(e_gt,0,254)
        interp_probab = (1 - alpha)*model_out + alpha*tf.roll(model_out,shift = -1,axis = -1)
        sparse_cel = tf.keras.losses.SparseCategoricalCrossentropy(reduction=tf.keras.losses.Reduction.NONE)(e_gt,interp_probab)
        loss_mod = sparse_cel + prob_compensation + gamma*regularization
        return loss_mod
    return loss

# Same as above, except a metric
def metric_oginterploss(y_true,y_pred):
    p = y_pred[:,:,0:1]
    model_out = y_pred[:,:,2:]
    e_gt = tf_l2u(y_true - p)
    prob_compensation = tf.squeeze((K.abs(e_gt - 128)/128.0)*K.log(256.0))
    alpha = e_gt - tf.math.floor(e_gt)
    alpha = tf.tile(alpha,[1,1,256])
    e_gt = tf.cast(e_gt,'int32')
    e_gt = tf.clip_by_value(e_gt,0,254)
    interp_probab = (1 - alpha)*model_out + alpha*tf.roll(model_out,shift = -1,axis = -1)
    sparse_cel = tf.keras.losses.SparseCategoricalCrossentropy(reduction=tf.keras.losses.Reduction.NONE)(e_gt,interp_probab)
    loss_mod = sparse_cel + prob_compensation
    return loss_mod

# Interpolated cross entropy loss metric
def metric_icel(y_true, y_pred):
    p = y_pred[:,:,0:1]
    model_out = y_pred[:,:,2:]
    e_gt = tf_l2u(y_true - p)
    alpha = e_gt - tf.math.floor(e_gt)
    alpha = tf.tile(alpha,[1,1,256])
    e_gt = tf.cast(e_gt,'int32')
    e_gt = tf.clip_by_value(e_gt,0,254) #Check direction
    interp_probab = (1 - alpha)*model_out + alpha*tf.roll(model_out,shift = -1,axis = -1)
    sparse_cel = tf.keras.losses.SparseCategoricalCrossentropy(reduction=tf.keras.losses.Reduction.NONE)(e_gt,interp_probab)
    return sparse_cel

# Non-interpolated (rounded) cross entropy loss metric
def metric_cel(y_true, y_pred):
    y_true = tf.cast(y_true, 'float32')
    p = y_pred[:,:,0:1]
    model_out = y_pred[:,:,2:]
    e_gt = tf_l2u(y_true - p)
    e_gt = tf.round(e_gt)
    e_gt = tf.cast(e_gt,'int32')
    e_gt = tf.clip_by_value(e_gt,0,255)
    sparse_cel = tf.keras.losses.SparseCategoricalCrossentropy(reduction=tf.keras.losses.Reduction.NONE)(e_gt,model_out)
    return sparse_cel

# Variance metric of the output excitation
def metric_exc_sd(y_true,y_pred):
    p = y_pred[:,:,0:1]
    e_gt = tf_l2u(y_true - p)
    sd_egt = tf.keras.losses.MeanSquaredError(reduction=tf.keras.losses.Reduction.NONE)(e_gt,128)
    return sd_egt

def loss_matchlar():
    def loss(y_true,y_pred):
        model_rc = y_pred[:,:,:16]
        #y_true = lpc2rc(y_true)
        loss_lar_diff = K.log((1.01 + model_rc)/(1.01 - model_rc)) - K.log((1.01 + y_true)/(1.01 - y_true))
        loss_lar_diff = tf.square(loss_lar_diff)
        return tf.reduce_mean(loss_lar_diff, axis=-1)
    return loss
