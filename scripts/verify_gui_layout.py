#!/usr/bin/env python3
import json,subprocess,sys,tempfile
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
GEN=ROOT/"scripts/generate_gui.py"

COMMON_REQUIRED=[
    "HardwareFaceplate","BrandLogo","UIScale","VULeft","VURight","ClipL","ClipR",
    "VUSourceSelector","QualitySelector","InputRefSelector","ConsoleModeSelector","TubeVoiceSelector","TapeSpeedSelector",
    "Bypass","Input","ConsolePower","ConsoleDrive","ConsoleNoise","TubePower","TubeAmount",
    "TapePower","TapeAmount","TapeStability","TapeHiss","GluePower","GlueAmount","GlueCharacter",
    "VinylPower","VinylCharacter","VinylWear","VinylNoise","Depth","Width","LowMono","Output","LevelMatch",
    "LabelVUSource","LabelVURef","LabelMixTitle","LabelQuality","LabelBypass",
    "LabelInputTitle","LabelConsoleTitle","LabelTubeTitle","LabelTapeTitle","LabelGlueTitle","LabelVinylTitle","LabelStereoTitle","LabelOutputTitle",
    "LabelInputGain","LabelRefLevel","LabelDbfs","LabelDrive","LabelMode","LabelConsoleNoise",
    "LabelTubeAmount","LabelVoice","LabelTapeAmount","LabelSpeed","LabelIps","LabelStability","LabelHiss",
    "LabelGlueAmount","LabelResponse","LabelColor","LabelWear","LabelSurface","LabelDepth","LabelWidth",
    "LabelLowMono","LabelFixed120","LabelOutputGain","LabelLevelMatch"
]

def verify(layout_name,ui_name,expect_crosstalk):
    layout=ROOT/"resource"/layout_name
    ui=ROOT/"resource"/ui_name
    L=json.loads(layout.read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory() as td:
        tmp=Path(td)/ui_name
        subprocess.run([sys.executable,str(GEN),"--layout",str(layout),"--output",str(tmp)],check=True)
        generated=tmp.read_text(encoding="utf-8")
    actual=ui.read_text(encoding="utf-8")
    if actual!=generated:
        raise SystemExit(f"{ui_name} is stale: regenerate it from {layout_name} before building")

    root=ET.fromstring(generated)
    tpl=root.find("template")
    assert tpl is not None and tpl.attrib.get("size")=="1440,800"
    views=[x for x in tpl if x.attrib.get("custom-view-name")]
    names=[x.attrib["custom-view-name"] for x in views]
    required=list(COMMON_REQUIRED)
    if expect_crosstalk:
        required += ["Crosstalk","LabelCrosstalk"]
    assert not [x for x in required if x not in names],(ui_name,"missing required views")
    assert len(names)==len(set(names)),(ui_name,"duplicate view names")
    assert ("Crosstalk" in names)==expect_crosstalk,(ui_name,"Crosstalk visibility mismatch")
    assert ("LabelCrosstalk" in names)==expect_crosstalk,(ui_name,"Crosstalk label visibility mismatch")

    M=L["modules"];C=L["controls"]
    for mod,ctrl in [("Console","ConsoleDrive"),("Tube","TubeAmount"),("Tape","TapeAmount"),("Glue","GlueAmount"),("Vinyl","VinylCharacter"),("Output","Output")]:
        assert M[mod]==C[ctrl][0],f"{ui_name}: {ctrl} off module axis"
    for ctrl in ["Depth","Width","LowMono"]:
        assert M["Stereo"]==C[ctrl][0],f"{ui_name}: {ctrl} off Stereo axis"
    if expect_crosstalk:
        assert C["Crosstalk"][0] < M["Console"] < C["ConsoleNoise"][0],f"{ui_name}: Console lower row is not symmetric"
    else:
        assert C["ConsoleNoise"][0] == M["Console"],f"{ui_name}: Channel Console Noise is not centered"

    for v in views:
        x,y=map(int,v.attrib["origin"].split(","));w,h=map(int,v.attrib["size"].split(","))
        assert x>=0 and y>=0 and x+w<=1440 and y+h<=800,(ui_name,v.attrib["custom-view-name"],x,y,w,h)

    def box(name):
        v=next(x for x in views if x.attrib["custom-view-name"]==name)
        x,y=map(int,v.attrib["origin"].split(","));w,h=map(int,v.attrib["size"].split(","))
        return x,y,w,h
    def above(name,top_edge):
        _,y,_,h=box(name)
        assert y+h<=top_edge-6,f"{ui_name}: {name} collides with recessed hardware border"
    def below(name,bottom_edge):
        _,y,_,_=box(name)
        assert y>=bottom_edge+6,f"{ui_name}: {name} collides with recessed hardware border"

    above("LabelVUSource",92)
    below("LabelVURef",158)
    above("LabelQuality",104)
    above("LabelBypass",188)
    above("LabelRefLevel",586)
    below("LabelDbfs",628)
    above("LabelMode",574)
    above("LabelVoice",574)
    above("LabelSpeed",574)
    below("LabelIps",618)
    above("LabelLevelMatch",582)

    assert not list(tpl.findall('view[@class="CTextLabel"]')),f"{ui_name}: standard CTextLabel returned"
    assert not list(tpl.findall('view[@class="CSegmentButton"]')),f"{ui_name}: standard CSegmentButton returned"
    assert "<bitmap " not in generated,f"{ui_name}: bitmap asset returned"
    assert "MixEngineFaceplate" not in generated,f"{ui_name}: legacy faceplate binding returned"
    assert "mixengine_faceplate_1440x800.png" not in generated,f"{ui_name}: legacy raster resource returned"
    assert 'custom-view-name="HardwareFaceplate"' in generated,f"{ui_name}: procedural faceplate missing"

verify("mixengine.layout.json","mixengine.uidesc",True)
verify("mixengine_channel.layout.json","mixengine_channel.uidesc",False)
print("Generated GUI contracts PASS: Mix FX with Crosstalk, Channel without Crosstalk")

# Flagship V3 presentation contracts: explicit scalable UI factors and no stale V2 face identity.
controller=(ROOT/"source/controller.cpp").read_text(encoding="utf-8")
controls=(ROOT/"source/HardwareControls.cpp").read_text(encoding="utf-8")
assert 'label("LabelMixTitle","MIX ENGINE V3",13.0,true,false)' in controller,"V3 GUI title missing"
assert '"MIX ENGINE V2"' not in controller,"stale V2 GUI title returned"
assert 'constexpr double factors[] {0.75,1.0,1.25,1.5};' in controls,"75/100/125/150 UI scale contract missing"
assert 'editor_->setZoomFactor' in controls,"UI scale control no longer drives editor zoom"
print("V3 flagship GUI identity/scale contracts PASS")

# Visual-comfort invariants for the flagship composition.
assert 'rect(466,288,308,12)' in controls,"ANALOG ENGINE group label drifted into module-title row"
assert 'setFont(VSTGUI::kNormalFont,8.8,VSTGUI::kBoldFace)' in controls,"ANALOG ENGINE label lost 75% readability floor"
assert 'setFont(VSTGUI::kNormalFont,8.8,0)' in controls,"V3 version label lost 75% readability floor"
for legend in [
    'label("LabelFixed120","FIXED 120 Hz",8.6,false,true)',
    'label("LabelDbfs","dBFS",8.8,false,true)',
    'label("LabelIps","ips",8.8,false,true)'
]:
    assert legend in controller,f"small-legibility contract missing: {legend}"
print("V3 visual-comfort hierarchy contracts PASS")
