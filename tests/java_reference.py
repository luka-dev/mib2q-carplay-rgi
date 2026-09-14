"""Normalize known decompiler scaffolding for comparisons; never edit firmware input."""
import re


def normalize_class_literals(source):
    # Java 1.4 javac's class$ cache, retained by the refreshed VF export even
    # though its synthetic class$(String) helper is omitted by that decompiler.
    pattern = re.compile(
        r'(?P<f>[\w.$]*class\$[\w$]+)\s*==\s*null\s*\?\s*\(\s*'
        r'(?P=f)\s*=\s*(?:[\w.$]+\.)?class\$\(\s*"(?P<type>[^"\n]+)"\s*\)\s*\)\s*:\s*(?P=f)')
    source = pattern.sub(lambda m: m['type'].replace('$', '.') + '.class', source)
    source = re.sub(r'^\s*(?:public |private |protected )?static Class class\$[\w$]+;\n', '', source, flags=re.M)
    return source.replace('"isNativeLittleEndian"', '""').replace('"isNativeLittleEdian"', '""')
