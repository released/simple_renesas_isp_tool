"""Render the Mermaid-expanded F13 update guide as a reviewed A4 PDF."""

from __future__ import annotations

import argparse
import html
import re
from pathlib import Path

from PIL import Image as PillowImage
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4, landscape, portrait
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    CondPageBreak,
    Frame,
    Image,
    KeepTogether,
    NextPageTemplate,
    PageBreak,
    PageTemplate,
    Paragraph,
    Preformatted,
    Spacer,
    Table,
    TableStyle,
)


FONT_REGULAR = Path(r"C:\Windows\Fonts\msjh.ttc")
FONT_BOLD = Path(r"C:\Windows\Fonts\msjhbd.ttc")
FONT_MONO = Path(r"C:\Windows\Fonts\consola.ttf")
LANDSCAPE_PAGE = landscape(A4)
PORTRAIT_PAGE = portrait(A4)
LEFT_MARGIN = 14 * mm
RIGHT_MARGIN = 14 * mm
TOP_MARGIN = 14 * mm
BOTTOM_MARGIN = 14 * mm


def register_fonts() -> None:
    if not FONT_REGULAR.exists() or not FONT_BOLD.exists():
        raise RuntimeError("Microsoft JhengHei fonts are required for the PDF.")
    pdfmetrics.registerFont(
        TTFont("JHei", str(FONT_REGULAR), subfontIndex=0)
    )
    pdfmetrics.registerFont(
        TTFont("JHei-Bold", str(FONT_BOLD), subfontIndex=0)
    )
    pdfmetrics.registerFontFamily(
        "JHei", normal="JHei", bold="JHei-Bold",
        italic="JHei", boldItalic="JHei-Bold"
    )
    if FONT_MONO.exists():
        pdfmetrics.registerFont(TTFont("GuideMono", str(FONT_MONO)))
    else:
        pdfmetrics.registerFont(
            TTFont("GuideMono", str(FONT_REGULAR), subfontIndex=0)
        )


def inline_markup(value: str) -> str:
    escaped = html.escape(value.strip())
    escaped = re.sub(
        r"\[([^\]]+)\]\(([^)]+)\)",
        r'<u><font color="#175A9C">\1</font></u>',
        escaped,
    )
    escaped = re.sub(
        r"`([^`]+)`", r'<font name="GuideMono">\1</font>', escaped
    )
    escaped = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", escaped)
    return escaped


def make_styles() -> dict[str, ParagraphStyle]:
    sample = getSampleStyleSheet()
    base = ParagraphStyle(
        "GuideBody",
        parent=sample["BodyText"],
        fontName="JHei",
        fontSize=9.2,
        leading=13.4,
        textColor=colors.HexColor("#1F2933"),
        spaceAfter=4,
        wordWrap="CJK",
    )
    return {
        "body": base,
        "title": ParagraphStyle(
            "GuideTitle",
            parent=base,
            fontName="JHei-Bold",
            fontSize=21,
            leading=27,
            alignment=TA_CENTER,
            textColor=colors.HexColor("#17365D"),
            spaceAfter=14,
            keepWithNext=True,
        ),
        "h2": ParagraphStyle(
            "GuideH2",
            parent=base,
            fontName="JHei-Bold",
            fontSize=15,
            leading=20,
            textColor=colors.HexColor("#17365D"),
            spaceBefore=10,
            spaceAfter=6,
            keepWithNext=True,
        ),
        "h3": ParagraphStyle(
            "GuideH3",
            parent=base,
            fontName="JHei-Bold",
            fontSize=11.5,
            leading=16,
            textColor=colors.HexColor("#245B82"),
            spaceBefore=7,
            spaceAfter=4,
            keepWithNext=True,
        ),
        "bullet": ParagraphStyle(
            "GuideBullet",
            parent=base,
            leftIndent=13,
            firstLineIndent=-9,
            spaceAfter=2,
        ),
        "quote": ParagraphStyle(
            "GuideQuote",
            parent=base,
            leftIndent=10,
            rightIndent=10,
            textColor=colors.HexColor("#4A5568"),
            backColor=colors.HexColor("#EEF3F7"),
            borderColor=colors.HexColor("#91A4B7"),
            borderWidth=0.5,
            borderPadding=6,
            spaceBefore=4,
            spaceAfter=7,
        ),
        "code": ParagraphStyle(
            "GuideCode",
            parent=base,
            fontName="GuideMono",
            fontSize=7.5,
            leading=10,
            leftIndent=6,
            rightIndent=6,
            backColor=colors.HexColor("#F4F6F8"),
            borderColor=colors.HexColor("#CCD5DD"),
            borderWidth=0.5,
            borderPadding=6,
            spaceBefore=4,
            spaceAfter=7,
        ),
        "table_header": ParagraphStyle(
            "GuideTableHeader",
            parent=base,
            fontName="JHei-Bold",
            fontSize=8.2,
            leading=11,
            textColor=colors.white,
        ),
        "table_cell": ParagraphStyle(
            "GuideTableCell",
            parent=base,
            fontSize=8.1,
            leading=11,
            spaceAfter=0,
        ),
    }


