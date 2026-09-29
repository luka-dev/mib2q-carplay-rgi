#!/usr/bin/env python3
"""Run the original 22 s transition diagnostic on review-35 checkpoints.

This is a protocol-comparability audit after the frozen full-song gate. It does
not change the review-35 promotion rule or train any checkpoint.
"""

import os

os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

import diag_transitions as DT
import sep_net_state as State


if __name__ == "__main__":
    DT.SN = State
    DT.main()
