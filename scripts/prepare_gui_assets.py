from pathlib import Path
import sys
from collections import deque
from PIL import Image

FRAMES = 128
KNOB_COLS = 16
KNOB_ROWS = 8
MASTER_FRAME = 384
RESAMPLE = Image.Resampling.LANCZOS

VU_COLS = 16
VU_ROWS = 8

def load_knob_master(source: Path) -> Image.Image:
    image = Image.open(source).convert("RGBA")
    expected = (MASTER_FRAME, MASTER_FRAME * FRAMES)
    if image.size != expected:
        raise RuntimeError(f"{source.name}: expected {expected}, got {image.size}")
    return image

def make_knob_atlas(master: Image.Image, frame_size: int, target: Path) -> None:
    atlas = Image.new("RGBA", (frame_size * KNOB_COLS, frame_size * KNOB_ROWS), (0, 0, 0, 0))
    for index in range(FRAMES):
        frame = master.crop((0, index * MASTER_FRAME, MASTER_FRAME, (index + 1) * MASTER_FRAME))
        if frame_size != MASTER_FRAME:
            frame = frame.resize((frame_size, frame_size), RESAMPLE)
        atlas.paste(frame, ((index % KNOB_COLS) * frame_size, (index // KNOB_COLS) * frame_size))
    target.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(target, format="PNG", optimize=True)
    print(f"{target.name}: {atlas.width}x{atlas.height}")

def clear_connected_dark_border(image: Image.Image, threshold: int = 26) -> Image.Image:
    image = image.copy()
    px = image.load()
    w, h = image.size
    seen = bytearray(w * h)
    q = deque()

    def eligible(x, y):
        r, g, b, a = px[x, y]
        return a > 0 and max(r, g, b) <= threshold

    for x in range(w):
        if eligible(x, 0): q.append((x, 0))
        if eligible(x, h - 1): q.append((x, h - 1))
    for y in range(h):
        if eligible(0, y): q.append((0, y))
        if eligible(w - 1, y): q.append((w - 1, y))

    while q:
        x, y = q.popleft()
        idx = y * w + x
        if seen[idx]:
            continue
        seen[idx] = 1
        if not eligible(x, y):
            continue
        r, g, b, _ = px[x, y]
        px[x, y] = (r, g, b, 0)
        if x > 0: q.append((x - 1, y))
        if x + 1 < w: q.append((x + 1, y))
        if y > 0: q.append((x, y - 1))
        if y + 1 < h: q.append((x, y + 1))
    return image

def make_vu_atlas(source: Path, frame_size: tuple[int, int], target: Path) -> None:
    frame_w, frame_h = frame_size
    strip = Image.open(source).convert("RGBA")
    expected = (frame_w, frame_h * FRAMES)
    if strip.size != expected:
        raise RuntimeError(f"{source.name}: expected {expected}, got {strip.size}")
    atlas = Image.new("RGBA", (frame_w * VU_COLS, frame_h * VU_ROWS), (0,0,0,0))
    for index in range(FRAMES):
        frame = strip.crop((0,index*frame_h,frame_w,(index+1)*frame_h))
        frame = clear_connected_dark_border(frame)
        atlas.alpha_composite(frame, ((index % VU_COLS)*frame_w,(index // VU_COLS)*frame_h))
    target.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(target,format="PNG",optimize=True)
    print(f"{target.name}: {atlas.width}x{atlas.height} / frame {frame_w}x{frame_h}")

def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: prepare_gui_assets.py <assets/mixengine>")
    root=Path(sys.argv[1]).resolve()
    generated=root/"generated"

    vernier=load_knob_master(root/"knobs"/"125A_MixEngine_Vernier_L_384px_300pct_128f.png")
    gunmetal=load_knob_master(root/"knobs"/"125A_MixEngine_Small_Gunmetal_384px_300pct_128f.png")

    make_knob_atlas(vernier,128,generated/"125A_MixEngine_Vernier_L_128px_16x8_128f.png")
    make_knob_atlas(vernier,384,generated/"125A_MixEngine_Vernier_L_384px_16x8_128f.png")
    make_knob_atlas(gunmetal,64,generated/"125A_MixEngine_Gunmetal_S_64px_16x8_128f.png")
    make_knob_atlas(gunmetal,192,generated/"125A_MixEngine_Gunmetal_S_192px_16x8_128f.png")
    make_knob_atlas(gunmetal,96,generated/"125A_MixEngine_Gunmetal_M_96px_16x8_128f.png")
    make_knob_atlas(gunmetal,288,generated/"125A_MixEngine_Gunmetal_M_288px_16x8_128f.png")

    # Full original meter frames, repacked only. No reconstructed face, no synthetic
    # needle, no estimated pivot. 320x184 is exact 200% source, 480x276 exact 300%.
    make_vu_atlas(root/"meters"/"125A_MixEngine_VU_Meter_320x184px_200pct_128f.png",
                  (320,184),generated/"125A_MixEngine_VU_320x184_16x8_128f.png")
    make_vu_atlas(root/"meters"/"125A_MixEngine_VU_Meter_480x276px_300pct_128f.png",
                  (480,276),generated/"125A_MixEngine_VU_480x276_16x8_128f.png")

if __name__=="__main__":
    main()
