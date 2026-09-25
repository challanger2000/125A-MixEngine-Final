from pathlib import Path
import sys
from PIL import Image

FRAMES = 128
COLS = 16
ROWS = 8
FRAME = 384

def make_atlas(source: Path, target: Path) -> None:
    with Image.open(source) as image:
        image = image.convert("RGBA")
        expected = (FRAME, FRAME * FRAMES)
        if image.size != expected:
            raise RuntimeError(f"{source.name}: expected {expected}, got {image.size}")
        atlas = Image.new("RGBA", (FRAME * COLS, FRAME * ROWS), (0, 0, 0, 0))
        for index in range(FRAMES):
            frame = image.crop((0, index * FRAME, FRAME, (index + 1) * FRAME))
            atlas.paste(frame, ((index % COLS) * FRAME, (index // COLS) * FRAME))
        target.parent.mkdir(parents=True, exist_ok=True)
        atlas.save(target, format="PNG", optimize=False)
        print(f"{source.name} -> {target.name}: {atlas.width}x{atlas.height}")

def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: prepare_gui_assets.py <assets/mixengine>")
    root = Path(sys.argv[1]).resolve()
    generated = root / "generated"
    make_atlas(root / "knobs" / "125A_MixEngine_Vernier_L_384px_300pct_128f.png",
               generated / "125A_MixEngine_Vernier_L_384px_16x8_128f.png")
    make_atlas(root / "knobs" / "125A_MixEngine_Small_Gunmetal_384px_300pct_128f.png",
               generated / "125A_MixEngine_Small_Gunmetal_384px_16x8_128f.png")

if __name__ == "__main__":
    main()
