#!/usr/bin/env python3
import argparse,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def rc(cx,cy,w,h): return [round(cx-w/2),round(cy-h/2),w,h]
def osz(r):
    x,y,w,h=r
    return f"{x},{y}",f"{w},{h}"
def view(name,r,mouse=None):
    o,s=osz(r)
    extra="" if mouse is None else f' mouse-enabled="{str(mouse).lower()}"'
    return f'    <view class="CView" custom-view-name="{name}" origin="{o}" size="{s}"{extra}/>'
def centered(cx,y,w,h): return [round(cx-w/2),y,w,h]

def generate(L):
    top=L["top"]; m=L["modules"]; rows=L["rows"]
    c=L["controls"]; sz=L["sizes"]; sel=L["selectors"]; tog=L["toggles"]

    out=[
      '<?xml version="1.0" encoding="UTF-8"?>',
      '<vstgui-ui-description version="1">',
      '  <colors><color name="Background" rgba="#101217FF"/></colors>',
      f'  <template name="view" class="CViewContainer" origin="0,0" size="{L["canvas"]["w"]},{L["canvas"]["h"]}" transparent="false" background-color="Background">',
      f'    <view class="CView" custom-view-name="HardwareFaceplate" origin="0,0" size="{L["canvas"]["w"]},{L["canvas"]["h"]}" mouse-enabled="false"/>'
    ]

    labels=[
      ("LabelVUSource",top["VUSourceLabel"]),("LabelVURef",top["VURefLabel"]),
      ("LabelMixTitle",top["MixTitle"]),("LabelQuality",top["QualityLabel"]),("LabelBypass",top["BypassLabel"]),
      ("LabelInputTitle",centered(m["Input"],rows["moduleTitleY"],140,22)),
      ("LabelConsoleTitle",centered(m["Console"],rows["moduleTitleY"],140,22)),
      ("LabelTubeTitle",centered(m["Tube"],rows["moduleTitleY"],140,22)),
      ("LabelTapeTitle",centered(m["Tape"],rows["moduleTitleY"],140,22)),
      ("LabelGlueTitle",centered(m["Glue"],rows["moduleTitleY"],140,22)),
      ("LabelVinylTitle",centered(m["Vinyl"],rows["moduleTitleY"],140,22)),
      ("LabelStereoTitle",centered(m["Stereo"],rows["moduleTitleY"],140,22)),
      ("LabelOutputTitle",centered(m["Output"],rows["moduleTitleY"],140,22)),
      ("LabelInputGain",centered(c["Input"][0],rows["mainLabelY"],110,16)),
      ("LabelRefLevel",sel["InputRefLabel"]),("LabelDbfs",sel["InputDbfs"]),
      ("LabelDrive",centered(c["ConsoleDrive"][0],rows["mainLabelY"],92,16)),
      ("LabelMode",sel["ConsoleModeLabel"]),
      ("LabelConsoleNoise",centered(c["ConsoleNoise"][0],rows["smallLowerLabelY"],62,16)),
      ("LabelTubeAmount",centered(c["TubeAmount"][0],rows["mainLabelY"],92,16)),
      ("LabelVoice",sel["TubeVoiceLabel"]),
      ("LabelTapeAmount",centered(c["TapeAmount"][0],rows["mainLabelY"],92,16)),
      ("LabelSpeed",sel["TapeSpeedLabel"]),("LabelIps",sel["TapeIps"]),
      ("LabelStability",centered(c["TapeStability"][0],rows["smallLowerLabelY"],84,16)),
      ("LabelHiss",centered(c["TapeHiss"][0],rows["smallLowerLabelY"],50,16)),
      ("LabelGlueAmount",centered(c["GlueAmount"][0],rows["mainLabelY"],92,16)),
      ("LabelResponse",centered(c["GlueCharacter"][0],rows["smallUpperLabelY"],82,16)),
      ("LabelColor",centered(c["VinylCharacter"][0],rows["mainLabelY"],92,16)),
      ("LabelWear",centered(c["VinylWear"][0],rows["smallUpperLabelY"],58,16)),
      ("LabelSurface",centered(c["VinylNoise"][0],rows["smallUpperLabelY"],76,16)),
      ("LabelDepth",centered(c["Depth"][0],472,72,16)),
      ("LabelWidth",centered(c["Width"][0],600,72,16)),
      ("LabelLowMono",centered(c["LowMono"][0],rows["stereoLowLabelY"],96,16)),
      ("LabelFixed120",centered(c["LowMono"][0],rows["fixed120Y"],104,14)),
      ("LabelOutputGain",centered(c["Output"][0],rows["mainLabelY"],110,16)),
      ("LabelLevelMatch",sel["LevelMatchLabel"])
    ]
    if "Crosstalk" in c:
        noise_index=next(i for i,(n,_) in enumerate(labels) if n=="LabelConsoleNoise")
        labels.insert(noise_index,("LabelCrosstalk",centered(c["Crosstalk"][0],rows["smallLowerLabelY"],78,16)))
    out.extend(view(n,r,False) for n,r in labels)

    for n in ["BrandLogo","VULeft","ClipL","VURight","ClipR"]:
        out.append(view(n,top[n],False))
    out.append(view("UIScale",top["UIScale"]))

    out += [
      view("VUSourceSelector",top["VUSource"]),
      view("QualitySelector",top["Quality"]),
      view("Bypass",top["Bypass"])
    ]

    def knob(name,size): return view(name,rc(*c[name],size,size))

    out += [
      knob("Input",sz["main"]),view("InputRefSelector",sel["InputRef"]),
      view("ConsolePower",tog["ConsolePower"]),knob("ConsoleDrive",sz["main"]),
      view("ConsoleModeSelector",sel["ConsoleMode"]),
    ]
    if "Crosstalk" in c:
        out.append(knob("Crosstalk",sz["small"]))
    out += [
      knob("ConsoleNoise",sz["small"]),
      view("TubePower",tog["TubePower"]),knob("TubeAmount",sz["main"]),view("TubeVoiceSelector",sel["TubeVoice"]),
      view("TapePower",tog["TapePower"]),knob("TapeAmount",sz["main"]),view("TapeSpeedSelector",sel["TapeSpeed"]),
      knob("TapeStability",sz["small"]),knob("TapeHiss",sz["small"]),
      view("GluePower",tog["GluePower"]),knob("GlueAmount",sz["main"]),knob("GlueCharacter",sz["small"]),
      view("VinylPower",tog["VinylPower"]),knob("VinylCharacter",sz["main"]),knob("VinylWear",sz["small"]),knob("VinylNoise",sz["small"]),
      knob("Depth",sz["stereo"]),knob("Width",sz["stereo"]),knob("LowMono",sz["small"]),
      knob("Output",sz["main"]),view("LevelMatch",sel["LevelMatch"])
    ]

    out += ['  </template>','</vstgui-ui-description>']
    return "\n".join(out)+"\n"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--layout",default=ROOT/"resource/mixengine.layout.json")
    ap.add_argument("--output",default=ROOT/"resource/mixengine.uidesc")
    a=ap.parse_args()
    L=json.loads(Path(a.layout).read_text(encoding="utf-8"))
    Path(a.output).write_text(generate(L),encoding="utf-8",newline="\n")
    print(f"Generated {a.output}")

if __name__=="__main__":
    main()
