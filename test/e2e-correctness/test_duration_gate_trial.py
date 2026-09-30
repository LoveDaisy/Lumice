"""Throwaway: two deliberately slow, unregistered tests that the duration registry gate must reject.

Never merged. Exists only on a trial branch to show the gate turning CI red.
"""
import time

import pytest


def test_unregistered_slow_fast_suite():
    time.sleep(70)


@pytest.mark.slow
def test_unregistered_slow_slow_suite():
    time.sleep(70)
