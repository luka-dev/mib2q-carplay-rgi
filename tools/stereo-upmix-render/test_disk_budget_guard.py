import math
import unittest
from disk_budget_guard import DiskBudgetGuard


class DiskBudgetGuardTests(unittest.TestCase):
    def test_hysteresis(self):
        g=DiskBudgetGuard()
        self.assertIsNone(g.observe(97))
        self.assertEqual(g.observe(22),'pause')
        self.assertTrue(g.paused)
        self.assertIsNone(g.observe(23))
        self.assertTrue(g.paused)
        self.assertIsNone(g.observe(10))
        self.assertEqual(g.observe(27),'resume')
        self.assertFalse(g.paused)
        self.assertIsNone(g.observe(26))

    def test_invalid_thresholds(self):
        for low,high in ((0,27),(27,22),(22,22),(math.nan,27)):
            with self.assertRaises(ValueError):DiskBudgetGuard(low,high)

    def test_invalid_reading(self):
        for value in (-1,math.inf,math.nan):
            with self.assertRaises(ValueError):DiskBudgetGuard().observe(value)


if __name__=='__main__':unittest.main()
