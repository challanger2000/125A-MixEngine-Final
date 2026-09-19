from pathlib import Path
import re
from xml.sax.saxutils import escape
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak

ROOT=Path(__file__).resolve().parents[1]
DOCS=ROOT/"docs"
ACCENT=colors.HexColor("#D71920")
DARK=colors.HexColor("#17191d")
MID=colors.HexColor("#51555b")
styles=getSampleStyleSheet()
styles.add(ParagraphStyle(name="Cover",parent=styles["Title"],fontName="Helvetica-Bold",fontSize=27,leading=31,textColor=DARK,alignment=1,spaceAfter=8))
styles.add(ParagraphStyle(name="Sub",parent=styles["Normal"],fontName="Helvetica",fontSize=11,leading=15,textColor=MID,alignment=1))
styles.add(ParagraphStyle(name="H1x",parent=styles["Heading1"],fontName="Helvetica-Bold",fontSize=17,leading=21,textColor=DARK,spaceBefore=8,spaceAfter=7))
styles.add(ParagraphStyle(name="Bodyx",parent=styles["BodyText"],fontName="Helvetica",fontSize=9.1,leading=12.8,textColor=DARK,spaceAfter=5))
styles.add(ParagraphStyle(name="Smallx",parent=styles["BodyText"],fontName="Helvetica",fontSize=7.2,leading=9.1,textColor=DARK))

def inline(s):
    s=escape(s)
    s=re.sub(r"\*\*(.+?)\*\*",r"<b>\1</b>",s)
    s=re.sub(r"`(.+?)`",r'<font name="Courier">\1</font>',s)
    return s

def footer(canvas,doc,label):
    canvas.saveState(); w,h=A4
    canvas.setStrokeColor(colors.HexColor("#d5d5d2")); canvas.setLineWidth(.4)
    canvas.line(17*mm,14*mm,w-17*mm,14*mm)
    canvas.setFont("Helvetica",7.4); canvas.setFillColor(MID)
    canvas.drawString(17*mm,9.5*mm,label)
    canvas.drawRightString(w-17*mm,9.5*mm,str(doc.page)); canvas.restoreState()

def parse_table(lines,i):
    rows=[]
    while i<len(lines) and lines[i].lstrip().startswith("|"):
        cells=[x.strip() for x in lines[i].strip().strip("|").split("|")]
        rows.append(cells); i+=1
    if len(rows)>=2 and all(set(x)<=set("-: ") for x in rows[1]):
        rows.pop(1)
    data=[[Paragraph(inline(c),styles["Smallx"]) for c in r] for r in rows]
    n=max(len(r) for r in rows)
    if n==5: widths=[29*mm,22*mm,30*mm,18*mm,80*mm]
    elif n==2: widths=[48*mm,131*mm]
    else: widths=[179*mm/n]*n
    t=Table(data,colWidths=widths,repeatRows=1,hAlign="LEFT")
    ts=[("VALIGN",(0,0),(-1,-1),"TOP"),("GRID",(0,0),(-1,-1),.35,colors.HexColor("#d2d2cf")),
        ("LEFTPADDING",(0,0),(-1,-1),3),("RIGHTPADDING",(0,0),(-1,-1),3),
        ("TOPPADDING",(0,0),(-1,-1),2.6),("BOTTOMPADDING",(0,0),(-1,-1),2.6),
        ("BACKGROUND",(0,0),(-1,0),DARK),("TEXTCOLOR",(0,0),(-1,0),colors.white)]
    for r in range(1,len(data)):
        if r%2==0: ts.append(("BACKGROUND",(0,r),(-1,r),colors.HexColor("#f7f7f5")))
    t.setStyle(TableStyle(ts)); return t,i

def render(src,out,label):
    lines=src.read_text(encoding="utf-8").splitlines()
    title=lines[0].lstrip("# ").strip()
    version=next((x for x in lines[1:6] if x.strip()),"Version 1.0.0")
    story=[Spacer(1,30*mm),Paragraph("125A",styles["Cover"]),Paragraph("MIXENGINE",styles["Cover"]),
           Spacer(1,3*mm),Paragraph(inline(title),styles["Sub"]),Paragraph(inline(version),styles["Sub"]),
           Spacer(1,17*mm),Paragraph("125A AUDIO SOFTWARE",styles["Sub"]),PageBreak()]
    i=1; para=[]
    def flush():
        nonlocal para
        if para:
            story.append(Paragraph(inline(" ".join(x.strip() for x in para)),styles["Bodyx"])); para=[]
    while i<len(lines):
        line=lines[i]
        if not line.strip(): flush(); i+=1; continue
        if line.startswith("## "):
            flush(); story.append(Paragraph(inline(line[3:].strip()),styles["H1x"])); i+=1; continue
        if line.lstrip().startswith("|"):
            flush(); t,i=parse_table(lines,i); story.append(t); story.append(Spacer(1,4*mm)); continue
        if line.startswith("- "):
            flush(); story.append(Paragraph("• "+inline(line[2:].strip()),styles["Bodyx"])); i+=1; continue
        if line.startswith("# "): i+=1; continue
        para.append(line); i+=1
    flush()
    doc=SimpleDocTemplate(str(out),pagesize=A4,leftMargin=16*mm,rightMargin=16*mm,topMargin=15*mm,bottomMargin=19*mm,
        title=title,author="125A Audio Software",subject="125A MixEngine v1.0.0")
    doc.build(story,onFirstPage=lambda c,d:footer(c,d,label),onLaterPages=lambda c,d:footer(c,d,label))

render(DOCS/"MANUAL_DE.md",DOCS/"125A_MixEngine_Bedienungsanleitung_DE.pdf","125A MixEngine - Bedienungsanleitung | v1.0.0")
render(DOCS/"MANUAL_EN.md",DOCS/"125A_MixEngine_User_Manual_EN.pdf","125A MixEngine - User Manual | v1.0.0")
print("PDF manuals generated")
