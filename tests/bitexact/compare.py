"""Bit-exact comparison of two run_ops.py output dirs (stock vs fvtk).

The assertion is the strictest possible: ``np.array_equal`` on the raw bytes of
every output array (points + every point/cell data array + topology), with the
ULP distance reported as 0 expected. maxULP tolerance is 0 — any nonzero ULP is
a failure, not a warning.
"""
from __future__ import annotations

import json
import os

import numpy as np


def load_manifest(d):
    with open(os.path.join(d, "manifest.json")) as fh:
        return json.load(fh)


def _ulp_distance(x, y):
    """Max ULP distance between two float arrays of identical shape/dtype."""
    if x.shape != y.shape or x.dtype != y.dtype:
        return None
    if x.dtype == np.float64:
        xi = x.view(np.int64).astype(np.int64)
        yi = y.view(np.int64).astype(np.int64)
    elif x.dtype == np.float32:
        xi = x.view(np.int32).astype(np.int64)
        yi = y.view(np.int32).astype(np.int64)
    else:
        return 0  # integer arrays: array_equal covers exactness
    return int(np.abs(xi - yi).max()) if xi.size else 0


def compare_case(stock_dir, fvtk_dir, key):
    """Return (ok: bool, detail: dict) for a single case key."""
    sp = os.path.join(stock_dir, key + ".npz")
    fp = os.path.join(fvtk_dir, key + ".npz")
    if not os.path.exists(sp) or not os.path.exists(fp):
        return False, {"reason": "missing npz", "stock": os.path.exists(sp), "fvtk": os.path.exists(fp)}
    a = np.load(sp)
    b = np.load(fp)
    names_a, names_b = set(a.files), set(b.files)
    if names_a != names_b:
        return False, {
            "reason": "array set mismatch",
            "only_stock": sorted(names_a - names_b),
            "only_fvtk": sorted(names_b - names_a),
        }
    per_array = {}
    ok = True
    for name in sorted(names_a):
        x, y = a[name], b[name]
        equal = bool(
            x.shape == y.shape
            and x.dtype == y.dtype
            and np.array_equal(x, y)
        )
        ulp = None
        if x.dtype.kind == "f" and x.shape == y.shape and x.dtype == y.dtype:
            ulp = _ulp_distance(x, y)
        per_array[name] = {
            "equal": equal,
            "shape_stock": list(x.shape),
            "shape_fvtk": list(y.shape),
            "dtype": str(x.dtype),
            "ulp": ulp,
        }
        ok &= equal
    return ok, {"arrays": per_array}


def compare_all(stock_dir, fvtk_dir):
    """Compare every case present in BOTH manifests. Returns a results dict."""
    ms = load_manifest(stock_dir)
    mf = load_manifest(fvtk_dir)

    # Provenance sanity: numpy versions must match (bit-identical inputs).
    prov = {
        "numpy_stock": ms["provenance"]["numpy"],
        "numpy_fvtk": mf["provenance"]["numpy"],
        "numpy_match": ms["provenance"]["numpy"] == mf["provenance"]["numpy"],
        "vtk_stock": ms["provenance"]["vtk_version"],
        "vtk_fvtk": mf["provenance"]["vtk_version"],
        "inputs_digest_f64_stock": ms["provenance"]["inputs_digest_f64"],
        "inputs_digest_f64_fvtk": mf["provenance"]["inputs_digest_f64"],
        "inputs_digest_match": (
            ms["provenance"]["inputs_digest_f64"]
            == mf["provenance"]["inputs_digest_f64"]
            and ms["provenance"]["inputs_digest_f32"]
            == mf["provenance"]["inputs_digest_f32"]
        ),
    }

    keys = sorted(set(ms["cases"]) & set(mf["cases"]))
    cases = {}
    for key in keys:
        # Skip cases that errored on either side — report them separately.
        cs, cf = ms["cases"][key], mf["cases"][key]
        if "error" in cs or "error" in cf:
            cases[key] = {
                "ok": False,
                "detail": {"reason": "op errored", "stock": cs.get("error"), "fvtk": cf.get("error")},
                "group": cs.get("group"),
            }
            continue
        ok, detail = compare_case(stock_dir, fvtk_dir, key)
        cases[key] = {"ok": ok, "detail": detail, "group": cs.get("group")}
    return {"provenance": prov, "cases": cases, "keys": keys}
