from pathlib import Path
import sys
from collections import deque
from PIL import Image

FRAMES = 128
KNOB_COLS = 16
KNOB_ROWS = 8
MASTER_FRAME = 384
RESAMPLE = Image.Resampling.LANCZOS

def make_vu(master_path: Path, size: tuple[int, int], face_target: Path, overlay_target: Path) -> None:
    master = Image.open(master_path).convert("RGBA")
    if master.size != (400, 270):
        raise RuntimeError(f"{master_path.name}: expected (400, 270), got {master.size}")
    face = master.resize(size, RESAMPLE)
    face_target.parent.mkdir(parents=True, exist_ok=True)
    face.save(face_target, format="PNG", optimize=True)

    # NeedleCover occupies the lower-centre hardware cap. Repaint only this area
    # after the live vector needle so the needle appears mechanically behind it.
    overlay = Image.new("RGBA", size, (0, 0, 0, 0))
    x0 = round(size[0] * 165 / 400)
    x1 = round(size[0] * 235 / 400)
    y0 = round(size[1] * 205 / 270)
    overlay.paste(face.crop((x0, y0, x1, size[1])), (x0, y0))
    overlay.save(overlay_target, format="PNG", optimize=True)
    print(f"{face_target.name}: {size[0]}x{size[1]}")
    print(f"{overlay_target.name}: {size[0]}x{size[1]}")

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

    vu_master = root / "meters" / "125A_MixEngine_VU_Face_400x270px_Master.png"
    make_vu(vu_master, (320, 216),
            generated / "125A_MixEngine_VU_Face_320x216.png",
            generated / "125A_MixEngine_VU_Cover_320x216.png")
    make_vu(vu_master, (960, 648),
            generated / "125A_MixEngine_VU_Face_960x648.png",
            generated / "125A_MixEngine_VU_Cover_960x648.png")

if __name__=="__main__":
    main()
