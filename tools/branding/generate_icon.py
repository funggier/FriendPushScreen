from __future__ import annotations

from pathlib import Path
import math

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
BRANDING_DIR = ROOT / "assets" / "branding"
RESOURCE_DIR = ROOT / "resources"
MASTER_PATH = BRANDING_DIR / "FriendPushScreen-icon.png"
ICO_PATH = RESOURCE_DIR / "FriendPushScreen.ico"

SIZE = 1024


def lerp(a: int, b: int, t: float) -> int:
    return int(round(a + (b - a) * t))


def gradient(size: tuple[int, int], top: tuple[int, int, int], bottom: tuple[int, int, int]) -> Image.Image:
    w, h = size
    image = Image.new("RGB", size)
    px = image.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(lerp(top[i], bottom[i], t) for i in range(3))
        for x in range(w):
            px[x, y] = c
    return image


def horizontal_gradient(size: tuple[int, int], left: tuple[int, int, int], right: tuple[int, int, int]) -> Image.Image:
    w, h = size
    image = Image.new("RGB", size)
    px = image.load()
    for x in range(w):
        t = x / max(1, w - 1)
        c = tuple(lerp(left[i], right[i], t) for i in range(3))
        for y in range(h):
            px[x, y] = c
    return image


def rounded_mask(size: tuple[int, int], radius: int) -> Image.Image:
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius=radius, fill=255)
    return mask


def wave_points(x0: int, x1: int, base_y: float, amp: float, cycles: float, phase: float, samples: int = 80):
    points = []
    for i in range(samples + 1):
        t = i / samples
        x = x0 + (x1 - x0) * t
        y = base_y + amp * math.sin((t * cycles * math.tau) + phase)
        points.append((int(round(x)), int(round(y))))
    return points


def draw_icon() -> Image.Image:
    bg = Image.new(
        "RGBA",
        (SIZE, SIZE),
        (0, 0, 0, 0),
    )

    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(canvas)

    outer = (174, 155, 850, 700)
    shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle((outer[0] + 14, outer[1] + 22, outer[2] + 14, outer[3] + 22),
                         radius=58, fill=(0, 0, 0, 145))
    shadow = shadow.filter(ImageFilter.GaussianBlur(24))
    canvas.alpha_composite(shadow)

    frame_grad = horizontal_gradient((outer[2] - outer[0], outer[3] - outer[1]),
                                     (151, 86, 43), (246, 183, 91)).convert("RGBA")
    frame_mask = rounded_mask(frame_grad.size, 58)
    canvas.paste(frame_grad, (outer[0], outer[1]), frame_mask)

    # Metallic top highlight.
    d.rounded_rectangle(outer, radius=58, outline=(255, 218, 150, 230), width=6)

    bezel = (196, 177, 828, 678)
    d.rounded_rectangle(bezel, radius=45, fill=(19, 14, 12, 255), outline=(84, 54, 37, 255), width=5)

    screen = (214, 195, 810, 660)
    sw, sh = screen[2] - screen[0], screen[3] - screen[1]
    screen_img = horizontal_gradient((sw, sh), (88, 52, 39), (217, 131, 46)).convert("RGBA")

    # Oak-like layered flowing waves.
    wave_layer = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    wd = ImageDraw.Draw(wave_layer)
    specs = [
        (86, 27, 0.85, 0.15, (247, 188, 92, 245)),
        (145, 39, 0.75, 1.15, (177, 99, 52, 245)),
        (220, 30, 0.95, 0.55, (229, 141, 57, 245)),
        (285, 42, 0.75, 1.95, (103, 58, 45, 250)),
        (355, 26, 0.82, 2.55, (60, 38, 35, 250)),
    ]
    for base, amp, cycles, phase, color in specs:
        top_pts = wave_points(0, sw, base, amp, cycles, phase)
        poly = top_pts + [(sw, sh), (0, sh)]
        wd.polygon(poly, fill=color)
        wd.line(top_pts, fill=(255, 202, 112, 165), width=3)

    screen_img.alpha_composite(wave_layer)

    # Soft top-left screen highlight and diagonal reflection.
    refl = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    rd = ImageDraw.Draw(refl)
    rd.polygon([(0, 0), (sw * 0.72, 0), (sw * 0.42, sh), (0, sh)],
               fill=(255, 232, 192, 42))
    refl = refl.filter(ImageFilter.GaussianBlur(2))
    screen_img.alpha_composite(refl)

    screen_mask = rounded_mask((sw, sh), 31)
    canvas.paste(screen_img, (screen[0], screen[1]), screen_mask)
    d.rounded_rectangle(screen, radius=31, outline=(232, 160, 83, 210), width=4)

    # Stand.
    stand_shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ssd = ImageDraw.Draw(stand_shadow)
    ssd.ellipse((350, 804, 690, 880), fill=(0, 0, 0, 120))
    stand_shadow = stand_shadow.filter(ImageFilter.GaussianBlur(18))
    canvas.alpha_composite(stand_shadow)

    stand_poly = [(468, 697), (556, 697), (574, 805), (450, 805)]
    stand_grad = gradient((160, 130), (236, 170, 92), (132, 75, 43)).convert("RGBA")
    stand_mask = Image.new("L", (160, 130), 0)
    smd = ImageDraw.Draw(stand_mask)
    local_poly = [(18, 0), (106, 0), (124, 108), (0, 108)]
    smd.polygon(local_poly, fill=255)
    canvas.paste(stand_grad, (450, 697), stand_mask)

    base = (378, 792, 646, 850)
    base_grad = horizontal_gradient((base[2]-base[0], base[3]-base[1]),
                                    (150, 85, 46), (241, 177, 96)).convert("RGBA")
    base_mask = rounded_mask(base_grad.size, 24)
    canvas.paste(base_grad, (base[0], base[1]), base_mask)
    d.rounded_rectangle(base, radius=24, outline=(255, 205, 132, 180), width=3)

    bg.alpha_composite(canvas)
    return bg


def main() -> None:
    BRANDING_DIR.mkdir(parents=True, exist_ok=True)
    RESOURCE_DIR.mkdir(parents=True, exist_ok=True)

    icon = draw_icon()
    icon.save(MASTER_PATH, optimize=True)
    icon.save(
        ICO_PATH,
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )

    print(MASTER_PATH)
    print(ICO_PATH)


if __name__ == "__main__":
    main()
