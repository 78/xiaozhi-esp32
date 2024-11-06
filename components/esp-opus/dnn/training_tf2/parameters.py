""" module for handling extra model parameters for tf.keras models """

import tensorflow as tf


def set_parameter(model, parameter_name, parameter_value, dtype='float32'):
    """ stores parameter_value as non-trainable weight with name parameter_name:0 """

    weights = [weight for weight in model.weights if weight.name == (parameter_name + ":0")]

    if len(weights) == 0:
        model.add_weight(parameter_name, trainable=False, initializer=tf.keras.initializers.Constant(parameter_value), dtype=dtype)
    elif len(weights) == 1:
        weights[0].assign(parameter_value)
    else:
        raise ValueError(f"more than one weight starting with {parameter_name}:0 in model")


def get_parameter(model, parameter_name, default=None):
    """ returns parameter value if parameter is present in model and otherwise default """

    weights = [weight for weight in model.weights if weight.name == (parameter_name + ":0")]

    if len(weights) == 0:
        return default
    elif len(weights) > 1:
        raise ValueError(f"more than one weight starting with {parameter_name}:0 in model")
    else:
        return weights[0].numpy().item()
