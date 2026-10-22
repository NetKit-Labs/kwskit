#!/usr/bin/env python3
"""Export the maintained project README to docs/README.pdf."""
import argparse
import hashlib
from html import escape
from io import BytesIO
from pathlib import Path
import re

from markdown_it import MarkdownIt
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    SimpleDocTemplate,
    Paragraph,
    Preformatted,
    Spacer,
    Table,
    TableStyle,
    HRFlowable,
    KeepTogether,
)

ROOT = Path(__file__).resolve().parents[1]
WIDTH = 7.0 * inch
INK = colors.HexColor("#18283B")
ACCENT = colors.HexColor("#126578")
STYLES = getSampleStyleSheet()
STYLES.add(
    ParagraphStyle(
        "TitleKWS", fontName="Helvetica-Bold", fontSize=22, leading=26, textColor=INK, spaceAfter=16
    )
)
STYLES.add(
    ParagraphStyle(
        "HeadingKWS",
        fontName="Helvetica-Bold",
        fontSize=13,
        leading=17,
        textColor=ACCENT,
        spaceBefore=13,
        spaceAfter=7,
        keepWithNext=True,
    )
)
STYLES.add(
    ParagraphStyle(
        "BodyKWS",
        fontName="Helvetica",
        fontSize=9.3,
        leading=13.2,
        textColor=INK,
        spaceAfter=7,
        allowWidows=0,
    )
)
STYLES.add(ParagraphStyle("LeadCodeKWS", parent=STYLES["BodyKWS"], keepWithNext=True))
STYLES.add(
    ParagraphStyle("CellKWS", parent=STYLES["BodyKWS"], fontSize=8.1, leading=10.8, spaceAfter=0)
)
STYLES.add(
    ParagraphStyle(
        "CodeKWS",
        fontName="Courier",
        fontSize=8.2,
        leading=11.5,
        textColor=INK,
        spaceBefore=4,
        spaceAfter=9,
        backColor=colors.HexColor("#F1F5F8"),
        borderPadding=7,
    )
)


def ascii_text(text):
    return text.translate(
        str.maketrans(
            {
                "\u201c": '"',
                "\u201d": '"',
                "\u2018": "'",
                "\u2019": "'",
                "\u2013": "-",
                "\u2014": "-",
                "\u2011": "-",
                "\u2192": "->",
                "\u00d7": "x",
                "\u00b5": "u",
                "\u00a0": " ",
            }
        )
    )


def inline(children):
    result = []
    for token in children or []:
        if token.type in ("text", "code_inline"):
            value = escape(ascii_text(token.content))
            result.append(
                '<font face="Courier">' + value + "</font>"
                if token.type == "code_inline"
                else value
            )
        elif token.type in ("softbreak", "hardbreak"):
            result.append(" " if token.type == "softbreak" else "<br/>")
        elif token.type == "strong_open":
            result.append("<b>")
        elif token.type == "strong_close":
            result.append("</b>")
        elif token.type == "em_open":
            result.append("<i>")
        elif token.type == "em_close":
            result.append("</i>")
        elif token.type == "link_open":
            href = token.attrGet("href")
            if not href.startswith(("http://", "https://", "#")):
                href = "https://github.com/NetKit-Labs/KWS_Arm_CortexM4F/blob/main/" + href
            result.append('<a color="#126578" href="' + escape(href, quote=True) + '">')
        elif token.type == "link_close":
            result.append("</a>")
        elif token.type == "image":
            result.append(escape(ascii_text(token.content)))
    return "".join(result)


def footer(canvas, document):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor("#CAD7E0"))
    canvas.line(0.75 * inch, 0.56 * inch, 7.75 * inch, 0.56 * inch)
    canvas.setFont("Helvetica", 8)
    canvas.setFillColor(INK)
    canvas.drawString(0.75 * inch, 0.38 * inch, "KWS on ARM Cortex-M4F | Project guide")
    canvas.drawRightString(7.75 * inch, 0.38 * inch, str(document.page))
    canvas.restoreState()


def mermaid_table(source):
    """Keep every node/edge in the README's diagram, without a separate synopsis.

    Support the quoted nodes and directed links used by this guide. Fail on new
    Mermaid syntax so a future diagram edit cannot silently lose PDF content.
    """
    lines = source.strip().splitlines()
    if not lines or lines[0].strip() != "flowchart LR":
        raise ValueError("Unsupported Mermaid diagram; update the PDF renderer")
    node = r'([A-Za-z_][A-Za-z_0-9]*)(?:\["([^"\n]+)"\])?'
    edge = re.compile(node + r'\s+(-->|-\.\s+"([^"\n]+)"\s+\.->)\s+' + node)
    labels = {}
    connections = []
    for line in lines[1:]:
        match = edge.fullmatch(line.strip())
        if not match:
            raise ValueError(f"Unsupported Mermaid connection: {line.strip()}")
        start, start_label, _, signal, end, end_label = match.groups()
        for name, label in ((start, start_label), (end, end_label)):
            if label is not None:
                if name in labels and labels[name] != label:
                    raise ValueError(f"Conflicting Mermaid node: {name}")
                labels[name] = label
        connections.append((start, signal or "Flow", end))
    if not connections:
        raise ValueError("Empty Mermaid diagram")
    rows = [["From", "Connection", "To"]]
    for start, signal, end in connections:
        rows.append([labels[start], signal, labels[end]])
    table = Table(
        [
            [Paragraph(escape(ascii_text(value)), STYLES["CellKWS"]) for value in row]
            for row in rows
        ],
        colWidths=[WIDTH * 0.39, WIDTH * 0.22, WIDTH * 0.39],
        repeatRows=1,
        hAlign="LEFT",
    )
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#DDEBF0")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LINEBELOW", (0, 0), (-1, 0), 0.6, ACCENT),
                ("LINEBELOW", (0, 1), (-1, -1), 0.25, colors.HexColor("#D8E1E7")),
                ("LEFTPADDING", (0, 0), (-1, -1), 6),
                ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                ("TOPPADDING", (0, 0), (-1, -1), 4),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
            ]
        )
    )
    return table


