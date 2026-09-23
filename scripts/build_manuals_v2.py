from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.lib import colors
from reportlab.lib.units import mm
import html, pathlib, re, sys

styles=getSampleStyleSheet()
styles.add(ParagraphStyle(name='V2Title',parent=styles['Title'],fontName='Helvetica-Bold',fontSize=22,leading=26,alignment=TA_CENTER,spaceAfter=8))
styles.add(ParagraphStyle(name='V2Sub',parent=styles['Normal'],fontName='Helvetica',fontSize=10.5,leading=14,alignment=TA_CENTER,textColor=colors.HexColor('#555555'),spaceAfter=16))
styles.add(ParagraphStyle(name='V2H',parent=styles['Heading1'],fontName='Helvetica-Bold',fontSize=13.5,leading=17,spaceBefore=10,spaceAfter=6))
styles.add(ParagraphStyle(name='V2Body',parent=styles['BodyText'],fontName='Helvetica',fontSize=9.6,leading=13.4,spaceAfter=5.5))
styles.add(ParagraphStyle(name='V2Bullet',parent=styles['BodyText'],fontName='Helvetica',fontSize=9.6,leading=13.4,leftIndent=12,firstLineIndent=-7,spaceAfter=3.5))
styles.add(ParagraphStyle(name='V2Footer',parent=styles['BodyText'],fontName='Helvetica',fontSize=8.3,leading=11,textColor=colors.HexColor('#666666'),alignment=TA_CENTER,spaceBefore=12))

HEADERS_DE={'ÜBERBLICK','SIGNALFLUSS','V2 - WAS IST NEU?','INPUT / LEVEL MATCH','REFERENCE LEVEL','CONSOLE','TUBE','TAPE','GLUE','VINYL','STEREO','QUALITY','AUTOMATION','INSTALLATION','WELCHE EDITION?','PRAXIS','VALIDIERUNG'}
HEADERS_EN={'OVERVIEW','SIGNAL FLOW','V2 - WHAT IS NEW?','INPUT / LEVEL MATCH','REFERENCE LEVEL','CONSOLE','TUBE','TAPE','GLUE','VINYL','STEREO','QUALITY','AUTOMATION','INSTALLATION','WHICH EDITION?','PRACTICAL STARTING POINTS','VALIDATION'}

def esc(s):
    return html.escape(s).replace('\\','&#92;')

def build(src,out,lang):
    lines=pathlib.Path(src).read_text(encoding='utf-8').splitlines()
    heads=HEADERS_DE if lang=='DE' else HEADERS_EN
    story=[Spacer(1,8*mm),Paragraph(esc(lines[0]),styles['V2Title']),Paragraph(esc(lines[1])+'<br/>'+esc(lines[2]),styles['V2Sub'])]
    for raw in lines[3:]:
        line=raw.strip()
        if not line:
            story.append(Spacer(1,2.2*mm))
        elif line in heads:
            story.append(Paragraph(esc(line),styles['V2H']))
        elif line.startswith('- '):
            story.append(Paragraph('• '+esc(line[2:]),styles['V2Bullet']))
        elif re.match(r'^\d+\. ',line):
            story.append(Paragraph(esc(line),styles['V2Bullet']))
        else:
            story.append(Paragraph(esc(line),styles['V2Body']))
    story.append(Paragraph('125A Audio Software - 125A MixEngine V2 - Version 2.0.0',styles['V2Footer']))
    doc=SimpleDocTemplate(str(out),pagesize=A4,rightMargin=17*mm,leftMargin=17*mm,topMargin=14*mm,bottomMargin=14*mm,title=lines[0],author='125A Audio Software')
    doc.build(story)

if __name__=='__main__':
    out_dir=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'dist/manuals')
    out_dir.mkdir(parents=True,exist_ok=True)
    build('docs/125A_MixEngine_V2_Manual_DE.txt',out_dir/'125A_MixEngine_V2_Bedienungsanleitung_DE.pdf','DE')
    build('docs/125A_MixEngine_V2_Manual_EN.txt',out_dir/'125A_MixEngine_V2_User_Manual_EN.pdf','EN')
