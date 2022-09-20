#!/usr/bin/python3
# extract a common palette from N files
# save it to palette.png + encode it to stdout

import sys
from PIL import Image

DEBUG = True


def stack_image(images):
    "stack vertically, expanding as needed"
    w = max(i.size[0] for i in images)
    h = sum(i.size[1] for i in images)
    newimg = Image.new("RGB", (w, h))
    y = 0
    for im in images:
        newimg.paste(im, (0, y))
        y += im.size[1]
    return newimg


if __name__ == "__main__":

    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "file_in", nargs="+", help="Input files to extract palette from"
    )
    parser.add_argument("--file_out", help="Output file name.", default="palette.png")
    parser.add_argument(
        "--colors", type=int, help="number of colors in palette", default=256
    )
    args = parser.parse_args()

    if len(args.file_in) >= 2:
        srcs = [
            Image.open(filein).convert("RGB") for filein in args.file_in
        ]  # removing alpha here
        src = stack_image(srcs)
    else:
        src = Image.open(args.file_in[0])  # keep mode as is

    # set to 256c image (if needed)
    if src.mode != "P" or len(src.palette.asbytes()) // 3 != args.colors:
        src_pal = src.quantize(colors=args.colors)
    else:
        src_pal = src
    if DEBUG:
        src_pal.save("_debug.png")  # after quantization

    # save small palette image
    src_pal = src_pal.crop((0, 0, 16, (args.colors + 15) / 16))
    src_pal.putdata(range(args.colors))
    src_pal.save(args.file_out)

    # export to palette.c
    px = src_pal.convert("RGB").load()
    print("#include <stdint.h>")
    print(f"const uint8_t game_palette[{args.colors}*3]= {{")

    for i in range(args.colors):
        c = px[i % 16, i / 16]
        sys.stdout.write(f"\t0x{c[0]:02x},0x{c[1]:02x},0x{c[2]:02x},\n")
    print("};")