def split_table_row(line: str) -> list[str]:
    return [cell.strip() for cell in line.strip().strip("|").split("|")]


def table_flowable(lines: list[str], styles: dict[str, ParagraphStyle], width: float) -> Table:
    rows = [split_table_row(line) for line in lines]
    if len(rows) >= 2 and all(re.fullmatch(r":?-{3,}:?", cell) for cell in rows[1]):
        rows.pop(1)
    column_count = max(len(row) for row in rows)
    normalized = [row + [""] * (column_count - len(row)) for row in rows]
    data = []
    for row_index, row in enumerate(normalized):
        style = styles["table_header"] if row_index == 0 else styles["table_cell"]
        data.append([Paragraph(inline_markup(cell), style) for cell in row])
    if column_count == 2:
        widths = [width * 0.31, width * 0.69]
    else:
        widths = [width / column_count] * column_count
    table = Table(data, colWidths=widths, repeatRows=1, hAlign="LEFT")
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#245B82")),
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#AAB7C4")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1),
         [colors.white, colors.HexColor("#F5F8FA")]),
    ]))
    return table


def diagram_image(path: Path, available_width: float, available_height: float):
    if not path.exists():
        raise FileNotFoundError(f"Diagram not found: {path}")
    with PillowImage.open(path) as source:
        pixel_width, pixel_height = source.size
    scale = min(
        available_width / float(pixel_width),
        available_height / float(pixel_height),
    )
    draw_width = pixel_width * scale
    draw_height = pixel_height * scale
    diagram = Image(str(path), width=draw_width, height=draw_height)
    diagram.hAlign = "CENTER"
    return diagram, float(pixel_width) / float(pixel_height)


