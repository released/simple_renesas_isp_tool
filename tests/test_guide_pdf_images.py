"""Focused Markdown-image regression for the installed guide PDF renderer."""
from pathlib import Path
import importlib.util
import sys

from PIL import Image
import fitz


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    work = Path(sys.argv[1]).resolve()
    assert work.is_dir()
    spec = importlib.util.spec_from_file_location(
        "guide_pdf", root / "scripts/generate_f13_guide_pdf.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    cases = [
        ("![plain](diagrams/image.png)", "diagrams/image.png"),
        ('![title](diagrams/image.png "G16 I2C Boot receive and validation")', "diagrams/image.png"),
        ("![title](diagrams/image.png 'Single quoted title')", "diagrams/image.png"),
        ('![title](diagrams/image.png "Title (with parentheses)")', "diagrams/image.png"),
        ('![title](diagrams/image.png "Title with \\\"quoted\\\" text")', "diagrams/image.png"),
        ("![spaces](diagrams/with spaces(1).png)", "diagrams/with spaces(1).png"),
        ('![spaces](<diagrams/with spaces(1).png> "Title")', "diagrams/with spaces(1).png"),
        ('[![linked](diagrams/image.png "Title (wide)")](https://example.invalid/image.png)', "diagrams/image.png"),
        (r"![windows](diagrams\image.png)", "diagrams/image.png"),
    ]
    for markup, expected in cases:
        assert module.markdown_image_path(markup) == expected, markup
    for value in ["ordinary paragraph", "![missing](diagrams/image.png", "![empty]()"]:
        assert module.markdown_image_path(value) is None, value
    diagrams = work / "diagrams"
    diagrams.mkdir()
    for name in ["image.png", "with spaces(1).png"]:
        Image.new("RGB", (64, 32), "white").save(diagrams / name)
    markdown = work / "fixture.md"
    markdown.write_text("# Image titles\n\n" + "\n\n".join(m for m, _ in cases) + "\n", encoding="utf8")
    pdf = work / "fixture.pdf"
    module.create_pdf(markdown, pdf, "Image titles", "Fixture")
    with fitz.open(pdf) as document:
        assert len(document) > 0
        assert sum(len(page.get_image_info()) for page in document) == len(cases)
    print("PASS 12 Markdown-image parsing cases and all nine actual PDF image placements")


if __name__ == "__main__":
    main()