def render():
    source = (ROOT / "README.md").read_bytes()
    tokens = MarkdownIt("commonmark").enable("table").parse(source.decode("utf-8"))
    story = []
    index = 0
    in_list = 0
    while index < len(tokens):
        token = tokens[index]
        if token.type == "heading_open":
            style = STYLES["TitleKWS" if token.tag == "h1" else "HeadingKWS"]
            story.append(Paragraph(inline(tokens[index + 1].children), style))
            index += 3
            continue
        if token.type in ("bullet_list_open", "ordered_list_open"):
            in_list += 1
        elif token.type in ("bullet_list_close", "ordered_list_close"):
            in_list -= 1
        elif token.type == "paragraph_open":
            text = inline(tokens[index + 1].children)
            if in_list:
                text = "- " + text
            style = (
                STYLES["LeadCodeKWS"]
                if index + 3 < len(tokens) and tokens[index + 3].type == "fence"
                else STYLES["BodyKWS"]
            )
            story.append(Paragraph(text, style))
            index += 3
            continue
        elif token.type == "fence":
            code = token.content.rstrip()
            if token.info.strip() == "mermaid":
                story.extend([mermaid_table(code), Spacer(1, 9)])
                index += 1
                continue
            block = [Preformatted(ascii_text(code), STYLES["CodeKWS"], maxLineLength=99)]
            if (
                story
                and isinstance(story[-1], Paragraph)
                and story[-1].style is STYLES["LeadCodeKWS"]
            ):
                block.insert(0, story.pop())
            story.append(KeepTogether(block))
        elif token.type == "table_open":
            rows = []
            row = []
            index += 1
            while tokens[index].type != "table_close":
                cell = tokens[index]
                if cell.type == "tr_open":
                    row = []
                elif cell.type == "inline":
                    row.append(Paragraph(inline(cell.children), STYLES["CellKWS"]))
                elif cell.type == "tr_close":
                    rows.append(row)
                index += 1
            if len(rows[0]) == 2:
                proportions = [0.48, 0.52]
            elif rows[0][0].getPlainText() == "Allocation":
                proportions = [0.28, 0.22, 0.50]
            else:
                proportions = [0.20, 0.56, 0.24]
            widths = [WIDTH * fraction for fraction in proportions]
            table = Table(rows, colWidths=widths, repeatRows=1, hAlign="LEFT")
            table.setStyle(
                TableStyle(
                    [
                        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#DDEBF0")),
                        ("VALIGN", (0, 0), (-1, -1), "TOP"),
                        ("LINEBELOW", (0, 0), (-1, 0), 0.6, ACCENT),
                        ("LINEBELOW", (0, 1), (-1, -1), 0.25, colors.HexColor("#D8E1E7")),
                        ("LEFTPADDING", (0, 0), (-1, -1), 6),
                        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                        ("TOPPADDING", (0, 0), (-1, -1), 5),
                        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
                    ]
                )
            )
            story.extend([table, Spacer(1, 9)])
        elif token.type == "hr":
            story.append(HRFlowable(width="100%", thickness=0.5, color=ACCENT))
        index += 1
    output = BytesIO()
    document = SimpleDocTemplate(
        output,
        pagesize=(8.5 * inch, 11 * inch),
        leftMargin=0.75 * inch,
        rightMargin=0.75 * inch,
        topMargin=0.65 * inch,
        bottomMargin=0.75 * inch,
        title="Real-time keyword spotting on ARM Cortex-M4F",
        author="James Lavrenz",
        subject="Source: README.md; SHA-256: " + hashlib.sha256(source).hexdigest(),
        invariant=1,
        pageCompression=0,
    )
    document.build(story, onFirstPage=footer, onLaterPages=footer)
    return output.getvalue()


def export():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true", help="Check freshness without writing files"
    )
    args = parser.parse_args()
    content = render()
    tracked = ROOT / "docs/README.pdf"
    if args.check:
        if not tracked.exists() or tracked.read_bytes() != content:
            parser.exit(
                1, "docs/README.pdf is stale; run scripts/export_docs.py with the docs venv\n"
            )
        print("PASS: docs/README.pdf matches README.md and the current exporter")
    else:
        output = ROOT / "output/pdf/README.pdf"
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(content)
        tracked.write_bytes(content)
        print("Updated docs/README.pdf from README.md")


if __name__ == "__main__":
    export()