def build_story(markdown_path: Path, styles: dict[str, ParagraphStyle]):
    lines = markdown_path.read_text(encoding="utf-8").splitlines()
    landscape_width = LANDSCAPE_PAGE[0] - LEFT_MARGIN - RIGHT_MARGIN
    landscape_height = (
        LANDSCAPE_PAGE[1] - TOP_MARGIN - BOTTOM_MARGIN - 8 * mm
    )
    portrait_width = PORTRAIT_PAGE[0] - LEFT_MARGIN - RIGHT_MARGIN
    portrait_height = PORTRAIT_PAGE[1] - TOP_MARGIN - BOTTOM_MARGIN - 8 * mm
    story = []
    paragraph_lines: list[str] = []
    last_heading_index: int | None = None
    last_heading_text = ""
    last_heading_style = styles["h3"]
    index = 0

    def flush_paragraph() -> None:
        if paragraph_lines:
            story.append(Paragraph(
                inline_markup(" ".join(part.strip() for part in paragraph_lines)),
                styles["body"],
            ))
            paragraph_lines.clear()

    while index < len(lines):
        line = lines[index]
        stripped = line.strip()
        if not stripped:
            flush_paragraph()
            index += 1
            continue
        if stripped.startswith("<!--"):
            flush_paragraph()
            while "-->" not in lines[index]:
                index += 1
                if index >= len(lines):
                    raise ValueError("Unterminated Markdown HTML comment")
            index += 1
            continue
        if stripped.startswith("```"):
            flush_paragraph()
            code_lines = []
            index += 1
            while index < len(lines) and not lines[index].strip().startswith("```"):
                code_lines.append(lines[index])
                index += 1
            story.append(Preformatted("\n".join(code_lines), styles["code"]))
            index += 1
            continue
        image_match = re.fullmatch(r"!\[[^]]*\]\(([^)]+)\)", stripped)
        if image_match:
            flush_paragraph()
            relative = image_match.group(1).replace("\\", "/")
            image_path = (markdown_path.parent / relative).resolve()
            with PillowImage.open(image_path) as source:
                aspect_ratio = float(source.width) / float(source.height)
            if aspect_ratio <= 1.05:
                preamble = []
                if (last_heading_index is not None and
                        len(story) - last_heading_index <= 3):
                    preamble = story[last_heading_index:]
                    del story[last_heading_index:]
                if (len(story) >= 2 and
                        isinstance(story[-2], NextPageTemplate) and
                        isinstance(story[-1], PageBreak)):
                    story.pop()
                    story.pop()
                story.append(NextPageTemplate("portrait"))
                story.append(PageBreak())
                if preamble:
                    story.extend(preamble)
                elif last_heading_text:
                    story.append(Paragraph(
                        inline_markup(last_heading_text), last_heading_style
                    ))
                diagram, unused_ratio = diagram_image(
                    image_path, portrait_width, portrait_height - 26 * mm
                )
                story.append(KeepTogether([diagram, Spacer(1, 7)]))
                story.append(NextPageTemplate("landscape"))
                story.append(PageBreak())
            else:
                diagram, unused_ratio = diagram_image(
                    image_path, landscape_width, landscape_height
                )
                if diagram.drawHeight > landscape_height * 0.68:
                    preamble = []
                    if (last_heading_index is not None and
                            len(story) - last_heading_index <= 3):
                        preamble = story[last_heading_index:]
                        del story[last_heading_index:]
                    if not story or not isinstance(story[-1], PageBreak):
                        story.append(PageBreak())
                    if preamble:
                        story.extend(preamble)
                    elif last_heading_text:
                        story.append(Paragraph(
                            inline_markup(last_heading_text), last_heading_style
                        ))
                    diagram, unused_ratio = diagram_image(
                        image_path, landscape_width,
                        landscape_height - 26 * mm
                    )
                    story.append(KeepTogether([diagram, Spacer(1, 7)]))
                else:
                    story.extend([
                        CondPageBreak(diagram.drawHeight + 10),
                        KeepTogether([diagram, Spacer(1, 7)]),
                    ])
            last_heading_index = None
            index += 1
            continue
        if stripped.startswith("|") and "|" in stripped[1:]:
            flush_paragraph()
            table_lines = []
            while index < len(lines) and lines[index].strip().startswith("|"):
                table_lines.append(lines[index])
                index += 1
            story.append(table_flowable(table_lines, styles, landscape_width))
            story.append(Spacer(1, 7))
            continue
        heading = re.match(r"^(#{1,4})\s+(.+)$", stripped)
        if heading:
            flush_paragraph()
            level = len(heading.group(1))
            style_name = "title" if level == 1 else ("h2" if level == 2 else "h3")
            last_heading_index = len(story)
            last_heading_text = heading.group(2)
            last_heading_style = styles[style_name]
            story.append(Paragraph(inline_markup(heading.group(2)), styles[style_name]))
            index += 1
            continue
        if stripped.startswith(">"):
            flush_paragraph()
            quote_lines = []
            while index < len(lines) and lines[index].strip().startswith(">"):
                quote_lines.append(lines[index].strip().lstrip(">").strip())
                index += 1
            story.append(Paragraph(inline_markup(" ".join(quote_lines)), styles["quote"]))
            continue
        list_match = re.match(r"^(?:- |\* |\d+\. )(.*)$", stripped)
        if list_match:
            flush_paragraph()
            marker = "•"
            numbered = re.match(r"^(\d+)\. ", stripped)
            if numbered:
                marker = numbered.group(1) + "."
            item_parts = [list_match.group(1)]
            index += 1
            while index < len(lines):
                continuation = lines[index]
                if not continuation.strip() or re.match(
                    r"^(?:\s*)(?:- |\* |\d+\. )", continuation
                ):
                    break
                if continuation.startswith("  "):
                    item_parts.append(continuation.strip())
                    index += 1
                else:
                    break
            story.append(Paragraph(
                f"{marker} {inline_markup(' '.join(item_parts))}",
                styles["bullet"],
            ))
            continue
        paragraph_lines.append(line)
        index += 1

    flush_paragraph()
    return story


