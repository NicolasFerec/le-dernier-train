from collections import deque
from pathlib import Path

from PIL import Image, ImageChops, ImageFilter, ImageOps

ART = Path(__file__).resolve().parent
DEST = ART.parents[3] / 'player/conductor'
CELL = (184, 176)
PIVOT = (88, 158)
SCALE = .44


def isolate(frame):
    alpha = frame.getchannel('A')
    # Remove the bright red matte left outside the painted silhouette.
    keep_color = bytearray(255 if not (r > 155 and r > 3 * g and r > 3 * b) else 0
                           for r, g, b, _ in frame.getdata())
    alpha = ImageChops.multiply(alpha, Image.frombytes('L', frame.size, bytes(keep_color)))
    region = alpha.point(lambda x: 255 if x > 72 else 0).filter(ImageFilter.MaxFilter(5))
    width, height = region.size
    available = bytearray(region.tobytes())
    best = []
    for start in range(len(available)):
        if not available[start]:
            continue
        available[start] = 0
        component = [start]
        queue = deque([start])
        while queue:
            pixel = queue.popleft()
            x, y = pixel % width, pixel // width
            for adjacent in (pixel-1 if x else -1, pixel+1 if x+1 < width else -1,
                             pixel-width if y else -1, pixel+width if y+1 < height else -1):
                if adjacent >= 0 and available[adjacent]:
                    available[adjacent] = 0
                    component.append(adjacent)
                    queue.append(adjacent)
        if len(component) > len(best):
            best = component
    keep = bytearray(width * height)
    for pixel in best:
        keep[pixel] = 255
    alpha = ImageChops.multiply(alpha, Image.frombytes('L', region.size, bytes(keep)))
    frame.putalpha(alpha)
    return alpha


def cells(path, flip_front):
    sheet = Image.open(path).convert('RGBA')
    for row in range(4):
        for col in range(4):
            frame = sheet.crop((col*sheet.width//4, row*sheet.height//4,
                                (col+1)*sheet.width//4, (row+1)*sheet.height//4))
            if flip_front and row < 2:
                frame = ImageOps.mirror(frame)
            alpha = isolate(frame)
            left, top, right, bottom = alpha.getbbox()
            # Anchor the centre of the legs, ignoring an extended weapon or arm.
            feet = alpha.crop((0, max(0, bottom-58), frame.width, bottom-7)).getbbox()
            pivot_x = (feet[0] + feet[2]) / 2 if feet else (left + right) / 2
            pivot_y = bottom - 2
            crop = frame.crop((left, top, right, bottom))
            crop = crop.resize((round(crop.width*SCALE), round(crop.height*SCALE)), Image.Resampling.LANCZOS)
            origin = round(PIVOT[0] + (left - pivot_x)*SCALE), round(PIVOT[1] + (top - pivot_y)*SCALE)
            yield row, col, crop, origin


def bake(weapon):
    atlas = Image.new('RGBA', (4*CELL[0], 8*CELL[1]))
    for kind, action in (('mouvement', 0), ('frappes', 2)):
        path = ART / f'{weapon}-{kind}.png'
        if not path.exists():
            continue
        for row, col, crop, origin in cells(path, kind == 'mouvement'):
            # For the sabre, the generated overhead pose precedes the descending
            # contact. Reuse the initial guard as the final recovery pose.
            if kind == 'frappes' and weapon == 'sabre' and row % 2 == 1:
                if col == 2:
                    continue
                if col == 0:
                    atlas.alpha_composite(crop, (3*CELL[0]+origin[0], (row//2*4+3)*CELL[1]+origin[1]))
                elif col == 3:
                    col = 2
            dest_row = row // 2 * 4 + action + row % 2
            atlas.alpha_composite(crop, (col*CELL[0]+origin[0], dest_row*CELL[1]+origin[1]))
    atlas.save(DEST / f'workshop_{weapon}.png', optimize=True)


if __name__ == '__main__':
    for weapon in ('sabre', 'marteau'):
        bake(weapon)
