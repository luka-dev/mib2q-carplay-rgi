"""Do not let withdrawn six-bus diagnostics silently become current evidence."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import numpy as np

PATH=Path(__file__).resolve().parents[1]/'stereo-upmix-render/cabin_model.py'


class LegacyEntrypointTests(unittest.TestCase):
    def test_imported_analysis_requires_obsolete_model_opt_in(self):
        spec=importlib.util.spec_from_file_location('obsolete_cabin_model',PATH)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with self.assertRaisesRegex(ValueError,'obsolete transposed'):
            module.analyze(np.zeros((2,2)),np.zeros((2,6)),.3)

    def test_cli_refuses_before_any_render_or_output_write(self):
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/'should-not-exist'
            run=subprocess.run([sys.executable,str(PATH),'--renderer','absent',
                '--model','absent','--guard','absent','--out',str(output),'absent.wav'],
                capture_output=True,text=True)
            self.assertEqual(run.returncode,2)
            self.assertIn('obsolete transposed six-bus model',run.stderr)
            self.assertFalse(output.exists())


if __name__=='__main__':unittest.main()
