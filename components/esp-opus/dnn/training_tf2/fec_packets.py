"""
/* Copyright (c) 2022 Amazon
   Written by Jan Buethe */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
"""

import numpy as np



def write_fec_packets(filename, packets, rates=None):
    """ writes packets in binary format """

    assert np.dtype(np.float32).itemsize == 4
    assert np.dtype(np.int16).itemsize == 2

    # derive some sizes
    num_packets             = len(packets)
    subframes_per_packet    = packets[0].shape[-2]
    num_features            = packets[0].shape[-1]

    # size of float is 4
    subframe_size           = num_features * 4
    packet_size             = subframe_size * subframes_per_packet + 2 # two bytes for rate

    version = 1
    # header size (version, header_size, num_packets, packet_size, subframe_size, subrames_per_packet, num_features)
    header_size = 14

    with open(filename, 'wb') as f:

        # header
        f.write(np.int16(version).tobytes())
        f.write(np.int16(header_size).tobytes())
        f.write(np.int16(num_packets).tobytes())
        f.write(np.int16(packet_size).tobytes())
        f.write(np.int16(subframe_size).tobytes())
        f.write(np.int16(subframes_per_packet).tobytes())
        f.write(np.int16(num_features).tobytes())

        # packets
        for i, packet in enumerate(packets):
            if type(rates) == type(None):
                rate = 0
            else:
                rate = rates[i]

            f.write(np.int16(rate).tobytes())

            features = np.flip(packet, axis=-2)
            f.write(features.astype(np.float32).tobytes())


def read_fec_packets(filename):
    """ reads packets from binary format """

    assert np.dtype(np.float32).itemsize == 4
    assert np.dtype(np.int16).itemsize == 2

    with open(filename, 'rb') as f:

        # header
        version                 = np.frombuffer(f.read(2), dtype=np.int16).item()
        header_size             = np.frombuffer(f.read(2), dtype=np.int16).item()
        num_packets             = np.frombuffer(f.read(2), dtype=np.int16).item()
        packet_size             = np.frombuffer(f.read(2), dtype=np.int16).item()
        subframe_size           = np.frombuffer(f.read(2), dtype=np.int16).item()
        subframes_per_packet    = np.frombuffer(f.read(2), dtype=np.int16).item()
        num_features            = np.frombuffer(f.read(2), dtype=np.int16).item()

        dummy_features          = np.zeros((1, subframes_per_packet, num_features), dtype=np.float32)

        # packets
        rates = []
        packets = []
        for i in range(num_packets):

            rate = np.frombuffer(f.read(2), dtype=np.int16).item
            rates.append(rate)

            features = np.reshape(np.frombuffer(f.read(subframe_size * subframes_per_packet), dtype=np.float32), dummy_features.shape)
            packet = np.flip(features, axis=-2)
            packets.append(packet)

    return packets