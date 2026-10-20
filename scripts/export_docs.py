#!/usr/bin/env python3
"""Export the maintained project README to docs/README.pdf."""
from html import escape
from pathlib import Path
import shutil

from markdown_it import MarkdownIt
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Preformatted, Spacer, Table, TableStyle,
    HRFlowable, KeepTogether,
)

ROOT = Path(__file__).resolve().parents[1]
WIDTH = 7.0 * inch
INK = colors.HexColor('#18283B')
ACCENT = colors.HexColor('#126578')
STYLES = getSampleStyleSheet()
STYLES.add(ParagraphStyle('TitleKWS', fontName='Helvetica-Bold', fontSize=22,
                          leading=26, textColor=INK, spaceAfter=16))
STYLES.add(ParagraphStyle('HeadingKWS', fontName='Helvetica-Bold', fontSize=13,
                          leading=17, textColor=ACCENT, spaceBefore=13,
                          spaceAfter=7, keepWithNext=True))
STYLES.add(ParagraphStyle('BodyKWS', fontName='Helvetica', fontSize=9.3,
                          leading=13.2, textColor=INK, spaceAfter=7))
STYLES.add(ParagraphStyle('LeadCodeKWS', parent=STYLES['BodyKWS'], keepWithNext=True))
STYLES.add(ParagraphStyle('CellKWS', parent=STYLES['BodyKWS'], fontSize=8.1,
                          leading=10.8, spaceAfter=0))
STYLES.add(ParagraphStyle('CodeKWS', fontName='Courier', fontSize=8.2,
                          leading=11.5, textColor=INK, spaceBefore=4,
                          spaceAfter=9, backColor=colors.HexColor('#F1F5F8'),
                          borderPadding=7))


def ascii_text(text):
    return text.translate(str.maketrans({
        '\u201c': '"', '\u201d': '"', '\u2018': "'", '\u2019': "'",
        '\u2013': '-', '\u2014': '-', '\u2011': '-', '\u2192': '->',
        '\u00d7': 'x', '\u00b5': 'u', '\u00a0': ' ',
    }))


def inline(children):
    result = []
    for token in children or []:
        if token.type in ('text', 'code_inline'):
            value = escape(ascii_text(token.content))
            result.append('<font face="Courier">' + value + '</font>'
                          if token.type == 'code_inline' else value)
        elif token.type in ('softbreak', 'hardbreak'):
            result.append(' ' if token.type == 'softbreak' else '<br/>')
        elif token.type == 'strong_open': result.append('<b>')
        elif token.type == 'strong_close': result.append('</b>')
        elif token.type == 'em_open': result.append('<i>')
        elif token.type == 'em_close': result.append('</i>')
        elif token.type == 'link_open':
            href = token.attrGet('href')
            if not href.startswith(('http://', 'https://', '#')):
                href = 'https://github.com/jameslavrenz/KWS_Arm_CortexM4F/blob/main/' + href
            result.append('<a color="#126578" href="' + escape(href, quote=True) + '">')
        elif token.type == 'link_close': result.append('</a>')
        elif token.type == 'image': result.append(escape(ascii_text(token.content)))
    return ''.join(result)


def footer(canvas, document):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor('#CAD7E0'))
    canvas.line(0.75*inch, 0.56*inch, 7.75*inch, 0.56*inch)
    canvas.setFont('Helvetica', 8)
    canvas.setFillColor(INK)
    canvas.drawString(0.75*inch, 0.38*inch, 'KWS on ARM Cortex-M4F | Project guide')
    canvas.drawRightString(7.75*inch, 0.38*inch, str(document.page))
    canvas.restoreState()


def export():
    tokens = MarkdownIt('commonmark').enable('table').parse((ROOT/'README.md').read_text())
    story = []
    index = 0
    in_list = 0
    while index < len(tokens):
        token = tokens[index]
        if token.type == 'heading_open':
            style = STYLES['TitleKWS' if token.tag == 'h1' else 'HeadingKWS']
            story.append(Paragraph(inline(tokens[index+1].children), style))
            index += 3
            continue
        if token.type in ('bullet_list_open', 'ordered_list_open'): in_list += 1
        elif token.type in ('bullet_list_close', 'ordered_list_close'): in_list -= 1
        elif token.type == 'paragraph_open':
            text = inline(tokens[index+1].children)
            if in_list: text = '- ' + text
            style = STYLES['LeadCodeKWS'] if index+3 < len(tokens) and tokens[index+3].type == 'fence' else STYLES['BodyKWS']
            story.append(Paragraph(text, style))
            index += 3
            continue
        elif token.type == 'fence':
            code = token.content.rstrip()
            if token.info.strip() == 'mermaid':
                code = ('PDM / DMA -> interrupt -> circular audio buffer\n'
                        '  -> frame overlap / FFT / log-mel -> int8 model\n'
                        '  -> confidence filter -> confirmed event -> LED\n'
                        'Main loop: scheduling, recovery, nonblocking telemetry')
            story.append(KeepTogether([Preformatted(ascii_text(code), STYLES['CodeKWS'], maxLineLength=99)]))
        elif token.type == 'table_open':
            rows = []
            row = []
            index += 1
            while tokens[index].type != 'table_close':
                cell = tokens[index]
                if cell.type == 'tr_open': row = []
                elif cell.type == 'inline': row.append(Paragraph(inline(cell.children), STYLES['CellKWS']))
                elif cell.type == 'tr_close': rows.append(row)
                index += 1
            widths = [WIDTH*.48, WIDTH*.52] if len(rows[0]) == 2 else [WIDTH*.2, WIDTH*.56, WIDTH*.24]
            table = Table(rows, colWidths=widths, repeatRows=1, hAlign='LEFT')
            table.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#DDEBF0')),
                ('VALIGN', (0, 0), (-1, -1), 'TOP'),
                ('LINEBELOW', (0, 0), (-1, 0), .6, ACCENT),
                ('LINEBELOW', (0, 1), (-1, -1), .25, colors.HexColor('#D8E1E7')),
                ('LEFTPADDING', (0, 0), (-1, -1), 6),
                ('RIGHTPADDING', (0, 0), (-1, -1), 6),
                ('TOPPADDING', (0, 0), (-1, -1), 5),
                ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
            ]))
            story.extend([table, Spacer(1, 9)])
        elif token.type == 'hr':
            story.append(HRFlowable(width='100%', thickness=.5, color=ACCENT))
        index += 1
    output = ROOT / 'output/pdf/README.pdf'
    output.parent.mkdir(parents=True, exist_ok=True)
    document = SimpleDocTemplate(str(output), pagesize=(8.5*inch, 11*inch),
                                leftMargin=.75*inch, rightMargin=.75*inch,
                                topMargin=.65*inch, bottomMargin=.75*inch,
                                title='Real-time keyword spotting on ARM Cortex-M4F',
                                author='James Lavrenz')
    document.build(story, onFirstPage=footer, onLaterPages=footer)
    shutil.copyfile(output, ROOT / 'docs/README.pdf')
    print('Updated docs/README.pdf from README.md')


if __name__ == '__main__':
    export()
