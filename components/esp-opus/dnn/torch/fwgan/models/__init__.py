from .fwgan400 import FWGAN400ContLarge
from .fwgan500 import FWGAN500Cont

model_dict = {
    'fwgan400': FWGAN400ContLarge,
    'fwgan500': FWGAN500Cont
}