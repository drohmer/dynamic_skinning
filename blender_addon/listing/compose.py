# Listing images of the extension: featured image (1920x1080), comparison (1920x1080), icon (256x256)
#   python compose.py <renders dir> <output dir>
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1920, 1080
FONT = "/System/Library/Fonts/Avenir Next.ttc"


def font(size, index):
    return ImageFont.truetype(FONT, size, index=index)   # 0: Bold, 2: Demi Bold, 7: Regular


def background(w, h, inner=(52, 64, 76), outer=(22, 26, 32)):
    y, x = np.mgrid[0:h, 0:w]
    r = np.sqrt(((x - 0.68 * w) / w) ** 2 + ((y - 0.5 * h) / h) ** 2) / 0.75
    r = np.clip(r, 0, 1)[..., None]
    c = (1 - r) * np.array(inner) + r * np.array(outer)
    return Image.fromarray(np.dstack([c, np.full((h, w), 255)]).astype(np.uint8), "RGBA")


def comparison_background():
    """Background and labels of the side by side comparison: Armature only (left), Dynamic Skinning (right)."""
    im = background(W, H, inner=(48, 56, 66))
    d = ImageDraw.Draw(im)
    d.line((960, 120, 960, H - 80), fill=(90, 100, 112), width=3)
    for k, label in enumerate(("Armature only", "With Dynamic Skinning")):
        f = font(54, 2)
        w = d.textlength(label, font=f)
        d.text((960 * k + (960 - w) / 2, 70), label, font=f, fill=(255, 255, 255) if k else (190, 198, 208))
    return im


def faded(im, alpha, tint=(120, 190, 255)):
    """Ghost of a pose: tinted, transparent."""
    a = np.asarray(im, float)
    rgb = 0.45 * a[..., :3] + 0.55 * np.array(tint)
    return Image.fromarray(np.dstack([rgb, a[..., 3] * alpha]).astype(np.uint8), "RGBA")


def main(src, out):
    os.makedirs(out, exist_ok=True)

    def load(name):
        return Image.open(os.path.join(src, name + ".png")).convert("RGBA")

    # ---- Featured image: trail of the previous poses, current pose in front, title on the left ----
    times = ["02.80", "02.95", "03.10", "03.25"]
    img = background(W, H)
    for t, alpha in zip(times[:-1], (0.18, 0.28, 0.42)):
        img.alpha_composite(faded(load("ds_" + t), alpha))
    img.alpha_composite(load("ds_" + times[-1]))
    d = ImageDraw.Draw(img)
    d.text((120, 360), "Dynamic", font=font(132, 0), fill=(255, 255, 255))
    d.text((120, 500), "Skinning", font=font(132, 0), fill=(255, 255, 255))
    d.text((126, 680), "Secondary motion driven by", font=font(46, 7), fill=(200, 215, 230))
    d.text((126, 740), "the velocity of the bones", font=font(46, 7), fill=(200, 215, 230))
    img.convert("RGB").save(os.path.join(out, "featured.png"))

    # ---- Comparison: Armature only vs with dynamic skinning, same instant ----
    t = "03.25"
    cmp_img = comparison_background()
    x0, x1 = 760, 1880   # horizontal extent of the cow in the renders
    for k, name in enumerate(("lbs_", "ds_")):
        crop = load(name + t).crop((x0 - 60, 0, x1 - 60, H)).resize((960, int(H * 960 / (x1 - x0))))
        cmp_img.alpha_composite(crop, (960 * k, (H - crop.height) // 2 + 40))
    cmp_img.convert("RGB").save(os.path.join(out, "comparison.png"))

    # ---- Icon: head of Spot with its trail, on a rounded square ----
    S = 1024
    icon = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    tile = background(S, S, inner=(60, 120, 190), outer=(28, 52, 92))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, S - 1, S - 1), 200, fill=255)
    icon.paste(tile, (0, 0), mask)
    box = (1120, 50, 1920, 850)   # head of Spot in the 1920x1080 renders
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    for t, alpha in zip(times[:-1], (0.25, 0.4, 0.6)):
        layer.alpha_composite(faded(load("ds_" + t).crop(box).resize((S, S)), alpha, tint=(200, 230, 255)))
    layer.alpha_composite(load("ds_" + times[-1]).crop(box).resize((S, S)))
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    shadow.putalpha(layer.getchannel("A").point(lambda v: v * 0.35).filter(ImageFilter.GaussianBlur(18)))
    clip = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    clip.alpha_composite(shadow, (0, 16))
    clip.alpha_composite(layer)
    icon.paste(clip, (0, 0), Image.composite(clip, Image.new("RGBA", (S, S)), mask).getchannel("A"))
    icon.resize((256, 256), Image.LANCZOS).save(os.path.join(out, "icon.png"))
    print("written:", sorted(os.listdir(out)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
