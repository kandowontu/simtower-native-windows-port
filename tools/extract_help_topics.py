#!/usr/bin/env python3
"""Turn HelpDeco/LibreOffice XHTML into searchable, offline help topics."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image


IMAGE_RE = re.compile(r"\{bml\s+([^}]+)\}", re.IGNORECASE)
SPACE_RE = re.compile(r"[ \t\r\f\v]+")
MARKER_RE = re.compile(r"^(?:[+$#]\s*\d*)+")


def clean_text(value: str) -> str:
    value = value.replace("\u00a0", " ").replace("\u2006", " ")
    value = MARKER_RE.sub("", value)
    value = SPACE_RE.sub(" ", value)
    return value.strip()


def child_text(element: ET.Element) -> tuple[str, list[str]]:
    raw = "".join(element.itertext())
    images = IMAGE_RE.findall(raw)
    raw = IMAGE_RE.sub("", raw)
    # LibreOffice materializes WinHelp footnote anchors as their sequence number.
    for node in element.iter():
        title = node.attrib.get("title", "")
        if title.startswith("Footnote: "):
            anchor_text = "".join(node.itertext())
            if anchor_text:
                raw = raw.replace(anchor_text, "", 1)
    return clean_text(raw), images


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("html", type=Path)
    parser.add_argument("help_decompiled", type=Path)
    parser.add_argument("decoded_graphics", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    image_output = args.output / "images"
    image_output.mkdir(parents=True, exist_ok=True)

    tree = ET.parse(args.html)
    root = tree.getroot()
    namespace = root.tag.partition("}")[0] + "}" if root.tag.startswith("{") else ""
    body = root.find(f"{namespace}body")
    if body is None:
        raise ValueError("XHTML body not found")

    topics: list[dict[str, object]] = []
    current: dict[str, object] | None = None
    for element in body:
        classes = element.attrib.get("class", "")
        if "paragraph-Footnote" in classes:
            break
        footnotes = [node.attrib.get("title", "") for node in element.iter() if "title" in node.attrib]
        title_note = next((item for item in footnotes if item.startswith("Footnote: $ ")), None)
        context_note = next((item for item in footnotes if item.startswith("Footnote: # ")), None)
        browse_note = next((item for item in footnotes if item.startswith("Footnote: + ")), None)
        text, images = child_text(element)
        if title_note:
            if current:
                topics.append(current)
            current = {
                "id": len(topics),
                "title": title_note.removeprefix("Footnote: $ ").strip(),
                "context": context_note.removeprefix("Footnote: # ").strip() if context_note else "",
                "browse": browse_note.removeprefix("Footnote: + ").strip() if browse_note else "",
                "heading": text,
                "paragraphs": [],
                "images": images,
            }
        elif current:
            if text:
                paragraphs = current["paragraphs"]
                assert isinstance(paragraphs, list)
                paragraphs.append(text)
            current_images = current["images"]
            assert isinstance(current_images, list)
            current_images.extend(images)
    if current:
        topics.append(current)

    referenced: set[str] = set()
    for topic in topics:
        converted: list[str] = []
        for source_name in topic["images"]:
            source_stem = Path(str(source_name)).stem
            destination_name = f"{source_stem}.png"
            if destination_name not in referenced:
                decoded = args.decoded_graphics / destination_name
                direct = args.help_decompiled / source_name
                destination = image_output / destination_name
                if decoded.exists():
                    shutil.copyfile(decoded, destination)
                elif direct.exists():
                    with Image.open(direct) as image:
                        image.save(destination)
                else:
                    raise FileNotFoundError(f"missing help graphic {source_name}")
                referenced.add(destination_name)
            converted.append(f"images/{destination_name}")
        topic["images"] = converted

    payload = {"topic_count": len(topics), "topics": topics}
    (args.output / "topics.json").write_text(
        json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"Extracted {len(topics)} help topics with {len(referenced)} referenced images")


if __name__ == "__main__":
    main()
