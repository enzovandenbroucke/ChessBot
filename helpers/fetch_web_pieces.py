"""Fetch the attributed Cburnett SVGs for the browser UI (not used at runtime)."""
from pathlib import Path
import hashlib
import json
import re
import time
import urllib.request

root = Path(__file__).resolve().parents[1] / "web" / "pieces"
root.mkdir(parents=True, exist_ok=True)
records = []
for color, suffix in (("white", "lt"), ("black", "dt")):
    for name, code in (("pawn", "p"), ("knight", "n"), ("bishop", "b"),
                       ("rook", "r"), ("queen", "q"), ("king", "k")):
        original = f"Chess_{code}{suffix}45.svg"
        source = f"https://commons.wikimedia.org/wiki/Special:Redirect/file/{original}"
        request = urllib.request.Request(source, headers={"User-Agent": "ChessBot-portfolio/1.0 (asset provenance)"})
        filename = f"{color}-{name}.svg"
        if (root / filename).exists():
            data = (root / filename).read_bytes()
            resolved = source
        else:
            time.sleep(2)
            with urllib.request.urlopen(request, timeout=30) as response:
                data = response.read()
                resolved = response.url
        if b"<svg" not in data or b"<html" in data.lower():
            raise ValueError(f"Unexpected artwork response for {original}")
        # The original 45px drawings need a viewBox to scale with the board.
        opening_tag = re.search(rb"<svg\b[^>]*>", data).group()
        if b"viewBox=" not in opening_tag:
            data = data.replace(opening_tag, opening_tag.replace(b"<svg", b'<svg viewBox="0 0 45 45"', 1), 1)
        (root / filename).write_bytes(data)
        records.append({"file": filename, "source": f"https://commons.wikimedia.org/wiki/File:{original}",
                        "download": resolved, "sha256": hashlib.sha256(data).hexdigest()})
        print(filename, flush=True)
(root / "sources.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
