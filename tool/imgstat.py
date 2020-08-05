import struct
import sys
import numpy as np

with open(sys.argv[1], "rb") as f:
    data = f.read()

image_data = np.zeros((96*96,))
for i in range(96*96):
    image_data[i] = struct.unpack_from("<H", data, i*2)[0]

image_data = np.reshape(image_data, (96, 96), 'C') # use [y][x]

# image stats

img_min = np.min(image_data)
img_max = np.max(image_data)

print("min", img_min, "max", img_max)

# stddev

print("std", np.std(image_data))

avg = np.zeros((96,)) 
for i in range(96):
    avg[i] = np.std(image_data[i, :])

print(avg.mean())