def draw_page(canvas, doc) -> None:
    page_width, unused_page_height = canvas._pagesize
    canvas.saveState()
    canvas.setFont("JHei", 7.5)
    canvas.setFillColor(colors.HexColor("#5F6B76"))
    canvas.drawString(
        LEFT_MARGIN,
        7 * mm,
        doc.guide_footer,
    )
    canvas.drawRightString(
        page_width - RIGHT_MARGIN,
        7 * mm,
        f"Page {doc.page}",
    )
    canvas.restoreState()


def create_pdf(markdown_path: Path, output_path: Path, title: str, footer: str) -> None:
    register_fonts()
    styles = make_styles()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    document = BaseDocTemplate(
        str(output_path),
        pagesize=LANDSCAPE_PAGE,
        leftMargin=LEFT_MARGIN,
        rightMargin=RIGHT_MARGIN,
        topMargin=TOP_MARGIN,
        bottomMargin=BOTTOM_MARGIN,
        title=title,
        author="FirmwareUpgradeTool project",
        subject="Firmware upgrade workflow and safety boundaries",
    )
    document.guide_footer = footer
    landscape_frame = Frame(
        LEFT_MARGIN,
        BOTTOM_MARGIN + 6 * mm,
        LANDSCAPE_PAGE[0] - LEFT_MARGIN - RIGHT_MARGIN,
        LANDSCAPE_PAGE[1] - TOP_MARGIN - BOTTOM_MARGIN - 6 * mm,
        id="guide_landscape_frame",
        leftPadding=0,
        rightPadding=0,
        topPadding=0,
        bottomPadding=0,
    )
    portrait_frame = Frame(
        LEFT_MARGIN,
        BOTTOM_MARGIN + 6 * mm,
        PORTRAIT_PAGE[0] - LEFT_MARGIN - RIGHT_MARGIN,
        PORTRAIT_PAGE[1] - TOP_MARGIN - BOTTOM_MARGIN - 6 * mm,
        id="guide_portrait_frame",
        leftPadding=0,
        rightPadding=0,
        topPadding=0,
        bottomPadding=0,
    )
    document.addPageTemplates([
        PageTemplate(
            id="landscape", frames=[landscape_frame],
            onPage=draw_page, pagesize=LANDSCAPE_PAGE
        ),
        PageTemplate(
            id="portrait", frames=[portrait_frame],
            onPage=draw_page, pagesize=PORTRAIT_PAGE
        ),
    ])
    document.build(build_story(markdown_path, styles))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--title", default="RL78/F13 CAN Firmware Update Guide (zh-TW)")
    parser.add_argument("--footer", default="FirmwareUpgradeTool - F13 CAN")
    args = parser.parse_args()
    create_pdf(args.source.resolve(), args.output.resolve(), args.title, args.footer)
    print(args.output.resolve())


if __name__ == "__main__":
    main()
