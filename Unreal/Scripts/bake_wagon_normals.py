"""Bake tangent-space normals for the painted wagon textures (requires Pillow)."""

from math import sqrt
from pathlib import Path
from random import Random

from PIL import Image, ImageDraw, ImageFilter

SIZE = 1024
ART = Path(__file__).resolve().parents[1] / 'SourceArt/Environment'


def pixels(value):
    return round(value * (SIZE - 1) / 1254)


def wood_height():
    height = Image.new('L', (SIZE, SIZE), 150)
    draw = ImageDraw.Draw(height)
    seams = (0, 111, 241, 365, 493, 621, 750, 877, 1008, 1128, 1254)
    ends = ((494, 1092), (188, 918), (82, 747), (395, 1026), (622, 1236),
            (176, 864), (423, 1007), (113, 819), (525, 1075), (246, 895))
    random = Random(406)
    for (top, bottom), split in zip(zip(seams, seams[1:]), ends):
        for _ in range(32):
            x = random.randrange(1254)
            y = random.randrange(top + 6, bottom - 6)
            draw.line((pixels(x), pixels(y), pixels(min(x + random.randrange(14, 190), 1254)), pixels(y + 1)),
                      fill=random.randrange(142, 155), width=1)
        for x in split:
            draw.line((pixels(x), pixels(top), pixels(x), pixels(bottom)), fill=80, width=4)
    for seam in seams:
        y = pixels(seam)
        draw.line((0, y, SIZE - 1, y), fill=77, width=5)
    return height.filter(ImageFilter.GaussianBlur(1.8))


def steel_height():
    height = Image.new('L', (SIZE, SIZE), 96)
    draw = ImageDraw.Draw(height)
    for inset in range(2, 17):
        draw.rectangle((inset, inset, SIZE - inset - 1, SIZE - inset - 1), fill=99 + inset * 3)
    for x in (66, 1188):
        for y in (63, 446, 805, 1171):
            cx, cy = pixels(x), pixels(y)
            for radius in range(20, 2, -1):
                draw.ellipse((cx-radius, cy-radius, cx+radius, cy+radius), fill=min(175, 124 + (20-radius)*4))
    return height.filter(ImageFilter.GaussianBlur(1.8))


def bake(height, strength, name):
    values = height.load()
    normal = Image.new('RGB', height.size)
    out = normal.load()
    for y in range(SIZE):
        up, down = (y - 1) % SIZE, (y + 1) % SIZE
        for x in range(SIZE):
            dx = (values[(x - 1) % SIZE, y] - values[(x + 1) % SIZE, y]) * strength
            dy = (values[x, up] - values[x, down]) * strength
            scale = 127.5 / sqrt(1. + dx*dx + dy*dy)
            out[x, y] = (round(127.5 + dx*scale), round(127.5 + dy*scale), round(127.5 + scale))
    normal.save(ART / name, optimize=True)


if __name__ == '__main__':
    bake(wood_height(), .023, 'WagonBoardsNormal.png')
    bake(steel_height(), .034, 'WagonSteelPlateNormal.png')
