"""Reproducible source inventory against the refreshed, read-only MU1316 tree."""
import difflib
import hashlib
import json
import sys
from pathlib import Path

from java_reference import normalize_class_literals

project, reference, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
inventory = []
for source in sorted((project / "java_patch").rglob("*.java")):
    relative = source.relative_to(project / "java_patch")
    stock = reference / relative
    entry = {"path": str(relative), "replacement": stock.is_file()}
    if stock.is_file():
        entry["stock_sha256"] = hashlib.sha256(stock.read_bytes()).hexdigest()
        diff = difflib.unified_diff(
            normalize_class_literals(stock.read_text()).splitlines(True),
            source.read_text().splitlines(True),
            fromfile=str(stock), tofile=str(source))
        (output / (source.name + ".diff")).write_text("".join(diff))
    inventory.append(entry)
(output / "sources.json").write_text(json.dumps(inventory, indent=2) + "\n")
replacements = sum(entry["replacement"] for entry in inventory)
print("Java source inventory: %d sources / %d full stock replacements" % (len(inventory), replacements))
