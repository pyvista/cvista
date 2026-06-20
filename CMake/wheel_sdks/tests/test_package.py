from __future__ import annotations

import importlib.metadata

import fvtk_sdk as m


def test_version():
    assert importlib.metadata.version("fvtk_sdk") == m.__version__
