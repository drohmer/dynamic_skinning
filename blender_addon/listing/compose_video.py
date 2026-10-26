# Preview video of the extension: side by side, Armature only (left) and with Dynamic Skinning (right)
#   python compose_video.py <frames dir of render_video.py> <output .mp4> [start (s), default 1.7]
# Composes each frame (same background and labels as comparison.png), then encodes with ffmpeg (H.264).
import glob
import os
import subprocess
import sys

from PIL import Image

from compose import comparison_background

src, output = sys.argv[1], sys.argv[2]
start = float(sys.argv[3]) if len(sys.argv) > 3 else 1.7   # the animation is still during its first 2 s
frames = os.path.join(src, "composed")
os.makedirs(frames, exist_ok=True)

base = comparison_background()

names = sorted(os.path.basename(p) for p in glob.glob(os.path.join(src, "ds", "*.png")))
for n, name in enumerate(names):
    im = base.copy()
    for k, side in enumerate(("lbs", "ds")):
        im.alpha_composite(Image.open(os.path.join(src, side, name)).convert("RGBA"), (960 * k, 40))
    im.convert("RGB").save(os.path.join(frames, f"{n:04d}.png"))

subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", "30", "-start_number", str(round(start * 30)),
                "-i", os.path.join(frames, "%04d.png"), "-c:v", "libx264", "-preset", "slow", "-crf", "20", "-pix_fmt", "yuv420p", "-movflags", "+faststart",
                output], check=True)
print("written:", output, len(names), "frames")
