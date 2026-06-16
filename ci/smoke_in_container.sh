#!/usr/bin/env bash
#
# Smoke-test an already-built fvtk manylinux wheel from INSIDE the
# manylinux2014 container (glibc 2.17, cp313). Import + compute + WebCore, then an
# offscreen EGL/OSMesa render under xvfb. The wheel is auditwheel-self-contained,
# so only the *system* GL/X runtime libs are installed here (no -devel, no build).
#
# Usage: bash ci/smoke_in_container.sh [WHEELDIR]   (default /src/wheelhouse)
#
# Intended to be invoked as the command of `docker run -v <repo>:/src:ro ...` from
# an ordinary host runner — that keeps actions/checkout (modern Node) on the host
# instead of the glibc-2.17 container where it cannot start.
set -euxo pipefail

WHEELDIR="${1:-/src/wheelhouse}"
SRC="${SRC:-/src}"

yum install -y \
  mesa-libGL mesa-libEGL mesa-libOSMesa mesa-libGLU libglvnd \
  libX11 libXcursor libXt libXext mesa-dri-drivers xorg-x11-server-Xvfb >/dev/null

PYBIN=/opt/python/cp313-cp313/bin
"$PYBIN/python" -m venv /tmp/v
. /tmp/v/bin/activate
pip install --upgrade pip numpy >/dev/null
pip install "$WHEELDIR"/*.whl

python - <<'PY'
import numpy as np, fvtk
from fvtk.vtkCommonCore import vtkVersion
print("VTK", vtkVersion.GetVTKVersion())
from fvtk.vtkWebCore import vtkWebApplication  # restored module (WebCore link fix)
from fvtk.util.numpy_support import numpy_to_vtk, vtk_to_numpy
a = np.arange(30, dtype=np.float64).reshape(10, 3)
assert np.allclose(a, vtk_to_numpy(numpy_to_vtk(a)))
from fvtk.vtkFiltersSources import vtkSphereSource
from fvtk.vtkFiltersCore import vtkTriangleFilter
s = vtkSphereSource(); s.SetThetaResolution(16); s.SetPhiResolution(16)
t = vtkTriangleFilter(); t.SetInputConnection(s.GetOutputPort()); t.Update()
assert t.GetOutput().GetNumberOfCells() > 0
print("import + compute + WebCore OK")
PY

FVTK_IMPORT_NAME=fvtk xvfb-run -a -s "-screen 0 1280x1024x24" python "$SRC/ci/smoke_test.py"
