#!/usr/bin/env python3
"""Original 22 s transition protocol on frozen review-37 checkpoints.

This is a comparability audit; it cannot replace the precommitted full-song gate.
"""

import os

os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

import diag_transitions as DT
import sep_net_fine_role as Fine


if __name__ == "__main__":
    DT.SN = Fine
    DT.main()
