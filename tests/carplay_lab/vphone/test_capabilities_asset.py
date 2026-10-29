import unittest

from .capabilities_asset import audit_selection, string_matches


class CapabilitiesAssetTests(unittest.TestCase):
    def test_case_insensitive_exact_and_symmetric_contains(self):
        self.assertTrue(string_matches("Audi MMI", "AUDI MMI"))
        self.assertTrue(string_matches("*MIB2*", "prefix-mib2-suffix"))
        self.assertTrue(string_matches("prefix-MIB2-suffix", "*mib2*"))
        for a, b in (("AUDI*", "AUDI MMI"), ("*AUDI", "MY AUDI"),
                     ("*", "anything"), ("*AUDI*", "*MMI*"), (30, "30")):
            self.assertFalse(string_matches(a, b))
        with self.assertRaises(ValueError):
            string_matches("é", "É")

    def test_missing_or_mismatching_predicate_rejects_whole_row(self):
        asset = {"properties": ["name", "model"], "vehicles": [
            {"name": "AUDI", "model": "MIB2", "features": {"example": 1}}]}
        for config in ({"name": "AUDI"}, {"name": "AUDI", "model": "MIB3"}):
            result = audit_selection(asset, config)
            self.assertEqual(result["possible_winners"], [])
            self.assertTrue(result["default_features_used"])

    def test_ppid_bucket_does_not_have_global_priority(self):
        asset = {"properties": ["PPID", "name", "model"], "vehicles": [
            {"PPID": "target", "features": {"userInfo": {"mapsRefreshRate": 30}}},
            {"name": "AUDI", "model": "MIB2", "features": {"insets": "8"}}]}
        result = audit_selection(asset, {"PPID": "target", "name": "AUDI", "model": "MIB2"})
        self.assertEqual(result["buckets"]["PPID"]["local_score"], 1)
        self.assertEqual(result["buckets"]["name"]["local_score"], 2)
        self.assertEqual(result["possible_winners"], [0, 1])
        self.assertTrue(result["ambiguous_selection"])

    def test_value_index_is_shared_and_ties_are_not_ordered(self):
        asset = {"properties": ["name", "model"], "vehicles": [
            {"name": "X", "features": {}}, {"model": "X", "features": {}}]}
        result = audit_selection(asset, {"name": "X", "model": "X"})
        self.assertEqual(result["buckets"]["name"]["satisfied_rows"], [0, 1])
        self.assertEqual(result["possible_winners"], [0, 1])


if __name__ == "__main__":
    unittest.main()
