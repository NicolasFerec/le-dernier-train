"""Build the body and weapon textures from the original conductor paintings."""
from pathlib import Path

from PIL import Image

SOURCE = Path(__file__).resolve().parent
DESTINATION = SOURCE.parents[2] / "player/conductor"
DENSITY = 2
CELL = (92 * DENSITY, 88 * DENSITY)
PIVOT = (44 * DENSITY, 79 * DENSITY)
ACTIONS = (("idle", 1), ("run", 4), ("dodge", 4), ("light", 4), ("heavy", 4), ("hit", 2), ("fall", 2))


def load(name):
    return Image.open(SOURCE / f"{name}.png").convert("RGBA")


def cell(image, column, row, columns, rows):
    return image.crop((
        image.width * column // columns, image.height * row // rows,
        image.width * (column + 1) // columns, image.height * (row + 1) // rows,
    ))


def clean_alpha(image):
    image.putalpha(image.getchannel("A").point(lambda alpha: max(0, min(255, round((alpha - 60) * 255 / 180)))))
    return image


def source_pose(sheet, action, direction, frame):
    if action == "idle":
        if direction == 1:
            return load("idle-sw"), 0
        index = (0, 0, 6, 3, 4, 5, 2, 7)[direction]
        return cell(sheet, index % 4, index // 4, 4, 2), 402
    if action in ("light", "heavy") and 1 <= direction <= 3:
        return cell(load("attacks-west"), frame, (direction - 1) * 2 + (action == "heavy"), 4, 6), 231
    return cell(sheet, frame + (2 if action == "fall" else 0), direction, 4, 8), 216


def pack_body():
    atlas = Image.new("RGBA", (8 * CELL[0], sum(count for _, count in ACTIONS) * CELL[1]))
    row = 0
    for action, count in ACTIONS:
        sheet = load("hit-fall" if action in ("hit", "fall") else action)
        for direction in range(8):
            frames = []
            for frame in range(count):
                image, reference_height = source_pose(sheet, action, direction, frame)
                clean_alpha(image)
                bounds = image.getchannel("A").point(lambda alpha: 255 if alpha > 110 else 0).getbbox()
                scale = 64 * DENSITY / (reference_height or bounds[3] - bounds[1])
                frames.append((image, bounds, scale))
            # Keep a common baseline so the body can rise and fall within each action.
            baseline = max(bounds[3] for _, bounds, _ in frames)
            for frame, (image, bounds, scale) in enumerate(frames):
                center = (bounds[0] + bounds[2]) / 2 if action == "idle" else image.width / 2
                offset = (round(PIVOT[0] - center * scale), round(PIVOT[1] - baseline * scale))
                scaled = image.resize((round(image.width * scale), round(image.height * scale)), Image.Resampling.LANCZOS)
                tile = Image.new("RGBA", CELL)
                tile.paste(scaled, offset)
                atlas.paste(tile, (direction * CELL[0], (row + frame) * CELL[1]))
        row += count
    atlas.save(DESTINATION / "poses.png", optimize=True)


def pack_weapon(name, sheet, crop, grip, span):
    image = load(sheet)
    image = clean_alpha(image.crop(crop)) if crop else clean_alpha(image)
    bounds = image.getchannel("A").getbbox()
    scale = span * DENSITY / (bounds[2] - grip[0])
    image = image.crop(bounds)
    scaled = image.resize((round(image.width * scale), round(image.height * scale)), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (128 * DENSITY, 64 * DENSITY))
    offset = tuple(32 * DENSITY - round((grip[axis] - bounds[axis]) * scale) for axis in range(2))
    canvas.paste(scaled, offset)
    canvas.save(DESTINATION / f"{name}.png", optimize=True)


if __name__ == "__main__":
    pack_body()
    pack_weapon("sabre", "weapons", (0, 0, 1086, 241), (250, 105), 46)
    pack_weapon("hammer", "hammer", None, (518, 419), 38)
