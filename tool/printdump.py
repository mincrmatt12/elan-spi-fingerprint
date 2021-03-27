import struct
from PIL import Image
import sys

if len(sys.argv) != 6:
    print("usage: printdump input_raw output_unprocessed.png output_processed.png sensor_width sensor_height")

with open(sys.argv[1], "rb") as f:
    data = f.read()

width = int(sys.argv[4])
height = int(sys.argv[5])

image_data = []
for i in range(width*height):
    image_data.append(struct.unpack_from("<H", data, i*2)[0])

mini = min(image_data)
print(mini)
maxi = max(image_data)
print(maxi)

rx = 256 / (maxi - mini)

real_data = [int(rx * (v - mini)) for v in image_data]

im = Image.new("L", (width, height))

for y in range(height):
    for x in range(width):
        im.putpixel((x, y), real_data[y * width + x])

im.save(sys.argv[2])

sorted_pixels = list(sorted(set(image_data)))
count = len(sorted_pixels)

lvl0 = sorted_pixels[0]
lvl1 = sorted_pixels[int(count * 0.08)]
lvl2 = sorted_pixels[int(count * 0.92)]
lvl3 = maxi

rx = 256 / (lvl2 - lvl1)

def lookup(val):
    if val < lvl1:
        return 0
    if lvl2 < val:
        return 255
    return int(rx * (val - lvl1))

real_data = [int(lookup(v)) for v in image_data]

for y in range(height):
    for x in range(width):
        im.putpixel((x, y), 255-real_data[y * width + x])

im.save(sys.argv[3])
