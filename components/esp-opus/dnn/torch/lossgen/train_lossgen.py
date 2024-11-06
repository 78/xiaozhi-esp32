import numpy as np
import torch
from torch import nn
import torch.nn.functional as F
import tqdm
from scipy.signal import lfilter
import os
import lossgen

class LossDataset(torch.utils.data.Dataset):
    def __init__(self,
                loss_file,
                sequence_length=997):

        self.sequence_length = sequence_length

        self.loss = np.loadtxt(loss_file, dtype='float32')

        self.nb_sequences = self.loss.shape[0]//self.sequence_length
        self.loss = self.loss[:self.nb_sequences*self.sequence_length]
        self.perc = lfilter(np.array([.001], dtype='float32'), np.array([1., -.999], dtype='float32'), self.loss)

        self.loss = np.reshape(self.loss, (self.nb_sequences, self.sequence_length, 1))
        self.perc = np.reshape(self.perc, (self.nb_sequences, self.sequence_length, 1))

    def __len__(self):
        return self.nb_sequences

    def __getitem__(self, index):
        r0 = np.random.normal(scale=.1, size=(1,1)).astype('float32')
        r1 = np.random.normal(scale=.1, size=(self.sequence_length,1)).astype('float32')
        perc = self.perc[index, :, :]
        perc = perc + (r0+r1)*perc*(1-perc)
        return [self.loss[index, :, :], perc]


adam_betas = [0.8, 0.98]
adam_eps = 1e-8
batch_size=256
lr_decay = 0.001
lr = 0.003
epsilon = 1e-5
epochs = 2000
checkpoint_dir='checkpoint'
os.makedirs(checkpoint_dir, exist_ok=True)
checkpoint = dict()

device = torch.device("cuda") if torch.cuda.is_available() else torch.device("cpu")

checkpoint['model_args']    = ()
checkpoint['model_kwargs']  = {'gru1_size': 16, 'gru2_size': 32}
model = lossgen.LossGen(*checkpoint['model_args'], **checkpoint['model_kwargs'])
dataset = LossDataset('loss_sorted.txt')
dataloader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=True, drop_last=True, num_workers=4)


optimizer = torch.optim.AdamW(model.parameters(), lr=lr, betas=adam_betas, eps=adam_eps)


# learning rate scheduler
scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer=optimizer, lr_lambda=lambda x : 1 / (1 + lr_decay * x))


if __name__ == '__main__':
    model.to(device)
    states = None
    for epoch in range(1, epochs + 1):

        running_loss = 0

        print(f"training epoch {epoch}...")
        with tqdm.tqdm(dataloader, unit='batch') as tepoch:
            for i, (loss, perc) in enumerate(tepoch):
                optimizer.zero_grad()
                loss = loss.to(device)
                perc = perc.to(device)

                out, states = model(loss, perc, states=states)
                states = [state.detach() for state in states]
                out = torch.sigmoid(out[:,:-1,:])
                target = loss[:,1:,:]

                loss = torch.mean(-target*torch.log(out+epsilon) - (1-target)*torch.log(1-out+epsilon))

                loss.backward()
                optimizer.step()

                scheduler.step()

                running_loss += loss.detach().cpu().item()
                tepoch.set_postfix(loss=f"{running_loss/(i+1):8.5f}",
                                   )

        # save checkpoint
        checkpoint_path = os.path.join(checkpoint_dir, f'lossgen_{epoch}.pth')
        checkpoint['state_dict'] = model.state_dict()
        checkpoint['loss'] = running_loss / len(dataloader)
        checkpoint['epoch'] = epoch
        torch.save(checkpoint, checkpoint_path)
