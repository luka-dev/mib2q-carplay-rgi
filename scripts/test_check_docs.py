#!/usr/bin/env python3
"""Regression checks for documentation validation; no third-party dependencies."""
import tempfile
import unittest
from pathlib import Path
import check_docs


class DocumentationChecks(unittest.TestCase):
    def test_examples_and_footnotes_are_not_links(self):
        text = '''`[not a link](missing.md)`
```md
[example](missing.md)
```
[^source]: Author, [real citation](source.md).
'''
        self.assertEqual(list(check_docs.links(text)), [(5, 'source.md')])

    def test_links_images_references_and_html(self):
        text = '[`code`](file.py) ![pic](image.png) [x](note.md#part)\n[ref]: <space%20name.md>\n<a href="other.md">read</a>'
        self.assertEqual({url for _, url in check_docs.links(text)},
                         {'file.py', 'image.png', 'note.md#part', 'space%20name.md', 'other.md'})

    def test_parenthesized_target(self):
        self.assertEqual(list(check_docs.links('[x](folder/a(b).md)')), [(1, 'folder/a(b).md')])

    def test_duplicate_and_explicit_anchors(self):
        text = '# Header\n## Header\n<a id="stable"></a>\n## `Code` & [link](a.md)\n```\n# Not a heading\n```'
        self.assertEqual(check_docs.anchors(text), {'header', 'header-1', 'stable', 'code--link'})

    def test_missing_target_and_anchor_fail_but_external_is_skipped(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            page = root / 'index.md'
            (root / 'target.md').write_text('# Present\n')
            page.write_text('[ok](target.md#present) [bad](target.md#absent) [missing](gone.md) [web](https://example.org/x)')
            original = check_docs.ROOT
            try:
                check_docs.ROOT = root
                errors = check_docs.check([page])
            finally:
                check_docs.ROOT = original
            self.assertEqual(len(errors), 2)
            self.assertTrue(any('missing anchor' in x for x in errors))
            self.assertTrue(any('missing target' in x for x in errors))


if __name__ == '__main__':
    unittest.main()
