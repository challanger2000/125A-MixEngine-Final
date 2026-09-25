#!/usr/bin/env python3
import sys
import xml.etree.ElementTree as ET

FILES = sys.argv[1:] or ["resource/mixengine.uidesc", "resource/mixengine_channel.uidesc"]

PANEL_CENTERS = {
    "Input": 96.0, "Console": 268.0, "Tube": 444.0, "Tape": 620.0,
    "Glue": 796.0, "Vinyl": 972.0, "Stereo": 1151.0, "Output": 1333.0,
}

EXPECTED_CENTERS = {
    "LabelInputTitle": 96.0,
    "LabelConsoleTitle": 268.0, "ConsolePower": 268.0, "ConsoleDrive": 268.0,
    "LabelDrive": 268.0, "LabelMode": 268.0, "ConsoleModeSelector": 268.0,
    "LabelTubeTitle": 444.0, "TubePower": 444.0, "TubeAmount": 444.0,
    "LabelTubeAmount": 444.0, "LabelVoice": 444.0, "TubeVoiceSelector": 444.0,
    "LabelTapeTitle": 620.0, "TapePower": 620.0, "TapeAmount": 620.0,
    "LabelTapeAmount": 620.0, "LabelSpeed": 620.0, "TapeSpeedSelector": 620.0,
    "LabelGlueTitle": 796.0, "GluePower": 796.0, "GlueAmount": 796.0,
    "LabelGlueAmount": 796.0, "GlueCharacter": 796.0, "LabelResponse": 796.0,
    "LabelVinylTitle": 972.0, "VinylPower": 972.0, "VinylCharacter": 972.0,
    "LabelColor": 972.0,
    "LabelStereoTitle": 1151.0, "Depth": 1151.0, "Width": 1151.0,
    "LowMono": 1151.0, "LabelDepth": 1151.0, "LabelWidth": 1151.0,
    "LabelLowMono": 1151.0, "LabelFixed120": 1151.0,
    "LabelOutputTitle": 1333.0, "Output": 1333.0, "LabelOutputGain": 1333.0,
    "LevelMatch": 1333.0, "LabelLevelMatch": 1333.0,
    "LabelVUSource": 720.0, "VUSourceSelector": 720.0, "LabelVURef": 720.0,
    "LabelMixTitle": 1323.0, "LabelQuality": 1323.0, "QualitySelector": 1323.0,
    "LabelBypass": 1323.0, "Bypass": 1323.0,
    "VULeft": 480.0, "VURight": 960.0,
}

def point(value):
    a, b = value.split(",")
    return float(a), float(b)

for path in FILES:
    tree = ET.parse(path)
    root = tree.getroot()
    template = root.find("template")
    if template is None:
        raise SystemExit(f"{path}: missing <template>")
    if template.get("size") != "1440,800":
        raise SystemExit(f"{path}: template must be exactly 1440x800")

    views = {}
    for view in template.findall("view"):
        name = view.get("custom-view-name")
        if not name:
            continue
        x, y = point(view.get("origin"))
        w, h = point(view.get("size"))
        if x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > 1440 or y + h > 800:
            raise SystemExit(f"{path}: {name} outside 1440x800: origin={x,y} size={w,h}")
        views[name] = (x, y, w, h)

    for name, expected in EXPECTED_CENTERS.items():
        if name not in views:
            raise SystemExit(f"{path}: required view missing: {name}")
        x, y, w, h = views[name]
        actual = x + w / 2.0
        if actual != expected:
            raise SystemExit(f"{path}: {name} centre {actual} != exact {expected}")

    for name, expected_box in {
        "VULeft": (320.0, 50.0, 320.0, 184.0),
        "VURight": (800.0, 50.0, 320.0, 184.0),
    }.items():
        if views[name] != expected_box:
            raise SystemExit(f"{path}: {name} box {views[name]} != exact {expected_box}")


    # All 64px secondary knobs share one exact lower baseline. This uses the
    # available vertical space and prevents Glue/Vinyl from visually floating.
    secondary_knobs = ["ConsoleNoise", "TapeStability", "TapeHiss",
                       "GlueCharacter", "VinylWear", "VinylNoise", "LowMono"]
    if path.endswith("mixengine.uidesc"):
        secondary_knobs.insert(0, "Crosstalk")
    for name in secondary_knobs:
        x, y, w, h = views[name]
        if (y, w, h) != (633.0, 64.0, 64.0):
            raise SystemExit(f"{path}: {name} must use exact lower row y=633, size=64x64; got {views[name]}")

    secondary_labels = ["LabelConsoleNoise", "LabelStability", "LabelHiss",
                        "LabelResponse", "LabelWear", "LabelSurface", "LabelLowMono"]
    if path.endswith("mixengine.uidesc"):
        secondary_labels.insert(0, "LabelCrosstalk")
    for name in secondary_labels:
        if views[name][1] != 706.0:
            raise SystemExit(f"{path}: {name} must use exact lower label baseline y=706; got y={views[name][1]}")

    # Exact bilateral placement inside two-control module rows.
    pairs = {
        "Tape": ("TapeStability", "TapeHiss", 38.0),
        "Vinyl": ("VinylWear", "VinylNoise", 38.0),
    }
    if path.endswith("mixengine.uidesc"):
        pairs["Console"] = ("Crosstalk", "ConsoleNoise", 38.0)
    for module, (left, right, offset) in pairs.items():
        centre = PANEL_CENTERS[module]
        lx, _, lw, _ = views[left]
        rx, _, rw, _ = views[right]
        if lx + lw/2.0 != centre-offset or rx + rw/2.0 != centre+offset:
            raise SystemExit(f"{path}: {module} secondary controls are not exactly symmetric")

    # Channel build has one Console secondary control; it must sit on the module axis.
    if path.endswith("mixengine_channel.uidesc"):
        x, _, w, _ = views["ConsoleNoise"]
        if x + w/2.0 != PANEL_CENTERS["Console"]:
            raise SystemExit(f"{path}: ConsoleNoise not centred in Channel build")

    print(f"{path}: XML + bounds + exact centre-axis layout PASS")
