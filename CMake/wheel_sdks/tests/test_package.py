from __future__ import annotations

import importlib.metadata

import cvista_sdk as m
from packaging.specifiers import SpecifierSet


def test_version():
    assert importlib.metadata.version("cvista_sdk") == m.__version__


def test_requires_python_admits_311():
    # cvista ships a Python 3.11 wheel, so the matching SDK must install there too.
    requires = importlib.metadata.metadata("cvista_sdk")["Requires-Python"]
    assert SpecifierSet(requires).contains("3.11")
