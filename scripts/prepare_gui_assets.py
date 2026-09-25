from pathlib import Path
import sys
from collections import deque
from PIL import Image, ImageChops, ImageFilter

FRAMES = 128
COLS = 16
ROWS = 8
MASTER_FRAME = 384
RESAMPLE = Image.Resampling.LANCZOS

VU_SOURCE_W = 480
VU_SOURCE_H = 276
VU_COLS = 8
VU_ROWS = 16
VU_NEEDLE_CROP = (72, 18, 408, 270)

def load_knob_master(source: Path) -> Image.Image:
    image = Image.open(source).convert("RGBA")
    expected = (MASTER_FRAME, MASTER_FRAME * FRAMES)
    if image.size != expected:
        raise RuntimeError(f"{source.name}: expected {expected}, got {image.size}")
    return image

def make_atlas(master: Image.Image, frame_size: int, target: Path) -> None:
    atlas = Image.new("RGBA", (frame_size * COLS, frame_size * ROWS), (0, 0, 0, 0))
    for index in range(FRAMES):
        frame = master.crop((0, index * MASTER_FRAME, MASTER_FRAME, (index + 1) * MASTER_FRAME))
        if frame_size != MASTER_FRAME:
            frame = frame.resize((frame_size, frame_size), RESAMPLE)
        atlas.paste(frame, ((index % COLS) * frame_size, (index // COLS) * frame_size))
    target.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(target, format="PNG", optimize=True)
    print(f"{target.name}: {atlas.width}x{atlas.height}")

def load_vu_frames(source: Path):
    strip = Image.open(source).convert("RGBA")
    expected = (VU_SOURCE_W, VU_SOURCE_H * FRAMES)
    if strip.size != expected:
        raise RuntimeError(f"{source.name}: expected {expected}, got {strip.size}")
    return [
        strip.crop((0, i * VU_SOURCE_H, VU_SOURCE_W, (i + 1) * VU_SOURCE_H))
        for i in range(FRAMES)
    ]

def reconstruct_vu_face(frames):
    face = frames[0].copy()
    for frame in frames[1:]:
        face = ImageChops.lighter(face, frame)
    return face

def clear_connected_dark_border(image: Image.Image, threshold: int = 30) -> Image.Image:
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

def make_original_vu(source: Path, size, face_target: Path, needle_target: Path, cover_target: Path) -> None:
    frames = load_vu_frames(source)
    raw_face = reconstruct_vu_face(frames)
    visible_face = clear_connected_dark_border(raw_face)

    face = visible_face.resize(size, RESAMPLE)
    face_target.parent.mkdir(parents=True, exist_ok=True)
    face.save(face_target, format="PNG", optimize=True)

    sx = size[0] / VU_SOURCE_W
    sy = size[1] / VU_SOURCE_H
    x0, y0, x1, y1 = VU_NEEDLE_CROP
    dx0, dy0, dx1, dy1 = round(x0*sx), round(y0*sy), round(x1*sx), round(y1*sy)
    tile_w, tile_h = dx1-dx0, dy1-dy0
    atlas = Image.new("RGBA", (tile_w*VU_COLS, tile_h*VU_ROWS), (0,0,0,0))

    for index, frame in enumerate(frames):
        diff = ImageChops.difference(frame, raw_face).convert("L")
        mask = diff.point(lambda p: 255 if p >= 3 else 0).filter(ImageFilter.MaxFilter(3))
        layer = Image.new("RGBA", frame.size, (0,0,0,0))
        layer.paste(frame, (0,0), mask)
        crop = layer.crop(VU_NEEDLE_CROP).resize((tile_w, tile_h), RESAMPLE)
        atlas.alpha_composite(crop, ((index % VU_COLS)*tile_w, (index // VU_COLS)*tile_h))

    atlas.save(needle_target, format="PNG", optimize=True)

    overlay = Image.new("RGBA", size, (0,0,0,0))
    cover_box = (190, 202, 290, 276)
    cx0, cy0, cx1, cy1 = round(cover_box[0]*sx), round(cover_box[1]*sy), round(cover_box[2]*sx), round(cover_box[3]*sy)
    overlay.alpha_composite(face.crop((cx0,cy0,cx1,cy1)), (cx0,cy0))
    overlay.save(cover_target, format="PNG", optimize=True)

    print(f"{face_target.name}: {size[0]}x{size[1]}")
    print(f"{needle_target.name}: {atlas.width}x{atlas.height} / tile {tile_w}x{tile_h}")
    print(f"{cover_target.name}: {size[0]}x{size[1]}")

def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: prepare_gui_assets.py <assets/mixengine>")
    root = Path(sys.argv[1]).resolve()
    generated = root / "generated"

    vernier = load_knob_master(root / "knobs" / "125A_MixEngine_Vernier_L_384px_300pct_128f.png")
    gunmetal = load_knob_master(root / "knobs" / "125A_MixEngine_Small_Gunmetal_384px_300pct_128f.png")

    make_atlas(vernier, 128, generated / "125A_MixEngine_Vernier_L_128px_16x8_128f.png")
    make_atlas(vernier, 384, generated / "125A_MixEngine_Vernier_L_384px_16x8_128f.png")
    make_atlas(gunmetal, 64, generated / "125A_MixEngine_Gunmetal_S_64px_16x8_128f.png")
    make_atlas(gunmetal, 192, generated / "125A_MixEngine_Gunmetal_S_192px_16x8_128f.png")
    make_atlas(gunmetal, 96, generated / "125A_MixEngine_Gunmetal_M_96px_16x8_128f.png")
    make_atlas(gunmetal, 288, generated / "125A_MixEngine_Gunmetal_M_288px_16x8_128f.png")

    vu_source = root / "meters" / "125A_MixEngine_VU_Meter_480x276px_300pct_128f.png"
    make_original_vu(vu_source, (320,184),
        generated / "125A_MixEngine_VU_Original_Face_320x184.png",
        generated / "125A_MixEngine_VU_Original_Needle_224x168_8x16_128f.png",
        generated / "125A_MixEngine_VU_Original_Cover_320x184.png")
    make_original_vu(vu_source, (960,552),
        generated / "125A_MixEngine_VU_Original_Face_960x552.png",
        generated / "125A_MixEngine_VU_Original_Needle_672x504_8x16_128f.png",
        generated / "125A_MixEngine_VU_Original_Cover_960x552.png")

if __name__ == "__main__":
    main()
